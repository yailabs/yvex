/* Owner: public server ABI.
 * Owns: HTTP parsing, server lifecycle, routing, and bounded responses.
 * Does not own: engine capability, model policy, or transport-independent runtime truth.
 * Invariants: declarations are format-stable, externally consumable, and independently includable.
 * Boundary: server orchestration over admitted backend and runtime contracts.
 * Purpose: Expose server orchestration over admitted backend and runtime contracts.
 * Inputs: Typed caller-owned values and immutable borrowed views as declared below.
 * Effects: Only functions with explicit lifecycle or I/O contracts mutate external state.
 * Failure: Typed status and error outputs remain authoritative; declarations add no capability. */
#ifndef YVEX_SERVER_H
#define YVEX_SERVER_H

#include <stddef.h>
#include <yvex/backend.h>
#include <yvex/core.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Server lifecycle and HTTP boundary. */
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
