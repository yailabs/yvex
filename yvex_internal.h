/*
 * yvex_internal.h - Private declarations shared by implementation files.
 *
 * This header is not part of the public C API. Keep it small and use it only
 * when a private type or helper must cross a real source-file boundary.
 */
#ifndef YVEX_INTERNAL_H
#define YVEX_INTERNAL_H

#include <stddef.h>
#include <stdio.h>

#include <yvex/yvex.h>

typedef struct yvex_backend_vtable {
    int (*close)(yvex_backend *backend, yvex_error *err);
    int (*memory_stats)(const yvex_backend *backend,
                        yvex_backend_memory_stats *out,
                        yvex_error *err);
    int (*device_info)(const yvex_backend *backend,
                       yvex_backend_device_info *out,
                       yvex_error *err);
    int (*tensor_alloc)(yvex_backend *backend,
                        const yvex_backend_tensor_desc *desc,
                        yvex_device_tensor **out,
                        yvex_error *err);
    void (*tensor_free)(yvex_backend *backend, yvex_device_tensor *tensor);
    int (*tensor_write)(yvex_backend *backend,
                        yvex_device_tensor *tensor,
                        const void *src,
                        unsigned long long len,
                        yvex_error *err);
    int (*tensor_read)(yvex_backend *backend,
                       const yvex_device_tensor *tensor,
                       void *dst,
                       unsigned long long len,
                       yvex_error *err);
    int (*tensor_copy)(yvex_backend *backend,
                       yvex_device_tensor *dst,
                       const yvex_device_tensor *src,
                       yvex_error *err);
    int (*sync)(yvex_backend *backend, yvex_error *err);
    int (*supports)(const yvex_backend *backend, yvex_backend_capability capability);
    int (*op_embed)(yvex_backend *backend,
                    const yvex_device_tensor *embedding,
                    const unsigned int *token_ids,
                    unsigned long long token_count,
                    yvex_device_tensor *out,
                    yvex_error *err);
} yvex_backend_vtable;

struct yvex_backend {
    yvex_backend_kind kind;
    yvex_backend_status status;
    const yvex_backend_vtable *vtable;
    yvex_backend_memory_stats stats;
    yvex_backend_device_info device_info;
    char device_name_storage[128];
    void *impl;
    unsigned long long tensor_id_next;
};

struct yvex_device_tensor {
    yvex_backend *owner;
    unsigned long long owner_id;
    char *name;
    yvex_dtype dtype;
    unsigned int rank;
    unsigned long long dims[YVEX_TENSOR_MAX_DIMS];
    unsigned long long bytes;
    unsigned char *data;
    int is_written;
};

char *yvex_backend_strdup(const char *text);
int yvex_backend_desc_is_valid(const yvex_backend_tensor_desc *desc, yvex_error *err);
int yvex_backend_tensor_same_shape(const yvex_device_tensor *a,
                                   const yvex_device_tensor *b);
int yvex_backend_tensor_owner_is(const yvex_backend *backend,
                                 const yvex_device_tensor *tensor);

int yvex_backend_open_cpu_impl(yvex_backend **out,
                               unsigned long long memory_limit_bytes,
                               yvex_error *err);
int yvex_backend_open_cuda_impl(yvex_backend **out,
                                const char *device,
                                unsigned long long memory_limit_bytes,
                                yvex_error *err);

typedef struct {
    yvex_engine *engine;
    yvex_backend *backend;
    yvex_session *session;
    yvex_metrics *metrics;
    yvex_trace *trace;
    char *model_path;
    char *backend_name;
    unsigned long long accepted_turns;
} yvex_chat_runtime;

typedef struct {
    char model_name[128];
    char backend_name[32];
    char session_state[32];
    unsigned long long prompt_tokens;
    unsigned long long accepted_tokens;
    unsigned long long position;
    int execution_ready;
    char generation[32];
    char reason[160];
    char run_id[YVEX_RUN_ID_CAP];
    char run_dir[YVEX_PATH_CAP];
    char metrics_out[YVEX_PATH_CAP];
    char trace_out[YVEX_PATH_CAP];
    char profile_out[YVEX_PATH_CAP];
} yvex_chat_accept_result;

int yvex_chat_runtime_open(yvex_chat_runtime *runtime,
                           const char *model_path,
                           const char *backend_name,
                           unsigned long long context_length,
                           yvex_error *err);
void yvex_chat_runtime_close(yvex_chat_runtime *runtime);
int yvex_chat_runtime_accept_user_text(yvex_chat_runtime *runtime,
                                       const char *system_text,
                                       const char *user_text,
                                       yvex_chat_accept_result *out,
                                       yvex_error *err);
int yvex_chat_runtime_reset(yvex_chat_runtime *runtime, yvex_error *err);
int yvex_chat_runtime_get_summary(const yvex_chat_runtime *runtime,
                                  yvex_session_summary *out,
                                  yvex_error *err);
void yvex_chat_runtime_set_observers(yvex_chat_runtime *runtime,
                                     yvex_metrics *metrics,
                                     yvex_trace *trace);
int yvex_chat_runtime_print_status(FILE *fp,
                                   const yvex_chat_runtime *runtime,
                                   yvex_error *err);
int yvex_run_command_plain(FILE *fp, const yvex_chat_accept_result *result);
int yvex_run_command_json(FILE *fp, const yvex_chat_accept_result *result);
int yvex_status_line_print(FILE *fp,
                           const char *phase,
                           unsigned long long tokens,
                           unsigned long long position);

typedef enum {
    YVEX_SLASH_NOT_COMMAND = 0,
    YVEX_SLASH_HELP,
    YVEX_SLASH_STATUS,
    YVEX_SLASH_MODEL,
    YVEX_SLASH_BACKEND,
    YVEX_SLASH_TOKENS,
    YVEX_SLASH_RESET,
    YVEX_SLASH_QUIT,
    YVEX_SLASH_UNKNOWN
} yvex_slash_command;

yvex_slash_command yvex_slash_parse(const char *line);
const char *yvex_slash_command_name(yvex_slash_command command);

typedef struct {
    char run_id[YVEX_RUN_ID_CAP];
    char run_dir[YVEX_PATH_CAP];
    char command_path[YVEX_PATH_CAP];
    char metrics_path[YVEX_PATH_CAP];
    char trace_path[YVEX_PATH_CAP];
    char profile_path[YVEX_PATH_CAP];
    int has_run_dir;
    int has_metrics;
    int has_trace;
    int has_profile;
} yvex_run_artifacts;

unsigned long long yvex_time_monotonic_ns(void);
int yvex_json_write_string(FILE *fp, const char *text);
int yvex_run_artifacts_prepare(yvex_run_artifacts *out,
                               int save_run,
                               const char *run_dir,
                               const char *metrics_out,
                               const char *trace_out,
                               const char *profile_out,
                               yvex_error *err);
int yvex_run_artifacts_write_command(const yvex_run_artifacts *artifacts,
                                     int argc,
                                     char **argv,
                                     yvex_error *err);

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

int yvex_http_parse_request(const char *request,
                            yvex_http_request *out,
                            yvex_error *err);
int yvex_http_response_format(char *out,
                              size_t cap,
                              const yvex_http_response *response,
                              yvex_error *err);
const char *yvex_http_status_reason(int status_code);

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

#endif /* YVEX_INTERNAL_H */
