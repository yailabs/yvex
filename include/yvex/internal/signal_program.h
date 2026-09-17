/* Source projection for multistage signal programs. Names are cold parameter
 * linkage only; the returned physical SSA program owns all computation. */
#ifndef INCLUDE_YVEX_INTERNAL_SIGNAL_PROGRAM_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_SIGNAL_PROGRAM_H_INCLUDED
#include <yvex/internal/program_physical.h>
#ifdef __cplusplus
extern "C" {
#endif
#define YVEX_ALIAS_DECODER_MAX_STAGES 8u
#define YVEX_ALIAS_DECODER_MAX_RESBLOCKS 4u
#define YVEX_ALIAS_DECODER_MAX_LAYERS 4u
typedef enum {
    YVEX_ALIAS_DECODER_INPUT_WEIGHT = 0,
    YVEX_ALIAS_DECODER_INPUT_BIAS,
    YVEX_ALIAS_DECODER_PRE_WEIGHT,
    YVEX_ALIAS_DECODER_PRE_GAIN,
    YVEX_ALIAS_DECODER_PRE_BIAS,
    YVEX_ALIAS_DECODER_UP_WEIGHT,
    YVEX_ALIAS_DECODER_UP_GAIN,
    YVEX_ALIAS_DECODER_UP_BIAS,
    YVEX_ALIAS_DECODER_ACT_ALPHA,
    YVEX_ALIAS_DECODER_ACT_BETA,
    YVEX_ALIAS_DECODER_ACT_UP_FILTER,
    YVEX_ALIAS_DECODER_ACT_DOWN_FILTER,
    YVEX_ALIAS_DECODER_RES1_WEIGHT,
    YVEX_ALIAS_DECODER_RES1_GAIN,
    YVEX_ALIAS_DECODER_RES1_BIAS,
    YVEX_ALIAS_DECODER_RES2_WEIGHT,
    YVEX_ALIAS_DECODER_RES2_GAIN,
    YVEX_ALIAS_DECODER_RES2_BIAS,
    YVEX_ALIAS_DECODER_POST_ACT_ALPHA,
    YVEX_ALIAS_DECODER_POST_ACT_BETA,
    YVEX_ALIAS_DECODER_POST_ACT_UP_FILTER,
    YVEX_ALIAS_DECODER_POST_ACT_DOWN_FILTER,
    YVEX_ALIAS_DECODER_POST_WEIGHT,
    YVEX_ALIAS_DECODER_POST_GAIN,
    YVEX_ALIAS_DECODER_WEIGHT_ROLE_COUNT
} yvex_alias_decoder_weight_role;

typedef struct {
    const char *input_projection, *pre_convolution, *upsamples;
    const char *residual_blocks, *post_activation, *post_convolution;
    unsigned long long blocks_per_stage;
} yvex_alias_decoder_name_templates;

typedef int (*yvex_alias_decoder_weight_name_fn)(
    void *context, yvex_alias_decoder_weight_role role,
    unsigned long long stage, unsigned long long block,
    unsigned long long layer, char output[256], yvex_error *err);
typedef struct {
    unsigned long long input_channels, projection_channels, input_kernel;
    unsigned long long pre_channels, pre_kernel;
    unsigned long long stage_count, residual_blocks, residual_layers;
    unsigned long long rates[YVEX_ALIAS_DECODER_MAX_STAGES];
    unsigned long long upsample_kernels[YVEX_ALIAS_DECODER_MAX_STAGES];
    unsigned long long residual_kernels[YVEX_ALIAS_DECODER_MAX_RESBLOCKS];
    unsigned long long residual_dilations[YVEX_ALIAS_DECODER_MAX_LAYERS];
    unsigned long long final_channels, final_kernel;
} yvex_alias_decoder_recipe;
int yvex_alias_decoder_template_name(
    void *context, yvex_alias_decoder_weight_role role,
    unsigned long long stage, unsigned long long block,
    unsigned long long layer, char output[256], yvex_error *err);
typedef struct {
    yvex_program_physical *physical;
    char (*parameter_names)[256];
    size_t parameter_count;
    unsigned long long output_length, output_values;
} yvex_signal_program;
int yvex_signal_program_compile(yvex_signal_program *, const yvex_alias_decoder_recipe *,
    const char *, unsigned long long, unsigned long long,
    yvex_alias_decoder_weight_name_fn, void *, yvex_error *);
int yvex_signal_program_parameter_name(void *, unsigned long long, char[256], yvex_error *);
void yvex_signal_program_close(yvex_signal_program *);

typedef enum {
    YVEX_SPATIAL_INPUT, YVEX_SPATIAL_NORM1, YVEX_SPATIAL_CONV1,
    YVEX_SPATIAL_NORM2, YVEX_SPATIAL_CONV2, YVEX_SPATIAL_SHORTCUT,
    YVEX_SPATIAL_DOWNSAMPLE, YVEX_SPATIAL_FINAL_NORM,
    YVEX_SPATIAL_FINAL_CONV, YVEX_SPATIAL_OUTPUT
} yvex_spatial_parameter_role;
typedef struct yvex_spatial_encoder_recipe {
    const char *semantic_identity;
    unsigned long long input_channels, entry_channels, output_channels, projected_channels;
    unsigned long long kernel_size, groups, stage_count;
    struct { unsigned long long channels, blocks; int downsample; } stages[8];
    double epsilon;
    /* Source names are resolved once during import, never by the executor. */
    int (*parameter_name)(void *, yvex_spatial_parameter_role, int,
        unsigned long long, unsigned long long, char[256], yvex_error *);
    void *parameter_context;
} yvex_spatial_encoder_recipe;
/* output_length is the flattened output spatial extent; physical result types
 * retain the independent height and width identities. */
int yvex_spatial_encoder_compile(yvex_signal_program *, const yvex_spatial_encoder_recipe *,
    const char *, unsigned long long, unsigned long long, unsigned long long, yvex_error *);
#ifdef __cplusplus
}
#endif
#endif
