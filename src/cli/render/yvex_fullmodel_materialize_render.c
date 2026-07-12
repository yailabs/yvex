/*
 * yvex_fullmodel_materialize_surface.c - fullmodel materialize command surface.
 * Owner: src/cli/render
 * Owns: existing fullmodel materialization-plan/materialize diagnostic command behavior.
 * Does not own: artifact emission capability, runtime generation, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a; preserves existing refusal/report behavior.
 * Boundary: diagnostic materialization reports do not imply supported-family runtime generation.
 */
#include "yvex_fullmodel_surface.h"

static void fullmodel_print_largest(const yvex_fullmodel_largest_tensor top[16],
                                    unsigned int top_count)
{
    unsigned int i;

    for (i = 0; i < top_count; ++i) {
        char dims[128];
        const yvex_tensor_info *tensor = top[i].tensor;
        if (!tensor) continue;
        dims_to_text(tensor->dims, tensor->rank, dims, sizeof(dims));
        yvex_cli_out_writef(stdout, "largest_tensor_%u: name=%s dtype=%s role=%s dims=%s bytes=%llu\n",
               i,
               tensor->name ? tensor->name : "",
               yvex_dtype_name(tensor->dtype),
               yvex_tensor_role_name(tensor->role),
               dims,
               tensor->storage_bytes);
    }
}

const char *fullmodel_identity_status(const yvex_model_ref *ref,
                                             unsigned long long artifact_bytes)
{
    if (!ref || ref->kind != YVEX_MODEL_REF_ALIAS) return "not-checked";
    if (!ref->sha256 || !ref->sha256[0]) return "registered-without-digest";
    if (ref->registered_file_size != 0ull && ref->registered_file_size != artifact_bytes) {
        return "registered-size-drift";
    }
    return "registered-size-match";
}

int fullmodel_residency_is_future_unsupported(const char *residency)
{
    return residency &&
           (strcmp(residency, "ssd-streamed") == 0 ||
            strcmp(residency, "managed-memory") == 0 ||
            strcmp(residency, "distributed") == 0);
}

static const char *fullmodel_placement_for_residency(const char *backend,
                                                     const char *residency,
                                                     int present)
{
    if (!present) return "not-planned";
    if (fullmodel_residency_is_future_unsupported(residency)) return "unsupported";
    if (strcmp(residency, "host-staged") == 0) return "host-staged";
    if (strcmp(residency, "ssd-staged") == 0) return "ssd-staged";
    if (strcmp(residency, "hybrid") == 0) return "hybrid";
    return backend && strcmp(backend, "cuda") == 0 ? "cuda-resident" : "cpu-resident";
}

static const char *fullmodel_required_bool(int value)
{
    return value ? "true" : "false";
}

void fullmodel_probe_backend_fit(const char *backend,
                                        unsigned long long required_bytes,
                                        yvex_fullmodel_backend_fit *fit)
{
    yvex_backend *opened = NULL;
    yvex_backend_options options;
    yvex_backend_memory_stats stats;
    yvex_backend_device_info device_info;
    yvex_error err;
    int rc;

    if (!fit) return;
    memset(fit, 0, sizeof(*fit));
    fit->required_bytes = required_bytes;
    fit->fit_status = "unknown";
    snprintf(fit->fit_reason, sizeof(fit->fit_reason),
             "system memory availability is not queried");

    if (!backend || strcmp(backend, "cpu") == 0) {
        fit->available = 1;
        return;
    }

    memset(&options, 0, sizeof(options));
    memset(&stats, 0, sizeof(stats));
    memset(&device_info, 0, sizeof(device_info));
    yvex_error_clear(&err);
    options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&opened, &options, &err);
    if (rc != YVEX_OK) {
        fit->available = 0;
        fit->fit_status = "unsupported";
        snprintf(fit->fit_reason, sizeof(fit->fit_reason),
                 "CUDA backend unavailable: %s", yvex_error_message(&err));
        yvex_error_clear(&err);
        return;
    }

    fit->available = 1;
    rc = yvex_backend_get_memory_stats(opened, &stats, &err);
    if (rc == YVEX_OK &&
        yvex_backend_get_device_info(opened, &device_info, &err) == YVEX_OK &&
        device_info.total_memory_bytes > 0ull) {
        fit->memory_known = 1;
        fit->total_bytes = device_info.total_memory_bytes;
        fit->available_bytes = device_info.free_memory_bytes;
        if (required_bytes <= device_info.free_memory_bytes) {
            fit->fit_status = "fits";
            snprintf(fit->fit_reason, sizeof(fit->fit_reason),
                     "required bytes fit current CUDA free memory; no allocation attempted");
        } else {
            fit->fit_status = "does-not-fit";
            snprintf(fit->fit_reason, sizeof(fit->fit_reason),
                     "required bytes exceed current CUDA free memory; no allocation attempted");
        }
    } else {
        fit->fit_status = "unknown";
        snprintf(fit->fit_reason, sizeof(fit->fit_reason),
                 "CUDA memory info unavailable; no allocation attempted");
        yvex_error_clear(&err);
    }
    yvex_backend_close(opened);
}

int fullmodel_has_attention_collection(const yvex_fullmodel_collections *collections)
{
    return collections &&
           collections->has_attention_q &&
           collections->has_attention_k &&
           collections->has_attention_v &&
           collections->has_attention_out;
}

int fullmodel_has_mlp_collection(const yvex_fullmodel_collections *collections)
{
    return collections &&
           collections->has_ffn_gate &&
           collections->has_ffn_up &&
           collections->has_ffn_down;
}

int fullmodel_has_normalization_collection(const yvex_fullmodel_collections *collections)
{
    return collections &&
           (collections->has_attention_norm ||
            collections->has_post_attention_norm ||
            collections->has_output_norm);
}

static unsigned int fullmodel_plan_missing_collection_blockers(const yvex_fullmodel_collections *collections)
{
    unsigned int count = 0u;

    if (!collections) return 0u;
    if (!collections->has_token_embedding) count++;
    if (!fullmodel_has_normalization_collection(collections)) count++;
    if (!fullmodel_has_attention_collection(collections)) count++;
    if (!fullmodel_has_mlp_collection(collections)) count++;
    if (!collections->has_moe_router && !collections->has_moe_expert) count++;
    if (!collections->has_output_head) count++;
    if (!collections->has_tokenizer_metadata) count++;
    return count;
}

static unsigned int fullmodel_plan_blocker_count(const yvex_fullmodel_collections *collections,
                                                 int selected_target,
                                                 const char *residency,
                                                 const yvex_fullmodel_backend_fit *fit)
{
    unsigned int count = 7u; /* runtime-consumer plus six generation-boundary blockers. */

    count += fullmodel_plan_missing_collection_blockers(collections);
    if (selected_target) count++;
    if (fullmodel_residency_is_future_unsupported(residency)) count++;
    if (fit && !fit->available) count++;
    if (fit && strcmp(fit->fit_status ? fit->fit_status : "unknown", "does-not-fit") == 0) count++;
    return count;
}

static const char *fullmodel_plan_status(const yvex_fullmodel_collections *collections,
                                         int selected_target,
                                         const char *residency,
                                         const yvex_fullmodel_backend_fit *fit)
{
    if (fullmodel_residency_is_future_unsupported(residency)) return "unsupported";
    if (fit && !fit->available) return "partial";
    if (selected_target) return "partial";
    if (fullmodel_plan_missing_collection_blockers(collections) > 0u) return "partial";
    return "ready";
}

void fullmodel_print_phase(unsigned int index,
                                  const char *name,
                                  const char *status,
                                  unsigned long long tensor_count,
                                  unsigned long long tensor_bytes,
                                  const char *residency,
                                  int required,
                                  int blocked,
                                  const char *blocker)
{
    yvex_cli_out_writef(stdout, "phase.%u.name: %s\n", index, name ? name : "");
    yvex_cli_out_writef(stdout, "phase.%u.status: %s\n", index, status ? status : "planned");
    yvex_cli_out_writef(stdout, "phase.%u.tensor_count: %llu\n", index, tensor_count);
    yvex_cli_out_writef(stdout, "phase.%u.tensor_bytes: %llu\n", index, tensor_bytes);
    yvex_cli_out_writef(stdout, "phase.%u.residency: %s\n", index, residency ? residency : "not-applicable");
    yvex_cli_out_writef(stdout, "phase.%u.required: %s\n", index, required ? "true" : "false");
    yvex_cli_out_writef(stdout, "phase.%u.blocked: %s\n", index, blocked ? "true" : "false");
    yvex_cli_out_writef(stdout, "phase.%u.blocker: %s\n", index, blocker && blocker[0] ? blocker : "none");
}

static void fullmodel_print_collection_plan(const char *name,
                                            const char *status,
                                            unsigned long long tensor_count,
                                            unsigned long long tensor_bytes,
                                            int required_for_generation,
                                            int present,
                                            const char *placement,
                                            const char *phase,
                                            const char *runtime_consumer,
                                            const char *blocker)
{
    yvex_cli_out_writef(stdout, "collection.%s.status: %s\n", name, status ? status : "planned");
    yvex_cli_out_writef(stdout, "collection.%s.tensor_count: %llu\n", name, tensor_count);
    yvex_cli_out_writef(stdout, "collection.%s.tensor_bytes: %llu\n", name, tensor_bytes);
    yvex_cli_out_writef(stdout, "collection.%s.required_for_generation: %s\n",
           name, fullmodel_required_bool(required_for_generation));
    yvex_cli_out_writef(stdout, "collection.%s.present: %s\n", name, fullmodel_required_bool(present));
    yvex_cli_out_writef(stdout, "collection.%s.placement: %s\n", name, placement ? placement : "unknown");
    yvex_cli_out_writef(stdout, "collection.%s.materialization_phase: %s\n", name, phase ? phase : "collection-grouping");
    yvex_cli_out_writef(stdout, "collection.%s.runtime_consumer: %s\n", name, runtime_consumer ? runtime_consumer : "planned");
    yvex_cli_out_writef(stdout, "collection.%s.blocker: %s\n", name, blocker && blocker[0] ? blocker : "none");
}

unsigned int fullmodel_print_blocker(unsigned int index,
                                            const char *category,
                                            const char *severity,
                                            const char *message,
                                            int blocks_full_materialization,
                                            int blocks_generation)
{
    yvex_cli_out_writef(stdout, "blocker.%u.category: %s\n", index, category ? category : "runtime-consumer");
    yvex_cli_out_writef(stdout, "blocker.%u.severity: %s\n", index, severity ? severity : "warning");
    yvex_cli_out_writef(stdout, "blocker.%u.message: %s\n", index, message ? message : "");
    yvex_cli_out_writef(stdout, "blocker.%u.blocks_full_materialization: %s\n",
           index, blocks_full_materialization ? "true" : "false");
    yvex_cli_out_writef(stdout, "blocker.%u.blocks_generation: %s\n",
           index, blocks_generation ? "true" : "false");
    return index + 1u;
}

static void fullmodel_print_missing_collection_blockers(unsigned int *index,
                                                        const yvex_fullmodel_collections *collections)
{
    if (!index || !collections) return;
    if (!collections->has_token_embedding) {
        *index = fullmodel_print_blocker(*index, "role-coverage", "fatal",
                                         "embedding collection missing",
                                         1, 1);
    }
    if (!fullmodel_has_normalization_collection(collections)) {
        *index = fullmodel_print_blocker(*index, "role-coverage", "error",
                                         "normalization collection missing",
                                         1, 1);
    }
    if (!fullmodel_has_attention_collection(collections)) {
        *index = fullmodel_print_blocker(*index, "role-coverage", "fatal",
                                         "attention collection missing",
                                         1, 1);
    }
    if (!fullmodel_has_mlp_collection(collections)) {
        *index = fullmodel_print_blocker(*index, "role-coverage", "fatal",
                                         "MLP collection missing",
                                         1, 1);
    }
    if (!collections->has_moe_router && !collections->has_moe_expert) {
        *index = fullmodel_print_blocker(*index, "role-coverage", "warning",
                                         "MoE collection missing or not identified",
                                         1, 1);
    }
    if (!collections->has_output_head) {
        *index = fullmodel_print_blocker(*index, "role-coverage", "fatal",
                                         "output collection missing",
                                         1, 1);
    }
    if (!collections->has_tokenizer_metadata) {
        *index = fullmodel_print_blocker(*index, "tokenizer", "error",
                                         "tokenizer/full runtime metadata incomplete",
                                         1, 1);
    }
}

static void fullmodel_print_materialization_plan(const yvex_cli_fullmodel_options *options,
                                                 const yvex_model_ref *ref,
                                                 const char *target_id,
                                                 const char *target_class,
                                                 unsigned long long artifact_bytes,
                                                 yvex_arch arch,
                                                 unsigned long long tensor_count,
                                                 unsigned long long total_tensor_bytes,
                                                 const yvex_fullmodel_collections *collections,
                                                 const char *dtype_summary,
                                                 const char *role_coverage,
                                                 const char *missing_roles,
                                                 int selected_target)
{
    yvex_fullmodel_backend_fit fit;
    const char *plan_status;
    int materialization_plan_ready;
    unsigned int blocker_count;
    unsigned int blocker_index = 0u;
    const char *placement;
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *residency = options && options->residency ? options->residency : "resident";
    int backend_blocked;
    int future_residency;

    fullmodel_probe_backend_fit(backend, total_tensor_bytes, &fit);
    plan_status = fullmodel_plan_status(collections, selected_target, residency, &fit);
    future_residency = fullmodel_residency_is_future_unsupported(residency);
    materialization_plan_ready = !selected_target && !future_residency && tensor_count > 0ull;
    blocker_count = fullmodel_plan_blocker_count(collections, selected_target, residency, &fit);
    backend_blocked = !fit.available || strcmp(fit.fit_status, "does-not-fit") == 0;

    yvex_cli_out_writef(stdout, "fullmodel: materialization-plan\n");
    yvex_cli_out_writef(stdout, "status: fullmodel-materialization-plan\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target_id ? target_id : "path");
    yvex_cli_out_writef(stdout, "target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
    yvex_cli_out_writef(stdout, "artifact_exists: true\n");
    yvex_cli_out_writef(stdout, "artifact_bytes: %llu\n", artifact_bytes);
    yvex_cli_out_writef(stdout, "artifact_identity_status: %s\n", fullmodel_identity_status(ref, artifact_bytes));
    yvex_cli_out_writef(stdout, "tensor_inventory_status: pass\n");
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", tensor_count);
    yvex_cli_out_writef(stdout, "total_tensor_bytes: %llu\n", total_tensor_bytes);
    yvex_cli_out_writef(stdout, "architecture: %s\n", yvex_arch_name(arch));
    yvex_cli_out_writef(stdout, "family: %s\n", fullmodel_family_from_arch(arch));
    yvex_cli_out_writef(stdout, "qtype_summary: %s\n", dtype_summary ? dtype_summary : "none");
    yvex_cli_out_writef(stdout, "dtype_summary: %s\n", dtype_summary ? dtype_summary : "none");
    yvex_cli_out_writef(stdout, "required_role_coverage: %s\n", role_coverage ? role_coverage : "partial");
    yvex_cli_out_writef(stdout, "missing_required_roles: %s\n", missing_roles ? missing_roles : "unknown");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "residency: %s\n", residency);
    yvex_cli_out_writef(stdout, "plan_status: %s\n", plan_status);
    yvex_cli_out_writef(stdout, "materialization_plan_ready: %s\n", materialization_plan_ready ? "true" : "false");
    yvex_cli_out_writef(stdout, "materialization_attempted: false\n");
    yvex_cli_out_writef(stdout, "full_materialization_proof: false\n");
    yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(stdout, "plan_id: fullmodel-materialization:%s:%s:%s\n",
           target_id ? target_id : "path", backend, residency);
    yvex_cli_out_writef(stdout, "plan_kind: full-model-materialization\n");
    yvex_cli_out_writef(stdout, "plan_source: tensor-inventory\n");
    yvex_cli_out_writef(stdout, "plan_backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "plan_residency: %s\n", residency);
    yvex_cli_out_writef(stdout, "plan_tensor_count: %llu\n", tensor_count);
    yvex_cli_out_writef(stdout, "plan_tensor_bytes: %llu\n", total_tensor_bytes);
    yvex_cli_out_writef(stdout, "plan_collection_count: 10\n");
    yvex_cli_out_writef(stdout, "plan_phase_count: 11\n");
    yvex_cli_out_writef(stdout, "plan_blocker_count: %u\n", blocker_count);
    yvex_cli_out_writef(stdout, "plan_cleanup_required: true\n");
    yvex_cli_out_writef(stdout, "plan_cleanup_phases: release-backend-buffers,release-host-staging,release-scratch,clear-partial-residency,preserve-failure-report\n");

    yvex_cli_out_writef(stdout, "backend_available: %s\n", fit.available ? "true" : "false");
    yvex_cli_out_writef(stdout, "backend_memory_known: %s\n", fit.memory_known ? "true" : "false");
    if (fit.memory_known) {
        yvex_cli_out_writef(stdout, "backend_memory_total_bytes: %llu\n", fit.total_bytes);
        yvex_cli_out_writef(stdout, "backend_memory_available_bytes: %llu\n", fit.available_bytes);
    } else {
        yvex_cli_out_writef(stdout, "backend_memory_total_bytes: unknown\n");
        yvex_cli_out_writef(stdout, "backend_memory_available_bytes: unknown\n");
    }
    yvex_cli_out_writef(stdout, "backend_required_bytes: %llu\n", fit.required_bytes);
    yvex_cli_out_writef(stdout, "backend_fit_status: %s\n", fit.fit_status);
    yvex_cli_out_writef(stdout, "backend_fit_reason: %s\n", fit.fit_reason);
    yvex_cli_out_writef(stdout, "backend_allocation_attempted: false\n");

    fullmodel_print_phase(0u, "preflight", future_residency ? "unsupported" : "ready",
                          tensor_count, total_tensor_bytes, residency, 1,
                          future_residency, future_residency ? "residency mode is not implemented" : "none");
    fullmodel_print_phase(1u, "artifact-identity", "ready",
                          0ull, artifact_bytes, "host", 1, 0, "none");
    fullmodel_print_phase(2u, "tensor-directory", "ready",
                          tensor_count, total_tensor_bytes, "host", 1, 0, "none");
    fullmodel_print_phase(3u, "tensor-range-validation", "ready",
                          tensor_count, total_tensor_bytes, "host", 1, 0, "none");
    fullmodel_print_phase(4u, "collection-grouping",
                          selected_target || strcmp(role_coverage ? role_coverage : "partial", "observed") != 0
                              ? "partial"
                              : "ready",
                          tensor_count, total_tensor_bytes, "host", 1,
                          selected_target,
                          selected_target ? "selected artifacts cannot satisfy full materialization" : "none");
    fullmodel_print_phase(5u, "backend-capability",
                          !fit.available ? "blocked" : "ready",
                          0ull, 0ull, backend, 1, !fit.available,
                          fit.available ? "none" : fit.fit_reason);
    fullmodel_print_phase(6u, "host-residency", "planned",
                          tensor_count, total_tensor_bytes, "host-staged", 1, 0, "none");
    fullmodel_print_phase(7u, "backend-residency",
                          backend_blocked ? "blocked" : "planned",
                          tensor_count, total_tensor_bytes,
                          fullmodel_placement_for_residency(backend, residency, 1),
                          1, backend_blocked,
                          backend_blocked ? fit.fit_reason : "none");
    fullmodel_print_phase(8u, "kv-residency", "unsupported",
                          0ull, 0ull, "not-planned", 1, 1,
                          "real attention-backed KV not implemented");
    fullmodel_print_phase(9u, "scratch-residency", "planned",
                          0ull, 0ull, "host-staged", 1, 0,
                          "scratch sizing remains planned");
    fullmodel_print_phase(10u, "cleanup", "planned",
                          0ull, 0ull, "not-applicable", 1, 0, "none");

    placement = fullmodel_placement_for_residency(backend, residency,
                                                  collections && collections->embedding > 0ull);
    fullmodel_print_collection_plan("embedding",
                                    collections && collections->embedding > 0ull ? "planned" : "blocked",
                                    collections ? collections->embedding : 0ull,
                                    collections ? collections->embedding_bytes : 0ull,
                                    1, collections && collections->embedding > 0ull,
                                    placement, "backend-residency", "planned",
                                    collections && collections->embedding > 0ull ? "none" : "embedding collection missing");
    placement = fullmodel_placement_for_residency(backend, residency,
                                                  collections && collections->normalization > 0ull);
    fullmodel_print_collection_plan("normalization",
                                    collections && collections->normalization > 0ull ? "planned" : "blocked",
                                    collections ? collections->normalization : 0ull,
                                    collections ? collections->normalization_bytes : 0ull,
                                    1, collections && collections->normalization > 0ull,
                                    placement, "backend-residency", "planned",
                                    collections && collections->normalization > 0ull ? "none" : "normalization collection missing");
    placement = fullmodel_placement_for_residency(backend, residency,
                                                  collections && collections->attention > 0ull);
    fullmodel_print_collection_plan("attention",
                                    fullmodel_has_attention_collection(collections) ? "planned" : "blocked",
                                    collections ? collections->attention : 0ull,
                                    collections ? collections->attention_bytes : 0ull,
                                    1, collections && collections->attention > 0ull,
                                    placement, "backend-residency", "planned",
                                    fullmodel_has_attention_collection(collections) ? "none" : "attention collection missing");
    placement = fullmodel_placement_for_residency(backend, residency,
                                                  collections && collections->mlp > 0ull);
    fullmodel_print_collection_plan("mlp",
                                    fullmodel_has_mlp_collection(collections) ? "planned" : "blocked",
                                    collections ? collections->mlp : 0ull,
                                    collections ? collections->mlp_bytes : 0ull,
                                    1, collections && collections->mlp > 0ull,
                                    placement, "backend-residency", "planned",
                                    fullmodel_has_mlp_collection(collections) ? "none" : "MLP collection missing");
    placement = fullmodel_placement_for_residency(backend, residency,
                                                  collections && collections->moe > 0ull);
    fullmodel_print_collection_plan("moe",
                                    collections && collections->moe > 0ull ? "planned" : "blocked",
                                    collections ? collections->moe : 0ull,
                                    collections ? collections->moe_bytes : 0ull,
                                    1, collections && collections->moe > 0ull,
                                    placement, "backend-residency", "planned",
                                    collections && collections->moe > 0ull ? "none" : "MoE collection missing or not identified");
    placement = fullmodel_placement_for_residency(backend, residency,
                                                  collections && collections->output > 0ull);
    fullmodel_print_collection_plan("output",
                                    collections && collections->output > 0ull ? "planned" : "blocked",
                                    collections ? collections->output : 0ull,
                                    collections ? collections->output_bytes : 0ull,
                                    1, collections && collections->output > 0ull,
                                    placement, "backend-residency", "planned",
                                    collections && collections->output > 0ull ? "none" : "output collection missing");
    fullmodel_print_collection_plan("tokenizer-runtime-input",
                                    collections && collections->tokenizer > 0ull ? "planned" : "blocked",
                                    collections ? collections->tokenizer : 0ull,
                                    collections ? collections->tokenizer_bytes : 0ull,
                                    1, collections && collections->tokenizer > 0ull,
                                    collections && collections->tokenizer > 0ull ? "host-staged" : "not-planned",
                                    "preflight", "planned",
                                    collections && collections->tokenizer > 0ull ? "none" : "tokenizer/full runtime metadata incomplete");
    fullmodel_print_collection_plan("kv-cache-runtime", "unsupported",
                                    0ull, 0ull, 1, 0, "not-planned",
                                    "kv-residency", "unsupported",
                                    "real attention-backed KV not implemented");
    fullmodel_print_collection_plan("scratch-runtime", "planned",
                                    0ull, 0ull, 1, 0, "host-staged",
                                    "scratch-residency", "planned",
                                    "scratch sizing remains planned");
    fullmodel_print_collection_plan("unknown",
                                    collections && collections->unknown > 0ull ? "partial" : "not-applicable",
                                    collections ? collections->unknown : 0ull,
                                    collections ? collections->unknown_bytes : 0ull,
                                    0, collections && collections->unknown > 0ull,
                                    collections && collections->unknown > 0ull ? "unknown" : "not-planned",
                                    "collection-grouping", "unsupported",
                                    collections && collections->unknown > 0ull ? "unknown tensor role" : "none");

    if (selected_target) {
        blocker_index = fullmodel_print_blocker(blocker_index, "artifact", "fatal",
                                                "selected artifacts cannot satisfy full materialization",
                                                1, 1);
    }
    if (future_residency) {
        blocker_index = fullmodel_print_blocker(blocker_index, "backend-capability", "error",
                                                "requested residency mode is planned but unsupported",
                                                1, 1);
    }
    if (!fit.available) {
        blocker_index = fullmodel_print_blocker(blocker_index, "backend-capability", "error",
                                                fit.fit_reason, 1, 0);
    } else if (strcmp(fit.fit_status ? fit.fit_status : "unknown", "does-not-fit") == 0) {
        blocker_index = fullmodel_print_blocker(blocker_index, "backend-memory", "error",
                                                fit.fit_reason, 1, 0);
    }
    fullmodel_print_missing_collection_blockers(&blocker_index, collections);
    blocker_index = fullmodel_print_blocker(blocker_index, "runtime-consumer", "fatal",
                                            "full collection runtime consumers are not implemented",
                                            0, 1);
    blocker_index = fullmodel_print_blocker(blocker_index, "generation-boundary", "fatal",
                                            "real transformer prefill not implemented",
                                            0, 1);
    blocker_index = fullmodel_print_blocker(blocker_index, "generation-boundary", "fatal",
                                            "real attention-backed KV not implemented",
                                            0, 1);
    blocker_index = fullmodel_print_blocker(blocker_index, "generation-boundary", "fatal",
                                            "real DeepSeek decode not implemented",
                                            0, 1);
    blocker_index = fullmodel_print_blocker(blocker_index, "generation-boundary", "fatal",
                                            "real output-head logits not implemented",
                                            0, 1);
    blocker_index = fullmodel_print_blocker(blocker_index, "generation-boundary", "fatal",
                                            "real vocabulary sampling not implemented",
                                            0, 1);
    blocker_index = fullmodel_print_blocker(blocker_index, "runtime-family", "fatal",
                                            "runtime family adapter not implemented",
                                            0, 1);

    yvex_cli_out_writef(stdout, "cleanup_plan_required: true\n");
    yvex_cli_out_writef(stdout, "cleanup_plan_phases: release-backend-buffers,release-host-staging,release-scratch,clear-partial-residency,preserve-failure-report\n");
    yvex_cli_out_writef(stdout, "cleanup_idempotent_required: true\n");
    yvex_cli_out_writef(stdout, "cleanup_failure_policy: preserve-failure-report\n");
    yvex_cli_out_writef(stdout, "next_required_row: FULLMODEL.2\n");
    yvex_cli_out_writef(stdout, "proof_ready_for_fullmodel_2: false\n");
    yvex_cli_out_writef(stdout, "fullmodel_2_blockers: %s\n",
           selected_target
               ? "full tensor set missing; full materialization executor not implemented; cleanup proof not implemented"
               : "full materialization executor not implemented; cleanup proof not implemented; runtime descriptor not implemented");
}

static int fullmodel_tensor_is_materialize_required(const yvex_tensor_info *tensor)
{
    const char *name;

    if (!tensor) return 0;
    name = tensor->name ? tensor->name : "";
    switch (tensor->role) {
    case YVEX_TENSOR_ROLE_TOKEN_EMBEDDING:
    case YVEX_TENSOR_ROLE_OUTPUT_NORM:
    case YVEX_TENSOR_ROLE_OUTPUT_HEAD:
    case YVEX_TENSOR_ROLE_ATTENTION_NORM:
    case YVEX_TENSOR_ROLE_ATTENTION_Q:
    case YVEX_TENSOR_ROLE_ATTENTION_K:
    case YVEX_TENSOR_ROLE_ATTENTION_V:
    case YVEX_TENSOR_ROLE_ATTENTION_OUT:
    case YVEX_TENSOR_ROLE_FFN_NORM:
    case YVEX_TENSOR_ROLE_FFN_GATE:
    case YVEX_TENSOR_ROLE_FFN_UP:
    case YVEX_TENSOR_ROLE_FFN_DOWN:
        return 1;
    default:
        break;
    }
    return fullmodel_name_has(name, "token_embd") ||
           fullmodel_name_has(name, "attn_norm") ||
           fullmodel_name_has(name, "ffn_norm") ||
           fullmodel_name_has(name, "attn_q") ||
           fullmodel_name_has(name, "attn_k") ||
           fullmodel_name_has(name, "attn_v") ||
           fullmodel_name_has(name, "attn_output") ||
           fullmodel_name_has(name, "q_proj") ||
           fullmodel_name_has(name, "k_proj") ||
           fullmodel_name_has(name, "v_proj") ||
           fullmodel_name_has(name, "o_proj") ||
           fullmodel_name_has(name, "ffn_gate") ||
           fullmodel_name_has(name, "ffn_up") ||
           fullmodel_name_has(name, "ffn_down") ||
           fullmodel_name_has(name, "gate_proj") ||
           fullmodel_name_has(name, "up_proj") ||
           fullmodel_name_has(name, "down_proj") ||
           fullmodel_name_has(name, "output_norm") ||
           strcmp(name, "output.weight") == 0 ||
           fullmodel_name_has(name, "lm_head");
}

static int fullmodel_role_present(const yvex_fullmodel_collections *collections,
                                  const char *role)
{
    if (!collections || !role) return 0;
    if (strcmp(role, "token-embedding") == 0) return collections->has_token_embedding;
    if (strcmp(role, "attention-norm") == 0) return collections->has_attention_norm;
    if (strcmp(role, "post-attention-norm") == 0) return collections->has_post_attention_norm;
    if (strcmp(role, "attention-q-projection") == 0) return collections->has_attention_q;
    if (strcmp(role, "attention-k-projection") == 0) return collections->has_attention_k;
    if (strcmp(role, "attention-v-projection") == 0) return collections->has_attention_v;
    if (strcmp(role, "attention-output-projection") == 0) return collections->has_attention_out;
    if (strcmp(role, "mlp-gate") == 0) return collections->has_ffn_gate;
    if (strcmp(role, "mlp-up") == 0) return collections->has_ffn_up;
    if (strcmp(role, "mlp-down") == 0) return collections->has_ffn_down;
    if (strcmp(role, "moe-router") == 0) return collections->has_moe_router;
    if (strcmp(role, "moe-experts") == 0) return collections->has_moe_expert;
    if (strcmp(role, "final-norm") == 0) return collections->has_output_norm;
    if (strcmp(role, "output-head") == 0) return collections->has_output_head;
    if (strcmp(role, "tokenizer-metadata") == 0) return collections->has_tokenizer_metadata;
    return 0;
}

static int fullmodel_collection_present_by_name(const yvex_fullmodel_collections *collections,
                                                const char *collection)
{
    if (!collections || !collection) return 0;
    if (strcmp(collection, "embedding") == 0) return collections->embedding > 0ull;
    if (strcmp(collection, "normalization") == 0) return collections->normalization > 0ull;
    if (strcmp(collection, "attention") == 0) return fullmodel_has_attention_collection(collections);
    if (strcmp(collection, "mlp") == 0) return fullmodel_has_mlp_collection(collections);
    if (strcmp(collection, "moe") == 0) return collections->moe > 0ull;
    if (strcmp(collection, "output") == 0) return collections->output > 0ull;
    if (strcmp(collection, "tokenizer") == 0 ||
        strcmp(collection, "tokenizer-runtime-input") == 0) {
        return collections->has_tokenizer_metadata;
    }
    return 0;
}

static void fullmodel_materialize_missing_roles(const yvex_cli_fullmodel_options *options,
                                                const yvex_fullmodel_collections *collections,
                                                char *out,
                                                size_t out_cap)
{
    if (!out || out_cap == 0u) return;
    out[0] = '\0';
    if (!fullmodel_role_present(collections, "token-embedding")) fullmodel_csv_append(out, out_cap, "token-embedding");
    if (!fullmodel_role_present(collections, "attention-norm")) fullmodel_csv_append(out, out_cap, "attention-norm");
    if (!fullmodel_role_present(collections, "post-attention-norm")) fullmodel_csv_append(out, out_cap, "post-attention-norm");
    if (!fullmodel_role_present(collections, "attention-q-projection")) fullmodel_csv_append(out, out_cap, "attention-q-projection");
    if (!fullmodel_role_present(collections, "attention-k-projection")) fullmodel_csv_append(out, out_cap, "attention-k-projection");
    if (!fullmodel_role_present(collections, "attention-v-projection")) fullmodel_csv_append(out, out_cap, "attention-v-projection");
    if (!fullmodel_role_present(collections, "attention-output-projection")) fullmodel_csv_append(out, out_cap, "attention-output-projection");
    if (!fullmodel_role_present(collections, "mlp-gate")) fullmodel_csv_append(out, out_cap, "mlp-gate");
    if (!fullmodel_role_present(collections, "mlp-up")) fullmodel_csv_append(out, out_cap, "mlp-up");
    if (!fullmodel_role_present(collections, "mlp-down")) fullmodel_csv_append(out, out_cap, "mlp-down");
    if (!fullmodel_role_present(collections, "final-norm")) fullmodel_csv_append(out, out_cap, "final-norm");
    if (!fullmodel_role_present(collections, "output-head")) fullmodel_csv_append(out, out_cap, "output-head");
    if (!fullmodel_role_present(collections, "tokenizer-metadata")) fullmodel_csv_append(out, out_cap, "tokenizer-metadata");
    if (options && options->require_role &&
        !fullmodel_role_present(collections, options->require_role)) {
        fullmodel_csv_append(out, out_cap, options->require_role);
    }
    if (options && options->require_collection &&
        !fullmodel_collection_present_by_name(collections, options->require_collection)) {
        char tmp[160];
        snprintf(tmp, sizeof(tmp), "collection:%s", options->require_collection);
        fullmodel_csv_append(out, out_cap, tmp);
    }
    if (!out[0]) snprintf(out, out_cap, "none");
}

static int fullmodel_fail_after(const yvex_cli_fullmodel_options *options,
                                const char *phase)
{
    return options && options->fail_after_phase && phase &&
           strcmp(options->fail_after_phase, phase) == 0;
}

static int fullmodel_open_requested_backend(const char *backend_name,
                                            yvex_backend **out,
                                            yvex_error *err)
{
    yvex_backend_options options;

    if (!out) return YVEX_ERR_INVALID_ARG;
    *out = NULL;
    if (!backend_name || strcmp(backend_name, "cpu") == 0) {
        return yvex_backend_open_cpu(out, err);
    }
    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CUDA;
    return yvex_backend_open(out, &options, err);
}

static int fullmodel_allocate_required_tensors(const yvex_cli_fullmodel_options *options,
                                               yvex_cli_tokenizer_context *ctx,
                                               unsigned long long *materialized_count,
                                               unsigned long long *materialized_bytes,
                                               const char **failed_phase,
                                               const char **failed_reason)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor **allocated = NULL;
    yvex_error err;
    unsigned long long tensor_count;
    unsigned long long i;
    unsigned long long allocated_count = 0ull;
    int rc;

    if (materialized_count) *materialized_count = 0ull;
    if (materialized_bytes) *materialized_bytes = 0ull;
    if (failed_phase) *failed_phase = "none";
    if (failed_reason) *failed_reason = "none";
    if (!options || !ctx || !ctx->table) return YVEX_ERR_INVALID_ARG;

    tensor_count = yvex_tensor_table_count(ctx->table);
    allocated = (yvex_device_tensor **)calloc((size_t)(tensor_count ? tensor_count : 1ull),
                                              sizeof(*allocated));
    if (!allocated) {
        if (failed_phase) *failed_phase = "backend-preflight";
        if (failed_reason) *failed_reason = "allocation-list";
        return YVEX_ERR_NOMEM;
    }

    yvex_error_clear(&err);
    rc = fullmodel_open_requested_backend(options->backend, &backend, &err);
    if (rc != YVEX_OK) {
        if (failed_phase) *failed_phase = "backend-preflight";
        if (failed_reason) *failed_reason = "backend-open-failed";
        yvex_error_clear(&err);
        free(allocated);
        return rc;
    }

    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx->table, i);
        yvex_backend_tensor_desc desc;
        yvex_device_tensor *device_tensor = NULL;
        unsigned int d;

        if (!fullmodel_tensor_is_materialize_required(tensor)) continue;
        memset(&desc, 0, sizeof(desc));
        desc.name = tensor->name;
        desc.dtype = tensor->dtype;
        desc.rank = tensor->rank;
        desc.bytes = tensor->storage_bytes;
        for (d = 0; d < tensor->rank && d < YVEX_TENSOR_MAX_DIMS; ++d) {
            desc.dims[d] = tensor->dims[d];
        }
        rc = yvex_backend_tensor_alloc(backend, &desc, &device_tensor, &err);
        if (rc != YVEX_OK) {
            if (failed_phase) *failed_phase = "backend-preflight";
            if (failed_reason) *failed_reason = "tensor-allocation-failed";
            yvex_error_clear(&err);
            break;
        }
        allocated[allocated_count++] = device_tensor;
        if (materialized_count) (*materialized_count)++;
        if (materialized_bytes) *materialized_bytes += tensor->storage_bytes;
    }

    while (allocated_count > 0ull) {
        allocated_count--;
        yvex_backend_tensor_free(backend, allocated[allocated_count]);
    }
    yvex_backend_close(backend);
    free(allocated);
    return rc;
}

static int fullmodel_materialize_command_run(const yvex_cli_fullmodel_options *options,
                                             yvex_model_ref *ref,
                                             yvex_cli_tokenizer_context *ctx,
                                             const char *target_id,
                                             const char *target_class,
                                             unsigned long long artifact_bytes,
                                             unsigned long long tensor_count,
                                             unsigned long long total_tensor_bytes,
                                             const yvex_fullmodel_collections *collections,
                                             int selected_target)
{
    static const unsigned long long proof_byte_limit = 64ull * 1024ull * 1024ull;
    yvex_fullmodel_materialize_report report;
    char materialize_missing_roles[768];
    const char *unsupported =
        "runtime-family-adapter,real-transformer-prefill,real-attention-backed-KV,real-DeepSeek-decode,real-output-head-logits,real-vocabulary-sampling";
    unsigned long long required_tensor_count = 0ull;
    unsigned long long required_tensor_bytes = 0ull;
    unsigned long long materialized_count = 0ull;
    unsigned long long materialized_bytes = 0ull;
    unsigned long long i;
    const char *alloc_failed_phase = "none";
    const char *alloc_failed_reason = "none";
    int role_complete;
    int rc;

    (void)total_tensor_bytes;
    memset(&report, 0, sizeof(report));
    memset(materialize_missing_roles, 0, sizeof(materialize_missing_roles));
    fullmodel_materialize_missing_roles(options,
                                        collections,
                                        materialize_missing_roles,
                                        sizeof(materialize_missing_roles));
    role_complete = strcmp(materialize_missing_roles, "none") == 0;

    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx->table, i);
        if (!fullmodel_tensor_is_materialize_required(tensor)) continue;
        required_tensor_count++;
        required_tensor_bytes += tensor->storage_bytes;
    }

    report.options = options;
    report.status = "fullmodel-materialize-fail";
    report.model_resolved_path = ref && ref->path ? ref->path : "";
    report.target_id = target_id;
    report.target_class = target_class;
    report.artifact_identity_status = fullmodel_identity_status(ref, artifact_bytes);
    report.tensor_inventory_status = "pass";
    report.required_role_coverage = selected_target ? "partial" : (role_complete ? "complete" : "partial");
    report.missing_required_roles = selected_target ? materialize_missing_roles : materialize_missing_roles;
    report.unsupported_required_roles = unsupported;
    report.placement_plan_status = "pass";
    report.memory_budget_status = "pass";
    report.backend_preflight_status = "pass";
    report.materialization_mode = "controlled-fullmodel-proof";
    report.full_model_materialization = "controlled-tiny-proof";
    report.full_model_materialization_proof = "fail";
    report.phase = "failed";
    report.failed_phase = "none";
    report.failed_reason = "none";
    report.cleanup_attempted = "true";
    report.cleanup_status = "pass";
    report.cleanup_idempotent = "true";
    report.owned_state_released = "true";
    report.partial_materialization = "false";
    report.required_tensor_count = required_tensor_count;
    report.required_tensor_bytes = required_tensor_bytes;
    report.peak_planned_bytes = required_tensor_bytes;
    report.cpu_resident_bytes = strcmp(options->backend, "cuda") == 0 ? 0ull : required_tensor_bytes;
    report.cuda_resident_bytes = strcmp(options->backend, "cuda") == 0 ? required_tensor_bytes : 0ull;
    report.residency_plan = strcmp(options->backend, "cuda") == 0 ? "cuda-resident-controlled-proof" : "cpu-resident-controlled-proof";
    report.runtime_blockers = "runtime family adapter not implemented; real transformer prefill unsupported; decode/logits/sampling/generation remain unsupported-full-model";

    if (fullmodel_fail_after(options, "preflight")) {
        report.status = "fullmodel-materialize-fail";
        report.failed_phase = "preflight";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }
    if (fullmodel_fail_after(options, "resolve-model")) {
        report.failed_phase = "resolve-model";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }
    if (fullmodel_fail_after(options, "artifact-identity")) {
        report.failed_phase = "artifact-identity";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }
    if (fullmodel_fail_after(options, "tensor-inventory")) {
        report.failed_phase = "tensor-inventory";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    if (selected_target) {
        report.status = "fullmodel-materialize-refused";
        report.placement_plan_status = "refused";
        report.memory_budget_status = "skipped";
        report.backend_preflight_status = "skipped";
        report.materialization_mode = "selected-runtime-slice-refusal";
        report.full_model_materialization = "refused-selected-runtime-slice";
        report.full_model_materialization_proof = "refused";
        report.failed_phase = "role-coverage";
        report.failed_reason = "selected-runtime-slice";
        report.refused_tensor_count = tensor_count;
        report.skipped_tensor_count = tensor_count;
        report.residency_plan = "selected-slice-not-full-model";
        report.runtime_blockers = "selected runtime slice cannot satisfy full required tensor materialization";
        fullmodel_print_materialize_report(&report);
        return 0;
    }

    if (!role_complete) {
        report.status = "fullmodel-materialize-refused";
        report.placement_plan_status = "partial";
        report.memory_budget_status = "skipped";
        report.backend_preflight_status = "skipped";
        report.materialization_mode = "role-coverage-refusal";
        report.full_model_materialization = "refused-incomplete-role-coverage";
        report.full_model_materialization_proof = "refused";
        report.failed_phase = "role-coverage";
        report.failed_reason = "required-role-missing";
        report.refused_tensor_count = tensor_count;
        report.skipped_tensor_count = tensor_count;
        report.residency_plan = "not-planned";
        report.runtime_blockers = "required fullmodel proof roles are missing";
        fullmodel_print_materialize_report(&report);
        return 0;
    }

    if (fullmodel_fail_after(options, "role-coverage")) {
        report.failed_phase = "role-coverage";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }
    if (fullmodel_fail_after(options, "placement-plan")) {
        report.failed_phase = "placement-plan";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    if ((options->has_limit_bytes && required_tensor_bytes > options->limit_bytes) ||
        (!options->has_limit_bytes && required_tensor_bytes > proof_byte_limit)) {
        report.status = "fullmodel-materialize-fail";
        report.memory_budget_status = "fail";
        report.backend_preflight_status = "skipped";
        report.materialization_mode = "memory-budget-refusal";
        report.full_model_materialization = "failed";
        report.full_model_materialization_proof = "fail";
        report.failed_phase = "memory-budget";
        report.failed_reason = options->has_limit_bytes ? "byte-limit" : "controlled-proof-limit";
        report.refused_tensor_count = required_tensor_count;
        report.skipped_tensor_count = required_tensor_count;
        report.residency_plan = "not-planned";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_NOMEM);
    }

    if (fullmodel_fail_after(options, "memory-budget")) {
        report.failed_phase = "memory-budget";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    if (options->dry_run || options->plan_only) {
        report.status = options->plan_only ? "fullmodel-materialize-plan-only" : "fullmodel-materialize-dry-run";
        report.backend_preflight_status = "skipped";
        report.materialization_mode = options->plan_only ? "plan-only" : "dry-run";
        report.full_model_materialization = "planned";
        report.full_model_materialization_proof = "planned";
        report.phase = options->plan_only ? "placement-plan" : "memory-budget";
        report.cleanup_attempted = "false";
        report.cleanup_status = "not-needed";
        report.skipped_tensor_count = required_tensor_count;
        fullmodel_print_materialize_report(&report);
        return 0;
    }

    if (fullmodel_fail_after(options, "backend-preflight")) {
        report.failed_phase = "backend-preflight";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    rc = fullmodel_allocate_required_tensors(options,
                                             ctx,
                                             &materialized_count,
                                             &materialized_bytes,
                                             &alloc_failed_phase,
                                             &alloc_failed_reason);
    if (rc != YVEX_OK) {
        report.status = "fullmodel-materialize-fail";
        report.backend_preflight_status = strcmp(alloc_failed_phase, "backend-preflight") == 0 ? "fail" : "partial";
        report.full_model_materialization = "failed";
        report.full_model_materialization_proof = "fail";
        report.failed_phase = alloc_failed_phase ? alloc_failed_phase : "backend-preflight";
        report.failed_reason = alloc_failed_reason ? alloc_failed_reason : "allocation failed";
        report.materialized_tensor_count = materialized_count;
        report.materialized_tensor_bytes = materialized_bytes;
        report.partial_materialization = materialized_count > 0ull ? "true" : "false";
        report.refused_tensor_count = required_tensor_count > materialized_count
                                          ? required_tensor_count - materialized_count
                                          : 0ull;
        fullmodel_print_materialize_report(&report);
        return exit_for_status(rc);
    }

    if (fullmodel_fail_after(options, "materialize-embedding") ||
        fullmodel_fail_after(options, "materialize-normalization") ||
        fullmodel_fail_after(options, "materialize-attention") ||
        fullmodel_fail_after(options, "materialize-mlp") ||
        fullmodel_fail_after(options, "materialize-moe") ||
        fullmodel_fail_after(options, "materialize-output") ||
        fullmodel_fail_after(options, "materialize-tokenizer") ||
        fullmodel_fail_after(options, "cleanup")) {
        report.failed_phase = options->fail_after_phase;
        report.failed_reason = "injected-failure";
        report.materialized_tensor_count = materialized_count;
        report.materialized_tensor_bytes = materialized_bytes;
        report.partial_materialization = "false";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    report.status = "fullmodel-materialize-pass";
    report.full_model_materialization_proof = "pass";
    report.phase = "complete";
    report.materialized_tensor_count = materialized_count;
    report.materialized_tensor_bytes = materialized_bytes;
    report.refused_tensor_count = 0ull;
    report.skipped_tensor_count = tensor_count > materialized_count
                                      ? tensor_count - materialized_count
                                      : 0ull;
    fullmodel_print_materialize_report(&report);
    return 0;
}

int yvex_fullmodel_render_command(int arg_count, char **args)
{
    yvex_cli_fullmodel_options options;
    yvex_model_ref_options ref_options;
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_fullmodel_collections collections;
    yvex_fullmodel_dtype_bucket dtype_buckets[32];
    yvex_fullmodel_largest_tensor largest[16];
    yvex_error err;
    char dtype_summary[512];
    char missing_roles[768];
    char descriptor_missing_roles[768];
    char unsupported_roles[512];
    const char *target_id;
    const char *target_class;
    const char *source_artifact_class;
    const char *target_artifact_class;
    const char *model_class;
    const char *inventory_status;
    const char *role_coverage;
    const char *backend_placement_status;
    const char *cpu_placement;
    const char *cuda_placement;
    yvex_arch arch;
    unsigned long long artifact_bytes = 0ull;
    unsigned long long total_tensor_bytes = 0ull;
    unsigned long long tensor_count;
    unsigned long long i;
    unsigned int dtype_bucket_count = 0u;
    unsigned int largest_count = 0u;
    int selected_target = 0;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&collections, 0, sizeof(collections));
    memset(dtype_buckets, 0, sizeof(dtype_buckets));
    memset(largest, 0, sizeof(largest));
    memset(missing_roles, 0, sizeof(missing_roles));
    memset(descriptor_missing_roles, 0, sizeof(descriptor_missing_roles));
    memset(unsupported_roles, 0, sizeof(unsupported_roles));

    rc = parse_fullmodel_options(arg_count, args, &options);
    if (rc == 1) return 0;
    if (rc != 0) return rc;

    if (strcmp(options.model, "glm-5.2-official-safetensors") == 0 ||
        (options.target && strcmp(options.target, "glm-5.2-official-safetensors") == 0)) {
        if (options.command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN) {
            return print_fullmodel_source_only_plan(&options, "glm-5.2-official-safetensors");
        }
        if (options.command == YVEX_FULLMODEL_COMMAND_MATERIALIZE) {
            return print_fullmodel_source_only_materialize(&options, "glm-5.2-official-safetensors");
        }
        if (options.command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR) {
            return print_fullmodel_source_only_descriptor(&options, "glm-5.2-official-safetensors");
        }
        if (options.command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME) {
            return print_fullmodel_source_only_family_runtime(&options, "glm-5.2-official-safetensors");
        }
        return print_fullmodel_source_only_report("glm-5.2-official-safetensors", options.backend);
    }

    memset(&ref_options, 0, sizeof(ref_options));
    ref_options.allow_registry = 1;
    ref_options.registry_path = options.registry_path;
    rc = yvex_model_ref_resolve(&ref, options.model, &ref_options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (!fullmodel_file_size(ref.path, &artifact_bytes)) {
        rc = print_fullmodel_missing_report(&options, ref.path);
        yvex_model_ref_clear(&ref);
        return rc;
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        int out = print_fullmodel_parse_failure_report(&options, &ref, yvex_error_message(&err), rc);
        yvex_error_clear(&err);
        yvex_model_ref_clear(&ref);
        return out;
    }

    tensor_count = yvex_tensor_table_count(ctx.table);
    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx.table, i);
        if (!tensor) continue;
        total_tensor_bytes += tensor->storage_bytes;
        fullmodel_record_dtype(dtype_buckets, &dtype_bucket_count, tensor);
        fullmodel_record_largest(largest, &largest_count,
                                 (unsigned int)options.limit_tensors, tensor);
        fullmodel_classify_tensor(tensor, &collections);
    }
    if (yvex_gguf_metadata_find(ctx.gguf, "tokenizer.ggml.tokens") ||
        yvex_gguf_metadata_find(ctx.gguf, "tokenizer.ggml.model")) {
        collections.tokenizer = 1ull;
        collections.tokenizer_bytes = 0ull;
        collections.has_tokenizer_metadata = 1;
    }
    fullmodel_dtype_summary(dtype_summary, sizeof(dtype_summary),
                            dtype_buckets, dtype_bucket_count);

    arch = yvex_model_arch(ctx.model);
    target_id = options.target ? options.target :
                (ref.alias && ref.alias[0] ? ref.alias : "path");
    selected_target = fullmodel_is_selected_target(target_id) ||
                      fullmodel_is_selected_target(options.model);
    target_class = selected_target ? "selected-runtime-slice" :
                   (options.target && strcmp(options.target, "deepseek4-v4-flash") == 0
                        ? "full-runtime-model-planned"
                        : "candidate-GGUF-path");
    source_artifact_class = selected_target ? "YVEX-produced selected GGUF" : "GGUF artifact";
    target_artifact_class = selected_target ? "YVEX-produced selected GGUF" : "candidate GGUF artifact";
    model_class = selected_target ? "selected-runtime-slice" : "descriptor-only-candidate";
    inventory_status = selected_target ? "incomplete" : "partial";

    if (!collections.has_token_embedding) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "token-embedding");
    if (!collections.has_attention_norm) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "attention-norm");
    if (!collections.has_post_attention_norm) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "post-attention-norm");
    if (!collections.has_attention_q) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "attention-q-projection");
    if (!collections.has_attention_k) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "attention-k-projection");
    if (!collections.has_attention_v) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "attention-v-projection");
    if (!collections.has_attention_out) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "attention-output-projection");
    if (!collections.has_ffn_gate) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "mlp-gate");
    if (!collections.has_ffn_up) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "mlp-up");
    if (!collections.has_ffn_down) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "mlp-down");
    if (!collections.has_moe_router) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "moe-router");
    if (!collections.has_moe_expert) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "moe-experts");
    if (!collections.has_output_norm) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "final-norm");
    if (!collections.has_output_head) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "output-head");
    if (!collections.has_tokenizer_metadata) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "tokenizer-metadata");
    if (!missing_roles[0]) snprintf(missing_roles, sizeof(missing_roles), "none");

    fullmodel_csv_append(unsupported_roles, sizeof(unsupported_roles), "runtime-family-adapter");
    fullmodel_csv_append(unsupported_roles, sizeof(unsupported_roles), "real-transformer-prefill");
    fullmodel_csv_append(unsupported_roles, sizeof(unsupported_roles), "real-DeepSeek-decode");
    fullmodel_csv_append(unsupported_roles, sizeof(unsupported_roles), "real-output-head-logits");
    fullmodel_csv_append(unsupported_roles, sizeof(unsupported_roles), "real-vocabulary-sampling");

    role_coverage = strcmp(missing_roles, "none") == 0 ? "observed" : "partial";
    if (selected_target) role_coverage = "partial";
    fullmodel_materialize_missing_roles(&options,
                                        &collections,
                                        descriptor_missing_roles,
                                        sizeof(descriptor_missing_roles));
    backend_placement_status = selected_target ? "selected-tensor-plan-only" : "report-only";
    cpu_placement = selected_target ? "selected-tensors-only" : "planned-full-model";
    cuda_placement = strcmp(options.backend, "cuda") == 0
                         ? (yvex_backend_cuda_context_available() ? "selected-or-candidate-tensors-only" : "unavailable")
                         : "not-requested";

    if (options.command == YVEX_FULLMODEL_COMMAND_MATERIALIZE) {
        rc = fullmodel_materialize_command_run(&options,
                                               &ref,
                                               &ctx,
                                               target_id,
                                               target_class,
                                               artifact_bytes,
                                               tensor_count,
                                               total_tensor_bytes,
                                               &collections,
                                               selected_target);
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return rc;
    }

    if (options.command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR) {
        const char *descriptor_role_coverage =
            selected_target ? "partial" :
            strcmp(descriptor_missing_roles, "none") == 0 ? "complete" : "partial";
        if (options.output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            fullmodel_print_descriptor_normal(&options,
                                              target_id,
                                              target_class,
                                              descriptor_role_coverage,
                                              descriptor_missing_roles);
        } else {
            fullmodel_print_descriptor_report(&options,
                                              &ref,
                                              &ctx,
                                              target_id,
                                              target_class,
                                              artifact_bytes,
                                              arch,
                                              tensor_count,
                                              total_tensor_bytes,
                                              &collections,
                                              descriptor_role_coverage,
                                              descriptor_missing_roles,
                                              unsupported_roles,
                                              selected_target);
        }
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return 0;
    }

    if (options.command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME) {
        const char *descriptor_role_coverage =
            selected_target ? "partial" :
            strcmp(descriptor_missing_roles, "none") == 0 ? "complete" : "partial";
        if (options.output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            fullmodel_print_family_runtime_normal(&options,
                                                  target_id,
                                                  target_class,
                                                  descriptor_role_coverage,
                                                  descriptor_missing_roles);
            rc = 0;
        } else {
            rc = fullmodel_print_family_runtime_report(&options,
                                                       &ref,
                                                       &ctx,
                                                       target_id,
                                                       target_class,
                                                       artifact_bytes,
                                                       arch,
                                                       tensor_count,
                                                       total_tensor_bytes,
                                                       &collections,
                                                       descriptor_role_coverage,
                                                       descriptor_missing_roles,
                                                       unsupported_roles,
                                                       selected_target);
        }
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return rc;
    }

    if (options.command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN) {
        if (options.output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            fullmodel_print_plan_normal(&options,
                                        selected_target ? "blocked" : "partial",
                                        target_class,
                                        selected_target ? "blocked" : "unknown",
                                        selected_target ? "selected-slice-not-full-runtime" : "full-runtime candidate artifact required");
        } else {
            fullmodel_print_materialization_plan(&options,
                                                 &ref,
                                                 target_id,
                                                 target_class,
                                                 artifact_bytes,
                                                 arch,
                                                 tensor_count,
                                                 total_tensor_bytes,
                                                 &collections,
                                                 dtype_summary,
                                                 role_coverage,
                                                 missing_roles,
                                                 selected_target);
        }
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return 0;
    }

    if (options.output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        fullmodel_print_report_normal(&options,
                                      "fullmodel-report",
                                      target_id,
                                      target_class,
                                      role_coverage,
                                      selected_target ? "selected-slice-not-full-runtime" : "missing-full-runtime-tensor-coverage",
                                      "tensor/source/artifact row required");
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return 0;
    }

    yvex_cli_out_writef(stdout, "fullmodel: report\n");
    yvex_cli_out_writef(stdout, "status: fullmodel-report\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options.model);
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref.path ? ref.path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target_id);
    yvex_cli_out_writef(stdout, "target_class: %s\n", target_class);
    yvex_cli_out_writef(stdout, "source_artifact_class: %s\n", source_artifact_class);
    yvex_cli_out_writef(stdout, "target_artifact_class: %s\n", target_artifact_class);
    yvex_cli_out_writef(stdout, "artifact_exists: true\n");
    yvex_cli_out_writef(stdout, "artifact_bytes: %llu\n", artifact_bytes);
    yvex_cli_out_writef(stdout, "artifact_identity_status: %s\n", fullmodel_identity_status(&ref, artifact_bytes));
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", tensor_count);
    yvex_cli_out_writef(stdout, "tensor_inventory_status: pass\n");
    yvex_cli_out_writef(stdout, "metadata_status: pass\n");
    yvex_cli_out_writef(stdout, "architecture: %s\n", yvex_arch_name(arch));
    yvex_cli_out_writef(stdout, "family: %s\n", fullmodel_family_from_arch(arch));
    yvex_cli_out_writef(stdout, "model_class: %s\n", model_class);
    yvex_cli_out_writef(stdout, "fullmodel_inventory: %s\n", inventory_status);
    yvex_cli_out_writef(stdout, "full_runtime_model: false\n");
    yvex_cli_out_writef(stdout, "qtype_summary: %s\n", dtype_summary);
    yvex_cli_out_writef(stdout, "dtype_summary: %s\n", dtype_summary);
    yvex_cli_out_writef(stdout, "total_tensor_bytes: %llu\n", total_tensor_bytes);
    yvex_cli_out_writef(stdout, "estimated_cpu_resident_bytes: %llu\n", total_tensor_bytes);
    yvex_cli_out_writef(stdout, "estimated_cuda_resident_bytes: %llu\n", total_tensor_bytes);
    yvex_cli_out_writef(stdout, "estimated_kv_bytes: planned\n");
    yvex_cli_out_writef(stdout, "estimated_scratch_bytes: planned\n");
    yvex_cli_out_writef(stdout, "estimated_total_runtime_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", options.backend);
    yvex_cli_out_writef(stdout, "backend_placement_status: %s\n", backend_placement_status);
    yvex_cli_out_writef(stdout, "cpu_placement: %s\n", cpu_placement);
    yvex_cli_out_writef(stdout, "cuda_placement: %s\n", cuda_placement);
    yvex_cli_out_writef(stdout, "cuda_context_available: %s\n", yvex_backend_cuda_context_available() ? "true" : "false");
    yvex_cli_out_writef(stdout, "cuda_memory_status: %s\n", yvex_backend_cuda_context_available() ? "context-available-no-allocation" : "unavailable");
    yvex_cli_out_writef(stdout, "residency_plan: report-only-no-allocation\n");
    yvex_cli_out_writef(stdout, "tensor_collections_status: %s\n", role_coverage);
    yvex_cli_out_writef(stdout, "collection_detected: %s\n", tensor_count > 0ull ? "yes" : "no");
    yvex_cli_out_writef(stdout, "collection_supported: partial\n");
    yvex_cli_out_writef(stdout, "runtime_consumer: unsupported\n");
    yvex_cli_out_writef(stdout, "embedding_tensors: %llu\n", collections.embedding);
    yvex_cli_out_writef(stdout, "normalization_tensors: %llu\n", collections.normalization);
    yvex_cli_out_writef(stdout, "attention_tensors: %llu\n", collections.attention);
    yvex_cli_out_writef(stdout, "kv_cache_requirements: planned\n");
    yvex_cli_out_writef(stdout, "mlp_tensors: %llu\n", collections.mlp);
    yvex_cli_out_writef(stdout, "moe_tensors: %llu\n", collections.moe);
    yvex_cli_out_writef(stdout, "output_tensors: %llu\n", collections.output);
    yvex_cli_out_writef(stdout, "tokenizer_tensors: %llu\n", collections.tokenizer);
    yvex_cli_out_writef(stdout, "unknown_tensors: %llu\n", collections.unknown);
    yvex_cli_out_writef(stdout, "required_role_coverage: %s\n", role_coverage);
    yvex_cli_out_writef(stdout, "missing_required_roles: %s\n", missing_roles);
    yvex_cli_out_writef(stdout, "unsupported_required_roles: %s\n", unsupported_roles);
    yvex_cli_out_writef(stdout, "runtime_blockers: full tensor set missing; attention projection tensors may be missing; MLP/MoE tensors may be missing; output head may be missing; real transformer prefill unsupported; real DeepSeek decode unsupported; real output-head logits unsupported; real vocabulary sampling unsupported; full model materialization not implemented\n");
    print_fullmodel_common_boundaries();
    yvex_cli_out_writef(stdout, "largest_tensor_report_limit: %llu\n", options.limit_tensors);
    fullmodel_print_largest(largest, largest_count);

    close_model_context(&ctx);
    yvex_model_ref_clear(&ref);
    return 0;
}

void yvex_fullmodel_render_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex fullmodel report --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] [--limit-tensors N] [--registry FILE] [--" "audit | --" "output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "usage: " "yvex fullmodel materialization-plan --model FILE_OR_ALIAS [--backend cpu|cuda] [--residency resident|host-staged|ssd-staged|hybrid] [--target TARGET] [--limit-tensors N] [--registry FILE] [--" "audit | --" "output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "usage: " "yvex fullmodel materialize --model FILE_OR_ALIAS [--backend cpu|cuda] [--registry FILE] [--dry-run] [--plan-only] [--require-role ROLE] [--require-collection COLLECTION] [--limit-bytes N] [--fail-after-phase PHASE] [--report-dir DIR] [--" "audit | --" "output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "usage: " "yvex fullmodel descriptor --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] [--format text] [--limit-tensors N] [--require-role ROLE] [--require-collection COLLECTION] [--" "include-blockers] [--" "include-placement] [--" "include-graph] [--" "include-kv] [--" "include-logits] [--" "audit | --" "output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "usage: " "yvex fullmodel family-runtime --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--" "include-blockers] [--" "include-roles] [--" "include-graph] [--" "include-kv] [--" "include-moe] [--" "include-output] [--" "audit | --" "output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "alias: yvex fullmodel plan --model FILE_OR_ALIAS [options]\n");
    yvex_cli_out_writef(fp, "\nExamples:\n");
    yvex_cli_out_writef(fp, "  yvex fullmodel report --model deepseek4-v4-flash-selected-embed --backend cpu\n");
    yvex_cli_out_writef(fp, "  yvex fullmodel report --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --limit-tensors 8\n");
    yvex_cli_out_writef(fp, "  yvex fullmodel report --model ./candidate.gguf --target deepseek4-v4-flash --backend cuda\n");
    yvex_cli_out_writef(fp, "  yvex fullmodel materialization-plan --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --residency resident\n");
    yvex_cli_out_writef(fp, "  yvex fullmodel materialization-plan --model ./candidate.gguf --target deepseek4-v4-flash --backend cuda --residency hybrid\n");
    yvex_cli_out_writef(fp, "  yvex fullmodel materialize --model ./tiny-fullish.gguf --backend cpu --limit-bytes 1048576\n");
    yvex_cli_out_writef(fp, "  yvex fullmodel materialize --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu\n");
    yvex_cli_out_writef(fp, "  yvex fullmodel descriptor --model ./candidate.gguf --target deepseek4-v4-flash --backend cpu --limit-tensors 40\n");
    yvex_cli_out_writef(fp, "  yvex fullmodel family-runtime --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu\n");
    yvex_cli_out_writef(fp, "\nfullmodel report:\n");
    yvex_cli_out_writef(fp, "  inventory and placement pressure report.\n");
    yvex_cli_out_writef(fp, "  Default output is compact. Use --" "audit for full diagnostic fields.\n");
    yvex_cli_out_writef(fp, "\nfullmodel materialization-plan:\n");
    yvex_cli_out_writef(fp, "  planned placement phases and materialization preflight only.\n");
    yvex_cli_out_writef(fp, "\nfullmodel materialization proof:\n");
    yvex_cli_out_writef(fp, "  controlled proof over a tiny full-ish GGUF tensor set, or a clean refusal for selected/runtime-slice and incomplete artifacts.\n");
    yvex_cli_out_writef(fp, "\nfullmodel descriptor:\n");
    yvex_cli_out_writef(fp, "  planning/reporting boundary for tensor roles, tensor collections, residency expectations, graph requirements, prefill/KV/decode/logits/sampling requirements, output-head/tokenizer requirements, backend requirements, and blockers.\n");
    yvex_cli_out_writef(fp, "\nfullmodel family-runtime:\n");
    yvex_cli_out_writef(fp, "  maps descriptor facts into model-family runtime adapter facts. DeepSeek is the first concrete family report target. Qwen/Metal remains planned unless separately implemented.\n");
    yvex_cli_out_writef(fp, "\nFullmodel report reads GGUF metadata and tensor-directory facts only. Materialization-plan reuses those inventory facts to plan collection placement, residency, backend fit, blockers, and cleanup. Materialize allocates and releases only the bounded required proof tensors that pass role coverage and byte-limit checks. Descriptor builds a runtime requirement report only. Family-runtime maps descriptor facts into family-specific tensor roles, collection adapters, attention/KV/MoE/output requirements, graph requirements, blockers, and next-row dependencies. These reports do not execute the model, materialize full weights, run graph execution, write real KV, produce real logits, sample real vocabulary tokens, generate, evaluate, benchmark, or report throughput. They report why full transformer prefill, decode, logits, and generation remain unsupported.\n");
    yvex_cli_out_writef(fp, "Boundary: no full model execution, no inference readiness, no DeepSeek generation, no provider generation, no streaming generation, no eval, no benchmark, no throughput.\n");
}

/* Models command dispatch and help. */
