/* Qualify mixed-layer recurrent state lifecycle and coordinated publication. */
#include "tests/test.h"

#include <limits.h>
#include <string.h>

#include <yvex/internal/sequence_state.h>

static int sequence_state_plan(yvex_gated_delta_plan *plan, yvex_error *err)
{
    const yvex_gated_delta_requirement requirement = {
        .schema_version = YVEX_SEQUENCE_MIXER_GATED_DELTA_SCHEMA_V2,
        .output_normalization_weight_convention =
            YVEX_NORMALIZATION_WEIGHT_DIRECT,
        .query_heads = 1ull,
        .key_heads = 1ull,
        .value_heads = 2ull,
        .key_head_dimension = 2ull,
        .value_head_dimension = 3ull,
        .convolution_kernel = 3ull,
        .projected_dtype = YVEX_DTYPE_F32,
        .convolution_state_dtype = YVEX_DTYPE_F32,
        .recurrent_state_dtype = YVEX_DTYPE_F32,
        .accumulation_dtype = YVEX_DTYPE_F32,
        .output_dtype = YVEX_DTYPE_F32,
        .numeric_contract = YVEX_SEQUENCE_MIXER_NUMERIC_F32_RECURRENCE,
        .qk_normalization_epsilon = 1e-6,
        .output_normalization_epsilon = 1e-6,
        .query_scale = 0.7071067811865476,
        .deterministic = 1};

    return yvex_gated_delta_plan_seal(plan, &requirement, err);
}

static int sequence_state_write_layer(
    yvex_sequence_state *state, unsigned long long layer, float value,
    yvex_error *err)
{
    yvex_sequence_state_view committed;
    yvex_sequence_state_output candidate;
    unsigned long long index;

    if (yvex_sequence_state_layer(
            state, layer, &committed, &candidate, err) != YVEX_OK)
        return 0;
    for (index = 0ull; index < candidate.convolution_capacity; ++index)
        candidate.convolution[index] = committed.convolution[index] + value;
    for (index = 0ull; index < candidate.recurrent_capacity; ++index)
        candidate.recurrent[index] = committed.recurrent[index] + value;
    return yvex_sequence_state_stage(state, layer, err) == YVEX_OK;
}

static int sequence_state_commit(
    yvex_sequence_state *state, int status, yvex_error *err)
{
    yvex_runtime_transaction_participant participant;

    if (yvex_sequence_state_participant(state, &participant, err) != YVEX_OK)
        return yvex_error_code(err);
    return yvex_runtime_transaction_resolve(&participant, 1u, status, err);
}

static int sequence_state_measure_refusals(void)
{
    static const char identity[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    yvex_sequence_state_binding bindings[2];
    yvex_sequence_state_plan plan = {.schema_version = YVEX_SEQUENCE_STATE_SCHEMA_V1};
    yvex_sequence_state_geometry geometry;
    yvex_error err;
    memset(&geometry, 0xff, sizeof(geometry));
    YVEX_TEST_ASSERT(yvex_sequence_state_plan_measure(&plan, &geometry, &err) == YVEX_OK &&
        !geometry.bank_values && !geometry.committed_bytes && !geometry.candidate_bytes,
        "an empty computational state population requires zero provider banks");
    plan.binding_count = 1ull;
    YVEX_TEST_ASSERT(yvex_sequence_state_plan_measure(&plan, &geometry, &err) == YVEX_ERR_INVALID_ARG &&
        !geometry.bank_values && !geometry.committed_bytes,
        "missing binding directory refuses before any state allocation");
    plan.bindings = bindings;
    plan.binding_count = 2ull;
    YVEX_TEST_ASSERT(yvex_sequence_state_binding_seal(&bindings[0], 1ull, 4ull, 8ull, identity, &err) == YVEX_OK &&
        yvex_sequence_state_binding_seal(&bindings[1], 1ull, 4ull, 8ull, identity, &err) == YVEX_OK &&
        yvex_sequence_state_plan_measure(&plan, &geometry, &err) == YVEX_ERR_FORMAT && !geometry.bank_values,
        "individually sealed duplicate state roots cannot be measured as an admissible population");
    YVEX_TEST_ASSERT(yvex_sequence_state_binding_seal(&bindings[1], 3ull, 4ull, 8ull, identity, &err) == YVEX_OK,
        "restore unique state roots");
    bindings[1].identity[0] = bindings[1].identity[0] == '0' ? '1' : '0';
    YVEX_TEST_ASSERT(yvex_sequence_state_plan_measure(&plan, &geometry, &err) == YVEX_ERR_FORMAT &&
        !geometry.bank_values && !geometry.committed_bytes && !geometry.candidate_bytes,
        "a late corrupted binding publishes no partial capacity");
    plan.binding_count = 1ull;
    YVEX_TEST_ASSERT(yvex_sequence_state_binding_seal(
        &bindings[0], 1ull, 0ull, ULLONG_MAX / sizeof(float), identity, &err) == YVEX_OK &&
        yvex_sequence_state_plan_measure(&plan, &geometry, &err) == YVEX_ERR_BOUNDS && !geometry.bank_values,
        "one legal bank cannot conceal overflow of the provider's committed plus candidate storage");
    YVEX_TEST_ASSERT(yvex_sequence_state_plan_measure(NULL, &geometry, &err) == YVEX_ERR_INVALID_ARG &&
        yvex_sequence_state_plan_measure(&plan, NULL, &err) == YVEX_ERR_INVALID_ARG,
        "measurement requires its exact plan and result owner");
    return 0;
}

int yvex_test_sequence_state(void)
{
    yvex_gated_delta_plan mixer;
    yvex_sequence_state_binding bindings[2];
    yvex_sequence_state_plan plan;
    yvex_sequence_state *state = NULL, *forked = NULL;
    yvex_sequence_state_summary summary;
    yvex_sequence_state_geometry geometry;
    yvex_sequence_state_view committed;
    yvex_error err;

    if (sequence_state_measure_refusals() != 0) return 1;
    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(sequence_state_plan(&mixer, &err) == YVEX_OK,
                     "seal tiny recurrent plan");
    YVEX_TEST_ASSERT(yvex_sequence_state_binding_seal(
        &bindings[0], 1ull, mixer.convolution_state_values,
        mixer.recurrent_state_values, mixer.identity, &err) == YVEX_OK &&
        yvex_sequence_state_binding_seal(
        &bindings[1], 3ull, mixer.convolution_state_values,
        mixer.recurrent_state_values, mixer.identity, &err) == YVEX_OK,
        "seal independent state geometry");
    plan = (yvex_sequence_state_plan){
        .schema_version = YVEX_SEQUENCE_STATE_SCHEMA_V1,
        .bindings = bindings,
        .binding_count = 2ull};
    YVEX_TEST_ASSERT(yvex_sequence_state_plan_measure(&plan, &geometry, &err) == YVEX_OK &&
        geometry.convolution_values == 40ull && geometry.recurrent_values == 24ull &&
        geometry.bank_values == 64ull && geometry.committed_bytes == 256ull &&
        geometry.candidate_bytes == 256ull,
        "startup measures exact provider storage without a session, allocation or historical decoder");
    YVEX_TEST_ASSERT(yvex_sequence_state_open(&state, &plan, &err) == YVEX_OK,
                     "open heterogeneous recurrent state");
    YVEX_TEST_ASSERT(
        yvex_sequence_state_summary_copy(state, &summary, &err) == YVEX_OK &&
            summary.binding_count == 2ull &&
            summary.convolution_state_bytes == 160ull &&
            summary.recurrent_state_bytes == 96ull &&
            summary.committed_state_bytes == 256ull &&
            summary.candidate_state_bytes == 256ull &&
            summary.host_state_bytes == 512ull &&
            summary.device_state_bytes == 0ull &&
            summary.host_authoritative && !summary.device_authoritative &&
            summary.fork_supported &&
            yvex_sha256_hex_valid(summary.plan_identity),
        "state summary exposes exact committed and candidate bytes");

    YVEX_TEST_ASSERT(
        yvex_sequence_state_begin(state, 0ull, 2ull, &err) == YVEX_OK &&
            sequence_state_write_layer(state, 1ull, 1.0f, &err) &&
            sequence_state_write_layer(state, 3ull, 3.0f, &err) &&
            sequence_state_commit(state, YVEX_OK, &err) == YVEX_OK,
        "all recurrent layers publish atomically");
    YVEX_TEST_ASSERT(
        yvex_sequence_state_summary_copy(state, &summary, &err) == YVEX_OK &&
            summary.committed_position == 2ull && summary.generation == 1ull &&
            !summary.transaction_active &&
            yvex_sequence_state_committed(state, 3ull, &committed, &err) ==
                YVEX_OK &&
            committed.convolution[0] == 3.0f && committed.recurrent[0] == 3.0f,
        "commit advances position and swaps complete state banks");

    YVEX_TEST_ASSERT(
        yvex_sequence_state_fork(&forked, state, &err) == YVEX_OK &&
            yvex_sequence_state_committed(forked, 3ull, &committed, &err) ==
                YVEX_OK &&
            committed.convolution[0] == 3.0f,
        "fork receives exact committed mixed sequence state");

    YVEX_TEST_ASSERT(
        yvex_sequence_state_begin(state, 2ull, 1ull, &err) == YVEX_OK &&
            sequence_state_write_layer(state, 1ull, 7.0f, &err) &&
            sequence_state_commit(state, YVEX_OK, &err) != YVEX_OK &&
            yvex_sequence_state_summary_copy(state, &summary, &err) == YVEX_OK &&
            summary.committed_position == 2ull && !summary.transaction_active &&
            yvex_sequence_state_committed(state, 1ull, &committed, &err) ==
                YVEX_OK &&
            committed.convolution[0] == 1.0f,
        "partial-layer prepare failure aborts without visible mutation");
    YVEX_TEST_ASSERT(
        yvex_sequence_state_begin(state, 2ull, 1ull, &err) == YVEX_OK &&
            sequence_state_write_layer(state, 1ull, 7.0f, &err) &&
            sequence_state_write_layer(state, 3ull, 9.0f, &err) &&
            sequence_state_commit(state, YVEX_ERR_CANCELLED, &err) ==
                YVEX_ERR_CANCELLED &&
            yvex_sequence_state_committed(state, 3ull, &committed, &err) ==
                YVEX_OK &&
            committed.convolution[0] == 3.0f,
        "cancellation discards every recurrent candidate bank");

    YVEX_TEST_ASSERT(
        yvex_sequence_state_reset(state, &err) == YVEX_OK &&
            yvex_sequence_state_committed(state, 3ull, &committed, &err) ==
                YVEX_OK &&
            committed.convolution[0] == 0.0f && committed.recurrent[0] == 0.0f,
        "reset clears recurrent and convolution state together");
    yvex_sequence_state_close(&forked);
    YVEX_TEST_ASSERT(
        yvex_sequence_state_invalidate(state, &err) == YVEX_OK &&
            yvex_sequence_state_begin(state, 0ull, 1ull, &err) != YVEX_OK &&
            yvex_sequence_state_summary_copy(state, &summary, &err) == YVEX_OK &&
            summary.invalidated,
        "engine invalidation makes retained recurrent state unusable");
    yvex_sequence_state_close(&state);
    YVEX_TEST_ASSERT(!forked && !state, "close releases recurrent state owners");

    bindings[1].layer_index = 1ull;
    YVEX_TEST_ASSERT(
        yvex_sequence_state_open(&state, &plan, &err) != YVEX_OK && !state,
        "duplicate recurrent layer binding fails closed");
    return 0;
}
