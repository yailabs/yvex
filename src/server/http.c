/*
 * YVEX - HTTP parser and response builder
 *
 * File: src/server/http.c
 * Layer: server implementation
 *
 * Purpose:
 *   Implements the small HTTP/1.1 subset needed by K0 yvexd endpoints.
 */
#include "http_internal.h"

#include <stdio.h>
#include <string.h>

const char *yvex_http_status_reason(int status_code)
{
    switch (status_code) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 501: return "Not Implemented";
    default: return "Internal Server Error";
    }
}

int yvex_http_parse_request(const char *request,
                            yvex_http_request *out,
                            yvex_error *err)
{
    char version[16];
    int n;

    if (!request || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_http_parse_request",
                       "request and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    if (!strchr(request, '\n')) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_http_parse_request",
                       "request line is too long or incomplete");
        return YVEX_ERR_BOUNDS;
    }

    n = sscanf(request, "%7s %255s %15s", out->method, out->path, version);
    if (n != 3) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_http_parse_request",
                       "invalid HTTP request line");
        return YVEX_ERR_FORMAT;
    }
    if (strncmp(version, "HTTP/", 5) != 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_http_parse_request",
                       "invalid HTTP version");
        return YVEX_ERR_FORMAT;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_http_response_format(char *out,
                              size_t cap,
                              const yvex_http_response *response,
                              yvex_error *err)
{
    const char *reason;
    size_t body_len;
    int n;

    if (!out || cap == 0 || !response) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_http_response_format",
                       "out and response are required");
        return YVEX_ERR_INVALID_ARG;
    }

    reason = response->reason ? response->reason : yvex_http_status_reason(response->status_code);
    body_len = strlen(response->body);
    n = snprintf(out, cap,
                 "HTTP/1.1 %d %s\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %lu\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "%s",
                 response->status_code,
                 reason,
                 (unsigned long)body_len,
                 response->body);
    if (n < 0 || (size_t)n >= cap) {
        out[cap - 1u] = '\0';
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_http_response_format",
                       "HTTP response exceeds buffer capacity");
        return YVEX_ERR_BOUNDS;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}
