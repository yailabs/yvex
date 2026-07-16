/*
 * yvex_gguf_writer.h - immutable GGUF v3 writer plan ABI.
 *
 * Owner: TRACK.ARTIFACT.
 * Owns: deterministic metadata/directory serialization, tensor byte geometry,
 *   alignment, predicted file size, writer-plan identity, and plan lifecycle.
 * Does not own: quantization, source payload IO, output files, publication,
 *   reader validation, artifact admission, materialization, or runtime.
 * Invariants: a sealed plan bijects one quant decision to one lowering tensor;
 *   all offsets use checked 64-bit arithmetic and planning reads zero payload.
 * Boundary: a writer plan is not an emitted or admitted artifact.
 */
#ifndef YVEX_GGUF_WRITER_H
#define YVEX_GGUF_WRITER_H

#include "yvex_gguf_tokenizer_metadata.h"
#include "yvex_quant_plan.h"

#define YVEX_GGUF_WRITER_SCHEMA_VERSION 1u
#define YVEX_GGUF_WRITER_IDENTITY_CAP 65u
#define YVEX_GGUF_WRITER_NAME_CAP 192u

typedef enum {
    YVEX_GGUF_WRITER_OK = 0,
    YVEX_GGUF_WRITER_INVALID_ARGUMENT,
    YVEX_GGUF_WRITER_UNSEALED_PLAN,
    YVEX_GGUF_WRITER_IDENTITY_MISMATCH,
    YVEX_GGUF_WRITER_METADATA_INCOMPLETE,
    YVEX_GGUF_WRITER_DUPLICATE_METADATA,
    YVEX_GGUF_WRITER_UNSUPPORTED_METADATA,
    YVEX_GGUF_WRITER_DUPLICATE_TENSOR,
    YVEX_GGUF_WRITER_TENSOR_DIVERGENCE,
    YVEX_GGUF_WRITER_QTYPE_GEOMETRY,
    YVEX_GGUF_WRITER_ARITHMETIC_OVERFLOW,
    YVEX_GGUF_WRITER_RESOURCE_LIMIT,
    YVEX_GGUF_WRITER_ALLOCATION,
    YVEX_GGUF_WRITER_SERIALIZATION,
    YVEX_GGUF_WRITER_LIFECYCLE
} yvex_gguf_writer_code;

typedef struct {
    yvex_gguf_writer_code code;
    unsigned long long metadata_index;
    unsigned long long tensor_index;
    unsigned long long expected;
    unsigned long long actual;
    char name[YVEX_GGUF_WRITER_NAME_CAP];
} yvex_gguf_writer_failure;

typedef struct {
    char name[YVEX_GGUF_WRITER_NAME_CAP];
    unsigned int rank;
    unsigned long long dims[YVEX_GGUF_QTYPE_MAX_DIMS];
    unsigned int qtype;
    unsigned long long relative_offset;
    unsigned long long absolute_offset;
    unsigned long long raw_bytes;
    unsigned long long padded_bytes;
    unsigned long long absolute_end;
    unsigned long long padded_end;
} yvex_gguf_writer_tensor;

typedef struct {
    unsigned int schema_version;
    unsigned int gguf_version;
    unsigned int alignment;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
    unsigned long long structural_bytes;
    unsigned long long pre_data_padding_bytes;
    unsigned long long tensor_payload_bytes;
    unsigned long long tensor_padding_bytes;
    unsigned long long data_section_bytes;
    unsigned long long final_file_bytes;
    unsigned long long source_snapshot_identity;
    char payload_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char transform_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    unsigned long long mapping_identity;
    char profile_name[64];
    char profile_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char required_execution_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char writer_plan_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    unsigned long long qtype_tensor_counts[
        YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID + 1u];
    unsigned long long qtype_payload_bytes[
        YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID + 1u];
    unsigned long long tokenizer_token_count;
    unsigned long long tokenizer_merge_count;
    unsigned long long tokenizer_embedded_bytes;
    unsigned long long owned_bytes;
    unsigned long long payload_bytes_read;
    int complete;
} yvex_gguf_writer_plan_summary;

typedef struct {
    unsigned int alignment;
    size_t maximum_owned_bytes;
    const char *required_execution_identity;
} yvex_gguf_writer_plan_options;

typedef struct yvex_gguf_writer_plan yvex_gguf_writer_plan;

typedef struct {
    const char *name;
} yvex_gguf_writer_fixture_tensor;

void yvex_gguf_writer_plan_options_default(
    yvex_gguf_writer_plan_options *options);
int yvex_deepseek_gguf_writer_plan_build(
    yvex_gguf_writer_plan **out,
    const yvex_quant_plan *quant_plan,
    const yvex_deepseek_gguf_map *map,
    const yvex_source_verification *verification,
    const yvex_gguf_writer_plan_options *options,
    yvex_gguf_writer_failure *failure,
    yvex_error *err);
int yvex_gguf_writer_plan_build_fixture(
    yvex_gguf_writer_plan **out,
    const yvex_quant_plan *quant_plan,
    const yvex_gguf_writer_fixture_tensor *tensors,
    unsigned long long tensor_count,
    const yvex_gguf_writer_plan_options *options,
    yvex_gguf_writer_failure *failure,
    yvex_error *err);
void yvex_gguf_writer_plan_release(yvex_gguf_writer_plan **plan);

const yvex_gguf_writer_plan_summary *yvex_gguf_writer_plan_summary_get(
    const yvex_gguf_writer_plan *plan);
const yvex_gguf_writer_tensor *yvex_gguf_writer_plan_tensor_at(
    const yvex_gguf_writer_plan *plan, unsigned long long ordinal);
const unsigned char *yvex_gguf_writer_plan_prefix(
    const yvex_gguf_writer_plan *plan, size_t *byte_count);
const yvex_gguf_tokenizer_metadata *yvex_gguf_writer_plan_tokenizer(
    const yvex_gguf_writer_plan *plan);
const char *yvex_gguf_writer_code_name(yvex_gguf_writer_code code);

#endif
