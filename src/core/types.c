#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

String string_create(const char* cstr) {
    String s = {0};
    if (cstr) {
        s.length = (int)strlen(cstr);
        s.capacity = s.length + 1;
        s.data = (char*)malloc(s.capacity);
        memcpy(s.data, cstr, s.capacity);
    }
    return s;
}

String string_create_n(const char* data, int length) {
    String s = {0};
    if (data && length > 0) {
        s.length = length;
        s.capacity = length + 1;
        s.data = (char*)malloc(s.capacity);
        memcpy(s.data, data, length);
        s.data[length] = '\0';
    }
    return s;
}

String string_clone(String src) {
    return string_create_n(src.data, src.length);
}

void string_free(String* s) {
    if (s->data) { free(s->data); s->data = NULL; }
    s->length = 0; s->capacity = 0;
}

void string_append(String* s, const char* cstr) {
    if (!cstr) return;
    int addlen = (int)strlen(cstr);
    if (s->length + addlen + 1 > s->capacity) {
        s->capacity = (s->length + addlen + 1) * 2;
        s->data = (char*)realloc(s->data, s->capacity);
    }
    memcpy(s->data + s->length, cstr, addlen + 1);
    s->length += addlen;
}

void string_append_n(String* s, const char* data, int length) {
    if (!data || length <= 0) return;
    if (s->length + length + 1 > s->capacity) {
        s->capacity = (s->length + length + 1) * 2;
        s->data = (char*)realloc(s->data, s->capacity);
    }
    memcpy(s->data + s->length, data, length);
    s->length += length;
    s->data[s->length] = '\0';
}

void string_append_char(String* s, char c) {
    if (s->length + 2 > s->capacity) {
        s->capacity = (s->length + 2) * 2;
        s->data = (char*)realloc(s->data, s->capacity);
    }
    s->data[s->length++] = c;
    s->data[s->length] = '\0';
}

void string_clear(String* s) {
    if (s->data) s->data[0] = '\0';
    s->length = 0;
}

bool string_equals(String a, String b) {
    if (a.length != b.length) return false;
    if (a.data == NULL && b.data == NULL) return true;
    if (a.data == NULL || b.data == NULL) return false;
    return memcmp(a.data, b.data, a.length) == 0;
}

bool string_starts_with(String s, const char* prefix) {
    int plen = (int)strlen(prefix);
    if (s.length < plen) return false;
    return memcmp(s.data, prefix, plen) == 0;
}

bool string_ends_with(String s, const char* suffix) {
    int slen = (int)strlen(suffix);
    if (s.length < slen) return false;
    return memcmp(s.data + s.length - slen, suffix, slen) == 0;
}

int string_find(String s, const char* needle) {
    if (!needle || !s.data) return -1;
    int nlen = (int)strlen(needle);
    if (nlen == 0) return 0;
    for (int i = 0; i <= s.length - nlen; i++) {
        if (memcmp(s.data + i, needle, nlen) == 0) return i;
    }
    return -1;
}

String string_substr(String s, int start, int length) {
    if (start < 0 || start >= s.length) { String empty; memset(&empty, 0, sizeof(empty)); return empty; }
    if (start + length > s.length) length = s.length - start;
    return string_create_n(s.data + start, length);
}

String string_trim(String s) {
    if (!s.data) return s;
    int start = 0;
    while (start < s.length && (s.data[start] == ' ' || s.data[start] == '\t' || s.data[start] == '\n' || s.data[start] == '\r'))
        start++;
    int end = s.length - 1;
    while (end >= start && (s.data[end] == ' ' || s.data[end] == '\t' || s.data[end] == '\n' || s.data[end] == '\r'))
        end--;
    return string_create_n(s.data + start, end - start + 1);
}

void string_to_lower(String* s) {
    if (!s || !s->data) return;
    for (int i = 0; i < s->length; i++)
        s->data[i] = (char)tolower((unsigned char)s->data[i]);
}

void string_to_upper(String* s) {
    if (!s || !s->data) return;
    for (int i = 0; i < s->length; i++)
        s->data[i] = (char)toupper((unsigned char)s->data[i]);
}

void array_init(Array* arr, int element_size) {
    arr->data = NULL;
    arr->length = 0;
    arr->capacity = 0;
    arr->element_size = element_size;
}

void array_push(Array* arr, const void* element) {
    if (arr->length >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        arr->data = realloc(arr->data, arr->capacity * arr->element_size);
    }
    memcpy((char*)arr->data + arr->length * arr->element_size, element, arr->element_size);
    arr->length++;
}

void* array_get(Array* arr, int index) {
    if (index < 0 || index >= arr->length) return NULL;
    return (char*)arr->data + index * arr->element_size;
}

void array_remove(Array* arr, int index) {
    if (index < 0 || index >= arr->length) return;
    if (index < arr->length - 1) {
        memmove((char*)arr->data + index * arr->element_size,
                (char*)arr->data + (index + 1) * arr->element_size,
                (arr->length - index - 1) * arr->element_size);
    }
    arr->length--;
}

void array_clear(Array* arr) {
    arr->length = 0;
}

void array_free(Array* arr) {
    if (arr->data) free(arr->data);
    arr->data = NULL;
    arr->length = 0;
    arr->capacity = 0;
}

#define HASHMAP_BUCKETS 64

typedef struct HashMapNode {
    char* key;
    char* value;
    struct HashMapNode* next;
} HashMapNode;

void hashmap_init(HashMap* map) {
    map->items = NULL;
    map->count = 0;
    map->capacity = 0;
}

static unsigned int hashmap_hash(const char* key) {
    unsigned int hash = 5381;
    while (*key) hash = hash * 33 + (unsigned char)*key++;
    return hash;
}

void hashmap_put(HashMap* map, const char* key, const char* value) {
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->items[i].key, key) == 0) {
            free(map->items[i].value);
            map->items[i].value = _strdup(value);
            return;
        }
    }
    if (map->count >= map->capacity) {
        map->capacity = map->capacity == 0 ? 16 : map->capacity * 2;
        map->items = (KeyValuePair*)realloc(map->items, map->capacity * sizeof(KeyValuePair));
    }
    map->items[map->count].key = _strdup(key);
    map->items[map->count].value = _strdup(value);
    map->count++;
}

const char* hashmap_get(HashMap* map, const char* key) {
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->items[i].key, key) == 0)
            return map->items[i].value;
    }
    return NULL;
}

bool hashmap_contains(HashMap* map, const char* key) {
    return hashmap_get(map, key) != NULL;
}

void hashmap_remove(HashMap* map, const char* key) {
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->items[i].key, key) == 0) {
            free(map->items[i].key);
            free(map->items[i].value);
            if (i < map->count - 1) {
                memmove(&map->items[i], &map->items[i + 1], (map->count - i - 1) * sizeof(KeyValuePair));
            }
            map->count--;
            return;
        }
    }
}

void hashmap_free(HashMap* map) {
    for (int i = 0; i < map->count; i++) {
        free(map->items[i].key);
        free(map->items[i].value);
    }
    if (map->items) free(map->items);
    map->items = NULL;
    map->count = 0;
    map->capacity = 0;
}
