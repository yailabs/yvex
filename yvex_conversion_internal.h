/*
 * YVEX - Conversion bridge internals
 */
#ifndef YVEX_CONVERSION_INTERNAL_H
#define YVEX_CONVERSION_INTERNAL_H

#include <stdio.h>

#include <yvex/conversion.h>
#include <yvex/native_weights.h>
#include <yvex/tensor.h>

typedef struct {
    char native_name[256];
    char target_name[256];
    yvex_tensor_role role;
    yvex_convert_tensor_status status;
    yvex_convert_transform_kind transform;
    const yvex_native_weight_info *native;
    const char *target_qtype;
    unsigned int ggml_type;
    unsigned int target_scalar_bytes;
} yvex_conversion_tensor_plan;

const char *yvex_conversion_default_qtype_for_role(yvex_tensor_role role);
int yvex_conversion_map_tensor(const char *arch,
                               const yvex_native_weight_info *native,
                               const char *target_qtype,
                               yvex_conversion_tensor_plan *out,
                               yvex_error *err);
int yvex_conversion_read_payload(const char *native_source_dir,
                                 const yvex_native_weight_info *native,
                                 unsigned char **out,
                                 unsigned long long *out_len,
                                 yvex_error *err);
int yvex_conversion_convert_payload(const unsigned char *src,
                                    unsigned long long src_len,
                                    yvex_native_dtype src_dtype,
                                    const yvex_conversion_tensor_plan *plan,
                                    unsigned char **out,
                                    unsigned long long *out_len,
                                    yvex_error *err);
int yvex_conversion_write_single_gguf(const yvex_conversion_options *options,
                                      const yvex_conversion_tensor_plan *plan,
                                      const unsigned char *payload,
                                      unsigned long long payload_len,
                                      yvex_conversion_summary *summary,
                                      yvex_error *err);
int yvex_conversion_validate_roundtrip(const char *path, yvex_error *err);
int yvex_conversion_report_plan_json(FILE *fp,
                                     const yvex_conversion_options *options,
                                     const yvex_conversion_summary *summary,
                                     const yvex_conversion_tensor_plan *plans,
                                     unsigned long long plan_count,
                                     yvex_error *err);

#endif /* YVEX_CONVERSION_INTERNAL_H */
