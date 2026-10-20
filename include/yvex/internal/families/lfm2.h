/* Liquid LFM2 source geometry and complete tensor-role accounting.
 * These facts do not admit an artifact, backend, conversation or Engine. */
#ifndef INCLUDE_YVEX_INTERNAL_FAMILIES_LFM2_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_FAMILIES_LFM2_H_INCLUDED
#include <yvex/internal/source.h>
#ifdef __cplusplus
extern "C" {
#endif
#define YVEX_LFM2_TARGET "lfm2.5-24b-a2b"
#define YVEX_LFM2_REPOSITORY "LiquidAI/LFM2.5-24B-A2B"
#define YVEX_LFM2_REVISION "1769516ac588c4c558089c94f27d96b0dc67b183"
#define YVEX_LFM2_LAYER_CAP 128u
#define YVEX_LFM2_EXPERT_CAP 256u
typedef enum {
    YVEX_LFM2_MIXER_CONV = 1,
    YVEX_LFM2_MIXER_ATTENTION
} yvex_lfm2_mixer;
typedef enum {
    YVEX_LFM2_ROLE_UNKNOWN = 0,
    YVEX_LFM2_ROLE_EMBEDDING,
    YVEX_LFM2_ROLE_FINAL_NORM,
    YVEX_LFM2_ROLE_OPERATOR_NORM,
    YVEX_LFM2_ROLE_FFN_NORM,
    YVEX_LFM2_ROLE_CONV_WEIGHT,
    YVEX_LFM2_ROLE_CONV_IN,
    YVEX_LFM2_ROLE_CONV_OUT,
    YVEX_LFM2_ROLE_QUERY,
    YVEX_LFM2_ROLE_KEY,
    YVEX_LFM2_ROLE_VALUE,
    YVEX_LFM2_ROLE_ATTENTION_OUT,
    YVEX_LFM2_ROLE_QUERY_NORM,
    YVEX_LFM2_ROLE_KEY_NORM,
    YVEX_LFM2_ROLE_DENSE_GATE,
    YVEX_LFM2_ROLE_DENSE_UP,
    YVEX_LFM2_ROLE_DENSE_DOWN,
    YVEX_LFM2_ROLE_ROUTER,
    YVEX_LFM2_ROLE_ROUTER_BIAS,
    YVEX_LFM2_ROLE_EXPERT_GATE,
    YVEX_LFM2_ROLE_EXPERT_UP,
    YVEX_LFM2_ROLE_EXPERT_DOWN,
    YVEX_LFM2_ROLE_COUNT
} yvex_lfm2_role;
typedef struct {
    unsigned long long hidden, layers, vocabulary, intermediate, expert_intermediate;
    unsigned long long heads, kv_heads, head_width, experts, selected_experts, dense_layers;
    unsigned long long context, convolution_kernel, bos, eos, pad;
    unsigned long long convolution_layers, attention_layers, required_tensors;
    double norm_epsilon, rope_theta, routed_scale;
    yvex_lfm2_mixer mixer[YVEX_LFM2_LAYER_CAP];
    char identity[65];
    int complete;
} yvex_lfm2_architecture;
typedef struct {
    yvex_lfm2_role role;
    unsigned long long layer, expert;
} yvex_lfm2_tensor_binding;
typedef struct {
    unsigned long long tensors, bytes, bf16_tensors, f32_tensors;
    char identity[65];
    int complete;
} yvex_lfm2_inventory;
typedef struct {
    unsigned int schema_version;
    int (*config_parse)(const char *, size_t, yvex_lfm2_architecture *, yvex_error *);
    int (*open)(const yvex_source_verification *, yvex_lfm2_architecture *, yvex_error *);
    int (*tensor_classify)(const yvex_lfm2_architecture *, const yvex_native_weight_info *,
                          yvex_lfm2_tensor_binding *, yvex_error *);
    int (*tensor_audit)(const yvex_lfm2_architecture *, const yvex_native_weight_table *,
                       yvex_lfm2_inventory *, yvex_error *);
    int (*snapshot_audit)(const yvex_lfm2_architecture *, const yvex_source_tensor_snapshot *,
                         yvex_lfm2_inventory *, yvex_error *);
} yvex_lfm2_api;
const yvex_lfm2_api *yvex_model_register_lfm2(void);
#ifdef __cplusplus
}
#endif
#endif
