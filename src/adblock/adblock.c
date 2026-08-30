#include "adblock.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static AdblockFilterType detect_filter_type(const char* line) {
    if (!line || !line[0]) return FILTER_TYPE_UNKNOWN;
    if (line[0] == '!' || line[0] == '[') return FILTER_TYPE_COMMENT;
    if (line[0] == '#' && line[1] == '#') return FILTER_TYPE_COSMETIC;
    if (line[0] == '#' && line[1] == '@') return FILTER_TYPE_COSMETIC;
    if (line[0] == '@' && line[1] == '@') return FILTER_TYPE_EXCEPTION;
    if (strncmp(line, "||", 2) == 0) return FILTER_TYPE_NETWORK;
    if (strstr(line, "##")) return FILTER_TYPE_COSMETIC;
    if (line[0] == '|' && line[1] == '|') return FILTER_TYPE_NETWORK;
    if (line[0] == '|') return FILTER_TYPE_NETWORK;
    if (strchr(line, '*') || strchr(line, '^')) return FILTER_TYPE_NETWORK;
    return FILTER_TYPE_UNKNOWN;
}

static void parse_filter_options(const char* options, uint32_t* flags, char* opt_domains, char* opt_not_domains) {
    if (!options) return;
    const char* p = options;
    while (*p) {
        while (*p == ',') p++;
        if (strncmp(p, "third-party", 10) == 0 || strncmp(p, "3p", 2) == 0) {
            *flags |= FILTER_FLAG_THIRD_PARTY;
            p += strcspn(p, ",");
        } else if (strncmp(p, "first-party", 11) == 0 || strncmp(p, "1p", 2) == 0) {
            *flags |= FILTER_FLAG_FIRST_PARTY;
            p += strcspn(p, ",");
        } else if (strncmp(p, "domain=", 7) == 0) {
            p += 7;
            int idx = 0;
            while (*p && *p != ',' && idx < ADBLOCK_MAX_HOSTNAME_LEN - 2) {
                if (*p == '|') { opt_domains[idx++] = '|'; p++; }
                else { opt_domains[idx++] = *p++; }
            }
            opt_domains[idx] = '\0';
        } else if (strncmp(p, "~domain=", 8) == 0) {
            p += 8;
            int idx = 0;
            while (*p && *p != ',' && idx < ADBLOCK_MAX_HOSTNAME_LEN - 2) {
                if (*p == '|') { opt_not_domains[idx++] = '|'; p++; }
                else { opt_not_domains[idx++] = *p++; }
            }
            opt_not_domains[idx] = '\0';
        } else {
            p += strcspn(p, ",");
        }
    }
}

static void parse_network_filter(AdblockFilter* f, const char* line) {
    const char* p = line;
    memset(f, 0, sizeof(AdblockFilter));
    f->flags = 0;

    if (p[0] == '@' && p[1] == '@') {
        f->is_exception = true;
        p += 2;
    }

    if (p[0] == '|' && p[1] == '|') {
        f->flags |= FILTER_FLAG_LEFT_ANCHOR;
        p += 2;
    } else if (p[0] == '|') {
        f->flags |= FILTER_FLAG_LEFT_ANCHOR;
        p += 1;
    }

    const char* options = NULL;
    char pattern_buf[ADBLOCK_MAX_PATTERN_LEN];
    int pi = 0;

    for (int i = 0; p[i] && i < ADBLOCK_MAX_PATTERN_LEN - 1; i++) {
        if (p[i] == '$') {
            options = &p[i + 1];
            break;
        }
        pattern_buf[pi++] = p[i];
    }
    pattern_buf[pi] = '\0';

    int plen = (int)strlen(pattern_buf);
    if (plen > 0 && pattern_buf[plen - 1] == '|') {
        f->flags |= FILTER_FLAG_RIGHT_ANCHOR;
        pattern_buf[plen - 1] = '\0';
        plen--;
    }

    if (strstr(pattern_buf, "*") || strstr(pattern_buf, "^") || strstr(pattern_buf, ".*")) {
        f->flags |= FILTER_FLAG_IS_REGEX;
    }

    strncpy(f->pattern, pattern_buf, ADBLOCK_MAX_PATTERN_LEN - 1);

    const char* host_end = f->pattern;
    if (f->pattern[0] == '^') {
        host_end = f->pattern;
    } else {
        const char* slash = strchr(f->pattern, '/');
        const char* caret = strchr(f->pattern, '^');
        if (slash) host_end = slash;
        else if (caret) host_end = caret;
        else host_end = f->pattern + strlen(f->pattern);
    }

    int hlen = (int)(host_end - f->pattern);
    if (hlen > 0 && hlen < ADBLOCK_MAX_HOSTNAME_LEN) {
        char hostname_buf[ADBLOCK_MAX_HOSTNAME_LEN] = {0};
        memcpy(hostname_buf, f->pattern, hlen);
        if (hostname_buf[0] == '^') hostname_buf[0] = '\0';
        strncpy(f->hostname, hostname_buf, ADBLOCK_MAX_HOSTNAME_LEN - 1);
    }

    if (options) {
        parse_filter_options(options, &f->flags, f->opt_domains, f->opt_not_domains);
    }
}

static bool match_pattern_simple(const char* url, const char* pattern, bool left_anchored, bool right_anchored) {
    if (!url || !pattern || !pattern[0]) return false;

    if (left_anchored && right_anchored) {
        return strcmp(url, pattern) == 0;
    }
    if (left_anchored) {
        return strncmp(url, pattern, strlen(pattern)) == 0;
    }
    if (right_anchored) {
        int ulen = (int)strlen(url);
        int plen = (int)strlen(pattern);
        if (plen > ulen) return false;
        return strcmp(url + ulen - plen, pattern) == 0;
    }
    return strstr(url, pattern) != NULL;
}

static bool match_wildcard(const char* url, const char* pattern) {
    if (!url || !pattern) return false;
    const char* up = url;
    const char* pp = pattern;

    while (*up && *pp) {
        if (*pp == '*') {
            pp++;
            if (!*pp) return true;
            const char* found = NULL;
            for (const char* s = up; *s; s++) {
                if (*s == *pp) { found = s; break; }
            }
            if (!found) return false;
            up = found;
        } else if (*pp == '^') {
            if (*up == '/' || *up == '?' || *up == '&' || *up == '=' || *up == '#' || *up == '.' || !*up) {
                pp++;
            } else {
                up++;
            }
        } else if (*pp == '|') {
            if (up == url || *up == '/' || *up == ':') {
                pp++;
            } else {
                up++;
            }
        } else {
            if (*up != *pp) return false;
            up++;
            pp++;
        }
    }
    while (*pp == '*') pp++;
    return !*pp && !*up;
}

static bool domain_matches(const char* domain, const char* list) {
    if (!domain || !list || !list[0]) return false;
    const char* p = list;
    while (*p) {
        while (*p == '|') p++;
        int len = (int)strcspn(p, "|");
        if (len > 0 && strncmp(domain, p, len) == 0) {
            int dlen = (int)strlen(domain);
            if (dlen == len || (dlen > len && domain[dlen - len - 1] == '.'))
                return true;
        }
        p += len;
    }
    return false;
}

void adblock_init(AdblockEngine* engine) {
    memset(engine, 0, sizeof(AdblockEngine));
    engine->enabled = true;
}

void adblock_load_from_string(AdblockEngine* engine, const char* filter_text) {
    if (!engine || !filter_text) return;

    const char* line = filter_text;
    while (*line) {
        const char* eol = strchr(line, '\n');
        int len = eol ? (int)(eol - line) : (int)strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ')) len--;

        if (len > 0 && len < 1024) {
            char buf[1024] = {0};
            memcpy(buf, line, len);
            buf[len] = '\0';

            AdblockFilterType type = detect_filter_type(buf);
            if (type == FILTER_TYPE_NETWORK || type == FILTER_TYPE_EXCEPTION) {
                if (engine->filter_count < ADBLOCK_MAX_FILTERS) {
                    AdblockFilter* f = &engine->filters[engine->filter_count];
                    parse_network_filter(f, buf);
                    if (type == FILTER_TYPE_EXCEPTION || f->is_exception) {
                        if (engine->exception_count < ADBLOCK_MAX_EXCEPTIONS) {
                            engine->exceptions[engine->exception_count++] = *f;
                        }
                    } else {
                        engine->filter_count++;
                    }
                }
            } else if (type == FILTER_TYPE_COSMETIC) {
                /* skip cosmetic for now */
            }
        }

        if (eol) line = eol + 1;
        else break;
    }
}

void adblock_load_hosts_from_string(AdblockEngine* engine, const char* hosts_text) {
    if (!engine || !hosts_text) return;

    const char* line = hosts_text;
    while (*line) {
        const char* eol = strchr(line, '\n');
        int len = eol ? (int)(eol - line) : (int)strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ')) len--;

        if (len > 3 && line[0] != '#' && line[0] != '!') {
            const char* p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (strncmp(p, "0.0.0.0", 7) == 0 || strncmp(p, "127.0.0.1", 9) == 0) {
                while (*p && *p != ' ' && *p != '\t') p++;
                while (*p == ' ' || *p == '\t') p++;
                if (*p && *p != '#' && engine->hosts_count < ADBLOCK_MAX_HOSTS) {
                    int hlen = (int)strcspn(p, " \t#");
                    if (hlen > 0 && hlen < ADBLOCK_MAX_HOSTNAME_LEN) {
                        strncpy(engine->hosts[engine->hosts_count].domain, p, hlen);
                        engine->hosts[engine->hosts_count].domain[hlen] = '\0';
                        engine->hosts_count++;
                    }
                }
            }
        }

        if (eol) line = eol + 1;
        else break;
    }
}

bool adblock_should_block(AdblockEngine* engine, const char* url, const char* source_domain, bool is_third_party) {
    if (!engine || !engine->enabled || !url || !url[0]) return false;

    for (int i = 0; i < engine->hosts_count; i++) {
        if (strstr(url, engine->hosts[i].domain))
            return true;
    }

    for (int i = 0; i < engine->filter_count; i++) {
        AdblockFilter* f = &engine->filters[i];
        bool matched = false;

        if (f->hostname[0]) {
            if (!strstr(url, f->hostname)) continue;
        }

        if (f->flags & FILTER_FLAG_IS_REGEX) {
            matched = match_wildcard(url, f->pattern);
        } else {
            matched = match_pattern_simple(url, f->pattern,
                (f->flags & FILTER_FLAG_LEFT_ANCHOR) != 0,
                (f->flags & FILTER_FLAG_RIGHT_ANCHOR) != 0);
        }

        if (matched) {
            if (f->flags & FILTER_FLAG_THIRD_PARTY && !is_third_party) continue;
            if (f->flags & FILTER_FLAG_FIRST_PARTY && is_third_party) continue;

            bool exception = false;
            for (int j = 0; j < engine->exception_count; j++) {
                AdblockFilter* e = &engine->exceptions[j];
                if (!e->hostname[0] || strstr(url, e->hostname)) {
                    if (e->flags & FILTER_FLAG_IS_REGEX) {
                        if (match_wildcard(url, e->pattern)) { exception = true; break; }
                    } else {
                        if (match_pattern_simple(url, e->pattern,
                            (e->flags & FILTER_FLAG_LEFT_ANCHOR) != 0,
                            (e->flags & FILTER_FLAG_RIGHT_ANCHOR) != 0)) { exception = true; break; }
                    }
                }
            }
            if (!exception) return true;
        }
    }

    return false;
}

bool adblock_should_hide(AdblockEngine* engine, const char* url, const char* css_selector) {
    (void)engine; (void)url; (void)css_selector;
    return false;
}
