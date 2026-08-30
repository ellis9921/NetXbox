#include "html.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static int safe_stricmp(const char* a, const char* b) {
    if (!a || !b) return a ? 1 : (b ? -1 : 0);
    while (*a && *b) {
        int ca = (unsigned char)*a;
        int cb = (unsigned char)*b;
        int la = tolower(ca);
        int lb = tolower(cb);
        if (la != lb) return la - lb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static const char* TAG_NAMES[] = {
    "unknown",
    "html", "head", "body",
    "title", "meta", "link", "style", "script",
    "div", "span", "p", "a", "br", "hr",
    "h1", "h2", "h3", "h4", "h5", "h6",
    "ul", "ol", "li", "dl", "dt", "dd",
    "table", "tr", "td", "th", "thead", "tbody",
    "img", "video", "audio", "canvas",
    "input", "button", "select", "option", "textarea",
    "form", "label", "fieldset", "legend",
    "pre", "code", "blockquote", "q",
    "strong", "em", "b", "i", "u", "s",
    "small", "sub", "sup", "del", "ins",
    "header", "footer", "nav", "main", "article", "section",
};

HtmlTag html_tag_from_string(const char* name) {
    for (int i = 1; i < HTML_TAG_COUNT; i++) {
        if (safe_stricmp(name, TAG_NAMES[i]) == 0) return (HtmlTag)i;
    }
    return HTML_TAG_UNKNOWN;
}

const char* html_tag_to_string(HtmlTag tag) {
    if (tag > HTML_TAG_UNKNOWN && tag < HTML_TAG_COUNT) return TAG_NAMES[tag];
    return TAG_NAMES[HTML_TAG_UNKNOWN];
}

static bool is_void_element(HtmlTag tag) {
    return tag == HTML_TAG_BR || tag == HTML_TAG_HR || tag == HTML_TAG_IMG ||
           tag == HTML_TAG_META || tag == HTML_TAG_LINK || tag == HTML_TAG_INPUT;
}

HtmlNode* html_create_node(HtmlNodeType type) {
    HtmlNode* node = (HtmlNode*)calloc(1, sizeof(HtmlNode));
    node->type = type;
    return node;
}

void html_node_add_child(HtmlNode* parent, HtmlNode* child) {
    if (!parent || !child) return;
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity = parent->child_capacity == 0 ? 8 : parent->child_capacity * 2;
        parent->children = (HtmlNode**)realloc(parent->children, parent->child_capacity * sizeof(HtmlNode*));
    }
    parent->children[parent->child_count++] = child;
    child->parent = parent;
}

const char* html_node_get_attribute(HtmlNode* node, const char* name) {
    if (!node) return NULL;
    for (int i = 0; i < node->attributes.count; i++) {
        if (safe_stricmp(node->attributes.items[i].name.data, name) == 0)
            return node->attributes.items[i].value.data;
    }
    return NULL;
}

bool html_node_has_attribute(HtmlNode* node, const char* name) {
    return html_node_get_attribute(node, name) != NULL;
}

typedef struct {
    const char* data;
    int length;
    int pos;
    int depth;
} HtmlParser;

static char parser_peek(HtmlParser* p) {
    if (p->pos >= p->length) return '\0';
    return p->data[p->pos];
}

static char parser_advance(HtmlParser* p) {
    if (p->pos >= p->length) return '\0';
    return p->data[p->pos++];
}

static void parser_skip_whitespace(HtmlParser* p) {
    while (p->pos < p->length && (p->data[p->pos] == ' ' || p->data[p->pos] == '\t' ||
           p->data[p->pos] == '\n' || p->data[p->pos] == '\r'))
        p->pos++;
}

static String parser_read_until(HtmlParser* p, const char* delimiters) {
    String result = {0};
    while (p->pos < p->length) {
        char c = p->data[p->pos];
        if (strchr(delimiters, c)) break;
        string_append_char(&result, c);
        p->pos++;
    }
    return result;
}

static String parser_read_tag_name(HtmlParser* p) {
    String result = {0};
    while (p->pos < p->length) {
        char c = p->data[p->pos];
        if (isalnum((unsigned char)c) || c == '-' || c == '_') {
            string_append_char(&result, (char)tolower((unsigned char)c));
            p->pos++;
        } else break;
    }
    return result;
}

static String parser_read_attribute_value(HtmlParser* p) {
    String result = {0};
    parser_skip_whitespace(p);

    if (p->pos < p->length && (p->data[p->pos] == '"' || p->data[p->pos] == '\'')) {
        char quote = parser_advance(p);
        while (p->pos < p->length && p->data[p->pos] != quote) {
            if (p->data[p->pos] == '&') {
                if (strncmp(p->data + p->pos, "&amp;", 5) == 0) {
                    string_append_char(&result, '&'); p->pos += 5;
                } else if (strncmp(p->data + p->pos, "&lt;", 4) == 0) {
                    string_append_char(&result, '<'); p->pos += 4;
                } else if (strncmp(p->data + p->pos, "&gt;", 4) == 0) {
                    string_append_char(&result, '>'); p->pos += 4;
                } else if (strncmp(p->data + p->pos, "&quot;", 6) == 0) {
                    string_append_char(&result, '"'); p->pos += 6;
                } else if (strncmp(p->data + p->pos, "&#39;", 5) == 0) {
                    string_append_char(&result, '\''); p->pos += 5;
                } else {
                    string_append_char(&result, parser_advance(p));
                }
            } else {
                string_append_char(&result, parser_advance(p));
            }
        }
        if (p->pos < p->length) p->pos++;
    } else {
        result = parser_read_until(p, " \t\n\r>");
    }
    return result;
}

static void parser_parse_attributes(HtmlParser* p, HtmlNode* node) {
    while (p->pos < p->length) {
        parser_skip_whitespace(p);
        char c = parser_peek(p);
        if (c == '>' || c == '/' || c == '\0') break;

        String name = parser_read_tag_name(p);
        if (name.length == 0) { string_free(&name); break; }

        parser_skip_whitespace(p);
        if (parser_peek(p) == '=') {
            parser_advance(p);
            String value = parser_read_attribute_value(p);

            if (node->attributes.count >= node->attributes.capacity) {
                node->attributes.capacity = node->attributes.capacity == 0 ? 8 : node->attributes.capacity * 2;
                node->attributes.items = (HtmlAttribute*)realloc(node->attributes.items,
                    node->attributes.capacity * sizeof(HtmlAttribute));
            }
            node->attributes.items[node->attributes.count].name = name;
            node->attributes.items[node->attributes.count].value = value;
            node->attributes.count++;
        } else {
            if (node->attributes.count >= node->attributes.capacity) {
                node->attributes.capacity = node->attributes.capacity == 0 ? 8 : node->attributes.capacity * 2;
                node->attributes.items = (HtmlAttribute*)realloc(node->attributes.items,
                    node->attributes.capacity * sizeof(HtmlAttribute));
            }
            node->attributes.items[node->attributes.count].name = name;
            node->attributes.items[node->attributes.count].value = string_create("");
            node->attributes.count++;
        }
    }
}

static HtmlNode* parser_parse_element(HtmlParser* p, HtmlNode* parent);

static HtmlNode* parser_parse_node(HtmlParser* p) {
    while (p->pos < p->length) {
        if (p->data[p->pos] == '<') {
            p->pos++;
            if (p->pos < p->length && p->data[p->pos] == '!') {
                while (p->pos < p->length && p->data[p->pos] != '>') p->pos++;
                if (p->pos < p->length) p->pos++;
                continue;
            }
            if (p->pos < p->length && p->data[p->pos] == '/') {
                p->pos++;
                return NULL;
            }
            return parser_parse_element(p, NULL);
        }

        HtmlNode* text = html_create_node(HTML_NODE_TEXT);
        text->text = parser_read_until(p, "<");
        return text;
    }
    return NULL;
}

static HtmlNode* parser_parse_element(HtmlParser* p, HtmlNode* parent) {
    if (p->depth > 1024) return NULL;
    p->depth++;
    parser_skip_whitespace(p);

    String tag_name = parser_read_tag_name(p);
    if (tag_name.length == 0) { p->depth--; return NULL; }

    HtmlNode* node = html_create_node(HTML_NODE_ELEMENT);
    node->tag_name = tag_name;
    node->tag = html_tag_from_string(tag_name.data);

    parser_parse_attributes(p, node);

    parser_skip_whitespace(p);

    bool self_closing = false;
    if (parser_peek(p) == '/') {
        p->pos++;
        self_closing = true;
    }

    if (parser_peek(p) == '>') p->pos++;

    if (node->tag == HTML_TAG_TITLE) {
        String content = {0};
        while (p->pos < p->length) {
            if (p->data[p->pos] == '<') {
                if (strncmp(p->data + p->pos, "</title>", 8) == 0) {
                    p->pos += 8;
                    break;
                }
            }
            string_append_char(&content, parser_advance(p));
        }
        node->text = content;
        p->depth--;
        return node;
    }

    if (node->tag == HTML_TAG_SCRIPT || node->tag == HTML_TAG_STYLE) {
        while (p->pos < p->length) {
            if (p->data[p->pos] == '<' && p->pos + 1 < p->length && p->data[p->pos + 1] == '/') {
                char end_buf[16] = {0};
                int saved_pos = p->pos + 2;
                int ei = 0;
                while (saved_pos < p->length && ei < 14) {
                    char c = p->data[saved_pos];
                    if (c == '>' || c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
                    end_buf[ei++] = (char)tolower((unsigned char)c);
                    saved_pos++;
                }
                if (strcmp(end_buf, node->tag_name.data) == 0) {
                    p->pos = saved_pos;
                    while (p->pos < p->length && p->data[p->pos] != '>') p->pos++;
                    if (p->pos < p->length) p->pos++;
                    break;
                }
            }
            p->pos++;
        }
        p->depth--;
        return node;
    }

    if (self_closing || is_void_element(node->tag)) {
        p->depth--;
        return node;
    }

    while (p->pos < p->length) {
        parser_skip_whitespace(p);

        if (p->pos < p->length && p->data[p->pos] == '<' &&
            p->pos + 1 < p->length && p->data[p->pos + 1] == '/') {
            p->pos += 2;
            String end_tag = parser_read_tag_name(p);
            while (p->pos < p->length && p->data[p->pos] != '>') p->pos++;
            if (p->pos < p->length) p->pos++;
            string_free(&end_tag);
            break;
        }

        HtmlNode* child = parser_parse_node(p);
        if (child) {
            html_node_add_child(node, child);
        } else {
            break;
        }
    }

    p->depth--;
    return node;
}

static void html_collect_title(HtmlNode* node, String* title) {
    if (!node) return;
    if (node->tag == HTML_TAG_TITLE && node->text.length > 0) {
        string_clear(title);
        string_append(title, node->text.data);
        return;
    }
    for (int i = 0; i < node->child_count; i++) {
        html_collect_title(node->children[i], title);
    }
}

HtmlDocument* html_parse(const char* html, int length) {
    if (!html || length <= 0) return NULL;

    HtmlDocument* doc = (HtmlDocument*)calloc(1, sizeof(HtmlDocument));
    doc->title = string_create("");

    HtmlParser parser;
    parser.data = html;
    parser.length = length;
    parser.pos = 0;

    doc->root = parser_parse_node(&parser);
    html_collect_title(doc->root, &doc->title);

    return doc;
}

static void html_free_node(HtmlNode* node) {
    if (!node) return;
    string_free(&node->tag_name);
    string_free(&node->text);
    for (int i = 0; i < node->attributes.count; i++) {
        string_free(&node->attributes.items[i].name);
        string_free(&node->attributes.items[i].value);
    }
    if (node->attributes.items) free(node->attributes.items);
    for (int i = 0; i < node->child_count; i++) {
        html_free_node(node->children[i]);
    }
    if (node->children) free(node->children);
    free(node);
}

void html_document_free(HtmlDocument* doc) {
    if (!doc) return;
    if (doc->root) html_free_node(doc->root);
    string_free(&doc->title);
    string_free(&doc->base_url);
    free(doc);
}

HtmlNode* html_find_element_by_id(HtmlNode* root, const char* id) {
    if (!root || !id) return NULL;
    const char* attr_id = html_node_get_attribute(root, "id");
    if (attr_id && strcmp(attr_id, id) == 0) return root;
    for (int i = 0; i < root->child_count; i++) {
        HtmlNode* found = html_find_element_by_id(root->children[i], id);
        if (found) return found;
    }
    return NULL;
}

HtmlNode* html_find_element_by_tag(HtmlNode* root, HtmlTag tag) {
    if (!root) return NULL;
    if (root->tag == tag) return root;
    for (int i = 0; i < root->child_count; i++) {
        HtmlNode* found = html_find_element_by_tag(root->children[i], tag);
        if (found) return found;
    }
    return NULL;
}
