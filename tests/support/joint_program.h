/* Conditioned component compiler pressure; zero gates give an independent
 * identity oracle while all attention/projection/FFN work still executes. */
#ifndef TESTS_SUPPORT_JOINT_PROGRAM_H
#define TESTS_SUPPORT_JOINT_PROGRAM_H
#include "tests/test.h"
#include <yvex/internal/joint_program.h>
#include <yvex/internal/program_stage.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/qtype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static yvex_transformer_joint_recipe joint_fixture_architecture(void)
{
    yvex_transformer_joint_recipe r = {.schema_version = YVEX_TRANSFORMER_JOINT_SCHEMA_V5,
        .qkv_layout = YVEX_TRANSFORMER_QKV_LAYOUT_PER_HEAD_THREE,
        .swiglu_layout = YVEX_TRANSFORMER_SWIGLU_LAYOUT_GATE_THEN_UP,
        .hidden_width = 8u, .attention_heads = 2u, .head_dimension = 4u, .attention_width = 8u,
        .ffn_width = 8u, .timestep_width = 4u, .rotary_width = 4u,
        .modality_count = 3u, .modulation_parameters = 6u, .block_count = 50u,
        .maximum_timesteps = 4u, .maximum_packed_rows = 16u};
    return r;
}

static int test_joint_target(void)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_transformer_joint_recipe a = joint_fixture_architecture();
    a.hidden_width = 5376u; a.video_input_width = 96u; a.audio_input_width = 32u;
    a.condition_input_width = 4u; a.refiner_block_count = 2u;
    yvex_joint_program_recipe r = {.architecture = &a, .source_identity = source, .rows = 3u,
        .timesteps = 1u, .blocks = 1u, .normalization_epsilon = 1e-5,
        .video_rows = 1u, .audio_rows = 1u, .text_rows = 1u,
        .position_axes = 2u, .time_embedding_width = 8u, .maximum_period = 10000.0};
    yvex_joint_program *p = NULL;
    yvex_program_physical *target = NULL, *repeat = NULL, *decoded = NULL, *refused = NULL;
    yvex_program_target_choice choices[2] = {
        {SIZE_MAX, "linear_bias.cuda.sm121.5376x96.f32.v1"},
        {SIZE_MAX, "linear_bias.cuda.sm121.5376x32.f32.v1"}};
    yvex_error err = {0};
    YVEX_TEST_ASSERT(yvex_joint_program_compile(&p, &r, &err) == YVEX_OK, "target source program compiles");
    const yvex_program_physical_summary before = *yvex_program_physical_summary_get(p->physical);
    for (size_t i = 0u; i < before.step_count; ++i) {
        const yvex_program_physical_step *s = yvex_program_physical_step_at(p->physical, i);
        if (strcmp(s->implementation, "linear_bias.f32.v1")) continue;
        unsigned long long id = yvex_program_physical_value_at(p->physical, s->operands[1])->tensor_id;
        if (id == 31u || id == 33u) choices[(id - 31u) / 2u].step = i;
    }
    YVEX_TEST_ASSERT(yvex_program_physical_target_compile(&target, p->physical, choices, 2u, &err) == YVEX_OK &&
        yvex_program_physical_target_compile(&repeat, p->physical, choices, 2u, &err) == YVEX_OK,
        "explicit target lowering admits two exact affine implementations");
    const yvex_program_physical_summary *after = yvex_program_physical_summary_get(target);
    YVEX_TEST_ASSERT(!strcmp(before.semantic_identity, after->semantic_identity) &&
        !strcmp(before.execution_identity, after->execution_identity) &&
        !strcmp(before.parameter_identity, after->parameter_identity) && strcmp(before.identity, after->identity) &&
        !strcmp(after->identity, yvex_program_physical_summary_get(repeat)->identity) &&
        !memcmp(&before, yvex_program_physical_summary_get(p->physical), sizeof(before)),
        "target changes only physical identity, repeats exactly, and never mutates source");
    yvex_core_bytes wire = {.maximum = 16777216u};
    YVEX_TEST_ASSERT(yvex_program_physical_encode(target, &wire, &err) == YVEX_OK &&
        yvex_program_physical_decode(&decoded, wire.data, wire.count, &err) == YVEX_OK &&
        !strcmp(after->identity, yvex_program_physical_summary_get(decoded)->identity),
        "target implementation survives authenticated binary import");
    free(wire.data);
    for (unsigned int negative = 0u; negative < 8u; ++negative) {
        yvex_program_target_choice bad[2] = {choices[0], choices[1]};
        size_t count = 1u;
        if (negative == 0u) bad[0].implementation = NULL;
        if (negative == 1u) bad[0].implementation = "not.admitted.v1";
        if (negative == 2u) bad[0].step = before.step_count;
        if (negative == 3u) bad[0].implementation = "linear_bias.cuda.sm121.5376x32.f32.v1";
        if (negative == 4u) bad[0].implementation = "linear_bias.bf16.v1";
        if (negative == 5u) bad[0].implementation = "linear.encoded.f32.v1";
        if (negative == 6u) { bad[1] = bad[0]; count = 2u; }
        if (negative == 7u) count = 0u;
        YVEX_TEST_ASSERT(yvex_program_physical_target_compile(&refused, p->physical, bad, count, &err) != YVEX_OK &&
            !refused && !strcmp(before.identity, yvex_program_physical_summary_get(p->physical)->identity),
            "invalid target identity, geometry, precision, semantics or population fails closed");
    }
    printf("Target lowering: 2 exact choices; semantic/execution/parameter identity unchanged; "
        "physical identity distinct and deterministic; binary roundtrip exact; 8 negatives refused\n");
    yvex_program_physical_close(&decoded); yvex_program_physical_close(&repeat);
    yvex_program_physical_close(&target); yvex_joint_program_close(&p);
    return 0;
}

static int test_joint_observations(void)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_transformer_joint_recipe a = joint_fixture_architecture();
    a.video_input_width = 2u; a.audio_input_width = 2u; a.condition_input_width = 4u; a.refiner_block_count = 2u;
    yvex_joint_program_recipe r = {.architecture = &a, .source_identity = source, .rows = 6u,
        .timesteps = 2u, .blocks = 2u, .normalization_epsilon = 1e-5,
        .video_rows = 2u, .audio_rows = 1u, .text_rows = 3u, .position_axes = 2u,
        .time_embedding_width = 8u, .maximum_period = 10000.0, .observe_blocks = 1, .observe_stages = 1,
        .observed_stage = YVEX_TRANSFORMER_JOINT_STAGE_COUNT};
    const unsigned int blocks[] = {1u, 3u, 0u, 1u, 3u}, counts[] = {35u, 6u, 3u, 22u, 3u};
    yvex_error err = {0};
    for (size_t scenario = 0u; scenario < 5u; ++scenario) {
        r.observed_scope = scenario < 2u ? YVEX_TRANSFORMER_JOINT_SCOPE_OMNI : YVEX_TRANSFORMER_JOINT_SCOPE_REFINER;
        r.observed_block = blocks[scenario];
        yvex_joint_program *p = NULL;
        int rc = yvex_joint_program_compile(&p, &r, &err);
        if (rc != YVEX_OK) fprintf(stderr, "observed joint: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == YVEX_OK, "complete component inspection lowers through ordered generic publications");
        const yvex_program_physical_summary *s = yvex_program_physical_summary_get(p->physical);
        size_t observations = 0u, block_events = 0u;
        for (size_t i = 0u; i < s->step_count; ++i) {
            const yvex_program_physical_step *op = yvex_program_physical_step_at(p->physical, i);
            if (strcmp(op->implementation, "observe.f32-storage.v1")) continue;
            unsigned long long tag = yvex_program_physical_attribute(op, "tag")->value.integer;
            observations++;
            if ((tag & 255u) == YVEX_TRANSFORMER_JOINT_STAGE_COUNT) block_events++;
            else YVEX_TEST_ASSERT((tag >> 56u) == r.observed_scope &&
                ((tag >> 8u) & 0xffffffffffu) == r.observed_block, "compiler selects exact source stage scope");
            YVEX_TEST_ASSERT(op->effects == (YVEX_IR_PUBLISH | YVEX_IR_ORDERED), "inspection cannot reorder or disappear");
        }
        if (observations != counts[scenario]) fprintf(stderr, "joint observation scenario=%zu expected=%u observed=%zu\n",
            scenario, counts[scenario], observations);
        size_t index_slots = 0u;
        for (size_t i = 0u; i < s->value_count; ++i) {
            const yvex_program_physical_value *v = yvex_program_physical_value_at(p->physical, i);
            index_slots += v->type.scalar == YVEX_IR_INDEX && v->storage != YVEX_IR_NONE;
        }
        YVEX_TEST_ASSERT(observations == counts[scenario] && block_events == 2u && s->result_count == 2u &&
            index_slots == 1u && s->storage_count - index_slots <= 40u,
            "observations retain bounded activation slots plus one explicit computed index slot");
        yvex_joint_program_close(&p);
    }
    printf("Joint inspection: five source scopes, 35/6/3/22/3 ordered publications; two final outputs; "
        "no retained per-block output array\n");
    return 0;
}

static int test_joint_time_profiles(void)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_transformer_joint_recipe a = joint_fixture_architecture();
    a.video_input_width = 2u; a.audio_input_width = 2u; a.condition_input_width = 4u; a.refiner_block_count = 2u;
    yvex_joint_program_recipe r = {.architecture = &a, .source_identity = source, .rows = 6u,
        .blocks = 2u, .normalization_epsilon = 1e-5, .video_rows = 2u, .audio_rows = 1u, .text_rows = 3u,
        .position_axes = 2u, .time_embedding_width = 8u, .maximum_period = 10000.0};
    yvex_joint_program *programs[3] = {0};
    yvex_error err;
    for (size_t i = 0u; i < 3u; ++i) {
        r.timesteps = i + 1u;
        YVEX_TEST_ASSERT(yvex_joint_program_compile(programs + i, &r, &err) == YVEX_OK,
            "all exact one/two/three-time signatures compile before iterative execution");
        const yvex_program_physical_value *full = yvex_program_physical_value_at(programs[i]->physical, 3u);
        const yvex_program_physical_value *step = yvex_program_physical_value_at(programs[i]->step, 3u);
        YVEX_TEST_ASSERT(full->type.shape[0].extent == i + 1u && step->type.shape[0].extent == i + 1u,
            "composed and step entrypoints preserve the exact time population without padding");
        yvex_joint_program *repeated = NULL;
        YVEX_TEST_ASSERT(yvex_joint_program_compile(&repeated, &r, &err) == YVEX_OK &&
            !strcmp(yvex_program_physical_summary_get(repeated->physical)->identity,
                yvex_program_physical_summary_get(programs[i]->physical)->identity),
            "cold-retained and freshly projected profile have identical computation");
        if (i) YVEX_TEST_ASSERT(strcmp(yvex_program_physical_summary_get(programs[i]->physical)->identity,
            yvex_program_physical_summary_get(programs[i - 1u]->physical)->identity),
            "distinct time geometry cannot alias one executable identity");
        yvex_joint_program_close(&repeated);
    }
    for (size_t i = 0u; i < 3u; ++i) yvex_joint_program_close(programs + i);
    printf("Joint time profiles: exact populations 1/2/3; composed/step signatures agree; "
        "cold versus fresh identity exact; distinct geometry does not alias\n");
    return 0;
}

static int test_joint_compiler(void)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_transformer_joint_recipe a = joint_fixture_architecture();
    yvex_joint_program_recipe r = {.architecture = &a, .source_identity = source, .rows = 3u,
        .timesteps = 2u, .blocks = 2u, .normalization_epsilon = 1e-5};
    yvex_error err = {0};
    for (unsigned int schema = 0u; schema < YVEX_TRANSFORMER_JOINT_SCHEMA_V5; ++schema) {
        yvex_joint_program *refused = NULL;
        _Alignas(yvex_transformer_joint_recipe) unsigned int header = schema;
        /* Only the schema header is readable: sanitizers detect stale-layout
         * field access if admission tries to interpret an older recipe. */
        r.architecture = (const void *)&header;
        YVEX_TEST_ASSERT(yvex_joint_program_compile(&refused, &r, &err) == YVEX_ERR_FORMAT && !refused,
            "stale joint recipe is rejected before reading its changed layout");
    }
    r.architecture = &a;
    printf("Joint import: 5 stale schema headers refused before geometry access\n");
    for (unsigned int scenario = 0u; scenario < 6u; ++scenario) {
        yvex_joint_program *p = NULL, *repeat = NULL;
        r.blocks = scenario == 0u ? 1u : scenario == 1u ? 2u : 50u;
        r.inspected_block = scenario == 3u ? 2u : 0u;
        r.block_results = scenario == 3u;
        if (scenario >= 4u) {
            r.blocks = scenario == 4u ? 1u : 50u;
            r.video_rows = 2u; r.audio_rows = 1u; r.text_rows = 3u; r.rows = 6u;
            r.position_axes = 2u; r.time_embedding_width = 8u; r.maximum_period = 10000.0;
            a.video_input_width = 2u; a.audio_input_width = 2u; a.condition_input_width = 4u;
            a.refiner_block_count = 2u;
        }
        int rc = yvex_joint_program_compile(&p, &r, &err);
        if (rc != YVEX_OK) fprintf(stderr, "joint compile: %s: %s\n",
            yvex_error_where(&err), yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == YVEX_OK, "conditioned blocks compile without backend topology ownership");
        YVEX_TEST_ASSERT(yvex_joint_program_compile(&repeat, &r, &err) == YVEX_OK &&
            !strcmp(yvex_program_physical_summary_get(p->physical)->identity,
                yvex_program_physical_summary_get(repeat->physical)->identity), "joint compiler identity is deterministic");
        const yvex_program_physical_summary *s = yvex_program_physical_summary_get(p->physical);
        size_t parameters = 0u, gates = 0u, attentions = 0u;
        for (size_t i = 0u; i < s->step_count; ++i) {
            const char *name = yvex_program_physical_step_at(p->physical, i)->implementation;
            parameters += !strcmp(name, "parameter.encoded.v1");
            gates += !strcmp(name, "indexed_gated_residual.bf16.v1");
            attentions += !strcmp(name, "attention.full.f32.v1");
        }
        YVEX_TEST_ASSERT(parameters == r.blocks * 10u + (r.video_rows ? 35u : 0u) &&
            gates == r.blocks * 2u && attentions == r.blocks + (r.video_rows ? 2u : 0u) &&
            s->input_count == (r.video_rows ? 10u : 5u) &&
            s->result_count == (r.video_rows ? 2u : 1u) + p->stage_result_count + p->block_result_count,
            "source geometry projects parameters, explicit dependencies and inspection results exactly");
        if (r.video_rows) {
            const yvex_program_physical_summary *prepare = yvex_program_physical_summary_get(p->preparation);
            const yvex_program_physical_summary *step = yvex_program_physical_summary_get(p->step);
            YVEX_TEST_ASSERT(prepare && step && prepare->input_count == 2u && prepare->result_count == 3u &&
                step->input_count == 11u && step->result_count == 2u && step->minimum_rows == 1u &&
                step->maximum_rows == 1u && step->row_multiple == 1u &&
                !strcmp(s->semantic_identity, prepare->semantic_identity) &&
                !strcmp(s->semantic_identity, step->semantic_identity) &&
                !strcmp(s->execution_identity, step->execution_identity),
                "prepare, step and composed forward have one module authority and typed retention seam");
            printf("Joint composition: prepare=%zu instructions, step=%zu instructions; "
                "three retained results; fixed-shape step is one invocation, not a dynamic batch\n",
                prepare->step_count, step->step_count);
        }
        printf("Joint compiler: blocks=%llu parameters=%zu attention=%zu gates=%zu steps=%zu slots=%zu "
            "block_results=%zu stage_results=%zu; repeat identity exact\n", r.blocks, parameters, attentions, gates,
            s->step_count, s->storage_count, p->block_result_count, p->stage_result_count);
        yvex_joint_program_close(&repeat); yvex_joint_program_close(&p);
    }
    return test_joint_target() || test_joint_observations() || test_joint_time_profiles();
}

static inline int test_joint_execution(void)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_transformer_joint_recipe a = joint_fixture_architecture();
    yvex_joint_program_recipe r = {.architecture = &a, .source_identity = source, .rows = 3u,
        .timesteps = 2u, .blocks = 2u, .normalization_epsilon = 1e-5};
    yvex_joint_program *p = NULL;
    yvex_program_stage *stage = NULL;
    yvex_backend *backend = NULL;
    yvex_device_tensor *resident = NULL;
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_backend_memory_stats before, after;
    yvex_backend_operation_facts facts;
    yvex_program_kernel_parameter parameters[20] = {0};
    yvex_error err = {0};
    unsigned char *weights = NULL;
    unsigned long long total = 0u;
    YVEX_TEST_ASSERT(yvex_joint_program_compile(&p, &r, &err) == YVEX_OK, "joint execution compiler");
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(p->physical);
    for (size_t i = 0u; i < s->value_count; ++i) {
        const yvex_program_physical_value *v = yvex_program_physical_value_at(p->physical, i);
        if (!v->parameter) continue;
        yvex_program_kernel_parameter *parameter = parameters + v->tensor_id;
        parameter->tensor_id = v->tensor_id;
        unsigned long long width = v->type.shape[v->type.rank - 1u].extent;
        unsigned long long rows = v->type.rank == 2u ? v->type.shape[0].extent : 1u;
        parameter->weight = (yvex_component_encoded_weight){.row_count = rows, .row_width = width,
            .row_bytes = width * 2u, .encoded_bytes = rows * width * 2u, .qtype = YVEX_GGUF_QTYPE_BF16};
        total += parameter->weight.encoded_bytes;
    }
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK, "joint backend opens");
    yvex_backend_tensor_desc desc = {.name = "joint-weights", .dtype = YVEX_DTYPE_I8,
        .rank = 1u, .dims = {total}, .bytes = total};
    YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &desc, &resident, &weights, &err) == YVEX_OK,
        "joint fixture parameter residency");
    memset(weights, 0, (size_t)total);
    unsigned long long offset = 0u;
    for (size_t i = 0u; i < 20u; ++i) {
        parameters[i].weight.encoded = weights + offset;
        offset += parameters[i].weight.encoded_bytes;
        if (i % 10u == 0u || i % 10u == 2u || i % 10u == 3u || i % 10u == 5u) {
            for (size_t j = 0u; j < parameters[i].weight.encoded_bytes / 2u; ++j) {
                unsigned short one = yvex_quant_bf16_encode(1.0f);
                memcpy((unsigned char *)parameters[i].weight.encoded + j * 2u, &one, 2u);
            }
        }
    }
    YVEX_TEST_ASSERT(yvex_backend_resident_attach(backend, weights, total, resident, 1u, &err) == YVEX_OK,
        "register joint fixture physical parameters");
    int rc = yvex_program_stage_open(&stage, p->physical, parameters, 20u, backend, 3u, 1,
        1048576u, 1048576u, &err);
    if (rc != YVEX_OK) fprintf(stderr, "joint bind: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "joint lowering binds admitted operations without a family backend");
    float x[24], time[8] = {0}, cosine[12], sine[12] = {0}, output[24];
    unsigned int indices[] = {0u, 5u, 2u};
    for (size_t i = 0u; i < 24u; ++i) x[i] = (float)((int)i - 12) / 16.0f;
    for (size_t i = 0u; i < 12u; ++i) cosine[i] = 1.0f;
    yvex_program_host_input inputs[] = {{.values = x}, {.values = time}, {.indices = indices},
        {.values = cosine}, {.values = sine}};
    float *outputs[] = {output};
    rc = yvex_program_stage_host_inputs(stage, 3u, inputs, 5u, outputs, 1u, NULL, NULL, &facts, &err);
    if (rc != YVEX_OK) fprintf(stderr, "joint execute: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && !memcmp(x, output, sizeof(x)), "zero gates yield exact residual identity");
    printf("Joint SSA CUDA: blocks=2 rows=3 timesteps=2 values=24 first=%.9g last=%.9g "
        "max_abs=0 tolerance=0 launches=%llu; independent zero-gate identity\n",
        (double)output[0], (double)output[23], facts.kernel_launches);
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK &&
        yvex_backend_resident_detach(backend, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        before.allocated_bytes == after.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "joint program resources return to baseline");
    yvex_joint_program_close(&p);
    return 0;
}
#endif
