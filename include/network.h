#pragma once

#include "platform.h"
#include "types.h"

typedef enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_HEAD
} HttpMethod;

typedef enum {
    HTTP_STATUS_IDLE,
    HTTP_STATUS_CONNECTING,
    HTTP_STATUS_SENDING,
    HTTP_STATUS_RECEIVING,
    HTTP_STATUS_DONE,
    HTTP_STATUS_ERROR
} HttpConnectionStatus;

typedef struct {
    int status_code;
    HashMap headers;
    String body;
    String content_type;
    int content_length;
    bool chunked;
} HttpResponse;

typedef struct {
    PlatformSocket socket;
    char host[256];
    uint16_t port;
    char path[1024];
    HttpMethod method;
    HttpConnectionStatus status;
    HttpResponse response;
    String request_buffer;
    String response_buffer;
    bool tls;
    PlatformThread thread;
    PlatformMutex lock;
} HttpClient;

typedef void (*HttpCallback)(HttpClient* client, void* user_data);

HttpClient* http_client_create(void);
void http_client_destroy(HttpClient* client);
bool http_client_request(HttpClient* client, const char* url, HttpMethod method);
void http_client_poll(HttpClient* client);
bool http_client_is_done(HttpClient* client);
HttpResponse* http_client_get_response(HttpClient* client);

String http_build_request(HttpMethod method, const char* host, const char* path, const HashMap* extra_headers);
bool http_parse_url(const char* url, char* host, int host_size, uint16_t* port, char* path, int path_size, bool* tls);
HttpResponse http_parse_response(const char* data, int length);
