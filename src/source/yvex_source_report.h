/*
 * yvex_source_report.h - source pressure report domain API.
 *
 * Owner: src/source.
 * Owns: typed source report request and report facts.
 * Does not own: CLI parsing, operator rendering, runtime, generation, eval, or benchmark.
 * Invariants: report facts are built from local metadata and safetensors headers only.
 * Boundary: source reports are not artifact emission, model support, or release readiness.
 */
#ifndef YVEX_SOURCE_REPORT_H
#define YVEX_SOURCE_REPORT_H

#include <stddef.h>
#include <yvex/error.h>
#include <yvex/fs.h>
#include <yvex/native_weights.h>

#include "yvex_source_verify.h"

typedef struct {
    const char *family_key;
    const char *display_family;
    const char *report_name;
    const char *target_id;
    const char *target_class;
    const char *model;
    const char *source_target_status;
    const char *source_family_profile_status;
    const char *source_artifact_class;
    const char *source_artifact_format;
    const char *source_artifact_origin;
    const char *source_artifact_authority;
    const char *source_tensor_container;
    const char *target_artifact_class;
    const char *target_artifact_origin;
    const char *target_artifact_required;
    const char *external_reference_status;
    const char *yvex_produced_artifact_status;
    const char *pressure_purpose;
    const char *runtime_shape;
    const char *hardware_lane;
    const char *backend_lane;
    const char *source_class;
    const char *source_path_blocker;
    const char *source_manifest_blocker;
    const char *native_inventory_blocker;
    const char *source_config_blocker;
    const char *tokenizer_blocker;
    const char *model_class_blocker;
    const char *model_class_next;
    const char *const *tail_blockers;
    unsigned long tail_blocker_count;
} yvex_source_family_profile;

typedef struct {
    const char *family;
    const char *release;
    const char *models_root;
    const char *source;
    const char *target;
    const yvex_source_family_profile *profile;
    char resolved_target[128];
    char resolved_model[128];
    int include_files;
    int include_config;
    int include_blockers;
    int include_next;
    int include_tensors;
    int strict;
    unsigned long long tensor_limit;
} yvex_source_report_request;

#define YVEX_SOURCE_TENSOR_SAMPLE_CAP 20u
#define YVEX_SOURCE_TENSOR_SHAPE_CAP 128u

typedef struct {
    char name[192];
    char file[192];
    char dtype[24];
    char shape[YVEX_SOURCE_TENSOR_SHAPE_CAP];
    unsigned long long rank;
    unsigned long long elements;
    unsigned long long declared_bytes;
} yvex_source_tensor_sample;

typedef struct {
    const char *verification_status;
    const char *repository_status;
    const char *revision_status;
    const char *config_identity_status;
    const char *tokenizer_verification_status;
    const char *generation_config_status;
    const char *shard_index_status;
    const char *inventory_authority;
    const char *upstream_index_identity_status;
    int tokenizer_verified;
    const char *model_name;
    const char *config_presence;
    const char *generation_config_presence;
    const char *tokenizer_json_presence;
    const char *tokenizer_config_presence;
    const char *tokenizer_status;
    const char *safetensors_status;
    const char *manifest_status;
    const char *native_inventory_report_status;
    const char *tensor_map_report_status;
    const char *tensor_role_map_report_status;
    const char *output_head_map_report_status;
    const char *tokenizer_map_report_status;
    const char *native_inventory_status;
    const char *native_inventory_source;
    const char *tensor_metadata_status;
    const char *tensor_metadata_source;
    const char *native_tensor_metadata_status;
    const char *native_tensor_payload_status;
    const char *sidecar_status;
    const char *tensor_payload_status;
    const char *target_artifact_status;
    const char *footprint_class;
    const char *footprint_status;
    const char *provenance_origin_normal;
    const char *provenance_origin_audit;
    const char *provenance_status;
    const char *identity_status;
    const char *authority;
    const char *authority_status;
    const char *manifest_provenance_status;
    const char *manifest_authority;
    const char *manifest_schema_status;
    const char *manifest_family_status;
    const char *manifest_target_status;
    const char *manifest_artifact_class_status;
    const char *manifest_footprint_status;
    const char *manifest_native_inventory_status;
    const char *manifest_tensor_metadata_status;
    const char *manifest_consistency_status;
    const char *manifest_hardening_status;
    int manifest_creation_performed;
} yvex_source_report_semantics;

typedef struct {
    const yvex_source_family_profile *profile;
    yvex_source_report_request request;
    const char *status;
    const char *source_state;
    const char *top_blocker;
    const char *next_row;
    char identity_target_id[128];
    char identity_model[128];
    char identity_family[32];
    char identity_repo_id[256];
    char identity_revision[128];
    char identity_local_source_dir[YVEX_PATH_CAP];
    char download_registry_path[YVEX_PATH_CAP];
    char download_report_path[YVEX_PATH_CAP];
    char tensor_map_path[YVEX_PATH_CAP];
    char output_head_map_path[YVEX_PATH_CAP];
    char tokenizer_map_path[YVEX_PATH_CAP];
    int download_registry_exists;
    int download_report_exists;
    int tensor_map_exists;
    int output_head_map_exists;
    int tokenizer_map_exists;
    int tensor_map_incomplete;
    int output_head_map_missing;
    int source_identity_from_path;
    int source_identity_from_download_sidecar;
    char source_path[YVEX_PATH_CAP];
    char source_path_source[64];
    char manifest_path[YVEX_PATH_CAP];
    char native_inventory_path[YVEX_PATH_CAP];
    int source_exists;
    int config_exists;
    int generation_config_exists;
    int tokenizer_json_exists;
    int tokenizer_config_exists;
    int readme_exists;
    int license_exists;
    int manifest_exists;
    int manifest_probe_checked;
    int manifest_probe_error;
    int manifest_has_schema;
    int manifest_schema_matches;
    int manifest_has_family;
    int manifest_family_matches;
    int manifest_has_target;
    int manifest_target_matches;
    int manifest_has_artifact_class;
    int manifest_has_footprint;
    int manifest_has_provenance;
    int manifest_has_native_inventory;
    int manifest_has_tensor_metadata;
    char manifest_schema_version[64];
    int native_inventory_exists;
    unsigned long long source_file_count;
    unsigned long long source_regular_file_count;
    unsigned long long safetensors_count;
    unsigned long long bin_count;
    unsigned long long dat_count;
    unsigned long long json_count;
    unsigned long long tokenizer_file_count;
    unsigned long long config_file_count;
    unsigned long long total_size_bytes;
    unsigned long long safetensors_size_bytes;
    unsigned long long sidecar_size_bytes;
    unsigned long long other_size_bytes;
    unsigned long long largest_source_file_bytes;
    char largest_source_file_name[YVEX_PATH_CAP];
    unsigned long long native_safetensors_count;
    unsigned long long native_safetensors_opened;
    unsigned long long native_safetensors_header_read_count;
    unsigned long long native_safetensors_header_error_count;
    unsigned long long native_safetensors_header_bytes;
    unsigned long long native_tensor_count;
    unsigned long long native_declared_data_bytes;
    unsigned long long native_declared_tensor_bytes;
    unsigned long long native_max_rank;
    unsigned long long native_max_tensor_elements;
    char native_largest_tensor_name[YVEX_PATH_CAP];
    unsigned long long native_largest_tensor_bytes;
    unsigned long long native_dtype_f16_count;
    unsigned long long native_dtype_bf16_count;
    unsigned long long native_dtype_f32_count;
    unsigned long long native_dtype_i8_count;
    unsigned long long native_dtype_i16_count;
    unsigned long long native_dtype_i32_count;
    unsigned long long native_dtype_i64_count;
    unsigned long long native_dtype_u8_count;
    unsigned long long native_dtype_other_count;
    unsigned long long native_invalid_file_count;
    unsigned long long native_inventory_error_count;
    unsigned long long source_tensor_count;
    unsigned long long source_tensor_name_count;
    unsigned long long source_tensor_file_count;
    unsigned long long source_tensor_dtype_count;
    unsigned long long source_tensor_rank_count;
    unsigned long long source_tensor_shape_count;
    unsigned long long source_tensor_declared_data_bytes;
    unsigned long long source_tensor_declared_tensor_bytes;
    unsigned long long source_tensor_total_elements;
    unsigned long long source_tensor_max_rank;
    unsigned long long source_tensor_max_elements;
    char source_tensor_largest_name[YVEX_PATH_CAP];
    char source_tensor_largest_file[YVEX_PATH_CAP];
    char source_tensor_largest_dtype[24];
    unsigned long long source_tensor_largest_rank;
    char source_tensor_largest_shape[YVEX_SOURCE_TENSOR_SHAPE_CAP];
    unsigned long long source_tensor_largest_elements;
    unsigned long long source_tensor_largest_declared_bytes;
    unsigned long long source_tensor_dtype_f16_count;
    unsigned long long source_tensor_dtype_bf16_count;
    unsigned long long source_tensor_dtype_f32_count;
    unsigned long long source_tensor_dtype_i8_count;
    unsigned long long source_tensor_dtype_i16_count;
    unsigned long long source_tensor_dtype_i32_count;
    unsigned long long source_tensor_dtype_i64_count;
    unsigned long long source_tensor_dtype_u8_count;
    unsigned long long source_tensor_dtype_other_count;
    unsigned long long source_tensor_rank_0_count;
    unsigned long long source_tensor_rank_1_count;
    unsigned long long source_tensor_rank_2_count;
    unsigned long long source_tensor_rank_3_count;
    unsigned long long source_tensor_rank_4_count;
    unsigned long long source_tensor_rank_other_count;
    unsigned long long source_tensor_metadata_error_count;
    unsigned long long source_tensor_name_embed_count;
    unsigned long long source_tensor_name_attn_count;
    unsigned long long source_tensor_name_mlp_count;
    unsigned long long source_tensor_name_norm_count;
    unsigned long long source_tensor_name_lm_head_count;
    unsigned long long source_tensor_name_other_count;
    yvex_source_tensor_sample source_tensor_samples[YVEX_SOURCE_TENSOR_SAMPLE_CAP];
    unsigned long long source_tensor_sample_count;
    const char *blockers[32];
    unsigned long blocker_count;
    yvex_source_verification verification;
    yvex_source_report_semantics semantics;
    int exit_code;
} yvex_source_report;

const yvex_source_family_profile *yvex_source_report_find_profile(const char *family);
int yvex_source_report_target_is_supported(const yvex_source_family_profile *profile,
                                           const char *target);
int yvex_source_report_build(const yvex_source_report_request *request,
                             yvex_source_report *report,
                             yvex_error *err);

#endif
