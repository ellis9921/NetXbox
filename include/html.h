#pragma once

#include "types.h"

typedef enum {
    HTML_NODE_ELEMENT,
    HTML_NODE_TEXT,
    HTML_NODE_COMMENT,
    HTML_NODE_DOCUMENT
} HtmlNodeType;

typedef enum {
    HTML_TAG_UNKNOWN,
    HTML_TAG_HTML, HTML_TAG_HEAD, HTML_TAG_BODY,
    HTML_TAG_TITLE, HTML_TAG_META, HTML_TAG_LINK, HTML_TAG_STYLE, HTML_TAG_SCRIPT,
    HTML_TAG_DIV, HTML_TAG_SPAN, HTML_TAG_P, HTML_TAG_A, HTML_TAG_BR, HTML_TAG_HR,
    HTML_TAG_H1, HTML_TAG_H2, HTML_TAG_H3, HTML_TAG_H4, HTML_TAG_H5, HTML_TAG_H6,
    HTML_TAG_UL, HTML_TAG_OL, HTML_TAG_LI, HTML_TAG_DL, HTML_TAG_DT, HTML_TAG_DD,
    HTML_TAG_TABLE, HTML_TAG_TR, HTML_TAG_TD, HTML_TAG_TH, HTML_TAG_THEAD, HTML_TAG_TBODY,
    HTML_TAG_IMG, HTML_TAG_VIDEO, HTML_TAG_AUDIO, HTML_TAG_CANVAS,
    HTML_TAG_INPUT, HTML_TAG_BUTTON, HTML_TAG_SELECT, HTML_TAG_OPTION, HTML_TAG_TEXTAREA,
    HTML_TAG_FORM, HTML_TAG_LABEL, HTML_TAG_FIELDSET, HTML_TAG_LEGEND,
    HTML_TAG_PRE, HTML_TAG_CODE, HTML_TAG_BLOCKQUOTE, HTML_TAG_Q,
    HTML_TAG_STRONG, HTML_TAG_EM, HTML_TAG_B, HTML_TAG_I, HTML_TAG_U, HTML_TAG_S,
    HTML_TAG_SMALL, HTML_TAG_SUB, HTML_TAG_SUP, HTML_TAG_DEL, HTML_TAG_INS,
    HTML_TAG_HEADER, HTML_TAG_FOOTER, HTML_TAG_NAV, HTML_TAG_MAIN, HTML_TAG_ARTICLE, HTML_TAG_SECTION,
    HTML_TAG_COUNT
} HtmlTag;

typedef struct HtmlNode HtmlNode;

typedef struct {
    String name;
    String value;
} HtmlAttribute;

typedef struct {
    HtmlAttribute* items;
    int count;
    int capacity;
} HtmlAttributeList;

struct HtmlNode {
    HtmlNodeType type;
    HtmlTag tag;
    String tag_name;
    String text;
    HtmlAttributeList attributes;
    HtmlNode** children;
    int child_count;
    int child_capacity;
    HtmlNode* parent;
};

typedef struct {
    HtmlNode* root;
    String title;
    String base_url;
} HtmlDocument;

HtmlDocument* html_parse(const char* html, int length);
void html_document_free(HtmlDocument* doc);
HtmlNode* html_create_node(HtmlNodeType type);
void html_node_add_child(HtmlNode* parent, HtmlNode* child);
const char* html_node_get_attribute(HtmlNode* node, const char* name);
bool html_node_has_attribute(HtmlNode* node, const char* name);
HtmlTag html_tag_from_string(const char* name);
const char* html_tag_to_string(HtmlTag tag);
HtmlNode* html_find_element_by_id(HtmlNode* root, const char* id);
HtmlNode* html_find_element_by_tag(HtmlNode* root, HtmlTag tag);
