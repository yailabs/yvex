/* Retained pre-cutover CUDA vision values, not an independent model oracle.
 * Recorded from src/backend/cuda/vision.c at ac8a34a9c6ce163813f097e003945db73a414564.
 * Exact three-layer fixture includes full patch, QKV, two GELU policies and four mergers. */
#ifndef TESTS_SUPPORT_VISION_PROGRAM_H
#define TESTS_SUPPORT_VISION_PROGRAM_H
#include "tests/test.h"
#include <yvex/internal/vision_program.h>
#include <yvex/internal/multimodal.h>
#include <yvex/internal/program_stage.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/qtype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const float vision_retired[512] = {
    0x1.72p+0f, -0x1.c2p+0f, 0x1.6p+1f, -0x1.4cp+1f,
    0x1.b8p+0f, -0x1.bp+0f, 0x1.82p+1f, -0x1.2cp+1f,
    -0x1.02p-6f, -0x1.12p+0f, 0x1.1p+0f, 0x1.04p-3f,
    -0x1.08p+0f, 0x1.28p+0f, 0x1.0ep-4f, 0x1.06p+1f,
    -0x1.72p+1f, 0x1.72p+0f, -0x1.c2p+0f, 0x1.6p+1f,
    -0x1.4cp+1f, 0x1.b8p+0f, -0x1.bp+0f, 0x1.82p+1f,
    -0x1.2cp+1f, -0x1.02p-6f, -0x1.12p+0f, 0x1.1p+0f,
    0x1.04p-3f, -0x1.08p+0f, 0x1.28p+0f, 0x1.0ep-4f,
    0x1.6ap-1f, -0x1.8cp+0f, 0x1.fp+0f, -0x1.08p+1f,
    0x1.8cp-1f, -0x1.32p+0f, 0x1.e4p+0f, -0x1.5cp+0f,
    -0x1p+0f, -0x1.e6p-3f, 0x1.2ap-5f, 0x1.e6p-1f,
    -0x1.9cp-1f, 0x1.a4p+0f, 0x1.e2p-3f, 0x1.62p+1f,
    -0x1.5cp+1f, 0x1.6ap-1f, -0x1.8cp+0f, 0x1.fp+0f,
    -0x1.08p+1f, 0x1.8cp-1f, -0x1.32p+0f, 0x1.e4p+0f,
    -0x1.5cp+0f, -0x1p+0f, -0x1.e6p-3f, 0x1.2ap-5f,
    0x1.e6p-1f, -0x1.9cp-1f, 0x1.a4p+0f, 0x1.e2p-3f,
    0x1.92p+0f, -0x1.f2p+0f, 0x1.82p+1f, -0x1.2ap+1f,
    0x1.ap-2f, -0x1.42p+0f, 0x1.a8p+0f, -0x1.12p+0f,
    -0x1.72p+0f, 0x1.4p-2f, -0x1.a4p-3f, 0x1.b4p+0f,
    -0x1.08p+1f, 0x1.14p+1f, -0x1.a8p-1f, 0x1.d2p+1f,
    -0x1.aap+1f, 0x1.92p+0f, -0x1.f2p+0f, 0x1.82p+1f,
    -0x1.2ap+1f, 0x1.ap-2f, -0x1.42p+0f, 0x1.a8p+0f,
    -0x1.12p+0f, -0x1.72p+0f, 0x1.4p-2f, -0x1.a4p-3f,
    0x1.b4p+0f, -0x1.08p+1f, 0x1.14p+1f, -0x1.a8p-1f,
    0x1.3ep+1f, -0x1.36p+1f, 0x1.08p+2f, -0x1.9p+1f,
    0x1.dp+0f, -0x1.d8p+0f, 0x1.a6p+1f, -0x1.46p+1f,
    -0x1.86p-1f, -0x1.b4p-1f, 0x1.5cp-1f, 0x1.6ep-1f,
    -0x1.3ap+1f, 0x1.08p+1f, -0x1.02p+0f, 0x1.e2p+1f,
    -0x1.f6p+1f, 0x1.3ep+1f, -0x1.36p+1f, 0x1.08p+2f,
    -0x1.9p+1f, 0x1.dp+0f, -0x1.d8p+0f, 0x1.a6p+1f,
    -0x1.46p+1f, -0x1.86p-1f, -0x1.b4p-1f, 0x1.5cp-1f,
    0x1.6ep-1f, -0x1.3ap+1f, 0x1.08p+1f, -0x1.02p+0f,
    -0x1.d4p-1f, -0x1.9cp-3f, -0x1.92p-3f, 0x1.2ap-2f,
    -0x1.78p-1f, 0x1.ecp-2f, 0x1.3p-4f, 0x1.16p+0f,
    -0x1.dep+0f, 0x1.24p-1f, -0x1.14p+0f, 0x1.38p+0f,
    -0x1.9ap-2f, 0x1.74p-1f, -0x1.d4p-3f, 0x1.6ep+0f,
    -0x1.02p-2f, -0x1.d4p-1f, -0x1.9cp-3f, -0x1.92p-3f,
    0x1.2ap-2f, -0x1.78p-1f, 0x1.ecp-2f, 0x1.3p-4f,
    0x1.16p+0f, -0x1.dep+0f, 0x1.24p-1f, -0x1.14p+0f,
    0x1.38p+0f, -0x1.9ap-2f, 0x1.74p-1f, -0x1.d4p-3f,
    -0x1.52p-1f, 0x1.0ep-3f, -0x1.2ep-3f, -0x1.62p-1f,
    -0x1.9cp-3f, -0x1.e2p-4f, 0x1.aap-2f, -0x1.fap-1f,
    -0x1.9ap-5f, -0x1.3ap-1f, 0x1.24p-1f, -0x1.3ep-3f,
    0x1.aep-1f, 0x1.8p-2f, 0x1.6ap+0f, 0x1.5ap-4f,
    -0x1.aep-3f, -0x1.52p-1f, 0x1.0ep-3f, -0x1.2ep-3f,
    -0x1.62p-1f, -0x1.9cp-3f, -0x1.e2p-4f, 0x1.aap-2f,
    -0x1.fap-1f, -0x1.9ap-5f, -0x1.3ap-1f, 0x1.24p-1f,
    -0x1.3ep-3f, 0x1.aep-1f, 0x1.8p-2f, 0x1.6ap+0f,
    -0x1.9ep-1f, 0x1.aep+0f, -0x1.0ep+0f, 0x1.acp-2f,
    -0x1.3p-1f, 0x1.2ap+0f, -0x1.ccp+0f, -0x1.92p-5f,
    -0x1.02p+0f, 0x1.5ap-1f, -0x1.8p-2f, 0x1.f6p-3f,
    0x1.3ep-2f, 0x1.08p+0f, 0x1.aep-1f, -0x1.98p+0f,
    0x1.dep-1f, -0x1.9ep-1f, 0x1.aep+0f, -0x1.0ep+0f,
    0x1.acp-2f, -0x1.3p-1f, 0x1.2ap+0f, -0x1.ccp+0f,
    -0x1.92p-5f, -0x1.02p+0f, 0x1.5ap-1f, -0x1.8p-2f,
    0x1.f6p-3f, 0x1.3ep-2f, 0x1.08p+0f, 0x1.aep-1f,
    0x1.46p+0f, -0x1.4p+0f, 0x1.a8p+0f, -0x1.7ap-1f,
    0x1.1cp+1f, -0x1.12p+0f, -0x1.acp-3f, -0x1.1cp-1f,
    0x1.4ap-1f, -0x1.3ap-1f, -0x1.b8p-3f, 0x1.88p-3f,
    0x1.ecp-2f, 0x1.ecp-2f, -0x1.dcp+0f, 0x1.4p-1f,
    -0x1.1p+0f, 0x1.46p+0f, -0x1.4p+0f, 0x1.a8p+0f,
    -0x1.7ap-1f, 0x1.1cp+1f, -0x1.12p+0f, -0x1.acp-3f,
    -0x1.1cp-1f, 0x1.4ap-1f, -0x1.3ap-1f, -0x1.b8p-3f,
    0x1.88p-3f, 0x1.ecp-2f, 0x1.ecp-2f, -0x1.dcp+0f,
    0x1.96p-1f, 0x1.76p+0f, -0x1.e8p+0f, -0x1.56p-1f,
    -0x1.02p+0f, 0x1.2cp-3f, -0x1.0ep-2f, 0x1.98p-3f,
    0x1.2cp-1f, 0x1.0cp+0f, -0x1.8ep-5f, -0x1.38p+0f,
    -0x1.bap-4f, -0x1.94p-2f, 0x1.7p-1f, -0x1.ccp-4f,
    0x1.92p-1f, 0x1.96p-1f, 0x1.76p+0f, -0x1.e8p+0f,
    -0x1.56p-1f, -0x1.02p+0f, 0x1.2cp-3f, -0x1.0ep-2f,
    0x1.98p-3f, 0x1.2cp-1f, 0x1.0cp+0f, -0x1.8ep-5f,
    -0x1.38p+0f, -0x1.bap-4f, -0x1.94p-2f, 0x1.7p-1f,
    0x1.fp-2f, 0x1.26p+0f, -0x1.98p-1f, -0x1.9p-1f,
    -0x1.b2p-1f, -0x1.34p-2f, -0x1.48p-2f, -0x1.ccp-4f,
    -0x1.ep-7f, 0x1.8ap-2f, 0x1.16p-2f, -0x1.a4p-1f,
    0x1.bp-4f, -0x1.dp-3f, 0x1.4p-1f, 0x1.2p-2f,
    0x1.d8p-1f, 0x1.fp-2f, 0x1.26p+0f, -0x1.98p-1f,
    -0x1.9p-1f, -0x1.b2p-1f, -0x1.34p-2f, -0x1.48p-2f,
    -0x1.ccp-4f, -0x1.ep-7f, 0x1.8ap-2f, 0x1.16p-2f,
    -0x1.a4p-1f, 0x1.bp-4f, -0x1.dp-3f, 0x1.4p-1f,
    0x1.3ap-1f, -0x1.3p-1f, 0x1.36p+0f, -0x1.84p+0f,
    0x1.68p-1f, -0x1.42p-1f, 0x1.96p+0f, -0x1.58p-1f,
    -0x1.2cp+0f, 0x1.86p-3f, -0x1.4p-2f, 0x1.28p-2f,
    -0x1.0ep+0f, 0x1.34p+0f, -0x1.5cp-2f, 0x1.f8p+0f,
    -0x1.7cp+0f, 0x1.3ap-1f, -0x1.3p-1f, 0x1.36p+0f,
    -0x1.84p+0f, 0x1.68p-1f, -0x1.42p-1f, 0x1.96p+0f,
    -0x1.58p-1f, -0x1.2cp+0f, 0x1.86p-3f, -0x1.4p-2f,
    0x1.28p-2f, -0x1.0ep+0f, 0x1.34p+0f, -0x1.5cp-2f,
    0x1.88p-3f, 0x1.56p-2f, 0x1.82p-2f, -0x1.ccp+0f,
    0x1.3ap+0f, -0x1.7p-1f, 0x1.3ep+1f, -0x1.6cp+0f,
    0x1.28p+0f, -0x1.eep-3f, 0x1.38p+1f, -0x1.5ep+1f,
    0x1.1p-1f, -0x1.8ap+0f, 0x1.86p+0f, -0x1.eep-1f,
    -0x1.aep-1f, 0x1.88p-3f, 0x1.56p-2f, 0x1.82p-2f,
    -0x1.ccp+0f, 0x1.3ap+0f, -0x1.7p-1f, 0x1.3ep+1f,
    -0x1.6cp+0f, 0x1.28p+0f, -0x1.eep-3f, 0x1.38p+1f,
    -0x1.5ep+1f, 0x1.1p-1f, -0x1.8ap+0f, 0x1.86p+0f,
    -0x1.e6p-2f, -0x1.aap-3f, 0x1.acp-3f, 0x1.96p-1f,
    0x1.ecp-1f, -0x1.02p+1f, 0x1.2ep-1f, -0x1.02p+0f,
    0x1.bcp+0f, -0x1.fep+0f, 0x1.94p+0f, -0x1.f4p-1f,
    0x1.58p+1f, -0x1.3ep+1f, 0x1.08p-1f, -0x1.6p+0f,
    0x1.74p+0f, -0x1.e6p-2f, -0x1.aap-3f, 0x1.acp-3f,
    0x1.96p-1f, 0x1.ecp-1f, -0x1.02p+1f, 0x1.2ep-1f,
    -0x1.02p+0f, 0x1.bcp+0f, -0x1.fep+0f, 0x1.94p+0f,
    -0x1.f4p-1f, 0x1.58p+1f, -0x1.3ep+1f, 0x1.08p-1f,
    0x1.3cp-3f, 0x1.fap-7f, 0x1.28p-1f, 0x1.eap-1f,
    0x1.8ep+0f, -0x1.ep+0f, 0x1.2ep-4f, -0x1.bp-1f,
    0x1.28p+0f, -0x1.94p+0f, 0x1.9ep-1f, -0x1.24p-1f,
    0x1.c6p+0f, -0x1.bcp+0f, -0x1.fap-3f, -0x1.b6p-1f,
    0x1.46p-1f, 0x1.3cp-3f, 0x1.fap-7f, 0x1.28p-1f,
    0x1.eap-1f, 0x1.8ep+0f, -0x1.ep+0f, 0x1.2ep-4f,
    -0x1.bp-1f, 0x1.28p+0f, -0x1.94p+0f, 0x1.9ep-1f,
    -0x1.24p-1f, 0x1.c6p+0f, -0x1.bcp+0f, -0x1.fap-3f,
    0x1.f2p-1f, -0x1.22p+0f, 0x1.0ep+0f, -0x1.dp-6f,
    0x1.3p+1f, -0x1.2cp+1f, 0x1.6p-1f, -0x1.14p+0f,
    0x1.f2p+0f, -0x1.bep+0f, 0x1.84p-2f, -0x1.2p-1f,
    0x1.74p+0f, -0x1.48p+0f, -0x1.98p-1f, -0x1.88p-3f,
    0x1.2p-2f, 0x1.f2p-1f, -0x1.22p+0f, 0x1.0ep+0f,
    -0x1.dp-6f, 0x1.3p+1f, -0x1.2cp+1f, 0x1.6p-1f,
    -0x1.14p+0f, 0x1.f2p+0f, -0x1.bep+0f, 0x1.84p-2f,
    -0x1.2p-1f, 0x1.74p+0f, -0x1.48p+0f, -0x1.98p-1f,
    0x1.aap-3f, -0x1.8ep+0f, 0x1.1ep+0f, -0x1.08p-2f,
    0x1.54p+1f, -0x1.76p+1f, 0x1.72p+0f, -0x1.8ap+0f,
    0x1.6ep+1f, -0x1.3p+1f, 0x1.8ep+0f, -0x1.04p+0f,
    0x1.6cp+1f, -0x1.44p+1f, -0x1.5p-2f, -0x1.16p+0f,
    0x1.dep-1f, 0x1.aap-3f, -0x1.8ep+0f, 0x1.1ep+0f,
    -0x1.08p-2f, 0x1.54p+1f, -0x1.76p+1f, 0x1.72p+0f,
    -0x1.8ap+0f, 0x1.6ep+1f, -0x1.3p+1f, 0x1.8ep+0f,
    -0x1.04p+0f, 0x1.6cp+1f, -0x1.44p+1f, -0x1.5p-2f,
};

static int vision_program_observe(void *context, unsigned int stage, unsigned long long layer,
    const float *values, unsigned long long rows, unsigned long long width, yvex_error *err)
{
    (void)err;
    if (!stage || layer >= 3u || !values || rows != 16u || !width) return YVEX_ERR_FORMAT;
    (*(unsigned int *)context)++;
    return YVEX_OK;
}
static int vision_program_cancel(void *context) { (void)context; return 1; }

static int vision_program_name(void *context, int group, unsigned long long layer,
    unsigned int slot, char name[256], yvex_error *err)
{
    (void)context; (void)err;
    unsigned long long ordinal = group == YVEX_VISION_WEIGHT_EXTERNAL ? slot :
        group == YVEX_VISION_WEIGHT_BLOCK ? 3u + layer * 12u + slot :
        39u + (group == YVEX_VISION_WEIGHT_DEEPSTACK ? layer + 1u : 0u) * 6u + slot;
    if (ordinal >= 63u) return YVEX_ERR_BOUNDS;
    snprintf(name, 256u, "fixture_%llu", ordinal);
    return YVEX_OK;
}

static int vision_program_weight(void *context, const char *name, yvex_component_encoded_weight *out, yvex_error *err)
{
    unsigned long long ordinal;
    char tail;
    (void)err;
    if (sscanf(name, "fixture_%llu%c", &ordinal, &tail) != 1 || ordinal >= 63u) return YVEX_ERR_FORMAT;
    *out = ((yvex_component_encoded_weight *)context)[ordinal];
    return YVEX_OK;
}

static int vision_program_workspace(void *context, unsigned long long bytes, yvex_error *err)
{
    (void)context; (void)bytes; (void)err;
    return YVEX_OK;
}

typedef struct { uint64_t seen; int missing; } vision_binding_probe;
static int vision_compiled_name(void *context, unsigned long long ordinal, char name[256], yvex_error *err)
{
    vision_binding_probe *probe = context;
    (void)err;
    if (ordinal >= 63u || (probe->seen & (UINT64_C(1) << ordinal))) return YVEX_ERR_FORMAT;
    probe->seen |= UINT64_C(1) << ordinal;
    if (probe->missing && ordinal == 62u) return YVEX_ERR_STATE;
    snprintf(name, 256u, "fixture_%llu", ordinal);
    return YVEX_OK;
}

static int vision_compiled_binding(const yvex_component_execution *component,
    const yvex_vision_program *program, const yvex_vision_request *original)
{
    yvex_vision_request request = *original;
    vision_binding_probe probe = {0};
    yvex_vision_result result;
    yvex_error err;
    request.recipe = NULL; request.weight_name = NULL; request.weight_name_context = NULL;
    YVEX_TEST_ASSERT(yvex_component_vision_program_execute(component, program, &request,
        vision_compiled_name, &probe, &result, &err) == YVEX_OK && result.complete &&
        !*component->program_stage && probe.seen == (UINT64_C(1) << 63u) - 1u,
        "runtime binds every compiled parameter once without a source recipe or role interpreter");
    for (size_t i = 0u; i < 512u; ++i)
        YVEX_TEST_ASSERT((i < 128u ? request.merged[i] : request.deepstack[i - 128u]) == vision_retired[i],
            "recipe-free runtime preserves all compiled vision outputs exactly");
    float sentinel = request.merged[0];
    probe = (vision_binding_probe){.missing = 1};
    YVEX_TEST_ASSERT(yvex_component_vision_program_execute(component, program, &request,
        vision_compiled_name, &probe, &result, &err) == YVEX_ERR_STATE &&
        !result.complete && !*component->program_stage && request.merged[0] == sentinel,
        "missing compiled parameter refuses before execution and publication");
    YVEX_TEST_ASSERT(yvex_component_vision_program_execute(component, NULL, &request,
        vision_compiled_name, &probe, &result, &err) == YVEX_ERR_INVALID_ARG && !result.complete &&
        yvex_component_vision_program_execute(component, program, &request, NULL, NULL,
        &result, &err) == YVEX_ERR_INVALID_ARG && !result.complete && !*component->program_stage,
        "missing program or parameter resolver cannot trigger recipe reconstruction");
    printf("vision_compiled_binding parameters=63 recipe=absent role_resolver=absent "
        "values=512 max_abs=0 tolerance=0 negatives=3 unpublished=true\n");
    return 0;
}

static int test_vision_program(void)
{
    const char *identity = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_vision_recipe recipe = {.schema_version = YVEX_VISION_RECIPE_SCHEMA_V1,
        .semantic_identity = identity, .patch_channels = 3u, .temporal_patch = 2u,
        .patch_height = 4u, .patch_width = 4u, .position_grid_side = 3u,
        .hidden_width = 32u, .ffn_width = 64u, .heads = 4u, .head_dimension = 8u,
        .layer_count = 3u, .merge = 2u, .output_width = 32u,
        .deepstack_layer_count = 3u, .deepstack_layers = {0u, 1u, 2u},
        .rope_theta = 10000u, .normalization_epsilon = 1.0e-6f};
    yvex_component_encoded_weight weights[63] = {0};
    yvex_backend *backend = NULL;
    yvex_device_tensor *resident = NULL;
    unsigned char *arena = NULL;
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_backend_tensor_desc desc = {.name = "vision-before", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    float patches[1536], merged[128], deep[384];
    yvex_vision_request request = {.recipe = &recipe, .patches = patches,
        .patch_rows = 16u, .patch_capacity = 1536u, .image_count = 1u,
        .grid_height = 4u, .grid_width = 4u, .merged = merged, .deepstack = deep,
        .merged_capacity = 128u, .deepstack_capacity = 384u};
    yvex_backend_vision_request execution = {.request = &request, .weights = weights,
        .weight_count = 63u, .residency_identity = identity};
    yvex_vision_result result;
    yvex_program_stage *stage = NULL;
    yvex_vision_program *program = NULL;
    yvex_component_execution component = {.schema_version = YVEX_COMPONENT_EXECUTION_SCHEMA_V2,
        .program_stage = &stage};
    yvex_backend_memory_stats before, after;
    unsigned int observed = 0u;
    yvex_error err;
    for (size_t i = 0u; i < 63u; ++i) {
        unsigned long long rows = 1u, width = 32u;
        if (i == 0u) { rows = 32u; width = 96u; }
        else if (i == 2u) rows = 9u;
        else if (i >= 3u && i < 39u) {
            unsigned int slot = (i - 3u) % 12u;
            if (slot == 2u) rows = 96u;
            if (slot == 3u) width = 96u;
            if (slot == 4u) rows = 32u;
            if (slot == 8u) rows = 64u;
            if (slot == 9u) width = 64u;
            if (slot == 10u) { rows = 32u; width = 64u; }
        } else if (i >= 39u) {
            unsigned int slot = (i - 39u) % 6u;
            width = i < 45u && slot < 2u ? 32u : 128u;
            if (slot == 2u) rows = 128u;
            if (slot == 4u) rows = 32u;
            if (slot == 5u) width = 32u;
        }
        weights[i] = (yvex_component_encoded_weight){.qtype = YVEX_GGUF_QTYPE_BF16,
            .row_count = rows, .row_width = width, .row_bytes = width * 2u,
            .encoded_bytes = rows * width * 2u};
        desc.bytes += weights[i].encoded_bytes;
    }
    desc.dims[0] = desc.bytes;
    int rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_OK) rc = yvex_backend_resident_alloc(backend, &desc, &resident, &arena, &err);
    if (rc != YVEX_OK) { fprintf(stderr, "%s\n", yvex_error_message(&err)); return 1; }
    component.backend = backend;
    component.materialization = (yvex_materialization_session *)&component;
    component.owner_context = weights;
    component.weight_view = vision_program_weight;
    component.workspace_reserve = vision_program_workspace;
    request.weight_name = vision_program_name;
    snprintf(component.residency_identity, sizeof(component.residency_identity), "%s", identity);
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK, "vision resource baseline");
    unsigned long long offset = 0u;
    for (size_t i = 0u; i < 63u; ++i) {
        weights[i].encoded = arena + offset;
        for (unsigned long long j = 0u; j < weights[i].encoded_bytes / 2u; ++j) {
            float value = (float)((int)((j + i * 3u) % 17u) - 8) / 64.0f;
            if (weights[i].row_count == 1u && (i % 2u)) value += 1.0f;
            unsigned short bits = yvex_quant_bf16_encode(value);
            arena[offset + j * 2u] = (unsigned char)bits;
            arena[offset + j * 2u + 1u] = (unsigned char)(bits >> 8u);
        }
        offset += weights[i].encoded_bytes;
    }
    for (size_t i = 0u; i < 1536u; ++i) patches[i] = (float)((int)(i % 19u) - 9) / 32.0f;
    execution.resident_bytes = desc.bytes;
    rc = yvex_backend_resident_attach(backend, arena, desc.bytes, resident, 1u, &err);
    for (int inspect = 0; inspect < 2; ++inspect) {
        request.observe = inspect ? vision_program_observe : NULL;
        request.observer_context = &observed;
        if (rc == YVEX_OK) rc = yvex_vision_program_compile(&program, &recipe, 4u, 4u, inspect, &err);
        if (rc == YVEX_OK) rc = yvex_vision_program_execute(program, &component, &execution, &result, &err);
        if (rc != YVEX_OK) fprintf(stderr, "vision: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == YVEX_OK && result.complete && !stage, "vision compiled execution and checked cleanup");
        for (size_t i = 0u; i < 512u; ++i)
            YVEX_TEST_ASSERT((i < 128u ? merged[i] : deep[i - 128u]) == vision_retired[i],
                "vision patch/attention/merger/deepstack preserves all retired BF16 values exactly");
        printf("Vision SSA CUDA: inspect=%d layers=%llu patch_rows=%llu merged_rows=%llu outputs=512 "
            "first=%.9g last=%.9g max_abs=0 tolerance=0 launches=%llu; retired=146; preservation not upstream\n",
            inspect, result.layer_count, result.patch_rows, result.merged_rows,
            (double)merged[0], (double)deep[383], result.kernel_launches);
        yvex_vision_program_close(&program);
    }
    YVEX_TEST_ASSERT(observed == 15u && yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        before.allocated_bytes == after.allocated_bytes, "all typed observations and allocation baseline retained");
    request.observe = NULL;
    YVEX_TEST_ASSERT(yvex_component_vision_execute(&component, &request, &result, &err) == YVEX_OK &&
        result.complete && !stage, "real component ingress compiles, binds and executes the same program");
    for (size_t i = 0u; i < 512u; ++i)
        YVEX_TEST_ASSERT((i < 128u ? merged[i] : deep[i - 128u]) == vision_retired[i],
            "compiler/component consumer preserves exact vision outputs");
    YVEX_TEST_ASSERT(yvex_vision_program_compile(&program, &recipe, 4u, 4u, 0, &err) == YVEX_OK,
        "vision negative admission program");
    if (vision_compiled_binding(&component, program, &request)) return 1;
    request.grid_height = 2u; request.grid_width = 8u;
    YVEX_TEST_ASSERT(yvex_vision_program_execute(program, &component, &execution, &result, &err) == YVEX_ERR_FORMAT &&
        !stage && !result.complete, "equal area cannot replace exact compiled spatial geometry");
    request.grid_height = request.grid_width = 4u;
    request.deepstack = merged;
    YVEX_TEST_ASSERT(yvex_vision_program_execute(program, &component, &execution, &result, &err) == YVEX_ERR_FORMAT &&
        !stage && !result.complete, "aliased vision publication refuses before execution");
    request.deepstack = deep;
    yvex_vision_program_close(&program);
    request.cancel_requested = vision_program_cancel;
    request.observe = NULL;
    for (size_t i = 0u; i < 128u; ++i) merged[i] = -123.0f;
    for (size_t i = 0u; i < 384u; ++i) deep[i] = -123.0f;
    YVEX_TEST_ASSERT(yvex_vision_program_compile(&program, &recipe, 4u, 4u, 0, &err) == YVEX_OK &&
        yvex_vision_program_execute(program, &component, &execution, &result, &err) == YVEX_ERR_CANCELLED &&
        !result.complete && !stage, "cancelled vision has no result and releases its stage");
    for (size_t i = 0u; i < 512u; ++i)
        YVEX_TEST_ASSERT((i < 128u ? merged[i] : deep[i - 128u]) == -123.0f, "vision cancellation preserves all output sentinels");
    yvex_vision_program_close(&program);
    for (unsigned int negative = 0u; negative < 5u; ++negative) {
        yvex_vision_recipe invalid = recipe;
        if (negative == 0u) invalid.merge = 0u;
        if (negative == 1u) invalid.head_dimension = 6u;
        if (negative == 2u) invalid.deepstack_layers[1] = invalid.deepstack_layers[0];
        if (negative == 3u) invalid.patch_channels = ULLONG_MAX;
        if (negative == 4u) invalid.normalization_epsilon = NAN;
        YVEX_TEST_ASSERT(yvex_vision_program_compile(&program, &invalid, 4u, 4u, 0, &err) == YVEX_ERR_FORMAT &&
            !program, "invalid merge/head/trajectory/overflow/epsilon refused at compilation");
    }
    printf("Vision SSA lifecycle: observations=15 allocation_delta=0; cancellation=unpublished; compiler_negatives=5\n");
    if (yvex_backend_resident_detach(backend, &err) != YVEX_OK) rc = 1;
    if (yvex_backend_tensor_release(backend, &resident, &err) != YVEX_OK) rc = 1;
    if (yvex_backend_close_checked(&backend, &err) != YVEX_OK) rc = 1;
    return rc == YVEX_OK ? 0 : 1;
}

#endif
