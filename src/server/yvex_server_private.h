/*
 * yvex_server_private.h - Server HTTP shell boundary.
 *
 * This header is private to the daemon entrypoint, server implementation, and
 * server tests.
 */
#ifndef YVEX_SERVER_PRIVATE_H
#define YVEX_SERVER_PRIVATE_H

#include <stddef.h>

#include <yvex/yvex.h>

#define YVEX_HTTP_METHOD_CAP 8
#define YVEX_HTTP_PATH_CAP 256
#define YVEX_HTTP_BODY_CAP 8192
#define YVEX_HTTP_RESPONSE_CAP 12288
#define YVEX_SERVER_LABEL_CAP 128

typedef struct {
    char method[YVEX_HTTP_METHOD_CAP];
    char path[YVEX_HTTP_PATH_CAP];
} yvex_http_request;

typedef struct {
    int status_code;
    const char *reason;
    char body[YVEX_HTTP_BODY_CAP];
} yvex_http_response;

struct yvex_server {
    yvex_server_status status;
    char host[YVEX_PATH_CAP];
    unsigned int port;
    char model_path[YVEX_PATH_CAP];
    char backend_name[YVEX_SERVER_LABEL_CAP];
    char engine_status[YVEX_SERVER_LABEL_CAP];
    char backend_status[YVEX_SERVER_LABEL_CAP];
    char model_name[YVEX_SERVER_LABEL_CAP];
    char architecture[YVEX_SERVER_LABEL_CAP];
    yvex_engine *engine;
    yvex_backend *backend;
    int listen_fd;
    int one_request;
    unsigned long long request_count;
    unsigned long long error_count;
};

int yvex_http_parse_request(const char *request,
                            yvex_http_request *out,
                            yvex_error *err);
int yvex_http_response_format(char *out,
                              size_t cap,
                              const yvex_http_response *response,
                              yvex_error *err);
const char *yvex_http_status_reason(int status_code);

int yvex_server_route(yvex_server *server,
                      const yvex_http_request *request,
                      yvex_http_response *response,
                      yvex_error *err);
int yvex_server_handle_health(const yvex_server *server,
                              yvex_http_response *response,
                              yvex_error *err);
int yvex_server_handle_metrics(const yvex_server *server,
                               yvex_http_response *response,
                               yvex_error *err);
int yvex_server_handle_models(const yvex_server *server,
                              yvex_http_response *response,
                              yvex_error *err);
int yvex_server_handle_unsupported_generation(yvex_http_response *response,
                                              yvex_error *err);
void yvex_server_record_response(yvex_server *server, int status_code);

#endif /* YVEX_SERVER_PRIVATE_H */
