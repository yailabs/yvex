/*
 * YVEX - HTTP internals
 *
 * File: src/server/http_internal.h
 * Layer: server implementation
 *
 * Purpose:
 *   Defines the tiny K0 HTTP parser/response builder used by yvexd. The
 *   parser intentionally supports only short request lines and status endpoints.
 */
#ifndef YVEX_HTTP_INTERNAL_H
#define YVEX_HTTP_INTERNAL_H

#include <stddef.h>

#include <yvex/yvex.h>

#define YVEX_HTTP_METHOD_CAP 8
#define YVEX_HTTP_PATH_CAP 256
#define YVEX_HTTP_BODY_CAP 8192
#define YVEX_HTTP_RESPONSE_CAP 12288

typedef struct {
    char method[YVEX_HTTP_METHOD_CAP];
    char path[YVEX_HTTP_PATH_CAP];
} yvex_http_request;

typedef struct {
    int status_code;
    const char *reason;
    char body[YVEX_HTTP_BODY_CAP];
} yvex_http_response;

int yvex_http_parse_request(const char *request,
                            yvex_http_request *out,
                            yvex_error *err);
int yvex_http_response_format(char *out,
                              size_t cap,
                              const yvex_http_response *response,
                              yvex_error *err);
const char *yvex_http_status_reason(int status_code);

#endif /* YVEX_HTTP_INTERNAL_H */
