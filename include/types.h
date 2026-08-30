#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char* data;
    int length;
    int capacity;
} String;

typedef struct {
    void* data;
    int length;
    int capacity;
    int element_size;
} Array;

typedef struct {
    char* key;
    char* value;
} KeyValuePair;

typedef struct {
    KeyValuePair* items;
    int count;
    int capacity;
} HashMap;

String string_create(const char* cstr);
String string_create_n(const char* data, int length);
String string_clone(String src);
void string_free(String* s);
void string_append(String* s, const char* cstr);
void string_append_n(String* s, const char* data, int length);
void string_append_char(String* s, char c);
void string_clear(String* s);
bool string_equals(String a, String b);
bool string_starts_with(String s, const char* prefix);
bool string_ends_with(String s, const char* suffix);
int string_find(String s, const char* needle);
String string_substr(String s, int start, int length);
String string_trim(String s);
void string_to_lower(String* s);
void string_to_upper(String* s);

void array_init(Array* arr, int element_size);
void array_push(Array* arr, const void* element);
void* array_get(Array* arr, int index);
void array_remove(Array* arr, int index);
void array_clear(Array* arr);
void array_free(Array* arr);

void hashmap_init(HashMap* map);
void hashmap_put(HashMap* map, const char* key, const char* value);
const char* hashmap_get(HashMap* map, const char* key);
bool hashmap_contains(HashMap* map, const char* key);
void hashmap_remove(HashMap* map, const char* key);
void hashmap_free(HashMap* map);
