/*
 * yvex_deepseek_transform_ir.h - DeepSeek Transformation IR construction.
 *
 * Owner:
 *   src/model/compilation
 *
 * Owns:
 *   artifact-neutral construction of the complete DeepSeek-V4-Flash tensor
 *   transformation graph from admitted architecture and coverage facts.
 *
 * Does not own:
 *   source parsing or IO, GGUF names/qtypes/layout, numeric conversion,
 *   quantization, artifact emission, runtime binding, rendering, or generation.
 *
 * Invariants:
 *   one successful build represents all 69,187 source contributions exactly
 *   once in 1,360 terminal logical outputs and reads zero payload bytes.
 *
 * Boundary:
 *   family construction supplies logical transformation truth to generic IR;
 *   physical lowering is a separate consumer.
 */
#ifndef YVEX_DEEPSEEK_TRANSFORM_IR_H
#define YVEX_DEEPSEEK_TRANSFORM_IR_H

#include "yvex_transform_ir.h"

#include "../architecture/yvex_deepseek_v4_ir.h"
#include "../target/yvex_deepseek_tensor_coverage.h"
#include "../../source/yvex_source_verify.h"

#define YVEX_DEEPSEEK_TRANSFORM_SOURCE_COUNT 69187ull
#define YVEX_DEEPSEEK_TRANSFORM_TERMINAL_COUNT 1360ull
#define YVEX_DEEPSEEK_TRANSFORM_MAIN_TERMINAL_COUNT 1328ull
#define YVEX_DEEPSEEK_TRANSFORM_AUX_TERMINAL_COUNT 32ull

/* Encodes the complete admitted logical architecture using the canonical
 * compilation-domain identity. It performs no IO or allocation and publishes
 * no partial output on failure. */
int yvex_deepseek_transform_architecture_identity(
    const yvex_deepseek_v4_ir *architecture,
    char output[YVEX_TRANSFORM_IR_IDENTITY_CAP]);

int yvex_deepseek_transform_ir_build(
    yvex_transform_ir **out,
    const yvex_source_verification *verification,
    const yvex_deepseek_v4_ir *architecture,
    const yvex_deepseek_tensor_coverage *coverage,
    const yvex_transform_builder_options *options,
    yvex_transform_failure *failure,
    yvex_error *err);

#endif
