/* Pinned Mamba2 source importer and source-faithful physical projection. */
#include "src/graph/private.h"

#include <yvex/internal/artifact.h>
#include <yvex/internal/artifact_lowering.h>
#include <yvex/internal/family_catalog.h>
#include <yvex/internal/compiler.h>
#include <yvex/internal/compiler_source.h>
#include <yvex/internal/compilation.h>
#include <yvex/internal/deployment.h>
#include <yvex/internal/families/mamba2.h>
#include <yvex/internal/graph.h>
#include <yvex/internal/operator_graph.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/source_catalog.h>
#include <yvex/internal/tokenizer.h>

#include <stdlib.h>
#include <string.h>

#define MAMBA_TENSOR_COUNT 579ull
#define MAMBA_PINNED_STANDARD_COUNT 3ull
#define MAMBA_EXTENSION_COUNT 576ull
#define MAMBA_MAPPING_IDENTITY 7607356851058801913ull
#define MAMBA_SOURCE_FAITHFUL_PRESET "mamba-codestral-source-faithful-v1"
#define MAMBA_LOGICAL_TRANSFORM_IDENTITY \
    "6cde84173a2cc012d1b3c475d971a45a27d5506019c95cc09ffa0aa7a4ceab08"

static int mamba_role_project(yvex_mamba2_role source, yvex_tensor_role *role,
                              yvex_tensor_collection *collection)
{
    static const yvex_tensor_role roles[YVEX_MAMBA2_ROLE_COUNT] = {
        [YVEX_MAMBA2_ROLE_EMBEDDING] = YVEX_TENSOR_ROLE_TOKEN_EMBEDDING,
        [YVEX_MAMBA2_ROLE_FINAL_NORM] = YVEX_TENSOR_ROLE_OUTPUT_NORM,
        [YVEX_MAMBA2_ROLE_LM_HEAD] = YVEX_TENSOR_ROLE_OUTPUT_HEAD,
        [YVEX_MAMBA2_ROLE_BLOCK_NORM] = YVEX_TENSOR_ROLE_SEQUENCE_MIXER_BLOCK_NORM,
        [YVEX_MAMBA2_ROLE_INPUT_PROJECTION] =
            YVEX_TENSOR_ROLE_SEQUENCE_MIXER_INPUT_PROJECTION,
        [YVEX_MAMBA2_ROLE_CONVOLUTION_WEIGHT] =
            YVEX_TENSOR_ROLE_SEQUENCE_MIXER_CONVOLUTION,
        [YVEX_MAMBA2_ROLE_CONVOLUTION_BIAS] =
            YVEX_TENSOR_ROLE_SEQUENCE_MIXER_CONVOLUTION_BIAS,
        [YVEX_MAMBA2_ROLE_DECAY_LOG] = YVEX_TENSOR_ROLE_SEQUENCE_MIXER_DECAY_LOG,
        [YVEX_MAMBA2_ROLE_SKIP] = YVEX_TENSOR_ROLE_SEQUENCE_MIXER_SKIP,
        [YVEX_MAMBA2_ROLE_TIME_BIAS] = YVEX_TENSOR_ROLE_SEQUENCE_MIXER_TIME_BIAS,
        [YVEX_MAMBA2_ROLE_GATED_NORM] =
            YVEX_TENSOR_ROLE_SEQUENCE_MIXER_OUTPUT_NORM,
        [YVEX_MAMBA2_ROLE_OUTPUT_PROJECTION] =
            YVEX_TENSOR_ROLE_SEQUENCE_MIXER_OUTPUT};

    if (!role || !collection || source <= YVEX_MAMBA2_ROLE_UNKNOWN ||
        source >= YVEX_MAMBA2_ROLE_COUNT || roles[source] == YVEX_TENSOR_ROLE_UNKNOWN)
        return 0;
    *role = roles[source];
    if (source == YVEX_MAMBA2_ROLE_EMBEDDING || source == YVEX_MAMBA2_ROLE_LM_HEAD)
        *collection = YVEX_TENSOR_COLLECTION_GLOBAL;
    else if (source == YVEX_MAMBA2_ROLE_FINAL_NORM ||
             source == YVEX_MAMBA2_ROLE_BLOCK_NORM)
        *collection = YVEX_TENSOR_COLLECTION_NORM;
    else
        *collection = YVEX_TENSOR_COLLECTION_SEQUENCE_MIXER;
    return 1;
}

static int mamba_transform_add(yvex_transform_recipe_sink *sink,
                               const yvex_native_weight_info *tensor,
                               const yvex_mamba2_tensor_binding *binding,
                               unsigned long long ordinal,
                               yvex_transform_failure *failure, yvex_error *err)
{
    yvex_transform_direct_recipe recipe = {0};
    yvex_tensor_role role;
    yvex_tensor_collection collection;
    unsigned int dimension;

    if (!tensor || !binding || tensor->dtype != YVEX_NATIVE_DTYPE_BF16 ||
        !tensor->rank || tensor->rank > YVEX_TRANSFORM_IR_MAX_RANK ||
        !mamba_role_project(binding->role, &role, &collection)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "mamba2.transform",
                       "tensor cannot be projected into the source-faithful transform");
        return YVEX_ERR_FORMAT;
    }
    recipe.source_name = tensor->name;
    recipe.role = role;
    recipe.collection = collection;
    recipe.scope = binding->layer_index == ~0ull
                       ? YVEX_TENSOR_SCOPE_GLOBAL : YVEX_TENSOR_SCOPE_MAIN_LAYER;
    recipe.layer = binding->layer_index;
    recipe.auxiliary = YVEX_TRANSFORM_IR_NO_ID;
    recipe.expert = YVEX_TRANSFORM_IR_NO_ID;
    recipe.requirement_index = ordinal;
    recipe.source_dtype = tensor->dtype;
    recipe.shape.rank = tensor->rank;
    for (dimension = 0u; dimension < tensor->rank; ++dimension)
        recipe.shape.dims[dimension] = tensor->dims[dimension];
    return yvex_transform_recipe_add_direct(sink, &recipe, failure, err);
}

typedef struct {
    const yvex_mamba2_architecture *architecture;
    yvex_source_tensor_snapshot *snapshot;
    unsigned long long tensor_count;
} mamba_transform_projection;

static int mamba_transform_project(void *context, yvex_transform_recipe_sink *sink,
                                   yvex_transform_failure *failure, yvex_error *err)
{
    mamba_transform_projection *projection = context;
    unsigned long long index;
    int rc = YVEX_OK;

    if (!projection || !sink) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "mamba2.transform",
                       "transform projection and compiler sink are required");
        return YVEX_ERR_INVALID_ARG;
    }
    for (index = 0ull; rc == YVEX_OK && index < projection->tensor_count; ++index) {
        const yvex_native_weight_info *tensor =
            yvex_source_tensor_snapshot_at(projection->snapshot, index);
        yvex_mamba2_tensor_binding binding = {0};

        rc = yvex_model_register_mamba2()->tensor_classify(
            projection->architecture, tensor, &binding, err);
        if (rc == YVEX_OK)
            rc = mamba_transform_add(sink, tensor, &binding, index, failure, err);
    }
    return rc;
}

static int mamba_transform_build(yvex_transform_ir **out,
                                 const yvex_source_verification *verification,
                                 const yvex_mamba2_architecture *architecture,
                                 yvex_source_tensor_snapshot *snapshot,
                                 const yvex_mamba2_inventory *inventory,
                                 yvex_transform_failure *failure, yvex_error *err)
{
    yvex_source_tensor_snapshot_facts facts = {0};
    yvex_transform_builder_options options = {0};
    yvex_transform_header header = {0};
    mamba_transform_projection projection = {0};

    if (out) *out = NULL;
    if (!out || !verification || !architecture || !snapshot || !inventory ||
        !inventory->complete || inventory->tensors != MAMBA_TENSOR_COUNT ||
        !verification->verified || verification->blocker_count ||
        !verification->manifest_payload_trusted ||
        !yvex_sha256_hex_valid(verification->manifest_payload_identity) ||
        yvex_source_tensor_snapshot_facts_get(snapshot, &facts, err) != YVEX_OK ||
        facts.identity != verification->source_snapshot_identity ||
        facts.tensor_count != MAMBA_TENSOR_COUNT) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "mamba2.transform",
                       "verified pinned source and complete tensor accounting are required");
        return YVEX_ERR_FORMAT;
    }
    header.schema_version = YVEX_TRANSFORM_IR_SPECIALIZATION_SCHEMA_VERSION;
    header.logical_model_identity = architecture->architecture_identity;
    header.source_snapshot_identity = facts.identity;
    header.required_payload_identity = verification->manifest_payload_identity;
    header.payload_trust_class = verification->manifest_payload_trust_class;
    header.architecture_identity = architecture->architecture_identity;
    header.role_map_identity = inventory->role_identity;
    header.source_population_count = facts.tensor_count;
    header.expected_source_count = MAMBA_TENSOR_COUNT;
    header.expected_terminal_count = MAMBA_TENSOR_COUNT;
    header.header_scan_count = facts.header_scan_count;
    yvex_transform_budget_default(&options.budget);
    options.source_snapshot = snapshot;
    projection.architecture = architecture;
    projection.snapshot = snapshot;
    projection.tensor_count = facts.tensor_count;
    return yvex_transform_recipe_compile(
        out, &header, mamba_transform_project, &projection, &options, failure, err);
}

static void mamba_metadata_string(yvex_artifact_lowering_metadata *entry,
                                  const char *key, const char *value)
{
    memset(entry, 0, sizeof(*entry));
    yvex_core_text_copy(entry->key, sizeof(entry->key), key);
    entry->type = YVEX_ARTIFACT_LOWERING_METADATA_STRING;
    yvex_core_text_copy(entry->string_value, sizeof(entry->string_value), value);
}

static void mamba_metadata_u64(yvex_artifact_lowering_metadata *entry,
                               const char *key, unsigned long long value)
{
    memset(entry, 0, sizeof(*entry));
    yvex_core_text_copy(entry->key, sizeof(entry->key), key);
    entry->type = YVEX_ARTIFACT_LOWERING_METADATA_U64;
    entry->u64_value = value;
}

static int mamba_lowering_build(yvex_artifact_lowering_map **out,
                                const yvex_transform_ir *transform,
                                const yvex_mamba2_architecture *architecture,
                                yvex_artifact_lowering_failure *failure,
                                yvex_error *err)
{
    yvex_artifact_lowering_metadata metadata[16];
    yvex_artifact_lowering_policy policy = {0};
    unsigned long long count = 0ull;

    mamba_metadata_string(&metadata[count++], "general.architecture", "mamba2");
    mamba_metadata_string(&metadata[count++], "general.name", "Mamba-Codestral-7B-v0.1");
    mamba_metadata_string(&metadata[count++], "general.source.repository", YVEX_MAMBA2_REPOSITORY);
    mamba_metadata_string(&metadata[count++], "general.source.revision", YVEX_MAMBA2_REVISION);
    mamba_metadata_string(&metadata[count++], "yvex.mamba2.token_policy", "mistral-tokenizer-v1");
    mamba_metadata_string(&metadata[count++], "yvex.mamba2.normalization_policy",
                          "gate-before-grouped-rms-v1");
    mamba_metadata_u64(&metadata[count++], "mamba2.block_count", architecture->layer_count);
    mamba_metadata_u64(&metadata[count++], "mamba2.embedding_length", architecture->hidden_size);
    mamba_metadata_u64(&metadata[count++], "mamba2.vocabulary_size", architecture->vocabulary_size);
    mamba_metadata_u64(&metadata[count++], "mamba2.feed_forward_length", architecture->expansion);
    mamba_metadata_u64(&metadata[count++], "mamba2.ssm.head_count",
                       architecture->mixer.requirement.heads);
    mamba_metadata_u64(&metadata[count++], "mamba2.ssm.head_dimension",
                       architecture->mixer.requirement.head_dimension);
    mamba_metadata_u64(&metadata[count++], "mamba2.ssm.state_dimension",
                       architecture->mixer.requirement.state_dimension);
    mamba_metadata_u64(&metadata[count++], "mamba2.ssm.group_count",
                       architecture->mixer.requirement.groups);
    mamba_metadata_u64(&metadata[count++], "mamba2.ssm.convolution_kernel",
                       architecture->mixer.requirement.convolution_kernel);
    mamba_metadata_u64(&metadata[count++], "mamba2.ssm.chunk_size", architecture->chunk_size);
    policy.schema_version = YVEX_ARTIFACT_LOWERING_POLICY_SCHEMA_V1;
    policy.source_contribution_count = MAMBA_TENSOR_COUNT;
    policy.descriptor_count = MAMBA_TENSOR_COUNT;
    policy.trunk_descriptor_count = MAMBA_TENSOR_COUNT;
    policy.pinned_standard_count = MAMBA_PINNED_STANDARD_COUNT;
    policy.extension_count = MAMBA_EXTENSION_COUNT;
    policy.trunk_collection_counts[YVEX_TENSOR_COLLECTION_GLOBAL] = 2ull;
    policy.trunk_collection_counts[YVEX_TENSOR_COLLECTION_NORM] = 65ull;
    policy.trunk_collection_counts[YVEX_TENSOR_COLLECTION_SEQUENCE_MIXER] = 512ull;
    policy.metadata = metadata;
    policy.metadata_count = count;
    return yvex_artifact_lowering_operations.build(
        out, transform, &policy, failure, err);
}

static int mamba_source_lower(yvex_transform_ir **transform,
                              yvex_artifact_lowering_map **lowering,
                              const yvex_source_verification *verification,
                              yvex_source_tensor_snapshot *snapshot,
                              yvex_compilation_source_failure *failure,
                              yvex_error *err)
{
    const yvex_mamba2_api *family = yvex_model_register_mamba2();
    yvex_mamba2_architecture architecture;
    yvex_mamba2_inventory inventory = {0};
    yvex_transform_failure transform_failure = {0};
    yvex_artifact_lowering_failure lowering_failure = {0};
    int rc;

    if (transform) *transform = NULL;
    if (lowering) *lowering = NULL;
    rc = family->open(verification, &architecture, err);
    if (rc == YVEX_OK)
        rc = family->snapshot_audit(&architecture, snapshot, &inventory, err);
    if (rc != YVEX_OK && failure)
        failure->code = YVEX_COMPILATION_SOURCE_FAILURE_SEMANTIC_MODEL;
    if (rc == YVEX_OK)
        rc = mamba_transform_build(transform, verification, &architecture, snapshot,
                                   &inventory, &transform_failure, err);
    if (rc != YVEX_OK && failure &&
        failure->code == YVEX_COMPILATION_SOURCE_FAILURE_NONE)
        failure->code = YVEX_COMPILATION_SOURCE_FAILURE_TRANSFORM_IR;
    if (rc == YVEX_OK)
        rc = mamba_lowering_build(lowering, *transform, &architecture,
                                  &lowering_failure, err);
    if (rc != YVEX_OK && failure && *transform && !*lowering)
        failure->code = YVEX_COMPILATION_SOURCE_FAILURE_LOWERING;
    return rc;
}

static const void *mamba_source_identity(void)
{
    return yvex_source_target_identity_find(YVEX_MAMBA2_TARGET);
}

static const yvex_compilation_source_projection mamba_source_projection = {
    .schema_version = YVEX_COMPILATION_SOURCE_PROJECTION_SCHEMA_V1,
    .expected_mapping_identity = MAMBA_MAPPING_IDENTITY,
    .required_contribution_mask =
        YVEX_COMPILATION_SOURCE_REQUIRE_DIRECT |
        YVEX_COMPILATION_SOURCE_REQUIRE_GLOBAL |
        YVEX_COMPILATION_SOURCE_REQUIRE_NORM |
        YVEX_COMPILATION_SOURCE_REQUIRE_OUTPUT_HEAD,
    .source_identity = mamba_source_identity,
    .lower = mamba_source_lower,
    .lowering = &yvex_artifact_lowering_operations};

static int mamba_semantic_model_build(yvex_semantic_model_ir **out,
    const yvex_source_verification *verification, const yvex_mamba2_architecture *architecture,
    yvex_error *err)
{
    yvex_semantic_reference_request references[] = {
        {"source-revision", YVEX_MAMBA2_REVISION},
        {"token-policy", "mistral-tokenizer-v1"},
        {"normalization-recipe", "mistral-inference@9eaeb91c17450e09021b6065a1d5cc69876507c8/mamba-ssm-mamba2"}};
    yvex_semantic_model_ir_request request = {0};
    yvex_model_execution_descriptor execution = {0};
    yvex_semantic_numeric_contract numeric = {0};
    yvex_ir_module *program = NULL;
    char schedule[YVEX_SHA256_HEX_BYTES], state[YVEX_SHA256_HEX_BYTES];
    yvex_model_execution_descriptor_request descriptor = {0};
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    int rc;

    if (out) *out = NULL;
    rc = yvex_mamba2_program_build(
        &program, architecture, verification->manifest_payload_identity, err);
    if (rc == YVEX_OK) {
        yvex_sha256_init(&hash);
        if (!yvex_sha256_update_text(&hash, "yvex.mamba2.program-schedule.v1") ||
            !yvex_sha256_update_text(&hash, architecture->architecture_identity) ||
            !yvex_sha256_update_u64(&hash, architecture->layer_count) ||
            !yvex_sha256_update_u64(&hash, architecture->hidden_size) ||
            !yvex_sha256_final(&hash, digest))
            rc = YVEX_ERR_STATE;
        else
            yvex_sha256_hex(digest, schedule);
    }
    if (rc == YVEX_OK) {
        yvex_sha256_init(&hash);
        if (!yvex_sha256_update_text(&hash, "yvex.mamba2.persistent-state.v1") ||
            !yvex_sha256_update_text(&hash, architecture->architecture_identity) ||
            !yvex_sha256_update_u64(
                &hash, architecture->mixer.convolution_state_values) ||
            !yvex_sha256_update_u64(
                &hash, architecture->mixer.recurrent_state_values) ||
            !yvex_sha256_update_u64(&hash, architecture->layer_count) ||
            !yvex_sha256_final(&hash, digest))
            rc = YVEX_ERR_STATE;
        else
            yvex_sha256_hex(digest, state);
    }
    if (rc == YVEX_OK) {
        descriptor = (yvex_model_execution_descriptor_request){
            .schema_version = YVEX_MODEL_EXECUTION_DESCRIPTOR_SCHEMA_V2,
            .logical_model_identity = architecture->architecture_identity,
            .source_model_identity = verification->manifest_payload_identity,
            .attention_schedule_identity = schedule,
            .persistent_state_identity = state,
            .maximum_context = 1048576ull,
            .original_context = 1048576ull,
            .rope_scaling = YVEX_MODEL_ROPE_SCALING_NONE,
            .layer_count = architecture->layer_count,
            .hidden_width = architecture->hidden_size,
            .vocabulary_size = architecture->vocabulary_size,
            .sequence_mixer_layers = architecture->layer_count,
            .normalization_epsilon = architecture->normalization_epsilon,
            .output_input_width = architecture->hidden_size,
            .output_vocabulary_size = architecture->vocabulary_size,
            .persistent_state_class_mask = YVEX_MODEL_STATE_CLASS_BIT(
                YVEX_MODEL_STATE_RECURRENT_SEQUENCE),
            .bos_token_id = architecture->effective_bos,
            .eos_token_id = architecture->effective_eos};
        rc = yvex_model_execution_descriptor_seal(
            &descriptor, &execution, err);
    }
    if (rc == YVEX_OK) {
        yvex_sha256_init(&hash);
        if (!yvex_sha256_update_text(&hash, "yvex.mamba2.numeric.v1") ||
            !yvex_sha256_update_text(&hash, architecture->architecture_identity) ||
            !yvex_sha256_update_u64(&hash, architecture->normalization_authority) ||
            !yvex_sha256_update_u64(
                &hash, architecture->mixer.requirement.normalization_groups) ||
            !yvex_sha256_update_u64(
                &hash, (unsigned int)architecture->mixer.requirement.norm_before_gate) ||
            !yvex_sha256_update_u64(&hash, YVEX_SEQUENCE_MIXER_NUMERIC_F32_RECURRENCE) ||
            !yvex_sha256_final(&hash, digest))
            rc = YVEX_ERR_STATE;
        else {
            numeric.schema_version = YVEX_SEMANTIC_NUMERIC_CONTRACT_SCHEMA_V1;
            numeric.numeric_schema_version = 1u;
            numeric.compute_policy_count = 1ull;
            numeric.activation_policy_count = 1ull;
            yvex_core_text_copy(
                numeric.algorithm_revision, sizeof(numeric.algorithm_revision),
                "mistral-mamba2-grouped-rms-gate-first-f32-state-v1");
            yvex_sha256_hex(digest, numeric.identity);
        }
    }
    if (rc == YVEX_OK) {
        request = (yvex_semantic_model_ir_request){
            .schema_version = YVEX_SEMANTIC_MODEL_IR_SCHEMA_V2,
            .family_adapter_id = YVEX_MAMBA2_ADAPTER_ID,
            .family_adapter_version = YVEX_MAMBA2_ADAPTER_VERSION,
            .target_id = YVEX_MAMBA2_TARGET,
            .source_model_identity = verification->manifest_payload_identity,
            .logical_model_identity = architecture->architecture_identity,
            .semantic_payload_identity = execution.identity,
            .execution_descriptor = &execution,
            .numeric_contract = &numeric,
            .references = references,
            .reference_count = sizeof(references) / sizeof(references[0]),
            .program = program};
        rc = yvex_semantic_model_ir_seal(out, &request, err);
    }
    yvex_ir_module_close(&program);
    return rc;
}

static int mamba_semantic_model_open(
    yvex_semantic_model_ir **out, const yvex_source_verification *verification,
    yvex_error *err)
{
    const yvex_mamba2_api *family = yvex_model_register_mamba2();
    yvex_mamba2_architecture architecture;

    if (out) *out = NULL;
    if (!out || !verification) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "mamba2.semantic-model",
                       "verified source facts are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (family->open(verification, &architecture, err) != YVEX_OK)
        return yvex_error_code(err);
    return mamba_semantic_model_build(out, verification, &architecture, err);
}

static int mamba_compilation_source_open(
    yvex_family_compilation_source *out,
    const yvex_compilation_runtime_binding_request *request, yvex_error *err)
{
    yvex_compilation_source_options options = {0};
    yvex_compilation_source_failure failure = {0};
    yvex_compilation_source_session *source = NULL;
    int rc;

    if (out) memset(out, 0, sizeof(*out));
    if (!out || !request || !request->source_path || !request->models_root) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "mamba2.compilation-source",
                       "exact source path and models root are required");
        return YVEX_ERR_INVALID_ARG;
    }
    options.source_path = request->source_path;
    options.models_root = request->models_root;
    options.manifest_path = request->source_manifest_path;
    yvex_source_payload_budget_default(&options.budget);
    if (request->source_stream_count > 64u) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "mamba2.compilation-source",
                       "source stream count exceeds the bounded compiler worker limit");
        return YVEX_ERR_BOUNDS;
    }
    if (request->source_stream_count) {
        options.budget.maximum_streams = request->source_stream_count;
        if (options.budget.maximum_open_handles < request->source_stream_count)
            options.budget.maximum_open_handles = request->source_stream_count;
        options.budget.maximum_inflight_host_bytes =
            options.budget.chunk_bytes * request->source_stream_count;
    }
    options.chunk_bytes = options.budget.chunk_bytes;
    options.page_bytes = options.budget.page_bytes;
    rc = yvex_compilation_source_operations.open(
        &source, &options, &mamba_source_projection, &failure, err);
    if (rc != YVEX_OK) {
        yvex_compilation_source_operations.close(source);
        return rc;
    }
    out->owner = source;
    out->verification = yvex_compilation_source_operations.verification(source);
    out->transform_ir = yvex_compilation_source_operations.transform(source);
    out->transform_binding = yvex_compilation_source_operations.binding(source);
    out->artifact_lowering = yvex_compilation_source_operations.lowering(source);
    out->source_summary = yvex_compilation_source_operations.summary(source);
    out->lowering_context = out->artifact_lowering;
    out->tokenizer_vocabulary_size = out->verification
                                         ? out->verification->tokenizer_effective_vocab_size
                                         : 0ull;
    if (!out->verification || !out->transform_ir || !out->transform_binding ||
        !out->artifact_lowering || !out->source_summary ||
        out->tokenizer_vocabulary_size != 32768ull) {
        yvex_compilation_source_operations.close(source);
        memset(out, 0, sizeof(*out));
        yvex_error_set(err, YVEX_ERR_STATE, "mamba2.compilation-source",
                       "Mamba2 source projection omitted an exact compiler input");
        return YVEX_ERR_STATE;
    }
    return YVEX_OK;
}

static void mamba_compilation_source_close(void *owner)
{
    yvex_compilation_source_operations.close(owner);
}

static int mamba_artifact_admit(
    const yvex_artifact *artifact, yvex_complete_artifact_admission *out,
    yvex_artifact_admission_failure *failure, yvex_error *err)
{
    static const yvex_complete_artifact_admission catalog = {
        .artifact_class = YVEX_ARTIFACT_CLASS_COMPLETE_YVEX,
        .metadata_count = 43ull,
        .tensor_count = MAMBA_TENSOR_COUNT,
        .payload_bytes = 14570807296ull,
        .file_bytes = 14574491136ull,
        .source_snapshot_identity = 0xdc8cab116bbcc072ull,
        .mapping_identity = MAMBA_MAPPING_IDENTITY,
        .payload_identity =
            "67f63fd2d04e240aaf09d436e2fe2eec6c086a28e4e78f2829cc303943b8a75e",
        .transform_identity = MAMBA_LOGICAL_TRANSFORM_IDENTITY,
        .profile_identity =
            "a10c94158265f53ae6e6add7a11d1fae3851e3b51d22129d63701e80fdfff0c9",
        .profile_name = MAMBA_SOURCE_FAITHFUL_PRESET,
        .quant_execution_identity =
            "8782999e6a87438cdd3ffa0afb26e325e855f63465f16eaec0960ff9fe3f6612",
        .payload_plan_identity =
            "f2d6c2bcba7e1aba6f610ac3fa358362352bb1457618243b8efa56b5a8cabd77",
        .payload_byte_identity =
            "a2b1f8642ec45b0c93655ada297f891b93490827c1431ce182def562c05c0e33",
        .writer_plan_identity =
            "74c40fad3b5048a11181c9424b6a3ec0db761b7a171fd4cdcc012fc0c6641afa",
        .artifact_identity =
            "bf0053bf02a235563342a0281acfc73e17eb287542a7109e975db414458a77d3",
        .official_reader_revision = YVEX_GGUF_OFFICIAL_READER_REVISION,
        .tokenizer_complete = 1,
        .native_reader_accepted = 1,
        .official_reader_accepted = 1,
        .payload_integrity_accepted = 1,
        .materialization_input_ready = 1};
    yvex_artifact_catalog_contract contract = {.catalog = &catalog};

    if (artifact && yvex_artifact_size(artifact) != catalog.file_bytes) {
        if (out) memset(out, 0, sizeof(*out));
        if (failure) {
            memset(failure, 0, sizeof(*failure));
            failure->code = YVEX_ARTIFACT_ADMISSION_IDENTITY_MISMATCH;
            failure->expected = catalog.file_bytes;
            failure->actual = yvex_artifact_size(artifact);
            yvex_core_text_copy(failure->field, sizeof(failure->field), "file-bytes");
        }
        yvex_error_set(err, YVEX_ERR_FORMAT, "mamba2.artifact-catalog",
                       "artifact extent is not the qualified Mamba2 representation");
        return YVEX_ERR_FORMAT;
    }
    return yvex_artifact_admit_catalog(
        artifact, NULL, NULL, &contract, out, failure, err);
}

static int mamba_runtime_unqualified(
    yvex_runtime_descriptor **out,
    const yvex_complete_artifact_admission *admission,
    yvex_materialization_session *materialization, const void *lowering_context,
    const yvex_semantic_model_ir *semantic_model, yvex_error *err)
{
    const yvex_semantic_model_ir_summary *semantic =
        yvex_semantic_model_ir_summary_get(semantic_model);
    const yvex_model_execution_descriptor *execution =
        semantic ? &semantic->execution_descriptor : NULL;
    const yvex_semantic_numeric_contract *numeric =
        semantic ? &semantic->numeric_contract : NULL;
    yvex_runtime_descriptor_family_facts facts = {0};
    yvex_runtime_descriptor_failure failure = {0};
    yvex_materialization_projection projection;
    int rc;

    if (!semantic || !execution || !numeric ||
        semantic->schema_version != YVEX_SEMANTIC_MODEL_IR_SCHEMA_V2 ||
        execution->schema_version != YVEX_MODEL_EXECUTION_DESCRIPTOR_SCHEMA_V2 ||
        numeric->schema_version != YVEX_SEMANTIC_NUMERIC_CONTRACT_SCHEMA_V1 ||
        semantic->decoder_layer_count || semantic->attention_layer_count ||
        execution->sequence_mixer_layers != execution->layer_count ||
        !yvex_sha256_hex_valid(numeric->identity)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "mamba2.runtime-descriptor",
                       "sealed pure-SSM execution and numeric contracts are required");
        return YVEX_ERR_FORMAT;
    }
    facts.logical_model_identity = execution->logical_model_identity;
    facts.runtime_numeric_identity = numeric->identity;
    facts.runtime_hadamard_revision = numeric->algorithm_revision;
    facts.runtime_numeric_schema_version = numeric->numeric_schema_version;
    facts.runtime_compute_policy_count = numeric->compute_policy_count;
    facts.runtime_activation_policy_count = numeric->activation_policy_count;
    facts.runtime_sparse_topk_policy_count = numeric->sparse_topk_policy_count;
    facts.layer_count = execution->layer_count;
    facts.vocabulary_size = execution->vocabulary_size;
    facts.model_execution = execution;
    rc = yvex_materialization_project_artifact_lowering(
        (const yvex_artifact_lowering_map *)lowering_context,
        &projection, err);
    return rc == YVEX_OK
               ? yvex_runtime_descriptor_build_projected(
                     out, admission, materialization, &facts, &projection,
                     &failure, err)
               : rc;
}

static const yvex_quant_artifact_lowering_rule mamba_quant_lowering_rules[] = {
    {YVEX_ARTIFACT_LOWERING_TRANSFORM_DIRECT, YVEX_TRANSFORM_OP_IDENTITY,
     YVEX_GGUF_QTYPE_BF16, YVEX_GGUF_QTYPE_BF16, 0}};

static const yvex_quant_artifact_lowering_policy mamba_quant_lowering_policy = {
    MAMBA_SOURCE_FAITHFUL_PRESET, MAMBA_SOURCE_FAITHFUL_PRESET,
    mamba_quant_lowering_rules,
    sizeof(mamba_quant_lowering_rules) / sizeof(mamba_quant_lowering_rules[0])};

static int mamba_quant_default(
    yvex_quant_plan **out, const yvex_transform_ir *transform,
    const yvex_transform_binding *binding, const void *lowering_context,
    yvex_error *err)
{
    yvex_quant_failure failure = {0};
    return yvex_quant_plan_build_artifact_lowering_profile(
        out, transform, binding,
        (const yvex_artifact_lowering_map *)lowering_context,
        &mamba_quant_lowering_policy, YVEX_QUANT_PROFILE_SOURCE_FAITHFUL,
        NULL, &failure, err);
}

static int mamba_quant_policy(
    yvex_quant_plan **out, const yvex_transform_ir *transform,
    const yvex_transform_binding *binding, const void *lowering_context,
    const yvex_quant_policy *policy, const char *imatrix_identity,
    yvex_error *err)
{
    yvex_quant_failure failure = {0};
    return yvex_quant_plan_build_artifact_lowering_policy(
        out, transform, binding,
        (const yvex_artifact_lowering_map *)lowering_context,
        &mamba_quant_lowering_policy, policy, imatrix_identity,
        NULL, &failure, err);
}

static void mamba_preset_rule(yvex_quant_policy_rule *rule)
{
    memset(rule, 0, sizeof(*rule));
    rule->schema_version = YVEX_QUANT_POLICY_SCHEMA_VERSION;
    rule->match_mask = YVEX_QUANT_MATCH_PHYSICAL_CLASS;
    rule->operation = YVEX_QUANT_POLICY_OPERATION_ANY;
    rule->scope = YVEX_TENSOR_SCOPE_GLOBAL;
    rule->physical_class = YVEX_QUANT_POLICY_PHYSICAL_QUANTIZABLE;
    rule->qtype = YVEX_QUANT_QTYPE_SOURCE;
    rule->requires_cpu_compute = 1;
    rule->priority = 10u;
    rule->label = "preserve pinned Mamba2 BF16 source representation";
}

static unsigned long long mamba_preset_count(void) { return 1ull; }

static const char *mamba_preset_name(unsigned long long index)
{
    return index == 0ull ? MAMBA_SOURCE_FAITHFUL_PRESET : NULL;
}

static int mamba_preset_open(yvex_quant_policy **out, const char *name,
                             yvex_error *err)
{
    yvex_quant_policy_rule rule;
    yvex_quant_policy_definition definition;
    if (!out || !name || strcmp(name, MAMBA_SOURCE_FAITHFUL_PRESET) != 0) {
        if (out) *out = NULL;
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "quant_policy_preset",
                        "unknown Mamba2 quantization preset: %s",
                        name ? name : "-");
        return YVEX_ERR_UNSUPPORTED;
    }
    mamba_preset_rule(&rule);
    definition = (yvex_quant_policy_definition){
        MAMBA_SOURCE_FAITHFUL_PRESET, YVEX_MAMBA2_TARGET,
        "built-in-preset", &rule, 1ull};
    return yvex_quant_policy_create_definition(out, &definition, err);
}

static const yvex_quant_preset_catalog *mamba_quant_presets(void)
{
    static const yvex_quant_preset_catalog catalog = {
        YVEX_QUANT_PRESET_CATALOG_SCHEMA_V1, YVEX_MAMBA2_TARGET,
        mamba_preset_count, mamba_preset_name, mamba_preset_open};
    return &catalog;
}

static int mamba_tokenizer_policy(yvex_tokenizer_family_policy *out,
                                  yvex_error *err)
{
    static const yvex_tokenizer_direct_policy policy = {
        .family_adapter_id = YVEX_MAMBA2_ADAPTER_ID,
        .family_adapter_version = YVEX_MAMBA2_ADAPTER_VERSION,
        .tokenizer_kind = YVEX_TOKENIZER_KIND_GGML_LLAMA,
        .model_policy = YVEX_TOKENIZER_MODEL_BPE_METASPACE,
        .prompt_policy = YVEX_TOKENIZER_PROMPT_VERBATIM,
        .vocabulary_size = 32768ull, .base_vocabulary_size = 32768ull,
        .merge_count = 58980ull, .added_token_count = 771ull,
        .special_token_count = 751ull,
        .bos_token_id = 1u, .eos_token_id = 2u, .unk_token_id = 0u,
        .bos_present = 1, .eos_present = 1, .unk_present = 1,
        .add_bos_token = 1, .add_eos_token = 0, .byte_fallback = 1,
        .architecture = "mamba2", .tokenizer_model = "llama",
        .tokenizer_pre = "sentencepiece",
        .tokenizer_json_identity =
            "f9fb70f3b36291190d91add421a062b42ef517d43cdd2e1f32ca19e84096b4ca",
        .tokenizer_config_identity =
            "c9b9bc491a0c198f87a7925e65657eda7544f4dbd218ffae0731bcedd03e0cae",
        .prompt_name = "verbatim-mistral-bos-v1"};
    return yvex_tokenizer_family_policy_compile_direct(out, &policy, err) == YVEX_OK;
}

static const yvex_graph_compiler_api *mamba_graph_compile(void)
{
    static const yvex_graph_compiler_api compiler = {0};
    return &compiler;
}

static int mamba_execution_capabilities(yvex_runtime_capabilities *out)
{
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    return yvex_runtime_capabilities_contract_valid(out);
}

static int mamba_transformer_policy(
    const yvex_runtime_descriptor_summary *runtime,
    yvex_transformer_family_policy *out)
{
    (void)runtime;
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    return 1;
}

static int mamba_logits_policy(yvex_logits_family_policy *out)
{
    if (!out) return 0;
    *out = (yvex_logits_family_policy){
        .schema_version = YVEX_RUNTIME_LOGITS_SCHEMA_V1,
        .separate_output_head = 1};
    return 1;
}

static int mamba_speculation_policy(
    const yvex_runtime_descriptor_summary *runtime,
    yvex_speculation_family_policy *out)
{
    (void)runtime;
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    return 1;
}

static const yvex_family_binding_pipeline mamba_binding_pipeline = {
    .schema_version = YVEX_FAMILY_BINDING_PIPELINE_SCHEMA_V1,
    .source_open = mamba_compilation_source_open,
    .source_close = mamba_compilation_source_close,
    .artifact_admit = mamba_artifact_admit,
    .semantic_model_build = mamba_semantic_model_open,
    .runtime_descriptor_build = mamba_runtime_unqualified,
    .quant_plan_default = mamba_quant_default,
    .quant_plan_policy = mamba_quant_policy,
    .tokenizer_architecture = "mamba2", .tokenizer_model = "llama",
    .tokenizer_pre = "sentencepiece"};

static const yvex_family_compiler_adapter mamba_compiler = {
    .schema_version = YVEX_FAMILY_COMPILER_SCHEMA_V2,
    .adapter_id = YVEX_MAMBA2_ADAPTER_ID,
    .adapter_version = YVEX_MAMBA2_ADAPTER_VERSION,
    .target_id = YVEX_MAMBA2_TARGET, .family = "mamba2",
    .logical_transform_identity = MAMBA_LOGICAL_TRANSFORM_IDENTITY,
    .graph = mamba_graph_compile,
    .operator_graph_build = yvex_operator_graph_ir_build_program,
    .execution_capabilities = mamba_execution_capabilities,
    .transformer_policy = mamba_transformer_policy,
    .logits_policy = mamba_logits_policy,
    .speculation_policy = mamba_speculation_policy,
    .tokenizer_policy = mamba_tokenizer_policy,
    .physical_variant = yvex_graph_physical_variant_api_get,
    .binding_pipeline = &mamba_binding_pipeline,
    .binding_compile = yvex_family_binding_compile};

static const yvex_model_deployment_defaults mamba_deployment_defaults = {
    .schema_version = YVEX_MODEL_DEPLOYMENT_DEFAULTS_SCHEMA_CURRENT,
    .logical_family = "mamba2", .logical_model = YVEX_MAMBA2_TARGET,
    .quant_preset = MAMBA_SOURCE_FAITHFUL_PRESET, .backend = "cpu",
    .engine_kind = "text", .execution_strategy = "target-only"};

static const yvex_graph_execution_binding *mamba_execution_binding(void)
{
    static const yvex_graph_execution_binding execution = {
        .schema_version = YVEX_GRAPH_EXECUTION_BINDING_SCHEMA_V1,
        .adapter_id = YVEX_MAMBA2_ADAPTER_ID,
        .adapter_version = YVEX_MAMBA2_ADAPTER_VERSION,
        .target_id = YVEX_MAMBA2_TARGET, .family_name = "mamba2",
        .logical_transform_identity = MAMBA_LOGICAL_TRANSFORM_IDENTITY,
        .operator_family_key = "mamba2",
        .operator_artifact_filename =
            "mamba-codestral-7b-v0.1-source-faithful.gguf",
        .source_manifest_filename =
            "mamba-codestral-7b-v0.1.source-manifest.json",
        .deployment_defaults = &mamba_deployment_defaults,
        .compiler = &mamba_compiler};
    return &execution;
}

const yvex_family_descriptor yvex_graph_family_descriptor_mamba2 = {
    .schema_version = YVEX_FAMILY_DESCRIPTOR_SCHEMA_V1,
    .target_id = YVEX_MAMBA2_TARGET, .family = "mamba2",
    .tokenizer_architecture = "mamba2", .tokenizer_pre = "sentencepiece",
    .execution = mamba_execution_binding,
    .quant_presets = mamba_quant_presets};
