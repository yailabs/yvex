/* Exact first Laya checkpoint authority; family facts, not runtime execution. */
#ifndef INCLUDE_YVEX_INTERNAL_FAMILIES_LAYA_SOURCE_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_FAMILIES_LAYA_SOURCE_H_INCLUDED

#include <yvex/internal/core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_LAYA_TYPED_SOURCE_SCHEMA_V1 1u
typedef struct {
    unsigned int schema_version;
    char repository[96], revision[65];
    char weight_path[1024];
    char weight_identity[YVEX_SHA256_HEX_BYTES];
    char tokenizer_identity[YVEX_SHA256_HEX_BYTES];
    char logical_model_identity[YVEX_SHA256_HEX_BYTES];
    unsigned long long weight_bytes, weight_tensor_count;
    unsigned long long maximum_tokens, layer_count, vocabulary_size;
    unsigned long long hidden_width, attention_heads, intermediate_width;
    unsigned long long head_layer_count;
    double epsilon;
} yvex_laya_typed_source_summary;

int yvex_laya_typed_source_admit(const char *checkpoint_directory,
    yvex_laya_typed_source_summary *summary, yvex_error *err);

#ifdef __cplusplus
}
#endif
#endif
