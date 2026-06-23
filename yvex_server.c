#define _POSIX_C_SOURCE 200809L

#include "yvex_internal.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static void copy_label(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) {
        return;
    }
    snprintf(dst, cap, "%s", src ? src : "");
    dst[cap - 1u] = '\0';
}

static void close_listen_fd(yvex_server *server)
{
    if (server && server->listen_fd >= 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
    }
}

static int load_server_runtime(yvex_server *server, yvex_error *err)
{
    yvex_backend_options backend_options;
    yvex_engine_summary engine_summary;
    int rc;

    if (!server || server->model_path[0] == '\0') {
        return YVEX_OK;
    }
    if (strcmp(server->backend_name, "cpu") != 0) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_server_create",
                       "server model backend is unsupported in server shell");
        return YVEX_ERR_UNSUPPORTED;
    }

    rc = yvex_engine_open_path(&server->engine, server->model_path, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    memset(&backend_options, 0, sizeof(backend_options));
    backend_options.kind = YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&server->backend, &backend_options, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    if (yvex_engine_get_summary(server->engine, &engine_summary, err) == YVEX_OK) {
        copy_label(server->engine_status, sizeof(server->engine_status),
                   yvex_engine_status_name(engine_summary.status));
        copy_label(server->model_name, sizeof(server->model_name), engine_summary.model_name);
        copy_label(server->architecture, sizeof(server->architecture), engine_summary.architecture);
    } else {
        copy_label(server->engine_status, sizeof(server->engine_status), "failed");
    }
    copy_label(server->backend_status, sizeof(server->backend_status),
               yvex_backend_status_name(yvex_backend_status_of(server->backend)));

    yvex_error_clear(err);
    return YVEX_OK;
}

const char *yvex_server_status_name(yvex_server_status status)
{
    switch (status) {
    case YVEX_SERVER_STATUS_CREATED: return "created";
    case YVEX_SERVER_STATUS_LISTENING: return "listening";
    case YVEX_SERVER_STATUS_STOPPED: return "stopped";
    case YVEX_SERVER_STATUS_FAILED: return "failed";
    default: return "unknown";
    }
}

int yvex_server_create(yvex_server **out,
                       const yvex_server_options *options,
                       yvex_error *err)
{
    yvex_server *server;
    const char *host = "127.0.0.1";
    const char *backend_name = "cpu";
    int should_load = 0;
    int rc;

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
    copy_label(server->host, sizeof(server->host), host);
    server->port = options ? options->port : 8080;
    copy_label(server->backend_name, sizeof(server->backend_name), backend_name);
    copy_label(server->engine_status, sizeof(server->engine_status), "not_loaded");
    copy_label(server->backend_status, sizeof(server->backend_status), "not_loaded");
    if (options && options->model_path) {
        copy_label(server->model_path, sizeof(server->model_path), options->model_path);
    }
    server->one_request = options ? options->one_request : 0;

    should_load = options && (options->load_engine || server->model_path[0] != '\0');
    if (should_load) {
        rc = load_server_runtime(server, err);
        if (rc != YVEX_OK) {
            yvex_server_close(server);
            *out = NULL;
            return rc;
        }
    }

    *out = server;
    yvex_error_clear(err);
    return YVEX_OK;
}

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

    yvex_server_record_response(server, response.status_code);
    rc = yvex_http_response_format(response_buf, sizeof(response_buf), &response, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    return write_all(client_fd, response_buf, strlen(response_buf), err);
}

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

void yvex_server_close(yvex_server *server)
{
    if (!server) {
        return;
    }
    close_listen_fd(server);
    yvex_backend_close(server->backend);
    yvex_engine_close(server->engine);
    free(server);
}

yvex_server_status yvex_server_status_of(const yvex_server *server)
{
    return server ? server->status : YVEX_SERVER_STATUS_STOPPED;
}

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

#include "yvex_internal.h"

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

#include "yvex_internal.h"

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

#include "yvex_internal.h"

void yvex_server_record_response(yvex_server *server, int status_code)
{
    if (!server) {
        return;
    }
    server->request_count += 1u;
    if (status_code >= 400) {
        server->error_count += 1u;
    }
}

#include "yvex_internal.h"

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
