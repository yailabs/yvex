/*
 * YVEX - Server router
 *
 * File: src/server/router.c
 * Layer: server implementation
 *
 * Purpose:
 *   Routes the small K0 HTTP surface to status handlers.
 */
#include "server_internal.h"

#include <stdio.h>
#include <string.h>

int yvex_server_route(yvex_server *server,
                      const yvex_http_request *request,
                      yvex_http_response *response,
                      yvex_error *err)
{
    int rc;

    if (!server || !request || !response) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_server_route",
                       "server, request, and response are required");
        return YVEX_ERR_INVALID_ARG;
    }

    if (strcmp(request->path, "/v1/completions") == 0 ||
        strcmp(request->path, "/v1/chat/completions") == 0 ||
        strcmp(request->path, "/v1/responses") == 0) {
        return yvex_server_handle_unsupported_generation(response, err);
    }

    if (strcmp(request->method, "GET") != 0) {
        response->status_code = 405;
        response->reason = yvex_http_status_reason(405);
        snprintf(response->body, sizeof(response->body),
                 "{\n"
                 "  \"schema\": \"yvex.error.v1\",\n"
                 "  \"status\": \"method_not_allowed\"\n"
                 "}\n");
        return YVEX_OK;
    }

    if (strcmp(request->path, "/") == 0 || strcmp(request->path, "/health") == 0) {
        rc = yvex_server_handle_health(server, response, err);
    } else if (strcmp(request->path, "/metrics") == 0) {
        rc = yvex_server_handle_metrics(server, response, err);
    } else if (strcmp(request->path, "/v1/models") == 0) {
        rc = yvex_server_handle_models(server, response, err);
    } else {
        response->status_code = 404;
        response->reason = yvex_http_status_reason(404);
        snprintf(response->body, sizeof(response->body),
                 "{\n"
                 "  \"schema\": \"yvex.error.v1\",\n"
                 "  \"status\": \"not_found\"\n"
                 "}\n");
        rc = YVEX_OK;
    }

    return rc;
}
