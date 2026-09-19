/* Mamba2 source importer: verified source becomes compiler-owned typed computation.
 * Artifact/deployment publication remains a distinct, fail-closed boundary. */
#include <yvex/internal/family_catalog.h>
#include <yvex/internal/compiler.h>
#include <yvex/internal/families/mamba2.h>
#include <yvex/internal/source_catalog.h>

#include <stdlib.h>
#include <string.h>

typedef struct {
    yvex_source_verification verification;
    yvex_semantic_model_ir *semantic_model;
} mamba_source_owner;

static void mamba_source_release(void *pointer)
{
    mamba_source_owner *owner = pointer;
    if (!owner) return;
    yvex_semantic_model_ir_close(&owner->semantic_model);
    free(owner);
}

static int mamba_semantic_model_build(yvex_semantic_model_ir **out,
    const yvex_source_verification *verification, const yvex_mamba2_architecture *architecture,
    yvex_error *err)
{
    yvex_semantic_reference_request references[] = {
        {"source-revision", YVEX_MAMBA2_REVISION},
        {"token-policy", "mistral-tokenizer-v1"},
        {"normalization-recipe", "mistral-inference@9eaeb91c17450e09021b6065a1d5cc69876507c8/mamba-ssm-mamba2"}};
    yvex_semantic_model_ir_request request = {0};
    yvex_ir_module *program = NULL;
    int rc;

    if (out) *out = NULL;
    rc = yvex_mamba2_program_build(
        &program, architecture, verification->manifest_payload_identity, err);
    if (rc == YVEX_OK) {
        request = (yvex_semantic_model_ir_request){
            .schema_version = YVEX_SEMANTIC_MODEL_IR_SCHEMA_V2,
            .family_adapter_id = YVEX_MAMBA2_ADAPTER_ID,
            .family_adapter_version = YVEX_MAMBA2_ADAPTER_VERSION,
            .target_id = YVEX_MAMBA2_TARGET,
            .source_model_identity = verification->manifest_payload_identity,
            .logical_model_identity = architecture->architecture_identity,
            .semantic_payload_identity = yvex_ir_identity(program),
            .references = references,
            .reference_count = sizeof(references) / sizeof(references[0]),
            .program = program};
        rc = yvex_semantic_model_ir_seal(out, &request, err);
    }
    yvex_ir_module_close(&program);
    return rc;
}

static int mamba_source_compile(yvex_family_source_products *out,
    const yvex_compilation_runtime_binding_request *request, yvex_error *err)
{
    const yvex_mamba2_api *family = yvex_model_register_mamba2();
    yvex_source_verify_options options = {0};
    yvex_source_verification verification;
    yvex_source_tensor_snapshot *snapshot = NULL;
    yvex_mamba2_architecture architecture;
    yvex_mamba2_inventory inventory;
    yvex_semantic_model_ir *semantic_model = NULL;
    mamba_source_owner *owner = NULL;
    const yvex_semantic_model_ir_summary *summary;
    int rc;

    if (out) memset(out, 0, sizeof(*out));
    if (!out || !request || !request->source_path || !request->models_root) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "mamba2.source-gate", "exact source is required");
        return YVEX_ERR_INVALID_ARG;
    }
    options.identity = yvex_source_target_identity_find(YVEX_MAMBA2_TARGET);
    options.source_path = request->source_path;
    options.models_root = request->models_root;
    options.manifest_path = request->source_manifest_path;
    options.promote_manifest = 1;
    rc = yvex_source_verify_with_snapshot(&options, &verification, &snapshot, err);
    if (rc == YVEX_OK) rc = family->open(&verification, &architecture, err);
    if (rc == YVEX_OK) rc = family->snapshot_audit(&architecture, snapshot, &inventory, err);
    yvex_source_tensor_snapshot_release(snapshot);
    if (rc == YVEX_OK)
        rc = mamba_semantic_model_build(&semantic_model, &verification, &architecture, err);
    if (rc != YVEX_OK) return rc;
    owner = calloc(1u, sizeof(*owner));
    if (!owner) {
        yvex_semantic_model_ir_close(&semantic_model);
        yvex_error_set(err, YVEX_ERR_NOMEM, "mamba2.source-import", "source product ownership failed");
        return YVEX_ERR_NOMEM;
    }
    owner->verification = verification;
    owner->semantic_model = semantic_model;
    summary = yvex_semantic_model_ir_summary_get(semantic_model);
    out->owner = owner;
    out->release = mamba_source_release;
    out->verification = &owner->verification;
    out->semantic_model = semantic_model;
    yvex_core_text_copy(out->derivation_identity, sizeof(out->derivation_identity), summary->identity);
    yvex_error_clear(err);
    return YVEX_OK;
}

static const yvex_family_source_adapter *mamba_source(void)
{
    static const yvex_family_source_adapter adapter = {
        .schema_version = YVEX_FAMILY_SOURCE_ADAPTER_SCHEMA_V1,
        .target_id = YVEX_MAMBA2_TARGET, .family = "mamba2",
        .tokenizer_architecture = "mamba2", .tokenizer_pre = "sentencepiece",
        .compile = mamba_source_compile};
    return &adapter;
}

const yvex_family_descriptor yvex_graph_family_descriptor_mamba2 = {
    .schema_version = YVEX_FAMILY_DESCRIPTOR_SCHEMA_V1,
    .target_id = YVEX_MAMBA2_TARGET, .family = "mamba2",
    .tokenizer_architecture = "mamba2", .tokenizer_pre = "sentencepiece",
    .source = mamba_source};
