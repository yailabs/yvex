/*
 * YVEX - Server endpoint handlers
 *
 * File: yvex_server_handlers.c
 * Layer: server implementation
 *
 * Purpose:
 *   Builds server shell yvexd JSON responses for health, metrics, model catalog shell,
 *   and unsupported generation routes.
 */
#include "yvex_server_internal.h"

#include <stdio.h>
#include <string.h>

static int response_body(yvex_http_response *response,
                         int status_code,
                         const char *body,
                         yvex_error *err)
{
    int n;

    if (!response || !body) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_server_response",
                       "response and body are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(response, 0, sizeof(*response));
    response->status_code = status_code;
    response->reason = yvex_http_status_reason(status_code);
    n = snprintf(response->body, sizeof(response->body), "%s", body);
    if (n < 0 || (size_t)n >= sizeof(response->body)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_server_response",
                       "response body exceeds capacity");
        return YVEX_ERR_BOUNDS;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_server_handle_health(const yvex_server *server,
                              yvex_http_response *response,
                              yvex_error *err)
{
    char body[YVEX_HTTP_BODY_CAP];
    const char *engine_status = server && server->engine ? server->engine_status : "not_loaded";
    const char *backend_status = server && server->backend ? server->backend_status : "not_loaded";

    snprintf(body, sizeof(body),
             "{\n"
             "  \"schema\": \"yvex.health.v1\",\n"
             "  \"status\": \"ok\",\n"
             "  \"server\": \"yvexd\",\n"
             "  \"generation_available\": false,\n"
             "  \"engine_status\": \"%s\",\n"
             "  \"backend_status\": \"%s\"\n"
             "}\n",
             engine_status, backend_status);
    return response_body(response, 200, body, err);
}

int yvex_server_handle_metrics(const yvex_server *server,
                               yvex_http_response *response,
                               yvex_error *err)
{
    char body[YVEX_HTTP_BODY_CAP];
    const char *engine_status = server && server->engine ? server->engine_status : "not_loaded";
    const char *backend_status = server && server->backend ? server->backend_status : "not_loaded";
    unsigned long long request_count = server ? server->request_count + 1u : 0;
    unsigned long long error_count = server ? server->error_count : 0;

    snprintf(body, sizeof(body),
             "{\n"
             "  \"schema\": \"yvex.server_metrics.v1\",\n"
             "  \"request_count\": %llu,\n"
             "  \"error_count\": %llu,\n"
             "  \"generation_available\": false,\n"
             "  \"engine_status\": \"%s\",\n"
             "  \"backend_status\": \"%s\"\n"
             "}\n",
             request_count, error_count, engine_status, backend_status);
    return response_body(response, 200, body, err);
}

int yvex_server_handle_models(const yvex_server *server,
                              yvex_http_response *response,
                              yvex_error *err)
{
    char body[YVEX_HTTP_BODY_CAP];

    if (!server || !server->engine) {
        snprintf(body, sizeof(body),
                 "{\n"
                 "  \"schema\": \"yvex.models.v1\",\n"
                 "  \"object\": \"list\",\n"
                 "  \"generation_available\": false,\n"
                 "  \"data\": []\n"
                 "}\n");
        return response_body(response, 200, body, err);
    }

    snprintf(body, sizeof(body),
             "{\n"
             "  \"schema\": \"yvex.models.v1\",\n"
             "  \"object\": \"list\",\n"
             "  \"generation_available\": false,\n"
             "  \"data\": [\n"
             "    {\n"
             "      \"id\": \"%s\",\n"
             "      \"object\": \"model\",\n"
             "      \"status\": \"descriptor-only\",\n"
             "      \"architecture\": \"%s\",\n"
             "      \"backend\": \"%s\",\n"
             "      \"inference\": \"not_implemented\"\n"
             "    }\n"
             "  ]\n"
             "}\n",
             server->model_name[0] ? server->model_name : "unknown",
             server->architecture[0] ? server->architecture : "unknown",
             server->backend_name[0] ? server->backend_name : "cpu");
    return response_body(response, 200, body, err);
}

int yvex_server_handle_unsupported_generation(yvex_http_response *response,
                                              yvex_error *err)
{
    return response_body(response, 501,
                         "{\n"
                         "  \"schema\": \"yvex.error.v1\",\n"
                         "  \"status\": \"unsupported\",\n"
                         "  \"error\": {\n"
                         "    \"code\": \"YVEX_ERR_UNSUPPORTED\",\n"
                         "    \"message\": \"generation endpoints are not implemented in server shell\"\n"
                         "  }\n"
                         "}\n",
                         err);
}
