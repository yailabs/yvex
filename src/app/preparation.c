/* Application facade for authenticated preparation and immutable binding recovery.
 * Product parsing, phase selection, presentation and physical emission are callers' responsibilities. */
#define _POSIX_C_SOURCE 200809L
#include <yvex/internal/model_preparation.h>

#include <yvex/artifact.h>
#include <yvex/gguf.h>
#include <yvex/internal/artifact.h>
#include <yvex/internal/compilation.h>
#include <yvex/internal/compiler.h>
#include <yvex/internal/deployment.h>
#include <yvex/internal/family_catalog.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/runtime.h>
#include <yvex/internal/model_lifecycle.h>
#include <yvex/internal/source_distribution.h>
#include <yvex/internal/source_catalog.h>
#include <yvex/internal/source_payload.h>
#include <yvex/quant.h>

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

typedef yvex_model_preparation_request model_prepare_options;

typedef struct {
    yvex_operator_paths operator_paths;
    const yvex_model_library_entry *model;
    const yvex_local_source_record *source;
    const yvex_source_target_identity *source_identity;
    const yvex_graph_execution_binding *execution;
    const yvex_model_deployment_defaults *deployment;
    const char *preset;
    yvex_local_source_record recovered_source;
    yvex_quant_plan_file_summary sealed_plan;
    char registry_path[YVEX_PATH_CAP];
    char manifest_path[YVEX_PATH_CAP];
    char plan_path[YVEX_PATH_CAP];
    char quant_policy_path[YVEX_PATH_CAP];
    char artifact_path[YVEX_PATH_CAP];
    char binding_dir[YVEX_PATH_CAP];
    char binding_path[YVEX_PATH_CAP];
    char profile_alias[YVEX_MODEL_LIBRARY_NAME_CAP];
    int rebind_existing_artifact;
} model_prepare_plan;

static int prepare_path_join(char *out, size_t capacity, const char *directory,
                              const char *leaf, yvex_error *err, const char *where)
{
    int written;
    if (!out || !capacity || !directory || !leaf) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "path requires directory and leaf");
        return YVEX_ERR_INVALID_ARG;
    }
    written = snprintf(out, capacity, "%s/%s", directory, leaf);
    if (written < 0 || (size_t)written >= capacity) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "path exceeds bounded capacity");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

static int prepare_path_expand(const char *input, char *out, size_t capacity,
                                yvex_error *err, const char *where)
{
    const char *home;
    int written;
    if (!input || !input[0] || strchr(input, '\n')) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "path must be nonempty and single-line");
        return YVEX_ERR_INVALID_ARG;
    }
    home = input[0] == '~' && input[1] == '/' ? getenv("HOME") : NULL;
    if (input[0] == '~' && input[1] == '/' && (!home || !home[0])) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "home path is unavailable");
        return YVEX_ERR_INVALID_ARG;
    }
    written = home ? snprintf(out, capacity, "%s/%s", home, input + 2u)
                   : snprintf(out, capacity, "%s", input);
    if (written < 0 || (size_t)written >= capacity) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "expanded path exceeds bounded capacity");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

static const yvex_local_source_record *prepare_exact_source(
    const yvex_model_library *library, unsigned long long model_index,
    const yvex_source_target_identity **identity_out)
{
    const yvex_local_source_record *selected = NULL;
    unsigned long long index;

    *identity_out = NULL;
    for (index = 0u; index < yvex_model_library_source_count(library, model_index); ++index) {
        const yvex_local_source_record *source =
            yvex_model_library_source_at(library, model_index, index);
        const yvex_source_target_identity *identity = source
            ? yvex_source_target_identity_find_repository(source->repository) : NULL;

        if (!source || !identity || strcmp(source->revision, identity->upstream_revision) ||
            strcmp(source->provider, "huggingface") ||
            /* This producer consumes the checkpoint tensor container, not a
             * metadata-only acquisition of the same revision. Format selects
             * a candidate only; later payload verification still authenticates it. */
            strcmp(source->format, "safetensors") ||
            strcmp(source->acquisition_state, "source-acquired") || !source->path[0] ||
            access(source->path, R_OK | X_OK) != 0)
            continue;
        if (selected) return NULL;
        selected = source;
        *identity_out = identity;
    }
    return selected;
}

static int prepare_text_path(char out[YVEX_PATH_CAP], const char *directory,
                             const char *name, const char *where, yvex_error *err)
{
    return prepare_path_join(out, YVEX_PATH_CAP, directory, name, err, where);
}

static int prepare_alias(char out[YVEX_MODEL_LIBRARY_NAME_CAP],
                         const char *target, const char *preset, const char *backend)
{
    char source[YVEX_MODEL_LIBRARY_NAME_CAP * 2u];
    size_t input, output = 0u;
    int written = snprintf(source, sizeof(source), "%s-%s-%s", target, preset, backend);

    if (written < 0 || (size_t)written >= sizeof(source)) return 0;
    for (input = 0u; source[input] && output + 1u < YVEX_MODEL_LIBRARY_NAME_CAP; ++input) {
        unsigned char value = (unsigned char)source[input];
        char normalized = (char)(isalnum(value) ? tolower(value) : '-');

        if (normalized == '-' && (!output || out[output - 1u] == '-')) continue;
        out[output++] = normalized;
    }
    while (output && out[output - 1u] == '-') output--;
    out[output] = '\0';
    return output != 0u && source[input] == '\0';
}

static int prepare_parent_path(const char *path, char out[YVEX_PATH_CAP])
{
    const char *slash;
    size_t count;

    if (!path || !path[0] || !(slash = strrchr(path, '/'))) return 0;
    count = slash == path ? 1u : (size_t)(slash - path);
    if (count >= YVEX_PATH_CAP) return 0;
    memcpy(out, path, count);
    out[count] = '\0';
    return 1;
}

static int prepare_regular_path(char out[YVEX_PATH_CAP], const char *directory,
                                const char *leaf)
{
    struct stat status;
    yvex_error ignored;

    yvex_error_clear(&ignored);
    return leaf && leaf[0] && strcmp(leaf, ".") && strcmp(leaf, "..") &&
           prepare_path_join(out, YVEX_PATH_CAP, directory, leaf, &ignored,
                      "model.prepare.rebind") == YVEX_OK &&
           lstat(out, &status) == 0 && S_ISREG(status.st_mode) &&
           !S_ISLNK(status.st_mode) && access(out, R_OK) == 0;
}

static int prepare_plan_matches_artifact(
    const yvex_quant_plan_file_summary *plan,
    const yvex_complete_artifact_admission *admission)
{
    return plan && plan->complete && admission && admission->complete &&
           !strcmp(plan->profile_identity, admission->profile_identity) &&
           !strcmp(plan->physical_variant_identity, admission->profile_identity) &&
           !strcmp(plan->payload_plan_identity, admission->payload_plan_identity) &&
           !strcmp(plan->required_payload_identity, admission->payload_identity) &&
           !strcmp(plan->transform_identity, admission->transform_identity) &&
           plan->source_snapshot_identity == admission->source_snapshot_identity &&
           plan->mapping_identity == admission->mapping_identity &&
           plan->encoded_bytes == admission->payload_bytes;
}

static const char *prepare_plan_artifact_mismatch(
    const yvex_quant_plan_file_summary *plan,
    const yvex_complete_artifact_admission *admission)
{
    if (!plan || !plan->complete) return "physical-plan";
    if (!admission || !admission->complete) return "artifact-admission";
    if (strcmp(plan->profile_identity, admission->profile_identity))
        return "profile-identity";
    if (strcmp(plan->physical_variant_identity, admission->profile_identity))
        return "physical-variant-identity";
    if (strcmp(plan->payload_plan_identity, admission->payload_plan_identity))
        return "payload-plan-identity";
    if (strcmp(plan->required_payload_identity, admission->payload_identity))
        return "payload-identity";
    if (strcmp(plan->transform_identity, admission->transform_identity))
        return "transform-identity";
    if (plan->source_snapshot_identity != admission->source_snapshot_identity)
        return "source-snapshot-identity";
    if (plan->mapping_identity != admission->mapping_identity)
        return "mapping-identity";
    if (plan->encoded_bytes != admission->payload_bytes)
        return "encoded-bytes";
    return NULL;
}

static int prepare_plan_discover(
    const char *directory, const yvex_complete_artifact_admission *admission,
    yvex_quant_plan_file_summary *selected, char out[YVEX_PATH_CAP])
{
    DIR *stream = opendir(directory);
    struct dirent *entry;
    int found = 0;

    if (!stream) return 0;
    while ((entry = readdir(stream)) != NULL) {
        char candidate[YVEX_PATH_CAP];
        yvex_quant_plan_file_summary summary;
        yvex_error ignored;

        if (!prepare_regular_path(candidate, directory, entry->d_name)) continue;
        yvex_error_clear(&ignored);
        if (yvex_quant_plan_file_probe(candidate, &summary, &ignored) != YVEX_OK ||
            !prepare_plan_matches_artifact(&summary, admission))
            continue;
        if (!found || strcmp(candidate, out) < 0) {
            *selected = summary;
            (void)snprintf(out, YVEX_PATH_CAP, "%s", candidate);
        }
        found = 1;
    }
    (void)closedir(stream);
    return found;
}

static int prepare_policy_discover(const char *directory,
                                   const char *required_identity,
                                   char out[YVEX_PATH_CAP])
{
    DIR *stream = opendir(directory);
    struct dirent *entry;
    int found = 0;

    if (!stream || !yvex_sha256_hex_is_valid(required_identity)) {
        if (stream) (void)closedir(stream);
        return 0;
    }
    while ((entry = readdir(stream)) != NULL) {
        char candidate[YVEX_PATH_CAP];
        yvex_quant_policy *policy = NULL;
        yvex_quant_policy_summary summary = {0};
        yvex_error ignored;
        int rc;

        if (!prepare_regular_path(candidate, directory, entry->d_name)) continue;
        yvex_error_clear(&ignored);
        rc = yvex_quant_policy_open(&policy, candidate, &ignored);
        if (rc == YVEX_OK)
            rc = yvex_quant_policy_get_summary(policy, &summary, &ignored);
        if (rc == YVEX_OK && !strcmp(summary.policy_identity, required_identity) &&
            (!found || strcmp(candidate, out) < 0)) {
            (void)snprintf(out, YVEX_PATH_CAP, "%s", candidate);
            found = 1;
        }
        yvex_quant_policy_close(policy);
    }
    (void)closedir(stream);
    return found;
}

static int prepare_artifact_imatrix_matches(
    const yvex_gguf *gguf, const yvex_quant_plan_file_summary *plan)
{
    const yvex_gguf_value *value;
    const char *text = NULL;
    unsigned long long count = 0ull;
    size_t expected;

    if (!gguf || !plan) return 0;
    value = yvex_gguf_metadata_find(gguf, "yvex.quant.imatrix.identity");
    if (!strcmp(plan->imatrix_identity, "none"))
        return value == NULL ||
               (yvex_gguf_value_as_string(value, &text, &count) == YVEX_OK &&
                count == 4ull && !memcmp(text, "none", 4u));
    expected = strlen(plan->imatrix_identity);
    return value && yvex_gguf_value_as_string(value, &text, &count) == YVEX_OK &&
           count == expected && memcmp(text, plan->imatrix_identity, expected) == 0;
}

static int prepare_rebind_candidate(
    const yvex_model_artifact_fact *fact,
    const yvex_graph_execution_binding *execution,
    yvex_quant_plan_file_summary *sealed_plan,
    char plan_path[YVEX_PATH_CAP], char policy_path[YVEX_PATH_CAP])
{
    yvex_artifact_options options = {0};
    yvex_complete_artifact_admission admission = {0};
    yvex_artifact_admission_failure failure = {0};
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    char directory[YVEX_PATH_CAP];
    yvex_error ignored;
    int rc, accepted = 0;

    if (!fact || strcasecmp(fact->format, "gguf") || !execution ||
        !execution->compiler || !execution->compiler->binding_pipeline ||
        !prepare_parent_path(fact->path, directory))
        return 0;
    options.path = fact->path;
    options.readonly = 1;
    yvex_error_clear(&ignored);
    rc = yvex_artifact_open(&artifact, &options, &ignored);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&gguf, artifact, &ignored);
    if (rc == YVEX_OK)
        rc = execution->compiler->binding_pipeline->artifact_admit(
            artifact, &admission, &failure, &ignored);
    if (rc == YVEX_OK && !strcmp(admission.artifact_identity, fact->identity) &&
        prepare_plan_discover(directory, &admission, sealed_plan, plan_path) &&
        prepare_artifact_imatrix_matches(gguf, sealed_plan) &&
        prepare_policy_discover(directory, sealed_plan->policy_identity,
                                policy_path))
        accepted = 1;
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return accepted;
}

static int prepare_plan_paths(const model_prepare_options *options,
                              model_prepare_plan *plan, yvex_error *err)
{
    yvex_paths paths = {0};
    char reports_family[YVEX_PATH_CAP], artifacts_family[YVEX_PATH_CAP];
    char provenance_root[YVEX_PATH_CAP];
    char registry_family[YVEX_PATH_CAP], binding_leaf[YVEX_MODEL_LIBRARY_NAME_CAP + 16u];
    char file[YVEX_MODEL_LIBRARY_NAME_CAP + 32u];
    int rc;

    rc = yvex_operator_paths_resolve(&paths, options->models_root,
                                     &plan->operator_paths, err);
    if (rc == YVEX_OK)
        rc = prepare_text_path(artifacts_family, plan->operator_paths.gguf_root,
                               plan->execution->target_id,
                               "model.prepare.paths", err);
    if (rc == YVEX_OK)
        rc = prepare_text_path(registry_family, plan->operator_paths.registry_root,
                               plan->execution->operator_family_key,
                               "model.prepare.paths", err);
    if (rc == YVEX_OK)
        rc = prepare_text_path(provenance_root, plan->operator_paths.registry_root,
                               "provenance", "model.prepare.paths", err);
    if (rc == YVEX_OK)
        rc = prepare_text_path(reports_family, provenance_root,
                               plan->execution->operator_family_key,
                               "model.prepare.paths", err);
    snprintf(file, sizeof(file), "%s", plan->execution->source_manifest_filename);
    if (rc == YVEX_OK)
        rc = prepare_text_path(plan->manifest_path, reports_family, file,
                               "model.prepare.paths", err);
    if (rc == YVEX_OK)
        rc = prepare_text_path(reports_family, plan->operator_paths.models_root,
                               "tmp/prepare", "model.prepare.paths", err);
    snprintf(file, sizeof(file), "%s-%ld.quant-plan", plan->preset, (long)getpid());
    if (rc == YVEX_OK)
        rc = prepare_text_path(plan->plan_path, reports_family, file,
                               "model.prepare.paths", err);
    snprintf(file, sizeof(file), "<physical-variant-identity>/model.gguf");
    if (rc == YVEX_OK)
        rc = prepare_text_path(plan->artifact_path, artifacts_family, file,
                               "model.prepare.paths", err);
    snprintf(binding_leaf, sizeof(binding_leaf), "%s-bindings", plan->execution->target_id);
    if (rc == YVEX_OK)
        rc = prepare_text_path(plan->binding_dir, registry_family, binding_leaf,
                               "model.prepare.paths", err);
    if (rc == YVEX_OK && options->registry_path)
        rc = prepare_path_expand(options->registry_path, plan->registry_path,
                                  sizeof(plan->registry_path), err, "model.prepare.paths");
    else if (rc == YVEX_OK)
        rc = yvex_model_registry_default_path(plan->registry_path,
                                              sizeof(plan->registry_path), err);
    if (rc == YVEX_OK && !prepare_alias(plan->profile_alias, plan->execution->target_id,
                                        plan->preset, plan->deployment->backend)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "model.prepare.paths",
                       "derived deployment profile alias exceeds capacity");
        rc = YVEX_ERR_BOUNDS;
    }
    return rc;
}

static int prepare_rebind_source(const model_prepare_options *options,
                                 model_prepare_plan *plan, yvex_error *err)
{
    yvex_paths paths = {0};
    char family_root[YVEX_PATH_CAP], provenance_root[YVEX_PATH_CAP];
    int rc;

    plan->source_identity = yvex_source_target_identity_find(
        plan->execution->target_id);
    if (!plan->source_identity || !plan->execution->source_manifest_filename ||
        !plan->execution->source_manifest_filename[0]) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "model.prepare.rebind",
                       "target has no exact source identity for binding recovery");
        return YVEX_ERR_UNSUPPORTED;
    }
    rc = yvex_operator_paths_resolve(&paths, options->models_root,
                                     &plan->operator_paths, err);
    if (rc == YVEX_OK &&
        !yvex_source_target_path(plan->recovered_source.path,
                                 sizeof(plan->recovered_source.path),
                                 plan->operator_paths.models_root,
                                 plan->source_identity)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "model.prepare.rebind",
                       "exact source path exceeds the bounded operator contract");
        rc = YVEX_ERR_BOUNDS;
    }
    if (rc == YVEX_OK)
        rc = prepare_text_path(provenance_root, plan->operator_paths.registry_root,
                               "provenance", "model.prepare.rebind", err);
    if (rc == YVEX_OK)
        rc = prepare_text_path(family_root, provenance_root,
                               plan->execution->operator_family_key,
                               "model.prepare.rebind", err);
    if (rc == YVEX_OK)
        rc = prepare_text_path(plan->manifest_path, family_root,
                               plan->execution->source_manifest_filename,
                               "model.prepare.rebind", err);
    if (rc != YVEX_OK) return rc;
    if (access(plan->recovered_source.path, R_OK | X_OK) != 0 ||
        access(plan->manifest_path, R_OK) != 0) {
        yvex_error_set(err, YVEX_ERR_IO, "model.prepare.rebind",
                       "exact source or authenticated source manifest is unavailable");
        return YVEX_ERR_IO;
    }
    (void)snprintf(plan->recovered_source.name,
                   sizeof(plan->recovered_source.name), "%s",
                   plan->source_identity->model_name);
    (void)snprintf(plan->recovered_source.family,
                   sizeof(plan->recovered_source.family), "%s",
                   plan->source_identity->family_key);
    (void)snprintf(plan->recovered_source.provider,
                   sizeof(plan->recovered_source.provider), "%s", "huggingface");
    (void)snprintf(plan->recovered_source.repository,
                   sizeof(plan->recovered_source.repository), "%s",
                   plan->source_identity->upstream_repo_id);
    (void)snprintf(plan->recovered_source.revision,
                   sizeof(plan->recovered_source.revision), "%s",
                   plan->source_identity->upstream_revision);
    (void)snprintf(plan->recovered_source.acquisition_state,
                   sizeof(plan->recovered_source.acquisition_state), "%s",
                   "source-acquired");
    (void)snprintf(plan->recovered_source.verification_state,
                   sizeof(plan->recovered_source.verification_state), "%s",
                   "manifest-bound");
    (void)snprintf(plan->recovered_source.format,
                   sizeof(plan->recovered_source.format), "%s", "safetensors");
    plan->source = &plan->recovered_source;
    return YVEX_OK;
}

static int prepare_rebind_profile_alias(
    const yvex_model_library *library, unsigned long long model_index,
    model_prepare_plan *plan)
{
    unsigned long long index;

    for (index = 0ull;
         index < yvex_model_library_profile_count(library, model_index); ++index) {
        const yvex_model_runtime_profile_fact *profile =
            yvex_model_library_profile_at(library, model_index, index);
        if (profile && !strcmp(profile->artifact_path, plan->artifact_path) &&
            !strcmp(profile->runtime_target, plan->execution->target_id) &&
            !strcmp(profile->backend, plan->deployment->backend) &&
            !strcmp(profile->execution_strategy,
                    plan->deployment->execution_strategy))
            (void)snprintf(plan->profile_alias, sizeof(plan->profile_alias),
                           "%s", profile->alias);
    }
    return plan->profile_alias[0] ||
           prepare_alias(plan->profile_alias, plan->execution->target_id,
                         plan->sealed_plan.profile_name,
                         plan->deployment->backend);
}

static int prepare_rebind_plan_build(
    const model_prepare_options *options, const yvex_model_library *library,
    unsigned long long model_index, model_prepare_plan *plan, yvex_error *err)
{
    char registry_family[YVEX_PATH_CAP];
    char binding_leaf[YVEX_MODEL_LIBRARY_NAME_CAP + 16u];
    const char *selected_identity =
        plan->deployment->rebind_artifact_identity;
    unsigned long long index, candidate_count = 0ull;
    int rc = prepare_rebind_source(options, plan, err);

    for (index = 0ull; rc == YVEX_OK &&
                        index < yvex_model_library_artifact_count(library, model_index);
         ++index) {
        const yvex_model_artifact_fact *artifact =
            yvex_model_library_artifact_at(library, model_index, index);
        yvex_quant_plan_file_summary candidate_plan = {0};
        char candidate_plan_path[YVEX_PATH_CAP] = {0};
        char candidate_policy_path[YVEX_PATH_CAP] = {0};

        if (!prepare_rebind_candidate(artifact, plan->execution, &candidate_plan,
                                      candidate_plan_path,
                                      candidate_policy_path))
            continue;
        if (selected_identity && selected_identity[0] &&
            strcmp(artifact->identity, selected_identity))
            continue;
        candidate_count++;
        plan->sealed_plan = candidate_plan;
        (void)snprintf(plan->artifact_path, sizeof(plan->artifact_path), "%s",
                       artifact->path);
        (void)snprintf(plan->plan_path, sizeof(plan->plan_path), "%s",
                       candidate_plan_path);
        (void)snprintf(plan->quant_policy_path,
                       sizeof(plan->quant_policy_path), "%s",
                       candidate_policy_path);
    }
    if (rc != YVEX_OK) return rc;
    if (candidate_count != 1ull) {
        yvex_error_set(
            err, candidate_count ? YVEX_ERR_STATE : YVEX_ERR_UNSUPPORTED,
            "model.prepare.rebind",
            candidate_count
                ? "multiple immutable artifacts have sealed binding-recovery evidence"
                : selected_identity && selected_identity[0]
                      ? "selected immutable artifact lacks sealed binding-recovery evidence"
                      : "no immutable artifact has sealed binding-recovery evidence");
        return candidate_count ? YVEX_ERR_STATE : YVEX_ERR_UNSUPPORTED;
    }
    plan->preset = plan->sealed_plan.profile_name;
    plan->rebind_existing_artifact = 1;
    if (!prepare_rebind_profile_alias(library, model_index, plan)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "model.prepare.rebind",
                       "recovered deployment alias exceeds capacity");
        return YVEX_ERR_BOUNDS;
    }
    rc = prepare_text_path(registry_family, plan->operator_paths.registry_root,
                           plan->execution->operator_family_key,
                           "model.prepare.rebind", err);
    (void)snprintf(binding_leaf, sizeof(binding_leaf), "%s-bindings",
                   plan->execution->target_id);
    if (rc == YVEX_OK)
        rc = prepare_text_path(plan->binding_dir, registry_family, binding_leaf,
                               "model.prepare.rebind", err);
    if (rc == YVEX_OK && options->registry_path)
        rc = prepare_path_expand(options->registry_path, plan->registry_path,
                                  sizeof(plan->registry_path), err,
                                  "model.prepare.rebind");
    else if (rc == YVEX_OK)
        rc = yvex_model_registry_default_path(plan->registry_path,
                                              sizeof(plan->registry_path), err);
    return rc;
}

static int prepare_plan_build(const model_prepare_options *options,
                              const yvex_model_library *library,
                              unsigned long long model_index,
                              model_prepare_plan *plan, yvex_error *err)
{
    yvex_quant_policy *policy = NULL;
    yvex_quant_policy_summary summary;
    const char *target;
    unsigned long long artifact_index;
    int source_available, selected_rebind_present = 0;
    int rc;

    memset(plan, 0, sizeof(*plan));
    plan->model = yvex_model_library_at(library, model_index);
    plan->source = prepare_exact_source(library, model_index, &plan->source_identity);
    source_available = plan->source && plan->source_identity;
    target = plan->model && plan->model->runtime_target[0]
                 ? plan->model->runtime_target
                 : plan->source_identity ? plan->source_identity->target_id : NULL;
    plan->execution = target ? yvex_graph_execution_find(0u, 0u, target) : NULL;
    plan->deployment = plan->execution ? plan->execution->deployment_defaults : NULL;
    if (!plan->execution || !plan->execution->compiler || !plan->deployment) {
        if (target && plan->source && !options->dry_run) {
            yvex_family_source_products products = {0};
            yvex_compilation_runtime_binding_request request = {0};
            yvex_paths paths = {0};
            yvex_operator_paths operator_paths;
            char directory[YVEX_PATH_CAP], leaf[YVEX_REMOTE_NAME_CAP + 32u];
            rc = yvex_operator_paths_resolve(&paths, options->models_root, &operator_paths, err);
            if (rc == YVEX_OK) rc = prepare_text_path(directory, operator_paths.reports_root,
                plan->source->family, "model.prepare.source-inspection", err);
            snprintf(leaf, sizeof(leaf), "%s.source-manifest.json", plan->source->name);
            if (rc == YVEX_OK) rc = prepare_text_path(plan->manifest_path, directory, leaf,
                "model.prepare.source-inspection", err);
            if (rc != YVEX_OK) return rc;
            request.source_path = plan->source->path;
            request.models_root = operator_paths.models_root;
            request.source_manifest_path = plan->manifest_path;
            rc = yvex_family_source_compile(target, &request, &products, err);
            yvex_family_source_products_release(&products);
            if (rc != YVEX_OK) return rc;
        }
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "model.prepare",
                       "model has no exact acquired source-to-ready compiler binding");
        return YVEX_ERR_UNSUPPORTED;
    }
    for (artifact_index = 0ull;
         artifact_index < yvex_model_library_artifact_count(library, model_index);
         ++artifact_index) {
        const yvex_model_artifact_fact *artifact =
            yvex_model_library_artifact_at(library, model_index, artifact_index);
        if (artifact && artifact->path[0] &&
            access(artifact->path, R_OK) == 0 &&
            plan->deployment->rebind_artifact_identity &&
            !strcmp(artifact->identity,
                    plan->deployment->rebind_artifact_identity)) {
            selected_rebind_present = 1;
            break;
        }
    }
    if (!source_available ||
        (selected_rebind_present && !options->quant && !options->imatrix)) {
        if (options->quant || options->imatrix) {
            yvex_error_set(
                err, YVEX_ERR_UNSUPPORTED, "model.prepare",
                "historical artifact recovery does not accept quantization overrides");
            return YVEX_ERR_UNSUPPORTED;
        }
        return prepare_rebind_plan_build(options, library, model_index, plan, err);
    }
    plan->preset = options->quant ? options->quant : plan->deployment->quant_preset;
    memset(&summary, 0, sizeof(summary));
    rc = yvex_quant_policy_preset_open(&policy, plan->preset, err);
    if (rc == YVEX_OK) rc = yvex_quant_policy_get_summary(policy, &summary, err);
    yvex_quant_policy_close(policy);
    if (rc != YVEX_OK) return rc;
    if (summary.requires_imatrix_count && !options->imatrix) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "model.prepare",
                        "quant preset %s requires --imatrix FILE", plan->preset);
        return YVEX_ERR_INVALID_ARG;
    }
    return prepare_plan_paths(options, plan, err);
}

static int prepare_parents(const model_prepare_plan *plan, yvex_error *err)
{
    const char *paths[] = {plan->manifest_path, plan->plan_path,
                           plan->registry_path};
    char binding_anchor[YVEX_PATH_CAP];
    size_t index;
    int rc = YVEX_OK;

    for (index = 0u; rc == YVEX_OK && index < sizeof(paths) / sizeof(paths[0]); ++index)
        rc = yvex_core_mkdir_parent(paths[index], "model.prepare", err);
    if (rc == YVEX_OK)
        rc = prepare_text_path(binding_anchor, plan->binding_dir, ".binding",
                               "model.prepare", err);
    if (rc == YVEX_OK)
        rc = yvex_core_mkdir_parent(binding_anchor, "model.prepare", err);
    return rc;
}

static int prepare_source_verify(const model_prepare_plan *plan,
                                 yvex_source_payload_verification_result *result,
                                 yvex_error *err)
{
    yvex_source_manifest_options manifest = {0};
    yvex_source_manifest_summary summary = {0};
    yvex_source_verify_options verification = {0};
    yvex_source_payload_budget budget;
    yvex_source_payload_failure failure = {0};
    int rc;

    if (access(plan->manifest_path, F_OK) != 0) {
        if (plan->rebind_existing_artifact) {
            yvex_error_set(err, YVEX_ERR_IO, "model.prepare.rebind",
                           "authenticated historical source manifest is unavailable");
            return YVEX_ERR_IO;
        }
        manifest.repo = plan->source_identity->upstream_repo_id;
        manifest.revision = plan->source_identity->upstream_revision;
        manifest.local_path = plan->source->path;
        manifest.node_name = plan->source->name;
        manifest.download_command = "yvex model pull";
        manifest.status = YVEX_SOURCE_STATUS_IN_PROGRESS;
        manifest.include_files = 1;
        rc = yvex_source_manifest_write_json(plan->manifest_path, &manifest, &summary, err);
        if (rc != YVEX_OK) return rc;
    }
    verification.identity = plan->source_identity;
    verification.source_path = plan->source->path;
    verification.models_root = plan->operator_paths.models_root;
    verification.manifest_path = plan->manifest_path;
    verification.promote_manifest = 1;
    yvex_source_payload_budget_default(&budget);
    budget.allow_local_snapshot_seal = 0;
    memset(result, 0, sizeof(*result));
    rc = yvex_source_payload_verify_snapshot(&verification, &budget, result,
                                             &failure, err);
    if (rc == YVEX_OK && plan->rebind_existing_artifact &&
        (result->payload.source_snapshot_identity !=
             plan->sealed_plan.source_snapshot_identity ||
         strcmp(result->payload.payload_identity,
                plan->sealed_plan.required_payload_identity))) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "model.prepare.rebind",
                       "source manifest does not match sealed artifact creation identity");
        rc = YVEX_ERR_FORMAT;
    }
    return rc;
}

static int prepare_existing_artifact(const model_prepare_plan *plan, yvex_error *err)
{
    yvex_artifact_options options = {0};
    yvex_complete_artifact_admission admission = {0};
    yvex_artifact_admission_failure failure = {0};
    yvex_quant_plan_file_summary sealed = {0};
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    int rc = yvex_quant_plan_file_probe(plan->plan_path, &sealed, err);
    options.path = plan->artifact_path;
    options.readonly = 1;
    if (rc == YVEX_OK) rc = yvex_artifact_open(&artifact, &options, err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK)
        rc = plan->execution->compiler->binding_pipeline->artifact_admit(artifact, &admission, &failure, err);
    if (rc == YVEX_OK && (!prepare_plan_matches_artifact(&sealed, &admission) ||
                         !prepare_artifact_imatrix_matches(gguf, &sealed))) {
        const char *field = prepare_plan_artifact_mismatch(&sealed, &admission);
        yvex_error_setf(
            err, YVEX_ERR_FORMAT, "model.prepare",
            "existing output does not match the exact transformation plan: %s",
            field ? field : "imatrix-identity");
        rc = YVEX_ERR_FORMAT;
    }
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return rc;
}

static int prepare_store_plan(model_prepare_plan *plan, yvex_error *err)
{
    yvex_quant_plan_file_summary sealed = {0};
    char target_root[YVEX_PATH_CAP], representation[YVEX_PATH_CAP];
    char stored_plan[YVEX_PATH_CAP];
    int rc = yvex_quant_plan_file_probe(plan->plan_path, &sealed, err);

    if (rc != YVEX_OK) return rc;
    if (!sealed.complete || !yvex_sha256_hex_valid(sealed.payload_plan_identity) ||
        !yvex_sha256_hex_valid(sealed.physical_variant_identity)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "model.prepare.store",
                       "complete immutable payload and physical variant identities are required");
        return YVEX_ERR_FORMAT;
    }
    rc = prepare_text_path(target_root, plan->operator_paths.gguf_root,
                           plan->execution->target_id, "model.prepare.store", err);
    if (rc == YVEX_OK)
        rc = prepare_text_path(representation, target_root, sealed.physical_variant_identity,
                               "model.prepare.store", err);
    if (rc == YVEX_OK)
        rc = prepare_text_path(plan->artifact_path, representation, "model.gguf",
                               "model.prepare.store", err);
    if (rc == YVEX_OK)
        rc = prepare_text_path(stored_plan, representation, "physical.plan",
                               "model.prepare.store", err);
    if (rc == YVEX_OK)
        rc = yvex_core_mkdir_parent(stored_plan, "model.prepare.store", err);
    if (rc != YVEX_OK) return rc;
    if (access(stored_plan, F_OK) == 0) {
        yvex_quant_plan_file_summary existing = {0};
        rc = yvex_quant_plan_file_probe(stored_plan, &existing, err);
        if (rc != YVEX_OK) return rc;
        if (strcmp(existing.payload_plan_identity, sealed.payload_plan_identity) ||
            strcmp(existing.required_payload_identity, sealed.required_payload_identity) ||
            strcmp(existing.physical_variant_identity, sealed.physical_variant_identity)) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "model.prepare.store",
                           "stored representation conflicts with the sealed plan");
            return YVEX_ERR_FORMAT;
        }
    } else if (link(plan->plan_path, stored_plan) != 0) {
        yvex_error_set(err, YVEX_ERR_IO, "model.prepare.store",
                       "cannot publish immutable plan without replacement");
        return YVEX_ERR_IO;
    }
    if (unlink(plan->plan_path) != 0) {
        yvex_error_set(err, YVEX_ERR_IO, "model.prepare.store",
                       "cannot retire temporary preparation plan");
        return YVEX_ERR_IO;
    }
    snprintf(plan->plan_path, sizeof(plan->plan_path), "%s", stored_plan);
    return YVEX_OK;
}

static int prepare_binding(model_prepare_plan *plan,
                           const model_prepare_options *options,
                           int *published, yvex_error *err)
{
    yvex_compilation_runtime_binding_request request = {0};

    request.source_path = plan->source->path;
    request.models_root = plan->operator_paths.models_root;
    request.source_manifest_path = plan->manifest_path;
    request.artifact_path = plan->artifact_path;
    request.directory = plan->binding_dir;
    request.quant_policy_path = plan->rebind_existing_artifact
                                    ? plan->quant_policy_path : NULL;
    request.quant_preset_name = plan->rebind_existing_artifact
                                    ? NULL : plan->preset;
    request.imatrix_path = plan->rebind_existing_artifact ? NULL : options->imatrix;
    request.physical_variant_plan_path = plan->plan_path;
    request.family_adapter_id = plan->execution->adapter_id;
    request.family_adapter_version = plan->execution->adapter_version;
    request.rebind_existing_artifact = plan->rebind_existing_artifact;
    return yvex_runtime_binding_compile_publish(plan->execution->compiler, &request,
                                                plan->binding_path, published, err);
}

static int prepare_ready_verify(const model_prepare_options *options,
                                 const yvex_model_library *library,
                                 unsigned long long model_index, yvex_error *err)
{
    yvex_paths defaults;
    yvex_operator_paths paths;
    unsigned long long profile_index, artifact_index;
    int rc = yvex_paths_default(&defaults, err);
    if (rc == YVEX_OK) rc = yvex_operator_paths_resolve(&defaults, options->models_root, &paths, err);
    if (rc != YVEX_OK) return rc;
    for (profile_index = 0u; profile_index < yvex_model_library_profile_count(library, model_index); ++profile_index) {
        const yvex_model_runtime_profile_fact *profile =
            yvex_model_library_profile_at(library, model_index, profile_index);
        if (!profile->launchable) continue;
        for (artifact_index = 0u; artifact_index < yvex_model_library_artifact_count(library, model_index);
             ++artifact_index) {
            const yvex_model_artifact_fact *artifact =
                yvex_model_library_artifact_at(library, model_index, artifact_index);
            yvex_artifact_snapshot snapshot;
            if (strcmp(artifact->identity, profile->artifact_identity) ||
                strcmp(artifact->path, profile->artifact_path))
                continue;
            rc = yvex_model_artifact_local_verify(artifact, paths.models_root, &snapshot, err);
            if (rc == YVEX_OK) return rc;
        }
    }
    yvex_error_set(err, YVEX_ERR_STATE, "model.prepare", "ready profile lacks an unchanged verified local artifact");
    return YVEX_ERR_STATE;
}

static int prepare_cached_plan(model_prepare_plan *plan, const yvex_model_library *library,
                                unsigned long long model_index, yvex_error *err)
{
    yvex_quant_plan_file_summary sealed = {0};
    unsigned long long profile_index, artifact_index;
    if (yvex_quant_plan_file_probe(plan->plan_path, &sealed, err) != YVEX_OK) return 0;
    for (profile_index = 0u; profile_index < yvex_model_library_profile_count(library, model_index); ++profile_index) {
        const yvex_model_runtime_profile_fact *profile =
            yvex_model_library_profile_at(library, model_index, profile_index);
        if (!profile->launchable || strcmp(profile->artifact_path, plan->artifact_path) ||
            strcmp(profile->backend, plan->deployment->backend) ||
            strcmp(profile->runtime_target, plan->execution->target_id)) continue;
        for (artifact_index = 0u; artifact_index < yvex_model_library_artifact_count(library, model_index);
             ++artifact_index) {
            const yvex_model_artifact_fact *artifact =
                yvex_model_library_artifact_at(library, model_index, artifact_index);
            yvex_artifact_snapshot snapshot;
            if (strcmp(artifact->identity, profile->artifact_identity) ||
                strcmp(artifact->physical_variant, sealed.physical_variant_identity)) continue;
            if (yvex_model_artifact_local_verify(artifact, plan->operator_paths.models_root,
                                                  &snapshot, err) != YVEX_OK) return 0;
            snprintf(plan->binding_path, sizeof(plan->binding_path), "%s", profile->runtime_binding);
            snprintf(plan->profile_alias, sizeof(plan->profile_alias), "%s", profile->alias);
            return 1;
        }
    }
    return 0;
}

struct yvex_model_preparation {
    model_prepare_options options;
    model_prepare_plan plan;
    int verified;
};

void yvex_model_preparation_close(yvex_model_preparation *context)
{
    free(context);
}

int yvex_model_preparation_ready_verify(const yvex_model_library *library,
    unsigned long long index, const yvex_model_preparation_request *request, yvex_error *err)
{
    if (!library || !request || index >= yvex_model_library_count(library)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model.prepare", "valid catalog selection is required");
        return YVEX_ERR_INVALID_ARG;
    }
    return prepare_ready_verify(request, library, index, err);
}

int yvex_model_preparation_open(yvex_model_preparation **out, const yvex_model_library *library,
    unsigned long long index, const yvex_model_preparation_request *request, yvex_error *err)
{
    yvex_model_preparation *context;
    int rc;
    if (out) *out = NULL;
    if (!out || !library || !request || index >= yvex_model_library_count(library)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model.prepare", "valid catalog selection is required");
        return YVEX_ERR_INVALID_ARG;
    }
    context = calloc(1u, sizeof(*context));
    if (!context) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "model.prepare", "preparation context allocation failed");
        return YVEX_ERR_NOMEM;
    }
    context->options = *request;
    rc = prepare_plan_build(&context->options, library, index, &context->plan, err);
    if (rc != YVEX_OK) {
        yvex_model_preparation_close(context);
        return rc;
    }
    *out = context;
    return YVEX_OK;
}

int yvex_model_preparation_view_get(const yvex_model_preparation *context,
                                   yvex_model_preparation_view *out, yvex_error *err)
{
    const model_prepare_plan *plan;
    if (out) memset(out, 0, sizeof(*out));
    if (!context || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model.prepare", "context and view are required");
        return YVEX_ERR_INVALID_ARG;
    }
    plan = &context->plan;
    out->source = plan->source->path;
    out->revision = plan->source_identity->upstream_revision;
    out->target = plan->execution->target_id;
    out->quant = plan->preset;
    out->backend = plan->deployment->backend;
    out->strategy = plan->deployment->execution_strategy;
    out->family = plan->deployment->logical_family;
    out->model = plan->deployment->logical_model;
    out->models_root = plan->operator_paths.models_root;
    out->registry = plan->registry_path;
    out->manifest = plan->manifest_path;
    out->plan = plan->plan_path;
    out->artifact = plan->artifact_path;
    out->binding = plan->binding_path;
    out->profile = plan->profile_alias;
    out->rebind = plan->rebind_existing_artifact;
    return YVEX_OK;
}

static int preparation_mutable(yvex_model_preparation *context, yvex_error *err)
{
    if (!context || context->options.dry_run) {
        yvex_error_set(err, YVEX_ERR_STATE, "model.prepare", "dry or absent context cannot mutate preparation");
        return YVEX_ERR_STATE;
    }
    return YVEX_OK;
}

int yvex_model_preparation_verify(yvex_model_preparation *context, yvex_error *err)
{
    yvex_source_payload_verification_result result = {0};
    int rc = preparation_mutable(context, err);
    if (rc == YVEX_OK) rc = prepare_parents(&context->plan, err);
    if (rc == YVEX_OK) rc = prepare_source_verify(&context->plan, &result, err);
    if (context) context->verified = rc == YVEX_OK;
    return rc;
}

int yvex_model_preparation_store_plan(yvex_model_preparation *context, yvex_error *err)
{
    int rc = preparation_mutable(context, err);
    if (rc != YVEX_OK) return rc;
    if (!context->verified || context->plan.rebind_existing_artifact) {
        yvex_error_set(err, YVEX_ERR_STATE, "model.prepare.store", "verified new-source plan is required");
        return YVEX_ERR_STATE;
    }
    return prepare_store_plan(&context->plan, err);
}

int yvex_model_preparation_cached(yvex_model_preparation *context, const yvex_model_library *library,
                                 unsigned long long index, int *cached, yvex_error *err)
{
    if (cached) *cached = 0;
    if (!context || !context->verified || !library || !cached ||
        index >= yvex_model_library_count(library)) {
        yvex_error_set(err, YVEX_ERR_STATE, "model.prepare.cached", "verified context and catalog are required");
        return YVEX_ERR_STATE;
    }
    *cached = prepare_cached_plan(&context->plan, library, index, err);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_model_preparation_artifact_verify(yvex_model_preparation *context, yvex_error *err)
{
    if (!context || !context->verified) {
        yvex_error_set(err, YVEX_ERR_STATE, "model.prepare", "authenticated source is required");
        return YVEX_ERR_STATE;
    }
    return prepare_existing_artifact(&context->plan, err);
}

int yvex_model_preparation_binding_publish(yvex_model_preparation *context,
                                          int *published, yvex_error *err)
{
    int rc = preparation_mutable(context, err);
    if (published) *published = 0;
    if (rc != YVEX_OK) return rc;
    if (!published || !context->verified) {
        yvex_error_set(err, YVEX_ERR_STATE, "model.prepare.binding", "verified context and receipt are required");
        return YVEX_ERR_STATE;
    }
    rc = prepare_existing_artifact(&context->plan, err);
    return rc == YVEX_OK
        ? prepare_binding(&context->plan, &context->options, published, err) : rc;
}
