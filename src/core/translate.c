#include "translate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ===========================================================================
 * Foreign-language detection (UTF-8 aware)
 * ========================================================================= */

static int utf8_cp_len(unsigned char c) {
    if (c < 0x80) return 1;
    if (c >= 0xC2 && c < 0xE0) return 2;
    if (c >= 0xE0 && c < 0xF0) return 3;
    if (c >= 0xF0) return 4;
    return 1;
}

/* Count how many UTF-8 codepoints in the buffer are "obviously non-English":
 * Cyrillic/Greek/CJK and the Latin-1 accented range. Punctuation/symbols are not. */
static int count_foreign_codepoints(const char* s, int len) {
    int count = 0, i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        int l = utf8_cp_len(c);
        uint32_t cp = c;
        if (l == 2 && i + 1 < len) {
            cp = ((c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F);
        } else if (l == 3 && i + 2 < len) {
            cp = ((c & 0x0F) << 12) | (((unsigned char)s[i + 1] & 0x3F) << 6) |
                 ((unsigned char)s[i + 2] & 0x3F);
        } else if (l == 4 && i + 3 < len) {
            cp = ((c & 0x07) << 18) | (((unsigned char)s[i + 1] & 0x3F) << 12) |
                 (((unsigned char)s[i + 2] & 0x3F) << 6) | ((unsigned char)s[i + 3] & 0x3F);
        }
        /* Accented Latin (Latin-1 supplement + Latin Extended):
         * common letters often carrying accents. */
        if ((cp >= 0x00C0 && cp <= 0x00FF) ||   /* À..ÿ */
            (cp >= 0x0100 && cp <= 0x02BF) ||   /* Ā..ȿ (Latin ext + IPA markers) */
            (cp >= 0x0400 && cp <= 0x04FF) ||   /* Cyrillic */
            (cp >= 0x0370 && cp <= 0x03FF) ||   /* Greek */
            cp >= 0x2E80) {                     /* CJK + misc ideographs */
            count++;
        }
        i += l;
    }
    return count;
}

bool translate_looks_foreign(const char* text) {
    if (!text) return false;
    int len = (int)strlen(text);
    if (len == 0) return false;

    /* Strongest signal: many non-ASCII foreign codepoints. */
    if (count_foreign_codepoints(text, len) >= 3) return true;

    /* Secondary signal: a handful of common foreign words even in ASCII
     * (e.g. Spanish/French/German words that share the Latin alphabet). */
    static const char* const ascii_markers[] = {
        "el ", "la ", "los ", "las ", "del ", "por ", "para ", "con ",
        "y ", "o ", "no ", "en el ", "de la ", "que ", "como ", "cuando ",
        "und ", "die ", "der ", "das ", "ist ", "mit ", "nicht ", "fuer ",
        "für ", "dass ", "auf ", "ein ", "eine ", "unden ", "af ", "og ",
        "det ", "med ", "och ", "att ", "av ", "som ", "il ", "lo ", "nel ",
        "che ", "per ", "come ", "della "
    };
    char lower[256];
    int lower_len = len < 255 ? len : 255;
    for (int i = 0; i < lower_len; i++) {
        char c = text[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        lower[i] = c;
    }
    lower[lower_len] = '\0';

    int marker_hits = 0;
    for (int i = 0; i < (int)(sizeof(ascii_markers) / sizeof(ascii_markers[0])); i++) {
        const char* m = ascii_markers[i];
        if (strstr(lower, m)) marker_hits++;
    }
    return marker_hits >= 2;
}

/* ===========================================================================
 * Dictionary (foreign -> English). Offline best-effort fallback.
 * Multi-word phrases are listed before single words so the longest match wins,
 * and lookups are done longest-first.
 * ========================================================================= */

typedef struct { const char* src; const char* dst; } DictEntry;

static const DictEntry k_dict[] = {
    /* --- Spanish --- */
    { "buenos dias", "good morning" },
    { "buenas noches", "good night" },
    { "por favor", "please" },
    { "muchas gracias", "thank you very much" },
    { "gracias", "thanks" },
    { "de nada", "you're welcome" },
    { "hola", "hello" },
    { "adios", "goodbye" },
    { "hasta luego", "see you later" },
    { "bienvenido", "welcome" },
    { "como estas", "how are you" },
    { "que tal", "how are you" },
    { "bueno", "good" },
    { "mal", "bad" },
    { "grande", "big" },
    { "pequeno", "small" },
    { "casa", "house" },
    { "hombre", "man" },
    { "mujer", "woman" },
    { "niño", "child" },
    { "nino", "child" },
    { "niña", "girl" },
    { "nina", "girl" },
    { "amigo", "friend" },
    { "amiga", "friend" },
    { "agua", "water" },
    { "comida", "food" },
    { "pan", "bread" },
    { "leche", "milk" },
    { "trabajo", "work" },
    { "trabajar", "to work" },
    { "escuela", "school" },
    { "universidad", "university" },
    { "libro", "book" },
    { "pelicula", "movie" },
    { "videojuego", "videogame" },
    { "mesa", "table" },
    { "silla", "chair" },
    { "ventana", "window" },
    { "puerta", "door" },
    { "coche", "car" },
    { "ciudad", "city" },
    { "país", "country" },
    { "pais", "country" },
    { "mundo", "world" },
    { "tiempo", "time" },
    { "dia", "day" },
    { "día", "day" },
    { "noche", "night" },
    { "hoy", "today" },
    { "mañana", "tomorrow" },
    { "manana", "tomorrow" },
    { "ayer", "yesterday" },
    { "ahora", "now" },
    { "si", "yes" },
    { "no", "no" },
    { "quiero", "I want" },
    { "necesito", "I need" },
    { "donde", "where" },
    { "cuando", "when" },
    { "quien", "who" },
    { "que", "what" },
    { "por que", "why" },
    { "como", "how" },
    { "el", "the" },
    { "la", "the" },
    { "los", "the" },
    { "las", "the" },
    { "un", "a" },
    { "una", "a" },
    { "con", "with" },
    { "sin", "without" },
    { "para", "for" },
    { "por", "by" },
    { "de", "of" },
    { "del", "of the" },
    { "en", "in" },
    { "y", "and" },
    /* --- French --- */
    { "bonjour", "hello" },
    { "bonsoir", "good evening" },
    { "merci", "thanks" },
    { "au revoir", "goodbye" },
    { "sil vous plait", "please" },
    { "s'il vous plaît", "please" },
    { "oui", "yes" },
    { "non", "no" },
    { "monsieur", "sir" },
    { "madame", "ma'am" },
    { "bon", "good" },
    { "mauvais", "bad" },
    { "grand", "big" },
    { "petit", "small" },
    { "maison", "house" },
    { "homme", "man" },
    { "femme", "woman" },
    { "enfant", "child" },
    { "ami", "friend" },
    { "eau", "water" },
    { "nourriture", "food" },
    { "pain", "bread" },
    { "livre", "book" },
    { "cinema", "movie theater" },
    { "cinéma", "movie theater" },
    { "ville", "city" },
    { "monde", "world" },
    { "temps", "time" },
    { "jour", "day" },
    { "nuit", "night" },
    { "aujourdhui", "today" },
    { "aujourd'hui", "today" },
    { "demain", "tomorrow" },
    { "hier", "yesterday" },
    { "maintenant", "now" },
    { "le", "the" },
    { "la", "the" },
    { "les", "the" },
    { "un", "a" },
    { "une", "a" },
    { "et", "and" },
    { "avec", "with" },
    { "sans", "without" },
    { "pour", "for" },
    { "dans", "in" },
    /* --- German --- */
    { "guten morgen", "good morning" },
    { "guten abend", "good evening" },
    { "gute nacht", "good night" },
    { "auf wiedersehen", "goodbye" },
    { "bitte", "please" },
    { "danke", "thanks" },
    { "vielen dank", "thank you very much" },
    { "hallo", "hello" },
    { "ja", "yes" },
    { "nein", "no" },
    { "gut", "good" },
    { "schlecht", "bad" },
    { "groß", "big" },
    { "gross", "big" },
    { "klein", "small" },
    { "haus", "house" },
    { "mann", "man" },
    { "frau", "woman" },
    { "kind", "child" },
    { "freund", "friend" },
    { "wasser", "water" },
    { "essen", "food" },
    { "brot", "bread" },
    { "milch", "milk" },
    { "arbeit", "work" },
    { "schule", "school" },
    { "buch", "book" },
    { "stadt", "city" },
    { "welt", "world" },
    { "zeit", "time" },
    { "tag", "day" },
    { "nacht", "night" },
    { "heute", "today" },
    { "morgen", "tomorrow" },
    { "jetzt", "now" },
    { "wo", "where" },
    { "wann", "when" },
    { "wer", "who" },
    { "warum", "why" },
    { "der", "the" },
    { "die", "the" },
    { "das", "the" },
    { "ein", "a" },
    { "eine", "a" },
    { "und", "and" },
    { "ist", "is" },
    { "mit", "with" },
    { "ohne", "without" },
    { "für", "for" },
    { "fuer", "for" },
    { "in", "in" },
    { "nicht", "not" },
    /* --- Italian --- */
    { "buongiorno", "good morning" },
    { "buonasera", "good evening" },
    { "buonanotte", "good night" },
    { "grazie", "thanks" },
    { "prego", "you're welcome" },
    { "arrivederci", "goodbye" },
    { "ciao", "hello" },
    { "si", "yes" },
    { "no", "no" },
    { "buono", "good" },
    { "cattivo", "bad" },
    { "grande", "big" },
    { "piccolo", "small" },
    { "casa", "house" },
    { "uomo", "man" },
    { "donna", "woman" },
    { "bambino", "child" },
    { "amico", "friend" },
    { "acqua", "water" },
    { "cibo", "food" },
    { "pane", "bread" },
    { "latte", "milk" },
    { "lavoro", "work" },
    { "scuola", "school" },
    { "libro", "book" },
    { "citta", "city" },
    { "città", "city" },
    { "mondo", "world" },
    { "tempo", "time" },
    { "giorno", "day" },
    { "notte", "night" },
    { "oggi", "today" },
    { "domani", "tomorrow" },
    { "ieri", "yesterday" },
    { "ora", "now" },
    { "il", "the" },
    { "lo", "the" },
    { "la", "the" },
    { "i", "the" },
    { "gli", "the" },
    { "le", "the" },
    { "un", "a" },
    { "uno", "a" },
    { "e", "and" },
    { "con", "with" },
    { "senza", "without" },
    { "per", "for" },
    { "in", "in" },
    /* --- Portuguese --- */
    { "bom dia", "good morning" },
    { "boa noite", "good night" },
    { "obrigado", "thank you" },
    { "obrigada", "thank you" },
    { "por favor", "please" },
    { "de nada", "you're welcome" },
    { "ola", "hello" },
    { "olá", "hello" },
    { "sim", "yes" },
    { "nao", "no" },
    { "não", "no" },
    { "bom", "good" },
    { "ruim", "bad" },
    { "grande", "big" },
    { "pequeno", "small" },
    { "casa", "house" },
    { "homem", "man" },
    { "mulher", "woman" },
    { "crianca", "child" },
    { "criança", "child" },
    { "amigo", "friend" },
    { "agua", "water" },
    { "água", "water" },
    { "comida", "food" },
    { "pao", "bread" },
    { "pão", "bread" },
    { "leite", "milk" },
    { "trabalho", "work" },
    { "escola", "school" },
    { "livro", "book" },
    { "cidade", "city" },
    { "mundo", "world" },
    { "tempo", "time" },
    { "dia", "day" },
    { "noite", "night" },
    { "hoje", "today" },
    { "amanha", "tomorrow" },
    { "amanhã", "tomorrow" },
    { "ontem", "yesterday" },
    { "agora", "now" },
    { "o", "the" },
    { "a", "the" },
    { "os", "the" },
    { "as", "the" },
    { "um", "a" },
    { "uma", "a" },
    { "e", "and" },
    { "com", "with" },
    { "sem", "without" },
    { "para", "for" },
    { "em", "in" },
};

static const int k_dict_count = (int)(sizeof(k_dict) / sizeof(k_dict[0]));

/* Case-insensitive compare of up to n chars, ASCII only. */
static int ci_cmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        char ca = a[i], cb = b[i];
        if (ca == '\0' && cb == '\0') return 0;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
    }
    return 0;
}

/* Word-boundary test (previous/next char is not a letter/digit/apostrophe). */
static bool is_boundary(char c) {
    if (c >= 'a' && c <= 'z') return false;
    if (c >= 'A' && c <= 'Z') return false;
    if (c >= '0' && c <= '9') return false;
    return true;
}

/* Simple ASCII-only lowercase copy (used to look up ASCII dict keys). */
static void ascii_lower(const char* src, char* out, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        char c = src[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out[i] = c;
        i++;
    }
    out[i] = '\0';
}

/* Apply basic capitalization: if the source token started uppercase, uppercase
 * the first letter of the result. */
static void apply_cap(const char* src, char* dst) {
    if (src[0] >= 'A' && src[0] <= 'Z') {
        if (dst[0] >= 'a' && dst[0] <= 'z') dst[0] = (char)(dst[0] - 'a' + 'A');
    }
}

int translate_string(const char* src, char* dst, int out_len) {
    if (!src || !dst || out_len <= 0) return 0;
    int src_len = (int)strlen(src);
    int oi = 0;      /* output index */
    int i = 0;       /* input index  */

    while (i < src_len) {
        /* Find the longest dictionary entry starting at i (phrases first). */
        const DictEntry* best = NULL;
        int best_len = 0;
        for (int e = 0; e < k_dict_count; e++) {
            int elen = (int)strlen(k_dict[e].src);
            if (elen < best_len) continue;
            if (i + elen > src_len) continue;
            /* phrase must match exactly (case-insensitive) */
            if (ci_cmp(src + i, k_dict[e].src, elen) != 0) continue;
            /* the following char must be a boundary for whole-word match */
            char after = (i + elen < src_len) ? src[i + elen] : ' ';
            if (!is_boundary(after)) continue;
            best = &k_dict[e];
            best_len = elen;
        }

        if (best) {
            int dlen = (int)strlen(best->dst);
            /* copy the replacement, preserving roughly the source case */
            char tmp[64];
            int tlen = dlen;
            if (tlen > 63) tlen = 63;
            memcpy(tmp, best->dst, (size_t)tlen);
            tmp[tlen] = '\0';
            apply_cap(src + i, tmp);
            if (oi + tlen < out_len) {
                memcpy(dst + oi, tmp, (size_t)tlen);
                oi += tlen;
            }
            i += best_len;
        } else {
            /* Copy this single char through unchanged. */
            if (oi + 1 < out_len) dst[oi++] = src[i];
            i++;
        }
    }
    dst[oi] = '\0';
    return oi;
}

bool translate_text_node(HtmlNode* node) {
    if (!node || node->type != HTML_NODE_TEXT || !node->text.data || node->text.length == 0)
        return false;

    if (!translate_looks_foreign(node->text.data))
        return false;

    char buf[4096];
    const char* data = node->text.data;

    translate_string(data, buf, (int)sizeof(buf));

    /* Only rewrite if at least one word was translated AND result differs. */
    if (buf[0] == '\0' || strcmp(buf, data) == 0)
        return false;

    string_free(&node->text);
    node->text = string_create(buf);
    return true;
}

static int walk_translate(HtmlNode* node, int* changed) {
    if (!node) return 0;
    int modified = 0;
    if (node->type == HTML_NODE_TEXT) {
        if (translate_text_node(node)) modified++;
    }
    for (int i = 0; i < node->child_count; i++) {
        modified += walk_translate(node->children[i], changed);
    }
    return modified;
}

int translate_document(HtmlDocument* doc) {
    if (!doc || !doc->root) return 0;
    return walk_translate(doc->root, NULL);
}
