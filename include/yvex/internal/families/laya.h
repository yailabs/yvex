/* Laya source semantics lowered to the common computational program. */
#ifndef INCLUDE_YVEX_INTERNAL_FAMILIES_LAYA_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_FAMILIES_LAYA_H_INCLUDED

#include <yvex/internal/program_physical.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_laya_program yvex_laya_program;

typedef struct {
    const char *source_identity;
    unsigned long long maximum_tokens;
    unsigned long long layer_count;
    unsigned long long vocabulary_size;
    unsigned long long hidden_width;
    unsigned long long attention_heads;
    unsigned long long intermediate_width;
    unsigned long long head_layer_count;
    double epsilon;
} yvex_laya_program_recipe;

/* Emits per-token raw logits. The consumer selects explicitly supplied marker
 * positions only after this entire bidirectional program has executed.
 * First scope is unpadded sequences shorter than the sliding window. */
int yvex_laya_program_compile(yvex_laya_program **out,
    const yvex_laya_program_recipe *recipe, yvex_error *err);
const yvex_program_physical *yvex_laya_program_physical(const yvex_laya_program *program);
size_t yvex_laya_program_parameter_count(const yvex_laya_program *program);
int yvex_laya_program_parameter_name(void *program, unsigned long long tensor_id,
    char name[256], yvex_error *err);
void yvex_laya_program_close(yvex_laya_program **program);

#ifdef __cplusplus
}
#endif
#endif
