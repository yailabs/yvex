/*
 * yvex_models_download_surface.h - models download CLI surface ownership.
 *
 * Owner:
 *   src/cli/model_artifacts
 *
 * Owns:
 *   private CLI-only types and cross-file function declarations for the
 *   models download/status/stop/resume/cleanup command family.
 *
 * Does not own:
 *   model registry storage, artifact identity, domain report construction,
 *   runtime generation, artifact emission, eval, benchmark, or release state.
 *
 * Invariants:
 *   this header is CLI-only and must not enter CORE_SRCS/libyvex.a.
 *
 * Boundary:
 *   download command surfaces route existing source download diagnostics only;
 *   they do not make downloaded artifacts generation-capable.
 */
#ifndef YVEX_MODELS_DOWNLOAD_SURFACE_H
#define YVEX_MODELS_DOWNLOAD_SURFACE_H

#include "yvex_model_artifacts_surface_common.h"

#define YVEX_MODEL_DOWNLOAD_PATTERN_CAP 32u

enum {
    YVEX_MODEL_DOWNLOAD_INTERRUPT_TIMEOUT_SECONDS = 5
};

typedef enum {
    YVEX_MODEL_DOWNLOAD_AUTH_AUTO = 0,
    YVEX_MODEL_DOWNLOAD_AUTH_REQUIRED,
    YVEX_MODEL_DOWNLOAD_AUTH_NEVER
} yvex_model_download_auth_mode;

typedef struct {
    const char *target_id;
    const char *family;
    const char *provider;
    const char *repo_id;
    const char *local_name;
    const char *revision_default;
    const char *artifact_class;
    const char *source_container;
    const char *model_class_hint;
    const char *boundary;
} yvex_model_download_catalog_row;

typedef enum {
    YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO = 0,
    YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE,
    YVEX_MODEL_DOWNLOAD_PROGRESS_PLAIN,
    YVEX_MODEL_DOWNLOAD_PROGRESS_LOG,
    YVEX_MODEL_DOWNLOAD_PROGRESS_OFF
} yvex_model_download_progress_mode;

typedef struct {
    const char *target;
    const char *repo;
    const char *family;
    const char *name;
    const char *revision;
    const char *source;
    const char *provider;
    const char *asset;
    const char *asset_name;
    const char *release;
    const char *github_source;
    const char *models_root;
    const char *token_env;
    const char *cli;
    const char *include_patterns[YVEX_MODEL_DOWNLOAD_PATTERN_CAP];
    const char *exclude_patterns[YVEX_MODEL_DOWNLOAD_PATTERN_CAP];
    unsigned int include_count;
    unsigned int exclude_count;
    unsigned long long max_workers;
    yvex_model_download_auth_mode auth_mode;
    int dry_run;
    int no_manifest;
    int no_native_inventory;
    int force_sidecars;
    int yes;
    int resume;
    int clear_stale_locks;
    int force;
    int match_provider_process;
    int cleanup_stale_locks;
    int cleanup_logs;
    int cleanup_receipts;
    int cleanup_failed_partials;
    int cleanup_all_provider_cache;
    yvex_models_output_mode output_mode;
    yvex_model_download_progress_mode progress_mode;
    unsigned long long tick_seconds;
    unsigned long long timeout_seconds;
} yvex_cli_models_download_options;

typedef struct {
    unsigned long long file_count;
    unsigned long long safetensors_count;
    unsigned long long total_regular_file_bytes;
    unsigned long long largest_file_bytes;
    unsigned long long partial_file_count;
    unsigned long long cache_file_count;
    unsigned long long lock_count;
    unsigned long long lock_age_seconds[YVEX_MODEL_DOWNLOAD_PATTERN_CAP];
    int config_present;
    int tokenizer_present;
    char largest_file_name[YVEX_PATH_CAP];
    char lock_paths[YVEX_MODEL_DOWNLOAD_PATTERN_CAP][YVEX_PATH_CAP];
} yvex_model_download_source_scan;

typedef struct {
    char status[64];
    char target_id[128];
    char family[32];
    char provider[32];
    char repo_id[256];
    char revision[128];
    char local_name[128];
    char local_source_dir[YVEX_PATH_CAP];
    char models_root[YVEX_PATH_CAP];
    char models_root_source[32];
    char reports_dir[YVEX_PATH_CAP];
    char registry_dir[YVEX_PATH_CAP];
    char manifest_path[YVEX_PATH_CAP];
    char native_inventory_path[YVEX_PATH_CAP];
    char download_report_path[YVEX_PATH_CAP];
    char registry_path[YVEX_PATH_CAP];
    char receipt_path[YVEX_PATH_CAP];
    char active_receipt_path[YVEX_PATH_CAP];
    char last_receipt_path[YVEX_PATH_CAP];
    char stdout_log_path[YVEX_PATH_CAP];
    char stderr_log_path[YVEX_PATH_CAP];
    char hf_cli_path[YVEX_PATH_CAP];
    char hf_cli_source[32];
    char provider_cli_path[YVEX_PATH_CAP];
    char provider_cli_source[64];
    char provider_cli_status[32];
    char auth_state[32];
    char credential_source[64];
    char account_hint[128];
    char accounts_state_path[YVEX_PATH_CAP];
    char token_env_name[64];
    char created_at[32];
    char top_blocker[128];
    char error[256];
    char stage_resolve_target[16];
    char stage_resolve_paths[16];
    char stage_prepare_dirs[16];
    char stage_account_provider[16];
    char stage_provider_cli[16];
    char stage_hf_cli[16];
    char stage_download[16];
    char stage_progress_stream[16];
    char stage_progress_ticks[16];
    char stage_source_scan[16];
    char stage_source_manifest[16];
    char stage_native_inventory[16];
    char stage_sidecar[16];
    yvex_model_download_source_scan source_scan;
    yvex_native_weight_summary native_summary;
    int hf_exit_code;
    int provider_exit_code;
    int stdout_streamed;
    int stderr_streamed;
    int interrupted;
    int interrupt_signal;
    int signal_forwarded;
    int child_terminated;
    int child_killed_after_timeout;
    int orphan_check_performed;
    int partial_source_preserved;
    int lock_files_deleted;
    pid_t provider_pid;
    pid_t provider_process_group;
    char child_exit_status[32];
    char orphan_check_status[16];
    unsigned long long stdout_bytes;
    unsigned long long stderr_bytes;
    unsigned long long tick_count;
    unsigned long long tick_last_elapsed_seconds;
    unsigned long long tick_last_file_count;
    unsigned long long tick_last_safetensors_count;
    unsigned long long tick_last_partial_file_count;
    unsigned long long tick_last_cache_file_count;
    unsigned long long tick_last_total_regular_file_bytes;
    unsigned long long tick_last_largest_file_bytes;
    char tick_last_largest_file_name[YVEX_PATH_CAP];
    int source_manifest_written;
    int native_inventory_written;
    int report_written;
    int registry_written;
} yvex_model_download_report;

typedef struct {
    int checked;
    int ok_count;
    int truncated_count;
    int invalid_count;
    char status[32];
} yvex_model_download_safetensors_check;

typedef struct {
    int found;
    char target_id[128];
    char family[32];
    char provider[32];
    char repo_id[256];
    char revision[128];
    char local_name[128];
    char local_source_dir[YVEX_PATH_CAP];
    char registry_path[YVEX_PATH_CAP];
    char download_report_path[YVEX_PATH_CAP];
    char manifest_path[YVEX_PATH_CAP];
    char native_inventory_path[YVEX_PATH_CAP];
} yvex_model_download_resolved_target;

typedef struct {
    unsigned int count;
    pid_t first_pid;
    pid_t first_pgid;
} yvex_model_download_process_match;

int yvex_models_download_surface_command(int arg_count, char **args);

const yvex_model_download_catalog_row *model_download_find_catalog(const char *target);
const char *model_download_progress_mode_name(yvex_model_download_progress_mode mode);
const char *model_download_signal_name(int signo);
const char *model_download_auth_mode_name(yvex_model_download_auth_mode mode);
yvex_model_download_progress_mode model_download_effective_progress_mode(
    yvex_model_download_progress_mode mode);
int parse_models_download_options_from(int arg_count,
                                       char **args,
                                       int start_index,
                                       yvex_cli_models_download_options *options);
unsigned int model_download_effective_include_count(const yvex_cli_models_download_options *options);
unsigned int model_download_effective_exclude_count(const yvex_cli_models_download_options *options);
const char *model_download_effective_include_at(const yvex_cli_models_download_options *options,
                                                unsigned int index);
const char *model_download_effective_exclude_at(const yvex_cli_models_download_options *options,
                                                unsigned int index);
void model_download_report_init(yvex_model_download_report *report);
void model_download_timestamp(char *out, size_t cap);
int model_download_source_path_allowed(const yvex_operator_paths *operator_paths,
                                       const char *path,
                                       yvex_model_download_report *report);
int model_download_file_name_ends_with(const char *path, const char *suffix);
int model_download_name_starts_with(const char *name, const char *prefix);
int model_download_name_contains(const char *name, const char *needle);
int model_download_scan_source(const char *root,
                               yvex_model_download_source_scan *scan,
                               yvex_error *err);
int model_download_check_safetensors_source(const char *root,
                                            yvex_model_download_safetensors_check *check,
                                            yvex_error *err);

int model_download_write_native_inventory_json(const char *path,
                                               const char *source_dir,
                                               const yvex_native_weight_table *table,
                                               yvex_error *err);
int model_download_write_json_sidecar(const char *path,
                                      const char *schema,
                                      const yvex_cli_models_download_options *options,
                                      const yvex_model_download_report *report,
                                      yvex_error *err);
int model_download_write_receipt(const char *path,
                                 const yvex_cli_models_download_options *options,
                                 const yvex_model_download_report *report,
                                 int token_present,
                                 yvex_error *err);
int model_download_write_control_receipt(const char *path,
                                         const yvex_cli_models_download_options *options,
                                         const yvex_model_download_report *report,
                                         const char *receipt_status,
                                         yvex_error *err);
int model_download_finalize_control_receipt(const yvex_cli_models_download_options *options,
                                            const yvex_model_download_report *report,
                                            const char *status);
extern volatile sig_atomic_t yvex_model_download_provider_signal_seen;
int model_download_write_all_fd(int fd, const void *buf, size_t len);
void model_download_mirror_provider_bytes(int fd, const char *buf, size_t len, int normalize_cr);
void model_download_print_start_progress(const yvex_model_download_report *report,
                                         yvex_model_download_progress_mode mode);
void model_download_format_bytes(char *out, size_t cap, unsigned long long bytes);
void model_download_format_elapsed(char *out, size_t cap, unsigned long long seconds);
void model_download_short_file_name(char *out, size_t cap, const char *path);
void model_download_print_tick_progress(const char *source_dir,
                                        time_t started_at,
                                        yvex_model_download_report *report,
                                        yvex_model_download_progress_mode mode);
int model_download_install_provider_signal_handlers(struct sigaction *old_int,
                                                    struct sigaction *old_term,
                                                    yvex_error *err);
void model_download_restore_provider_signal_handlers(const struct sigaction *old_int,
                                                     const struct sigaction *old_term);
void model_download_reset_child_signal_handlers(void);
int model_download_set_nonblocking(int fd);
void model_download_record_child_exit_status(yvex_model_download_report *report, int status);
void model_download_mark_provider_interrupted(yvex_model_download_report *report, int signo, pid_t pgid);
void model_download_orphan_check(yvex_model_download_report *report);

int model_download_run_hf(const yvex_cli_models_download_options *options,
                          yvex_model_download_report *report,
                          const char *token_value,
                          yvex_error *err);
int model_download_run_github(const yvex_cli_models_download_options *options,
                              const yvex_model_download_report *report,
                              yvex_error *err);
int model_download_finish(const yvex_cli_models_download_options *options,
                          yvex_model_download_report *report);
int model_download_read_small_file(const char *path, char *buf, size_t cap);
long long model_download_json_i64_field(const char *text, const char *key);
int model_download_json_string_field(const char *text,
                                     const char *key,
                                     char *out,
                                     size_t out_cap);
int model_download_resolve_downloaded_target(const char *target,
                                             const yvex_operator_paths *operator_paths,
                                             yvex_model_download_resolved_target *out,
                                             yvex_error *err);
int model_download_identity_paths(const char *target,
                                  const char *family,
                                  const yvex_operator_paths *operator_paths,
                                  yvex_model_download_resolved_target *out,
                                  yvex_error *err);
int model_download_read_identity_file(const char *path,
                                      const char *fallback_target,
                                      const char *fallback_family,
                                      yvex_model_download_resolved_target *out);

#endif
