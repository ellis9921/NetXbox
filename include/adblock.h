#pragma once

#include <stdbool.h>
#include <stdint.h>

#define ADBLOCK_MAX_FILTERS 4096
#define ADBLOCK_MAX_EXCEPTIONS 512
#define ADBLOCK_MAX_HOSTS 2048
#define ADBLOCK_MAX_PATTERN_LEN 512
#define ADBLOCK_MAX_HOSTNAME_LEN 256

typedef enum {
    FILTER_TYPE_NETWORK,
    FILTER_TYPE_COSMETIC,
    FILTER_TYPE_EXCEPTION,
    FILTER_TYPE_HOSTS,
    FILTER_TYPE_COMMENT,
    FILTER_TYPE_UNKNOWN
} AdblockFilterType;

typedef enum {
    FILTER_FLAG_LEFT_ANCHOR   = 1 << 0,
    FILTER_FLAG_RIGHT_ANCHOR  = 1 << 1,
    FILTER_FLAG_THIRD_PARTY   = 1 << 2,
    FILTER_FLAG_FIRST_PARTY   = 1 << 3,
    FILTER_FLAG_FROM_HTTP     = 1 << 4,
    FILTER_FLAG_FROM_HTTPS    = 1 << 5,
    FILTER_FLAG_IS_REGEX      = 1 << 6,
    FILTER_FLAG_ELEMENT_HIDE  = 1 << 7,
} AdblockFilterFlag;

typedef struct {
    char pattern[ADBLOCK_MAX_PATTERN_LEN];
    char hostname[ADBLOCK_MAX_HOSTNAME_LEN];
    char opt_domains[ADBLOCK_MAX_HOSTNAME_LEN];
    char opt_not_domains[ADBLOCK_MAX_HOSTNAME_LEN];
    uint32_t flags;
    bool is_exception;
    bool is_hosts;
} AdblockFilter;

typedef struct {
    char domain[ADBLOCK_MAX_HOSTNAME_LEN];
} AdblockHostsEntry;

typedef struct {
    AdblockFilter filters[ADBLOCK_MAX_FILTERS];
    int filter_count;
    AdblockFilter exceptions[ADBLOCK_MAX_EXCEPTIONS];
    int exception_count;
    AdblockHostsEntry hosts[ADBLOCK_MAX_HOSTS];
    int hosts_count;
    bool enabled;
} AdblockEngine;

void adblock_init(AdblockEngine* engine);
void adblock_load_from_string(AdblockEngine* engine, const char* filter_text);
void adblock_load_hosts_from_string(AdblockEngine* engine, const char* hosts_text);
bool adblock_should_block(AdblockEngine* engine, const char* url, const char* source_domain, bool is_third_party);
bool adblock_should_hide(AdblockEngine* engine, const char* url, const char* css_selector);
