/*
 * Owner: abi.server (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Server shell
 *
 * File: include/yvex/server.h
 * Layer: public server API
 *
 * Purpose:
 *   Defines the server shell yvexd server shell API. The server exposes local
 *   health, metrics, and model-catalog status endpoints. It does not implement
 *   generation, streamed output, sampler behavior, or compatibility claims.
 *
 * Owns:
 *   - yvex_server
 *   - yvex_server_options
 *   - yvex_server_summary
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_server
 */
#ifndef YVEX_SERVER_H
#define YVEX_SERVER_H

#include <stddef.h>

#include <yvex/backend.h>
#include <yvex/engine.h>
#include <yvex/fs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_server yvex_server;

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

typedef enum {
    YVEX_SERVER_STATUS_CREATED = 0,
    YVEX_SERVER_STATUS_LISTENING,
    YVEX_SERVER_STATUS_STOPPED,
    YVEX_SERVER_STATUS_FAILED
} yvex_server_status;

typedef struct {
    const char *host;
    unsigned int port;
    const char *model_path;
    const char *backend_name;
    int load_engine;
    int one_request;
} yvex_server_options;

typedef struct {
    yvex_server_status status;
    const char *host;
    unsigned int port;
    const char *model_path;
    const char *backend_name;
    const char *engine_status;
    const char *backend_status;
    unsigned long long request_count;
    unsigned long long error_count;
    int generation_available;
} yvex_server_summary;

int yvex_server_create(yvex_server **out,
                       const yvex_server_options *options,
                       yvex_error *err);
int yvex_server_serve(yvex_server *server,
                      yvex_error *err);
int yvex_server_stop(yvex_server *server,
                     yvex_error *err);
void yvex_server_close(yvex_server *server);

yvex_server_status yvex_server_status_of(const yvex_server *server);
const char *yvex_server_status_name(yvex_server_status status);

int yvex_server_get_summary(const yvex_server *server,
                            yvex_server_summary *out,
                            yvex_error *err);

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

#ifdef __cplusplus
}
#endif

#endif /* YVEX_SERVER_H */
