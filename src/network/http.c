#include "network.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#if defined(_WIN32) && !defined(PLATFORM_XBOX360)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#elif defined(PLATFORM_XBOX360)
#include <xtl.h>
#endif

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

bool http_parse_url(const char* url, char* host, int host_size, uint16_t* port, char* path, int path_size, bool* tls) {
    const char* p = url;
    *port = 80;
    *tls = false;
    
    if (strncmp(p, "https://", 8) == 0) {
        *tls = true;
        *port = 443;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    }
    
    const char* host_start = p;
    const char* slash = strchr(p, '/');
    const char* colon = strchr(p, ':');
    
    if (colon && (!slash || colon < slash)) {
        int hlen = (int)(colon - host_start);
        if (hlen >= host_size) hlen = host_size - 1;
        memcpy(host, host_start, hlen);
        host[hlen] = '\0';
        *port = (uint16_t)atoi(colon + 1);
    } else {
        int hlen = slash ? (int)(slash - host_start) : (int)strlen(host_start);
        if (hlen >= host_size) hlen = host_size - 1;
        memcpy(host, host_start, hlen);
        host[hlen] = '\0';
    }
    
    if (slash) {
        int plen = (int)strlen(slash);
        if (plen >= path_size) plen = path_size - 1;
        memcpy(path, slash, plen);
        path[plen] = '\0';
    } else {
        path[0] = '/'; path[1] = '\0';
    }
    
    return host[0] != '\0';
}

String http_build_request(HttpMethod method, const char* host, const char* path, const HashMap* extra_headers) {
    String req = {0};
    const char* method_str[] = {"GET", "POST", "PUT", "DELETE", "HEAD"};
    
    string_append(&req, method_str[method]);
    string_append(&req, " ");
    string_append(&req, path);
    string_append(&req, " HTTP/1.1\r\n");
    string_append(&req, "Host: ");
    string_append(&req, host);
    string_append(&req, "\r\n");
    string_append(&req, "Connection: close\r\n");
    string_append(&req, "User-Agent: NetXbox/1.0\r\n");
    string_append(&req, "Accept: text/html,application/xhtml+xml,*/*\r\n");
    string_append(&req, "Accept-Language: en-US,en;q=0.9\r\n");
    
    if (extra_headers) {
        for (int i = 0; i < extra_headers->count; i++) {
            string_append(&req, extra_headers->items[i].key);
            string_append(&req, ": ");
            string_append(&req, extra_headers->items[i].value);
            string_append(&req, "\r\n");
        }
    }
    
    string_append(&req, "\r\n");
    return req;
}

HttpResponse http_parse_response(const char* data, int length) {
    HttpResponse resp = {0};
    hashmap_init(&resp.headers);
    resp.body = string_create("");
    
    const char* header_end = NULL;
    for (int i = 0; i + 3 < length; i++) {
        if (data[i] == '\r' && data[i+1] == '\n' && data[i+2] == '\r' && data[i+3] == '\n') {
            header_end = data + i;
            break;
        }
    }
    if (!header_end) {
        string_append_n(&resp.body, data, length);
        return resp;
    }
    
    if (length >= 5 && data[0] == 'H' && data[1] == 'T' && data[2] == 'T' && data[3] == 'P' && data[4] == '/') {
        const char* p = data;
        while (p < header_end && *p != ' ') p++;
        if (*p == ' ') p++;
        resp.status_code = 0;
        while (p < header_end && *p >= '0' && *p <= '9') {
            resp.status_code = resp.status_code * 10 + (*p - '0');
            p++;
        }
    }
    
    const char* p = data;
    char line[4096];
    
    while (p < header_end) {
        const char* line_end = NULL;
        for (const char* q = p; q < header_end; q++) {
            if (q[0] == '\r' && q[1] == '\n') { line_end = q; break; }
        }
        if (!line_end) break;
        
        int line_len = (int)(line_end - p);
        if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        memcpy(line, p, line_len);
        line[line_len] = '\0';
        
        char* colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char* value = colon + 1;
            while (*value == ' ') value++;
            
            hashmap_put(&resp.headers, line, value);
            
            {
                int cmp_ct = 1, cmp_cl = 1, cmp_te = 1, cmp_ch = 1;
                const char* a; const char* b;
                a = line; b = "Content-Type"; cmp_ct = 0;
                while (*a && *b) { int ca = (unsigned char)*a; int cb = (unsigned char)*b; if (tolower(ca) != tolower(cb)) { cmp_ct = 1; break; } a++; b++; }
                if (cmp_ct == 0 && *a == '\0' && *b == '\0') {
                    resp.content_type = string_create(value);
                } else {
                    a = line; b = "Content-Length"; cmp_cl = 0;
                    while (*a && *b) { int ca = (unsigned char)*a; int cb = (unsigned char)*b; if (tolower(ca) != tolower(cb)) { cmp_cl = 1; break; } a++; b++; }
                    if (cmp_cl == 0 && *a == '\0' && *b == '\0') {
                        resp.content_length = atoi(value);
                    } else {
                        a = line; b = "Transfer-Encoding"; cmp_te = 0;
                        while (*a && *b) { int ca = (unsigned char)*a; int cb = (unsigned char)*b; if (tolower(ca) != tolower(cb)) { cmp_te = 1; break; } a++; b++; }
                        if (cmp_te == 0 && *a == '\0' && *b == '\0') {
                            a = value; b = "chunked"; cmp_ch = 0;
                            while (*a && *b) { int ca = (unsigned char)*a; int cb = (unsigned char)*b; if (tolower(ca) != tolower(cb)) { cmp_ch = 1; break; } a++; b++; }
                            if (cmp_ch == 0 && *a == '\0' && *b == '\0') {
                                resp.chunked = true;
                            }
                        }
                    }
                }
            }
        }
        
        p = line_end + 2;
    }
    
    int body_offset = (int)(header_end - data) + 4;
    int body_len = length - body_offset;
    if (body_len > 0) {
        string_append_n(&resp.body, data + body_offset, body_len);
    }
    
    return resp;
}

HttpClient* http_client_create(void) {
    HttpClient* client = (HttpClient*)calloc(1, sizeof(HttpClient));
    hashmap_init(&client->response.headers);
    client->response.body = string_create("");
    client->lock = platform_get_api()->mutex_create();
    return client;
}

void http_client_destroy(HttpClient* client) {
    if (!client) return;
    if (client->thread) platform_get_api()->thread_join(client->thread);
    if (client->socket) platform_get_api()->socket_destroy(client->socket);
    string_free(&client->request_buffer);
    string_free(&client->response_buffer);
    hashmap_free(&client->response.headers);
    string_free(&client->response.body);
    string_free(&client->response.content_type);
    if (client->lock) platform_get_api()->mutex_destroy(client->lock);
    free(client);
}

static void http_thread_func(void* arg) {
    HttpClient* client = (HttpClient*)arg;
    const PlatformAPI* api = platform_get_api();

#if defined(_WIN32) && !defined(PLATFORM_XBOX360)
    wchar_t whost[256];
    wchar_t wpath[2048];
    MultiByteToWideChar(CP_UTF8, 0, client->host, -1, whost, 256);
    MultiByteToWideChar(CP_UTF8, 0, client->path, -1, wpath, 2048);

    HINTERNET hSession = WinHttpOpen(L"NetXbox/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { api->mutex_lock(client->lock); client->status = HTTP_STATUS_ERROR; api->mutex_unlock(client->lock); return; }

    DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    BOOL decomp_ok = WinHttpSetOption(hSession, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));

    HINTERNET hConnect = WinHttpConnect(hSession, whost, client->port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); api->mutex_lock(client->lock); client->status = HTTP_STATUS_ERROR; api->mutex_unlock(client->lock); return; }

    const wchar_t* verb = client->method == HTTP_METHOD_GET ? L"GET" : L"POST";
    DWORD flags = client->tls ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, verb, wpath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); api->mutex_lock(client->lock); client->status = HTTP_STATUS_ERROR; api->mutex_unlock(client->lock); return; }

    DWORD security_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &security_flags, sizeof(security_flags));

    const wchar_t* headers = decomp_ok ? WINHTTP_NO_ADDITIONAL_HEADERS : L"Accept-Encoding: identity\r\n";
    BOOL sent = WinHttpSendRequest(hRequest, headers, -1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); api->mutex_lock(client->lock); client->status = HTTP_STATUS_ERROR; api->mutex_unlock(client->lock); return; }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        int err = (int)GetLastError();
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        api->mutex_lock(client->lock); client->status = HTTP_STATUS_ERROR; api->mutex_unlock(client->lock);
        return;
    }

    DWORD status_code = 0;
    DWORD sc_size = sizeof(status_code);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &sc_size, WINHTTP_NO_HEADER_INDEX);

    String full_response = string_create("");
    {
        char fake_hdr[128];
        int fake_len = _snprintf(fake_hdr, sizeof(fake_hdr), "HTTP/1.1 %d OK\r\n\r\n", (int)status_code);
        string_append_n(&full_response, fake_hdr, fake_len);
    }

    char recv_buf[65536];
    DWORD bytes_read = 0;
    while (WinHttpReadData(hRequest, recv_buf, sizeof(recv_buf), &bytes_read) && bytes_read > 0) {
        string_append_n(&full_response, recv_buf, (int)bytes_read);
        bytes_read = 0;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    api->mutex_lock(client->lock);
    string_free(&client->response_buffer);
    client->response_buffer = full_response;
    client->response = http_parse_response(client->response_buffer.data, client->response_buffer.length);
    if (client->response.status_code == 0) client->response.status_code = (int)status_code;
    client->status = HTTP_STATUS_DONE;
    api->mutex_unlock(client->lock);
#elif defined(PLATFORM_XBOX360)
    PlatformSocket sock = api->socket_create(PLATFORM_SOCKET_TCP);
    if (!sock) {
        api->mutex_lock(client->lock);
        client->status = HTTP_STATUS_ERROR;
        api->mutex_unlock(client->lock);
        return;
    }

    if (!api->socket_connect(sock, client->host, client->port)) {
        api->socket_destroy(sock);
        api->mutex_lock(client->lock);
        client->status = HTTP_STATUS_ERROR;
        api->mutex_unlock(client->lock);
        return;
    }

    const char* req_data = client->request_buffer.data;
    int req_len = client->request_buffer.length;
    int sent = 0;
    while (sent < req_len) {
        int n = api->socket_send(sock, req_data + sent, req_len - sent);
        if (n <= 0) { api->socket_destroy(sock); api->mutex_lock(client->lock); client->status = HTTP_STATUS_ERROR; api->mutex_unlock(client->lock); return; }
        sent += n;
    }

    char recv_buf[65536];
    String full_response = string_create("");
    for (;;) {
        int n = api->socket_recv(sock, recv_buf, sizeof(recv_buf));
        if (n > 0) {
            string_append_n(&full_response, recv_buf, n);
        } else {
            break;
        }
    }
    api->socket_destroy(sock);

    api->mutex_lock(client->lock);
    string_free(&client->response_buffer);
    client->response_buffer = full_response;
    client->response = http_parse_response(client->response_buffer.data, client->response_buffer.length);
    client->status = HTTP_STATUS_DONE;
    api->mutex_unlock(client->lock);
#else
    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[8];
    _snprintf(portStr, sizeof(portStr), "%u", client->port);

    if (getaddrinfo(client->host, portStr, &hints, &result) != 0 || !result) {
        api->mutex_lock(client->lock);
        client->status = HTTP_STATUS_ERROR;
        api->mutex_unlock(client->lock);
        return;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(result);
        api->mutex_lock(client->lock);
        client->status = HTTP_STATUS_ERROR;
        api->mutex_unlock(client->lock);
        return;
    }

    if (connect(sock, result->ai_addr, (int)result->ai_addrlen) != 0) {
        closesocket(sock);
        freeaddrinfo(result);
        api->mutex_lock(client->lock);
        client->status = HTTP_STATUS_ERROR;
        api->mutex_unlock(client->lock);
        return;
    }
    freeaddrinfo(result);

    const char* req_data = client->request_buffer.data;
    int req_len = client->request_buffer.length;
    int sent = 0;
    while (sent < req_len) {
        int n = send(sock, req_data + sent, req_len - sent, 0);
        if (n <= 0) { closesocket(sock); api->mutex_lock(client->lock); client->status = HTTP_STATUS_ERROR; api->mutex_unlock(client->lock); return; }
        sent += n;
    }

    char recv_buf[65536];
    String full_response = string_create("");
    for (;;) {
        int n = recv(sock, recv_buf, sizeof(recv_buf), 0);
        if (n > 0) {
            string_append_n(&full_response, recv_buf, n);
        } else {
            break;
        }
    }
    closesocket(sock);

    api->mutex_lock(client->lock);
    string_free(&client->response_buffer);
    client->response_buffer = full_response;
    client->response = http_parse_response(client->response_buffer.data, client->response_buffer.length);
    client->status = HTTP_STATUS_DONE;
    api->mutex_unlock(client->lock);
#endif
}

bool http_client_request(HttpClient* client, const char* url, HttpMethod method) {
    const PlatformAPI* api = platform_get_api();
    
    if (!http_parse_url(url, client->host, sizeof(client->host),
                        &client->port, client->path, sizeof(client->path), &client->tls)) {
        return false;
    }
    
    client->method = method;
    client->status = HTTP_STATUS_CONNECTING;
    client->request_buffer = http_build_request(method, client->host, client->path, NULL);
    client->response_buffer = string_create("");
    
    client->thread = api->thread_create(http_thread_func, client);
    return true;
}

void http_client_poll(HttpClient* client) {
    // All work is done in the background thread.
    // This just checks for completion status.
}

bool http_client_is_done(HttpClient* client) {
    if (!client) return false;
    const PlatformAPI* api = platform_get_api();
    api->mutex_lock(client->lock);
    bool done = (client->status == HTTP_STATUS_DONE || client->status == HTTP_STATUS_ERROR);
    api->mutex_unlock(client->lock);
    return done;
}

HttpResponse* http_client_get_response(HttpClient* client) {
    if (!client || client->status != HTTP_STATUS_DONE) return NULL;
    return &client->response;
}
