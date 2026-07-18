/*
 * quant_numeric.h - private canonical quantization numeric ABI.
 *
 * Owner: TRACK.QUANT (src/gguf numeric codec owners).
 * Owns: qtype arithmetic capability, scalar/block codecs, metrics, and CPU dot.
 * Does not own: GGUF storage geometry, transformation planning, payload IO,
 *   physical-profile selection, artifact writing, runtime, or rendering.
 * Invariants: qtype IDs come only from gguf_qtype.h; codecs are deterministic
 *   little-endian implementations of the pinned numeric contracts.
 * Boundary: encoded buffers and bounded dot products are not artifact or
 *   transformer-execution evidence.
 */
#ifndef YVEX_QUANT_NUMERIC_H
#define YVEX_QUANT_NUMERIC_H

#include <stddef.h>
#include <stdint.h>

#include <yvex/error.h>
#include <yvex/gguf_qtype.h>
#include <yvex/native_weights.h>

#include "src/model/compilation/ir.h"

#define YVEX_QUANT_NUMERIC_CONTRACT_VERSION 1u
#define YVEX_QUANT_Q8_0_ELEMENTS 32u
#define YVEX_QUANT_Q8_0_BYTES 34u
#define YVEX_QUANT_MXFP4_ELEMENTS 32u
#define YVEX_QUANT_MXFP4_BYTES 17u
#define YVEX_QUANT_Q2_K_ELEMENTS 256u
#define YVEX_QUANT_Q2_K_BYTES 84u

typedef enum {
    YVEX_QUANT_FAILURE_NONE = 0,
    YVEX_QUANT_FAILURE_INVALID_ARGUMENT,
    YVEX_QUANT_FAILURE_INVALID_STATE,
    YVEX_QUANT_FAILURE_PROFILE_SCHEMA,
    YVEX_QUANT_FAILURE_TRANSFORM_IDENTITY,
    YVEX_QUANT_FAILURE_MAPPING_IDENTITY,
    YVEX_QUANT_FAILURE_SOURCE_IDENTITY,
    YVEX_QUANT_FAILURE_PAYLOAD_IDENTITY,
    YVEX_QUANT_FAILURE_PAYLOAD_NOT_READABLE,
    YVEX_QUANT_FAILURE_MISSING_DECISION,
    YVEX_QUANT_FAILURE_DUPLICATE_DECISION,
    YVEX_QUANT_FAILURE_UNMATCHED_LOWERING,
    YVEX_QUANT_FAILURE_DUPLICATE_LOWERING,
    YVEX_QUANT_FAILURE_PRECISION_CONSTRAINT,
    YVEX_QUANT_FAILURE_APPROXIMATION_FORBIDDEN,
    YVEX_QUANT_FAILURE_UNKNOWN_QTYPE,
    YVEX_QUANT_FAILURE_REMOVED_QTYPE,
    YVEX_QUANT_FAILURE_QTYPE_OUTSIDE_BASELINE,
    YVEX_QUANT_FAILURE_ENCODER_UNAVAILABLE,
    YVEX_QUANT_FAILURE_DECODER_UNAVAILABLE,
    YVEX_QUANT_FAILURE_CPU_COMPUTE_UNAVAILABLE,
    YVEX_QUANT_FAILURE_CUDA_COMPUTE_UNAVAILABLE,
    YVEX_QUANT_FAILURE_CALIBRATION_REQUIRED,
    YVEX_QUANT_FAILURE_CALIBRATION_IDENTITY,
    YVEX_QUANT_FAILURE_UNSUPPORTED_OPERATION,
    YVEX_QUANT_FAILURE_INVALID_RANK,
    YVEX_QUANT_FAILURE_INVALID_DIMENSION,
    YVEX_QUANT_FAILURE_INVALID_ROW_AXIS,
    YVEX_QUANT_FAILURE_ROW_DIVISIBILITY,
    YVEX_QUANT_FAILURE_ELEMENT_OVERFLOW,
    YVEX_QUANT_FAILURE_BYTE_OVERFLOW,
    YVEX_QUANT_FAILURE_FP8_SCALE_PAIR,
    YVEX_QUANT_FAILURE_MXFP4_BLOCK,
    YVEX_QUANT_FAILURE_Q8_0_BLOCK,
    YVEX_QUANT_FAILURE_Q2_K_BLOCK,
    YVEX_QUANT_FAILURE_CAST_RANGE,
    YVEX_QUANT_FAILURE_NONFINITE,
    YVEX_QUANT_FAILURE_SOURCE_SHORT_READ,
    YVEX_QUANT_FAILURE_SOURCE_DRIFT,
    YVEX_QUANT_FAILURE_SINK_SHORT_WRITE,
    YVEX_QUANT_FAILURE_SINK_PROTOCOL,
    YVEX_QUANT_FAILURE_CANCELLED,
    YVEX_QUANT_FAILURE_RESOURCE_BUDGET,
    YVEX_QUANT_FAILURE_ALLOCATION,
    YVEX_QUANT_FAILURE_WORKER,
    YVEX_QUANT_FAILURE_CLEANUP,
    YVEX_QUANT_FAILURE_NUMERIC_BOUND,
    YVEX_QUANT_FAILURE_DIGEST_MISMATCH,
    YVEX_QUANT_FAILURE_INCOMPLETE
} yvex_quant_failure_code;

typedef struct {
    yvex_quant_failure_code code;
    unsigned long long terminal_ordinal;
    unsigned long long source_index;
    unsigned long long row_index;
    unsigned long long block_index;
    unsigned long long expected;
    unsigned long long actual;
    unsigned int qtype;
    yvex_transform_operation_kind operation;
} yvex_quant_failure;

typedef enum {
    YVEX_QUANT_CALIBRATION_NONE = 0,
    YVEX_QUANT_CALIBRATION_OPTIONAL,
    YVEX_QUANT_CALIBRATION_REQUIRED,
    YVEX_QUANT_CALIBRATION_UNSUPPORTED
} yvex_quant_calibration_requirement;

typedef enum {
    YVEX_QUANT_REFUSAL_NONE = 0,
    YVEX_QUANT_REFUSAL_UNKNOWN_IDENTITY,
    YVEX_QUANT_REFUSAL_REMOVED_IDENTITY,
    YVEX_QUANT_REFUSAL_OUTSIDE_PINNED_BASELINE,
    YVEX_QUANT_REFUSAL_STORAGE_UNAVAILABLE,
    YVEX_QUANT_REFUSAL_ENCODER_UNAVAILABLE,
    YVEX_QUANT_REFUSAL_DECODER_UNAVAILABLE,
    YVEX_QUANT_REFUSAL_CPU_COMPUTE_UNAVAILABLE,
    YVEX_QUANT_REFUSAL_CUDA_COMPUTE_UNAVAILABLE,
    YVEX_QUANT_REFUSAL_CALIBRATION_REQUIRED
} yvex_quant_refusal_code;

enum {
    YVEX_QUANT_TRANSFORM_IDENTITY = 1u << YVEX_TRANSFORM_OP_IDENTITY,
    YVEX_QUANT_TRANSFORM_SCALE_PAIR =
        1u << YVEX_TRANSFORM_OP_DECODE_SCALE_PAIR,
    YVEX_QUANT_TRANSFORM_CHECKED_CAST =
        1u << YVEX_TRANSFORM_OP_CHECKED_CAST,
    YVEX_QUANT_TRANSFORM_EXPERT =
        1u << YVEX_TRANSFORM_OP_EXPERT_AGGREGATE
};

typedef struct {
    unsigned int qtype;
    int identity_known;
    int storage_admitted;
    int encoder_available;
    int reference_decoder_available;
    int dedicated_cpu_compute_available;
    int dedicated_cuda_compute_available;
    yvex_quant_calibration_requirement calibration;
    unsigned int physical_class_mask;
    unsigned int transform_kind_mask;
    unsigned int numeric_contract_version;
    int deterministic_encoding;
    yvex_quant_refusal_code refusal;
} yvex_quant_numeric_capability;

typedef struct {
    unsigned long long element_count;
    unsigned long long finite_count;
    unsigned long long nonfinite_count;
    double maximum_absolute_error;
    double absolute_error_sum;
    double squared_error_sum;
    double relative_error_sum;
    double reference_squared_sum;
    double dot_reference;
    double dot_reconstructed;
} yvex_quant_metrics;

const yvex_quant_numeric_capability *yvex_quant_numeric_capability_at(
    unsigned int qtype);
const yvex_quant_numeric_capability *yvex_quant_numeric_capability_by_name(
    const char *name);
unsigned int yvex_quant_numeric_capability_count(void);
const char *yvex_quant_calibration_name(
    yvex_quant_calibration_requirement requirement);
const char *yvex_quant_refusal_name(yvex_quant_refusal_code refusal);
const char *yvex_quant_failure_name(yvex_quant_failure_code code);

float yvex_quant_f16_decode(unsigned short bits);
unsigned short yvex_quant_f16_encode(float value);
float yvex_quant_bf16_decode(unsigned short bits);
unsigned short yvex_quant_bf16_encode(float value);
float yvex_quant_fp8_e4m3fn_decode(unsigned char bits);
float yvex_quant_e8m0_decode(unsigned char bits);

int yvex_quant_source_scalar_decode(yvex_native_dtype dtype,
                                    const unsigned char *source,
                                    float *out,
                                    yvex_quant_failure *failure,
                                    yvex_error *err);
int yvex_quant_source_i64_decode(const unsigned char source[8],
                                 int64_t *out,
                                 yvex_quant_failure *failure,
                                 yvex_error *err);
int yvex_quant_source_mxfp4_decode(const unsigned char packed[16],
                                  unsigned char scale,
                                  float out[32],
                                  yvex_quant_failure *failure,
                                  yvex_error *err);
int yvex_quant_encode_block(unsigned int qtype,
                            const float *source,
                            unsigned long long elements,
                            unsigned char *encoded,
                            size_t encoded_capacity,
                            size_t *encoded_bytes,
                            yvex_quant_failure *failure,
                            yvex_error *err);
int yvex_quant_decode_block(unsigned int qtype,
                            const unsigned char *encoded,
                            size_t encoded_bytes,
                            float *out,
                            unsigned long long out_elements,
                            yvex_quant_failure *failure,
                            yvex_error *err);

void yvex_quant_metrics_init(yvex_quant_metrics *metrics);
int yvex_quant_metrics_update(yvex_quant_metrics *metrics,
                              const float *reference,
                              const float *reconstructed,
                              const float *dot_vector,
                              unsigned long long count);
double yvex_quant_metrics_mean_absolute_error(
    const yvex_quant_metrics *metrics);
double yvex_quant_metrics_rmse(const yvex_quant_metrics *metrics);
double yvex_quant_metrics_mean_relative_error(
    const yvex_quant_metrics *metrics);
double yvex_quant_metrics_dot_absolute_error(
    const yvex_quant_metrics *metrics);

int yvex_quant_cpu_dot(unsigned int qtype,
                       const unsigned char *encoded,
                       size_t encoded_bytes,
                       const float *vector,
                       unsigned long long elements,
                       float *out,
                       yvex_quant_failure *failure,
                       yvex_error *err);

#endif
