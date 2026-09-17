/* Source-declared text component topology lowers once into executable SSA. */
#ifndef INCLUDE_YVEX_INTERNAL_TEXT_PROGRAM_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_TEXT_PROGRAM_H_INCLUDED
#include <yvex/internal/program_physical.h>
#include <yvex/internal/media_target.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Architecture import facts, consumed before executable binding. They are
 * not a runtime layer plan or backend implementation contract. */
#define YVEX_COMPONENT_TEXT_RECIPE_SCHEMA_V1 1u
#define YVEX_COMPONENT_TEXT_LAYER_WEIGHT_COUNT 11u
typedef struct yvex_component_text_recipe {
    unsigned int schema_version;
    const char *semantic_identity;
    unsigned long long layer_capacity, hidden_width, ffn_width;
    unsigned long long query_heads, kv_heads, head_dimension;
    unsigned long long vocabulary_size, rope_theta;
    float normalization_epsilon;
} yvex_component_text_recipe;
/* Parameter handles follow the admitted import order: embedding, then the
 * eleven source-declared parameters for each block. Runtime binds these
 * handles once; it does not interpret block composition. Position inputs are
 * explicit independent index streams (equal streams for ordinary text). */
int yvex_text_program_compile(yvex_program_physical **, const yvex_component_text_recipe *,
    unsigned long long layers, unsigned long long maximum_rows, const unsigned long long sections[3],
    unsigned int injected_blocks, yvex_error *);
/* Cold component entry: compile before calling the bounded product adapter.
 * The adapter borrows program truth; no family topology reaches a backend. */
int yvex_text_program_condition(const yvex_component_text_recipe *, const yvex_media_conditioning_request *,
    yvex_runtime_av_conditioning_result *, const unsigned long long sections[3], unsigned int injected_blocks,
    int (*)(const yvex_media_conditioning_request *, yvex_runtime_av_conditioning_result *, yvex_error *),
    yvex_error *);
#ifdef __cplusplus
}
#endif
#endif
