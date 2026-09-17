/* Source-declared F32 component projection; executable computation is SSA. */
#ifndef INCLUDE_YVEX_INTERNAL_DENSE_PROGRAM_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_DENSE_PROGRAM_H_INCLUDED
#include <yvex/internal/program_physical.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const char *semantic_identity;
    unsigned long long rows, output_rows, width, heads, head_dimension, rotary_dimension;
    unsigned long long ffn_width, block_count, output_width;
    double epsilon;
} yvex_dense_program_recipe;
/* Parameter ordinals: twelve source roles per block, then final norm/bias,
 * output matrix/bias. No source payload or runtime resource is inspected. */
int yvex_dense_program_compile(yvex_program_physical **,
    const yvex_dense_program_recipe *, yvex_error *);
/* Two affine projections, learned rows and explicit zero padding. The source
 * importer owns these dimensions and exact five parameter bindings. No media
 * packing, file decoding or runtime placement is part of this program. */
typedef struct {
    const char *semantic_identity;
    unsigned long long rows, input_width, intermediate_width, output_width;
    unsigned long long learned_rows, padding_rows;
} yvex_dense_prefix_recipe;
int yvex_dense_prefix_compile(yvex_program_physical **,
    const yvex_dense_prefix_recipe *, yvex_error *);
#ifdef __cplusplus
}
#endif
#endif
