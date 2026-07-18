/*
 * Owner: abi.conversion (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Open-weight conversion bridge API
 */
#ifndef YVEX_CONVERSION_H
#define YVEX_CONVERSION_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_CONVERSION_STATUS_UNKNOWN = 0,
    YVEX_CONVERSION_STATUS_PLANNED,
    YVEX_CONVERSION_STATUS_EMITTED,
    YVEX_CONVERSION_STATUS_PARTIAL,
    YVEX_CONVERSION_STATUS_FAILED
} yvex_conversion_status;

typedef enum {
    YVEX_CONVERT_TENSOR_STATUS_UNKNOWN = 0,
    YVEX_CONVERT_TENSOR_STATUS_READY,
    YVEX_CONVERT_TENSOR_STATUS_EMITTED,
    YVEX_CONVERT_TENSOR_STATUS_SKIPPED,
    YVEX_CONVERT_TENSOR_STATUS_UNSUPPORTED_QTYPE,
    YVEX_CONVERT_TENSOR_STATUS_UNMAPPED,
    YVEX_CONVERT_TENSOR_STATUS_FAILED
} yvex_convert_tensor_status;

typedef enum {
    YVEX_CONVERT_TRANSFORM_NONE = 0,
    YVEX_CONVERT_TRANSFORM_TRANSPOSE_2D,
    YVEX_CONVERT_TRANSFORM_DTYPE_CAST,
    YVEX_CONVERT_TRANSFORM_QUANTIZE,
    YVEX_CONVERT_TRANSFORM_UNSUPPORTED
} yvex_convert_transform_kind;

typedef struct {
    const char *architecture;
    const char *source_manifest_path;
    const char *native_source_dir;
    const char *template_path;
    const char *quant_policy_path;
    const char *imatrix_manifest_path;
    const char *out_path;
    const char *tensor_name;
    const char *target_qtype;
    unsigned long long limit_tensors;
    int plan_only;
    int overwrite;
    int allow_unsupported_qtype;
    int require_all;
} yvex_conversion_options;

typedef struct {
    yvex_conversion_status status;
    const char *architecture;
    const char *out_path;
    unsigned long long native_tensor_count;
    unsigned long long planned_tensor_count;
    unsigned long long emitted_tensor_count;
    unsigned long long skipped_tensor_count;
    unsigned long long unsupported_qtype_count;
    unsigned long long unmapped_tensor_count;
    unsigned long long bytes_read;
    unsigned long long bytes_written;
    int roundtrip_validated;
    int execution_ready;
} yvex_conversion_summary;

int yvex_conversion_plan_write_json(const yvex_conversion_options *options,
                                    const char *plan_out_path,
                                    yvex_conversion_summary *summary_out,
                                    yvex_error *err);

int yvex_conversion_emit_gguf(const yvex_conversion_options *options,
                              yvex_conversion_summary *summary_out,
                              yvex_error *err);

int yvex_conversion_suggest_artifact_name(char *out,
                                          unsigned long long out_size,
                                          const char *family,
                                          const char *model,
                                          const char *scope,
                                          const char *artifact_class,
                                          const char *qprofile,
                                          const char *calibration,
                                          const char *producer,
                                          const char *schema,
                                          yvex_error *err);

const char *yvex_conversion_status_name(yvex_conversion_status status);
const char *yvex_convert_tensor_status_name(yvex_convert_tensor_status status);
const char *yvex_convert_transform_kind_name(yvex_convert_transform_kind transform);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_CONVERSION_H */
