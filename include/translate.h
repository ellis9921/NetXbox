#pragma once

#include "types.h"
#include "html.h"

/* Native, offline page translation for NetXbox.
 *
 * NetXbox's lightweight renderer cannot execute JavaScript, so "translation"
 * is implemented natively in C. It works in two steps:
 *
 *   1. translate_looks_foreign()  - cheap detection of non-English text
 *                                   (UTF-8 non-ASCII codepoints / marker words)
 *   2. translate_string()         - dictionary-based word/phrase substitution.
 *                                   Unknown words are preserved verbatim, so this
 *                                   is a best-effort offline fallback, not a full
 *                                   neural translation.
 *
 * translate_document() walks a parsed HtmlDocument and rewrites its text nodes
 * in place, so it must be called AFTER html_parse() and BEFORE the render
 * layout is built (browser_update_layout).
 */

/* Returns true if the given text appears to be non-English. */
bool translate_looks_foreign(const char* text);

/* Best-effort translation of src into a stack buffer dst (size = out_len).
 * Returns the number of characters written (excluding NUL). */
int translate_string(const char* src, char* dst, int out_len);

/* Rewrite a parsed document's text nodes in place. Returns the number of nodes
 * that were modified. */
int translate_document(HtmlDocument* doc);

/* Rewrite a single text node's string in place. Returns true if it changed. */
bool translate_text_node(HtmlNode* node);
