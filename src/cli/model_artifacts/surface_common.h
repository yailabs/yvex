/*
 * surface_common.h - shared CLI surface helpers.
 *
 * Owner:
 *   src/cli/model_artifacts
 *
 * Owns:
 *   small shared helper declarations for transitional model-artifacts CLI
 *   command-family surfaces.
 *
 * Does not own:
 *   command-family dispatch bodies, domain algorithms, renderer contracts,
 *   libyvex sources, artifact emission, runtime generation, eval, benchmark,
 *   or release decisions.
 *
 * Invariants:
 *   helpers are CLI-only and must not enter CORE_SRCS.
 *
 * Boundary:
 *   CLI helpers do not imply runtime support or artifact emission.
 */
#ifndef YVEX_MODEL_ARTIFACTS_SURFACE_COMMON_H
#define YVEX_MODEL_ARTIFACTS_SURFACE_COMMON_H

#include "src/core/operator.h"
#include "src/cli/render/private.h"
#include "src/cli/io/out.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <yvex/artifact_integrity.h>
#include <yvex/conversion.h>
#include <yvex/fs.h>
#include <yvex/materialize_gate.h>
#include <yvex/model_gate.h>
#include <yvex/model_ref.h>
#include <yvex/model_registry.h>
#include <yvex/native_weights.h>
#include <yvex/source_manifest.h>
#include <yvex/token_input.h>
#include <yvex/api.h>

typedef enum {
    YVEX_MODELS_OUTPUT_NORMAL = 0,
    YVEX_MODELS_OUTPUT_TABLE,
    YVEX_MODELS_OUTPUT_AUDIT
} yvex_models_output_mode;

#ifndef YVEX_MODEL_DOWNLOAD_PATTERN_CAP
#define YVEX_MODEL_DOWNLOAD_PATTERN_CAP 32u
#endif

char *model_artifacts_cli_strdup(const char *s);
int path_exists(const char *path);
int is_path_like_reference(const char *input);
int set_path_ref(yvex_model_ref *out, const char *input, yvex_error *err);

void write_escaped(FILE *fp, const char *s);
void write_field(FILE *fp, const char *indent, const char *key, const char *value, int comma);
void write_u64_field(FILE *fp,
                     const char *indent,
                     const char *key,
                     unsigned long long value,
                     int comma);
void write_bool_field(FILE *fp, const char *indent, const char *key, int value, int comma);

int yvex_model_registry_mkdir_parent(const char *path, yvex_error *err);
int parse_models_output_mode(const char *value, yvex_models_output_mode *mode);
void print_model_registry_entry_cli(const yvex_model_registry_entry *entry, int selected);
void print_model_registry_entry_normal(const yvex_model_registry_entry *entry, int selected);
void print_model_registry_entry_audit(const yvex_model_registry_entry *entry,
                                      int selected);
void print_model_registry_scan_entry_cli(const yvex_model_registry_entry *entry);
void dims_to_text(const unsigned long long *dims,
                  unsigned int rank,
                  char *out,
                  size_t out_cap);
int populate_registry_identity(yvex_model_registry_entry *entry,
                               char *sha256,
                               char *format,
                               char *architecture,
                               char *primary_name,
                               char *primary_role,
                               char *primary_dtype,
                               char *primary_dims,
                               yvex_error *err);
void model_stage_print(const char *stage, const char *status);
void model_print_runtime_generation(const char *runtime_execution);
int cli_arg_value_valid(const char *value);
int parse_models_value_option(const char *command,
                              const char *flag,
                              int arg_count,
                              char **args,
                              int *index,
                              const char **value);
int model_backend_kind_from_name(const char *backend_name, yvex_backend_kind *kind);
int expand_operator_path(const char *input,
                         char *out,
                         size_t out_cap,
                         yvex_error *err,
                         const char *where);
int path_join2(char *out,
               size_t out_cap,
               const char *dir,
               const char *file,
               yvex_error *err,
               const char *where);
int path_parent_dir(const char *path, char *out, size_t out_cap);

#endif
