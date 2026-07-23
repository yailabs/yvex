/* Owner: server.core.
 * Owns: HTTP server lifecycle, bounded request parsing, routing, and response framing.
 * Does not own: runtime semantics, backend policy, model support, or generation.
 * Invariants: one server owns one socket and every advertised generation fact remains false.
 * Boundary: provider status shell; it cannot open a runtime model or promote capability.
 * Purpose: expose health, metrics, and admitted model facts through a local HTTP endpoint.
 * Inputs: server options, accepted socket bytes, and immutable domain summaries.
 * Effects: owns sockets, request counters, and HTTP replies.
 * Failure: rejects invalid requests and unwinds only server-owned socket resources. */
#define _POSIX_C_SOURCE 200809L

/*
 * core.c - Provider daemon HTTP status shell.
 *
 * This file owns server state, HTTP parsing, routing, handlers, and response
 * formatting. Generation endpoints remain unsupported. */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <yvex/internal/core.h>
#include <yvex/server.h>

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
    int listen_fd;
    int one_request;
    unsigned long long request_count;
    unsigned long long error_count;
};

static void record_response(yvex_server *server, int status_code);

/* Purpose: close and invalidate the server's listening descriptor.
 * Inputs: nullable mutable server state.
 * Effects: closes one owned descriptor at most once.
 * Failure: close errors are ignored during deterministic cleanup.
 * Boundary: socket lifecycle only. */
static void close_listen_fd(yvex_server *server)
{
    if (server && server->listen_fd >= 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
    }
}

/* Purpose: construct one server and optionally load its admitted runtime handles.
 * Inputs: result slot, nullable options, and typed error output.
 * Effects: allocates server state and may open an engine and CPU backend.
 * Failure: validation, allocation, or runtime-open errors publish no server.
 * Boundary: server lifecycle construction; listening starts separately. */
int yvex_server_create(yvex_server **out,
                       const yvex_server_options *options,
                       yvex_error *err)
{
    yvex_server *server;
    const char *host = "127.0.0.1";
    const char *backend_name = "cpu";
    int should_load = 0;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_server_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (options && options->host && options->host[0] != '\0') {
        host = options->host;
    }
    if (options && options->backend_name && options->backend_name[0] != '\0') {
        backend_name = options->backend_name;
    }
    if (options && options->port == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_server_create",
                       "port must be in range 1..65535");
        return YVEX_ERR_INVALID_ARG;
    }

    server = (yvex_server *)calloc(1u, sizeof(*server));
    if (!server) {
        *out = NULL;
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_server_create", "failed to allocate server");
        return YVEX_ERR_NOMEM;
    }

    server->status = YVEX_SERVER_STATUS_CREATED;
    server->listen_fd = -1;
    yvex_core_text_copy(server->host, sizeof(server->host), host);
    server->port = options ? options->port : 8080;
    yvex_core_text_copy(server->backend_name, sizeof(server->backend_name), backend_name);
    yvex_core_text_copy(server->engine_status, sizeof(server->engine_status), "not_loaded");
    yvex_core_text_copy(server->backend_status, sizeof(server->backend_status), "not_loaded");
    if (options && options->model_path) {
        yvex_core_text_copy(server->model_path, sizeof(server->model_path), options->model_path);
    }
    server->one_request = options ? options->one_request : 0;

    should_load = options && (options->load_engine || server->model_path[0] != '\0');
    if (should_load) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_server_create",
                       "server runtime attachment is not implemented");
        yvex_server_close(server);
        *out = NULL;
        return YVEX_ERR_UNSUPPORTED;
    }

    *out = server;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: deliver an exact response byte range to one connected socket.
 * Inputs: valid descriptor, immutable bytes, exact length, and error output.
 * Effects: advances the socket stream until all bytes are sent.
 * Failure: the first send error aborts with typed I/O failure.
 * Boundary: bounded socket delivery; descriptor ownership stays with caller. */
static int write_all(int fd, const char *buf, size_t len, yvex_error *err)
{
    size_t off = 0;
    ssize_t n;

    while (off < len) {
        n = send(fd, buf + off, len - off, 0);
        if (n < 0) {
            yvex_error_setf(err, YVEX_ERR_IO, "yvex_server_serve",
                            "socket write failed: %s", strerror(errno));
            return YVEX_ERR_IO;
        }
        off += (size_t)n;
    }
    return YVEX_OK;
}

/* Purpose: parse, route, frame, and deliver one bounded HTTP request.
 * Inputs: mutable server, connected descriptor, and error output.
 * Effects: reads one request, records its status, and writes one response.
 * Failure: socket, parser, router, or framing failures return typed refusal.
 * Boundary: single-client transaction; caller closes the descriptor. */
static int serve_client(yvex_server *server, int client_fd, yvex_error *err)
{
    char request_buf[4096];
    char response_buf[YVEX_HTTP_RESPONSE_CAP];
    yvex_http_request request;
    yvex_http_response response;
    ssize_t n;
    int rc;

    memset(&response, 0, sizeof(response));
    n = recv(client_fd, request_buf, sizeof(request_buf) - 1u, 0);
    if (n <= 0) {
        yvex_error_set(err, YVEX_ERR_IO, "yvex_server_serve", "socket read failed");
        return YVEX_ERR_IO;
    }
    request_buf[n] = '\0';

    rc = yvex_http_parse_request(request_buf, &request, err);
    if (rc == YVEX_OK) {
        rc = yvex_server_route(server, &request, &response, err);
    }
    if (rc != YVEX_OK) {
        response.status_code = 400;
        response.reason = yvex_http_status_reason(400);
        snprintf(response.body, sizeof(response.body),
                 "{\n"
                 "  \"schema\": \"yvex.error.v1\",\n"
                 "  \"status\": \"bad_request\"\n"
                 "}\n");
        rc = YVEX_OK;
    }

    record_response(server, response.status_code);
    rc = yvex_http_response_format(response_buf, sizeof(response_buf), &response, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    return write_all(client_fd, response_buf, strlen(response_buf), err);
}

/* Purpose: bind and run the admitted blocking server loop.
 * Inputs: created server state and typed error output.
 * Effects: owns a listening socket and processes one or more client transactions.
 * Failure: socket, address, bind, listen, or accept failure poisons server status.
 * Boundary: provider transport only; request handlers preserve unsupported generation. */
int yvex_server_serve(yvex_server *server,
                      yvex_error *err)
{
    int fd;
    int opt = 1;
    struct sockaddr_in addr;

    if (!server) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_server_serve", "server is required");
        return YVEX_ERR_INVALID_ARG;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        server->status = YVEX_SERVER_STATUS_FAILED;
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_server_serve",
                        "socket failed: %s", strerror(errno));
        return YVEX_ERR_IO;
    }
    server->listen_fd = fd;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)server->port);
    if (inet_pton(AF_INET, server->host, &addr.sin_addr) != 1) {
        close_listen_fd(server);
        server->status = YVEX_SERVER_STATUS_FAILED;
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_server_serve",
                       "host must be an IPv4 address in server shell");
        return YVEX_ERR_INVALID_ARG;
    }
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close_listen_fd(server);
        server->status = YVEX_SERVER_STATUS_FAILED;
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_server_serve",
                        "cannot bind %s:%u: %s", server->host, server->port, strerror(errno));
        return YVEX_ERR_IO;
    }
    if (listen(fd, 8) != 0) {
        close_listen_fd(server);
        server->status = YVEX_SERVER_STATUS_FAILED;
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_server_serve",
                        "listen failed: %s", strerror(errno));
        return YVEX_ERR_IO;
    }

    server->status = YVEX_SERVER_STATUS_LISTENING;
    do {
        int client_fd = accept(fd, NULL, NULL);
        if (client_fd < 0) {
            close_listen_fd(server);
            server->status = YVEX_SERVER_STATUS_FAILED;
            yvex_error_setf(err, YVEX_ERR_IO, "yvex_server_serve",
                            "accept failed: %s", strerror(errno));
            return YVEX_ERR_IO;
        }
        (void)serve_client(server, client_fd, err);
        close(client_fd);
    } while (!server->one_request && server->status == YVEX_SERVER_STATUS_LISTENING);

    close_listen_fd(server);
    server->status = YVEX_SERVER_STATUS_STOPPED;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: stop accepting requests and transition a server to stopped.
 * Inputs: mutable server and typed error output.
 * Effects: closes the listening descriptor and updates lifecycle state.
 * Failure: NULL state is refused; cleanup itself is idempotent.
 * Boundary: server lifecycle control. */
int yvex_server_stop(yvex_server *server,
                     yvex_error *err)
{
    if (!server) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_server_stop", "server is required");
        return YVEX_ERR_INVALID_ARG;
    }
    close_listen_fd(server);
    server->status = YVEX_SERVER_STATUS_STOPPED;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: release all resources owned by one server.
 * Inputs: nullable owned server state.
 * Effects: closes the socket and server allocation in dependency order.
 * Failure: none is reportable through this terminal void operation.
 * Boundary: deterministic server teardown. */
void yvex_server_close(yvex_server *server)
{
    if (!server) {
        return;
    }
    close_listen_fd(server);
    free(server);
}

/* Purpose: expose an immutable snapshot of server and attached-runtime facts.
 * Inputs: server, caller-owned output, and typed error storage.
 * Effects: writes borrowed string views and scalar counters to the result.
 * Failure: missing input or output is refused before publication.
 * Boundary: report projection; generation availability remains explicit false. */
int yvex_server_get_summary(const yvex_server *server,
                            yvex_server_summary *out,
                            yvex_error *err)
{
    if (!server || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_server_get_summary",
                       "server and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    out->status = server->status;
    out->host = server->host;
    out->port = server->port;
    out->model_path = server->model_path;
    out->backend_name = server->backend_name;
    out->engine_status = server->engine_status;
    out->backend_status = server->backend_status;
    out->request_count = server->request_count;
    out->error_count = server->error_count;
    out->generation_available = 0;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: initialize a bounded JSON response body and matching reason phrase.
 * Inputs: output response, status code, immutable body, and error output.
 * Effects: replaces the response only when its body fits.
 * Failure: invalid input or excess body length returns typed refusal.
 * Boundary: response construction, not route selection. */
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

/* Purpose: construct the health endpoint from current server facts.
 * Inputs: immutable server, response output, and error output.
 * Effects: publishes one bounded HTTP 200 response.
 * Failure: response framing bounds are propagated.
 * Boundary: health reporting; generation remains explicitly unavailable. */
static int handle_health(const yvex_server *server,
                         yvex_http_response *response,
                         yvex_error *err)
{
    char body[YVEX_HTTP_BODY_CAP];
    const char *engine_status = server ? server->engine_status : "not_loaded";
    const char *backend_status = server ? server->backend_status : "not_loaded";

    snprintf(body, sizeof(body),
             "{\n"
             "  \"schema\": \"api.health.v1\",\n"
             "  \"status\": \"ok\",\n"
             "  \"server\": \"yvexd\",\n"
             "  \"generation_available\": false,\n"
             "  \"engine_status\": \"%s\",\n"
             "  \"backend_status\": \"%s\"\n"
             "}\n",
             engine_status, backend_status);
    return response_body(response, 200, body, err);
}

/* Purpose: construct the server metrics endpoint from monotonic counters.
 * Inputs: immutable server, response output, and error output.
 * Effects: publishes one JSON counter snapshot.
 * Failure: response-capacity failure is propagated.
 * Boundary: observational surface, not benchmark evidence. */
static int handle_metrics(const yvex_server *server,
                          yvex_http_response *response,
                          yvex_error *err)
{
    char body[YVEX_HTTP_BODY_CAP];
    const char *engine_status = server ? server->engine_status : "not_loaded";
    const char *backend_status = server ? server->backend_status : "not_loaded";
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

/* Purpose: construct the admitted descriptor-only model listing.
 * Inputs: immutable server, response output, and error output.
 * Effects: publishes an empty or one-entry model response.
 * Failure: response-capacity failure prevents success.
 * Boundary: inventory surface; it does not claim inference support. */
static int handle_models(const yvex_server *server,
                         yvex_http_response *response,
                         yvex_error *err)
{
    char body[YVEX_HTTP_BODY_CAP];

    (void)server;

    snprintf(body, sizeof(body),
             "{\n"
             "  \"schema\": \"yvex.models.v1\",\n"
             "  \"object\": \"list\",\n"
             "  \"generation_available\": false,\n"
             "  \"data\": []\n"
             "}\n");
    return response_body(response, 200, body, err);
}

/* Purpose: produce the canonical refusal for generation endpoints.
 * Inputs: response output and typed error storage.
 * Effects: writes an HTTP 501 response with unsupported status.
 * Failure: body framing errors are propagated.
 * Boundary: preserves the generation capability gate at false. */
static int handle_unsupported_generation(yvex_http_response *response,
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

/* Purpose: map admitted HTTP status codes to stable reason phrases.
 * Inputs: integer status code.
 * Effects: none; returned storage is static.
 * Failure: unrecognized codes use the internal-error phrase.
 * Boundary: transport rendering only. */
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

/* Purpose: parse one bounded HTTP request line into typed method and path fields.
 * Inputs: immutable request bytes, output structure, and error output.
 * Effects: clears and populates the result on successful syntax admission.
 * Failure: incomplete, malformed, or invalid-version requests are refused.
 * Boundary: syntax parser; headers and bodies are outside this shell contract. */
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

/* Purpose: frame one typed JSON response as complete HTTP/1.1 bytes.
 * Inputs: bounded destination, immutable response, and error output.
 * Effects: writes a terminated response only when it fits.
 * Failure: invalid input or capacity overflow returns typed refusal.
 * Boundary: transport encoding; response facts are owned by handlers. */
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

/* Purpose: update request and error counters after one routed response. */
static void record_response(yvex_server *server, int status_code)
{
    if (!server) {
        return;
    }
    server->request_count += 1u;
    if (status_code >= 400) {
        server->error_count += 1u;
    }
}

/* Purpose: route one parsed request to the closed set of server-shell handlers.
 * Inputs: mutable server, immutable request, response output, and error output.
 * Effects: constructs exactly one response without performing generation.
 * Failure: invalid input or handler framing failure returns typed refusal.
 * Boundary: routing policy for health, metrics, models, and explicit generation refusal. */
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
        return handle_unsupported_generation(response, err);
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
        rc = handle_health(server, response, err);
    } else if (strcmp(request->path, "/metrics") == 0) {
        rc = handle_metrics(server, response, err);
    } else if (strcmp(request->path, "/v1/models") == 0) {
        rc = handle_models(server, response, err);
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
