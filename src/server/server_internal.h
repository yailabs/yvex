/*
 * YVEX - Server internals
 *
 * File: src/server/server_internal.h
 * Layer: server implementation
 *
 * Purpose:
 *   Defines the private yvexd server shell state and route helpers.
 */
#ifndef YVEX_SERVER_INTERNAL_H
#define YVEX_SERVER_INTERNAL_H

#include <yvex/yvex.h>

#include "http_internal.h"

#define YVEX_SERVER_LABEL_CAP 128

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

#endif /* YVEX_SERVER_INTERNAL_H */
