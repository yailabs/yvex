/* Program P coordinates existing compiler sessions. It does not invent kernels,
 * reinterpret source roles, execute payloads or promote estimates to evidence. */
#include <yvex/optimization.h>
#include <yvex/quant.h>
#include <yvex/internal/compiler.h>
#include <yvex/internal/artifact.h>
#include <yvex/internal/core.h>
#include <yvex/internal/deployment.h>
#include <yvex/internal/graph.h>
#include <yvex/internal/family_catalog.h>
#include <yvex/internal/gguf_writer.h>
#include <yvex/internal/quant_numeric.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

typedef int (*optimization_method)(yvex_optimization_search **,
    const yvex_optimization_request *, unsigned long long, unsigned int, yvex_error *);

static int preset_search(yvex_optimization_search **out, const yvex_optimization_request *request,
    unsigned long long budget, unsigned int states, yvex_error *err)
{
    if (budget || states) {
        if (out) *out = NULL;
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "compiler.optimization",
            "fixed recipes do not consume allocation budgets");
        return YVEX_ERR_INVALID_ARG;
    }
    return yvex_optimization_search_open(out, request, err);
}

static const struct {
    yvex_optimization_technique descriptor;
    optimization_method search;
} techniques[] = {
    {{YVEX_OPTIMIZATION_TECHNIQUE_SCHEMA_V1, "fixed-recipes-v1", "presets",
      "authenticated source, canonical quant producers, optional expert policy/calibration",
      "bounded fixed recipe experiments; no predicted rates",
      "full capacity, execution, independent quality, performance, lifecycle"}, preset_search},
    {{YVEX_OPTIMIZATION_TECHNIQUE_SCHEMA_V1, "source-retention-allocation-v1", "source-retention",
      "canonical source/Q2 plans, coupled roles, explicit approximation, encoded-byte/state budgets",
      "exact byte/source-retention frontier; retention is not a quality or speed metric",
      "full capacity, execution, independent quality, performance, lifecycle"}, yvex_optimization_search_allocate}
};

unsigned int yvex_optimization_technique_count(void)
{
    return (unsigned int)(sizeof(techniques) / sizeof(techniques[0]));
}

const yvex_optimization_technique *yvex_optimization_technique_at(unsigned int index)
{
    return index < yvex_optimization_technique_count() ? &techniques[index].descriptor : NULL;
}

int yvex_optimization_search_run(yvex_optimization_search **out,
    const yvex_optimization_request *request, const char *identity,
    unsigned long long budget, unsigned int states, yvex_error *err)
{
    if (out) *out = NULL;
    if (out && identity) for (unsigned int i = 0u; i < yvex_optimization_technique_count(); ++i) {
        if (!strcmp(identity, techniques[i].descriptor.identity))
            return techniques[i].search(out, request, budget, states, err);
    }
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "compiler.optimization",
        "no implemented technique with this exact versioned identity");
    return YVEX_ERR_UNSUPPORTED;
}

/* Bounded initial exploration grammar. Every codec is checked against its
 * canonical producer; family precision/shape constraints are still enforced
 * by the existing quant planner. These are experiments, not qualified recipes. */
static const struct {
    const char *name, *codec;
    yvex_quant_qtype qtype;
    unsigned int routed_policy; /* 0: unchanged; 1: admitted matrix; 2: Q2_K. */
} synthesis[] = {
    {"goal-v1-source", "BF16", YVEX_QUANT_QTYPE_SOURCE, 0u},
    {"goal-v1-q8_0", "Q8_0", YVEX_QUANT_QTYPE_Q8_0, 0u},
    {"goal-v1-q2_k", "Q2_K", YVEX_QUANT_QTYPE_Q2_K, 0u},
    {"goal-v1-mxfp4", "MXFP4", YVEX_QUANT_QTYPE_MXFP4, 0u},
    {"goal-v1-routed-matrix-q8_0", "Q8_0", YVEX_QUANT_QTYPE_Q8_0, 1u},
    {"goal-v1-mxfp4-routed-q2_k", "MXFP4", YVEX_QUANT_QTYPE_MXFP4, 2u}
};

struct yvex_optimization_search {
    unsigned int count;
    yvex_optimization_context context;
    yvex_optimization_candidate candidates[YVEX_OPTIMIZATION_MAX_CANDIDATES];
    yvex_quant_policy *policies[YVEX_OPTIMIZATION_MAX_CANDIDATES];
};

static int refuse(yvex_error *err, yvex_status code, const char *reason)
{
    yvex_error_set(err, code, "compiler.optimization", reason);
    return code;
}

static int observation_valid(const yvex_optimization_observation *o)
{
    if (o->schema_version != YVEX_OPTIMIZATION_SCHEMA_V1 ||
        !yvex_sha256_hex_is_valid(o->candidate_identity) ||
        !yvex_sha256_hex_is_valid(o->comparison_identity) ||
        !yvex_sha256_hex_is_valid(o->evidence_identity) || (o->missing_evidence & ~63u))
        return 0;
    /* An absent independent reference is unknown, not a fabricated digest.
     * Such a row remains ineligible for measured selection. A supplied
     * reference still has to be a well-formed canonical identity. */
    if (o->quality_reference_identity) {
        if (!yvex_sha256_hex_is_valid(o->quality_reference_identity)) return 0;
    } else if (!(o->missing_evidence & YVEX_OPTIMIZATION_MISSING_QUALITY)) return 0;
    /* Missing rows retain their explicit state; numeric fields are unavailable,
     * not interpreted as zero measurements or compared against real values. */
    if (o->missing_evidence) return 1;
    return o->samples && o->peak_working_bytes &&
        isfinite(o->prefill_tokens_per_second) && o->prefill_tokens_per_second > 0.0 &&
        isfinite(o->decode_tokens_per_second) && o->decode_tokens_per_second > 0.0 &&
        isfinite(o->ttft_seconds) && o->ttft_seconds >= 0.0 &&
        isfinite(o->preparation_seconds) && o->preparation_seconds >= 0.0 &&
        isfinite(o->quality_loss);
}

static int observation_dominates(const yvex_optimization_observation *a,
                                 const yvex_optimization_observation *b)
{
    int no_worse = a->prefill_tokens_per_second >= b->prefill_tokens_per_second &&
        a->decode_tokens_per_second >= b->decode_tokens_per_second &&
        a->ttft_seconds <= b->ttft_seconds && a->preparation_seconds <= b->preparation_seconds &&
        a->quality_loss <= b->quality_loss && a->peak_working_bytes <= b->peak_working_bytes;
    int better = a->prefill_tokens_per_second > b->prefill_tokens_per_second ||
        a->decode_tokens_per_second > b->decode_tokens_per_second ||
        a->ttft_seconds < b->ttft_seconds || a->preparation_seconds < b->preparation_seconds ||
        a->quality_loss < b->quality_loss || a->peak_working_bytes < b->peak_working_bytes;
    return no_worse && better;
}

int yvex_optimization_select(const yvex_optimization_constraints *c,
    const yvex_optimization_observation *rows, unsigned int count,
    yvex_optimization_selection *out, yvex_error *err)
{
    yvex_optimization_selection selected[YVEX_OPTIMIZATION_MAX_CANDIDATES] = {{0}};
    if (!c || c->schema_version != YVEX_OPTIMIZATION_SCHEMA_V1 || !rows || !out ||
        !count || count > YVEX_OPTIMIZATION_MAX_CANDIDATES || !c->minimum_samples ||
        !c->maximum_working_bytes || !isfinite(c->minimum_prefill) || c->minimum_prefill < 0.0 ||
        !isfinite(c->minimum_decode) || c->minimum_decode < 0.0 ||
        !isfinite(c->maximum_ttft) || c->maximum_ttft < 0.0 || !isfinite(c->maximum_quality_loss))
        return refuse(err, YVEX_ERR_INVALID_ARG, "bounded selection constraints required");
    for (unsigned int i = 0u; i < count; ++i) {
        if (!observation_valid(&rows[i]))
            return refuse(err, YVEX_ERR_INVALID_ARG, "invalid or stale measured observation");
        if (strcmp(rows[0].comparison_identity, rows[i].comparison_identity))
            return refuse(err, YVEX_ERR_INVALID_ARG, "incomparable workload");
        for (unsigned int j = 0u; j < i; ++j) {
            if (rows[j].quality_reference_identity && rows[i].quality_reference_identity &&
                strcmp(rows[j].quality_reference_identity, rows[i].quality_reference_identity))
                return refuse(err, YVEX_ERR_INVALID_ARG, "incomparable quality reference");
            if (!strcmp(rows[j].candidate_identity, rows[i].candidate_identity))
                return refuse(err, YVEX_ERR_INVALID_ARG, "duplicate candidate in selection population");
        }
        selected[i].schema_version = YVEX_OPTIMIZATION_SCHEMA_V1;
        selected[i].dominator = count;
        if (rows[i].missing_evidence || rows[i].samples < c->minimum_samples) {
            selected[i].state = YVEX_OPTIMIZATION_EVIDENCE_INCOMPLETE;
        } else if (rows[i].peak_working_bytes > c->maximum_working_bytes ||
                   rows[i].quality_loss > c->maximum_quality_loss ||
                   rows[i].prefill_tokens_per_second < c->minimum_prefill ||
                   rows[i].decode_tokens_per_second < c->minimum_decode ||
                   (c->maximum_ttft > 0.0 && rows[i].ttft_seconds > c->maximum_ttft)) {
            selected[i].state = YVEX_OPTIMIZATION_OUTSIDE_CONSTRAINTS;
        }
    }
    for (unsigned int i = 0u; i < count; ++i) {
        if (selected[i].state != YVEX_OPTIMIZATION_FRONTIER) continue;
        for (unsigned int j = 0u; j < count; ++j) {
            if (selected[j].state != YVEX_OPTIMIZATION_FRONTIER &&
                selected[j].state != YVEX_OPTIMIZATION_DOMINATED) continue;
            if (i != j && observation_dominates(&rows[j], &rows[i])) {
                selected[i].state = YVEX_OPTIMIZATION_DOMINATED;
                selected[i].dominator = j;
                break;
            }
        }
    }
    memcpy(out, selected, count * sizeof(*out));
    yvex_error_clear(err);
    return YVEX_OK;
}

static int request_validate(const yvex_optimization_request *r, yvex_error *err)
{
    if (!r || r->schema_version != YVEX_OPTIMIZATION_SCHEMA_V1)
        return refuse(err, YVEX_ERR_INVALID_ARG, "unsupported optimization request schema");
    if (r->goal < YVEX_OPTIMIZATION_BALANCED || r->goal > YVEX_OPTIMIZATION_QUALITY ||
        r->backend < YVEX_BACKEND_KIND_CPU || r->backend > YVEX_BACKEND_KIND_METAL ||
        !r->device_count || !r->context_tokens || !r->prefill_tokens ||
        r->prefill_tokens > r->context_tokens || !r->concurrent_sequences ||
        !r->system_memory_bytes ||
        r->available_memory_bytes > r->system_memory_bytes ||
        r->memory_limit_bytes > r->system_memory_bytes ||
        !r->maximum_candidates || r->maximum_candidates > YVEX_OPTIMIZATION_MAX_CANDIDATES ||
        (r->allow_approximation != 0 && r->allow_approximation != 1) ||
        (r->require_routed_matrix != 0 && r->require_routed_matrix != 1) ||
        (r->backend == YVEX_BACKEND_KIND_CUDA && !r->compute_major))
        return refuse(err, YVEX_ERR_INVALID_ARG, "bounded hardware, workload and quality constraints required");
    if (r->system_reserve_bytes && r->system_reserve_bytes <
        yvex_execution_system_reserve(r->system_memory_bytes))
        return refuse(err, YVEX_ERR_INVALID_ARG, "optimization cannot reduce the canonical system reserve");
    return YVEX_OK;
}

int yvex_optimization_context_resolve(const yvex_optimization_request *r,
    yvex_optimization_context *out, yvex_error *err)
{
    yvex_family_source_products products = {0};
    yvex_optimization_context context = {0};
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    int rc = request_validate(r, err);
    if (rc != YVEX_OK) return rc;
    if (!out || !r->target_id || !r->source_path || !r->models_root || !r->source_manifest_path)
        return refuse(err, YVEX_ERR_INVALID_ARG, "complete profile source inputs required");
    const yvex_graph_execution_binding *execution = yvex_graph_execution_find(0ull, 0ull, r->target_id);
    if (!execution || !execution->compiler ||
        execution->compiler->schema_version != YVEX_FAMILY_COMPILER_SCHEMA_V3)
        return refuse(err, YVEX_ERR_UNSUPPORTED, "target has no canonical model profile producer");
    const yvex_compilation_runtime_binding_request source = {
        .source_path = r->source_path, .models_root = r->models_root,
        .source_manifest_path = r->source_manifest_path,
        .family_adapter_id = execution->compiler->adapter_id,
        .family_adapter_version = execution->compiler->adapter_version,
        .source_stream_count = 1u
    };
    rc = yvex_family_source_compile(r->target_id, &source, &products, err);
    const yvex_semantic_model_ir_summary *s = rc == YVEX_OK ?
        yvex_semantic_model_ir_summary_get(products.semantic_model) : NULL;
    if (rc == YVEX_OK && !s) rc = refuse(err, YVEX_ERR_UNSUPPORTED, "semantic model profile unavailable");
    if (rc == YVEX_OK && (!s->execution_descriptor.maximum_context ||
        r->context_tokens > s->execution_descriptor.maximum_context))
        rc = refuse(err, YVEX_ERR_BOUNDS, "requested context exceeds or lacks the verified semantic envelope");
    if (rc == YVEX_OK) {
        const yvex_model_execution_descriptor *d = &s->execution_descriptor;
        context.schema_version = YVEX_OPTIMIZATION_CONTEXT_SCHEMA_V1;
        yvex_core_text_copy(context.semantic_identity, sizeof(context.semantic_identity), s->identity);
        yvex_core_text_copy(context.source_identity, sizeof(context.source_identity), s->source_model_identity);
        yvex_core_text_copy(context.model_execution_identity, sizeof(context.model_execution_identity), d->identity);
        yvex_core_text_copy(context.family, sizeof(context.family), execution->compiler->family);
        context.maximum_context = d->maximum_context;
        context.layers = d->layer_count;
        context.attention_layers = s->attention_layer_count;
        context.sequence_mixer_layers = d->sequence_mixer_layers;
        context.routed_experts = d->routed_experts;
        context.experts_per_row = d->experts_per_row;
        context.draft_layers = d->draft_layer_count;
        context.context_tokens = r->context_tokens;
        context.prefill_tokens = r->prefill_tokens;
        context.concurrent_sequences = r->concurrent_sequences;
        context.backend = r->backend;
        context.compute_major = r->compute_major;
        context.compute_minor = r->compute_minor;
        context.device_count = r->device_count;
        context.system_memory_bytes = r->system_memory_bytes;
        context.available_memory_bytes = r->available_memory_bytes;
        context.reserve_bytes = r->system_reserve_bytes ? r->system_reserve_bytes :
            yvex_execution_system_reserve(r->system_memory_bytes);
        context.missing_evidence = 63u;
        /* Use canonical typed field encoding, as for candidate identities;
         * no path, timestamp or live free-memory observation enters it. */
        yvex_sha256_init(&hash);
        int hashed = yvex_sha256_update_text(&hash, "yvex.optimization-context.v1") &&
            yvex_sha256_update_text(&hash, context.semantic_identity);
        const unsigned long long fields[] = {r->goal, r->backend, r->compute_major, r->compute_minor,
            r->device_count, r->context_tokens, r->prefill_tokens, r->concurrent_sequences,
            r->system_memory_bytes, r->memory_limit_bytes, context.reserve_bytes,
            (unsigned int)r->allow_approximation, (unsigned int)r->require_routed_matrix};
        for (unsigned int i = 0u; i < sizeof(fields) / sizeof(fields[0]); ++i)
            hashed = hashed && yvex_sha256_update_u64(&hash, fields[i]);
        if (!hashed || !yvex_sha256_final(&hash, digest))
            rc = refuse(err, YVEX_ERR_STATE, "resolved context identity failed");
        else yvex_sha256_hex(digest, context.identity);
    }
    yvex_family_source_products_release(&products);
    if (rc == YVEX_OK) { *out = context; yvex_error_clear(err); }
    return rc;
}

const char *yvex_optimization_state_name(yvex_optimization_state state)
{
    switch (state) {
    case YVEX_OPTIMIZATION_NEEDS_QUALIFICATION: return "needs-qualification";
    case YVEX_OPTIMIZATION_RESOURCE_INFEASIBLE: return "resource-infeasible";
    case YVEX_OPTIMIZATION_NUMERICAL_INCOMPATIBLE: return "numerical-incompatible";
    case YVEX_OPTIMIZATION_IMPLEMENTATION_UNSUPPORTED: return "implementation-unsupported";
    case YVEX_OPTIMIZATION_COMPILATION_REFUSED: return "compilation-refused";
    default: return "unknown";
    }
}

const char *yvex_optimization_priority_basis(yvex_optimization_goal goal)
{
    switch (goal) {
    case YVEX_OPTIMIZATION_MEMORY:
        return "eligible first; smaller initial byte lower bound first; full working set and quality unknown";
    case YVEX_OPTIMIZATION_QUALITY:
        return "eligible first; source-preserving before approximate; no ranking within approximate quality";
    case YVEX_OPTIMIZATION_THROUGHPUT:
        return "eligible first; compatible routed matrix operands first; speed benefit unmeasured";
    case YVEX_OPTIMIZATION_BALANCED:
        return "eligible first; canonical recipe order; no unsupported scalar tradeoff score";
    default: return NULL;
    }
}

int yvex_optimization_candidate_priority(yvex_optimization_goal goal,
    const yvex_optimization_candidate *left,
    const yvex_optimization_candidate *right, int *order, yvex_error *err)
{
    unsigned long long a, b;
    if (order) *order = 0;
    if (!order || !yvex_optimization_priority_basis(goal) || !left || !right ||
        left->schema_version != YVEX_OPTIMIZATION_SCHEMA_V1 ||
        right->schema_version != YVEX_OPTIMIZATION_SCHEMA_V1 ||
        left->state < YVEX_OPTIMIZATION_NEEDS_QUALIFICATION ||
        left->state > YVEX_OPTIMIZATION_COMPILATION_REFUSED ||
        right->state < YVEX_OPTIMIZATION_NEEDS_QUALIFICATION ||
        right->state > YVEX_OPTIMIZATION_COMPILATION_REFUSED)
        return refuse(err, YVEX_ERR_INVALID_ARG, "current candidate assessments and goal required");
    a = left->state != YVEX_OPTIMIZATION_NEEDS_QUALIFICATION;
    b = right->state != YVEX_OPTIMIZATION_NEEDS_QUALIFICATION;
    if (a == b && !a) {
        if (goal == YVEX_OPTIMIZATION_MEMORY) {
            if (!left->initial_required_bytes || !right->initial_required_bytes)
                return refuse(err, YVEX_ERR_INVALID_ARG, "memory ordering requires known initial bounds");
            a = left->initial_required_bytes; b = right->initial_required_bytes;
        } else if (goal == YVEX_OPTIMIZATION_QUALITY) {
            a = left->approximate_tensors != 0ull; b = right->approximate_tensors != 0ull;
        } else if (goal == YVEX_OPTIMIZATION_THROUGHPUT) {
            a = !left->routed_tensors || left->matrix_incompatible_tensors != 0ull;
            b = !right->routed_tensors || right->matrix_incompatible_tensors != 0ull;
        }
    }
    *order = a < b ? -1 : a > b ? 1 : 0;
    yvex_error_clear(err);
    return YVEX_OK;
}

static void prioritize(yvex_optimization_search *search, yvex_optimization_goal goal)
{
    /* Stable insertion sort over the bounded population. Policies travel with
     * their exact immutable row; reordering never changes candidate identity. */
    for (unsigned int i = 1u; i < search->count; ++i) {
        unsigned int j = i;
        while (j) {
            int order = 0;
            if (yvex_optimization_candidate_priority(goal, &search->candidates[j],
                    &search->candidates[j - 1u], &order, NULL) != YVEX_OK || order >= 0) break;
            yvex_optimization_candidate candidate = search->candidates[j];
            yvex_quant_policy *policy = search->policies[j];
            search->candidates[j] = search->candidates[j - 1u];
            search->policies[j] = search->policies[j - 1u];
            search->candidates[j - 1u] = candidate;
            search->policies[j - 1u] = policy;
            --j;
        }
    }
}

static int assess_identity(const yvex_optimization_request *r,
                           yvex_optimization_candidate *c, yvex_error *err)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    /* Physical identity excludes ephemeral free-memory observations. The
     * search candidate additionally binds declared goals/workload/constraints. */
    if (!yvex_sha256_update_text(&hash, "yvex.optimization-candidate.v1") ||
        !yvex_sha256_update_text(&hash, c->physical_variant_identity) ||
        !yvex_sha256_update_text(&hash, c->source_identity) ||
        !yvex_sha256_update_u64(&hash, r->goal) ||
        !yvex_sha256_update_u64(&hash, r->backend) ||
        !yvex_sha256_update_u64(&hash, r->compute_major) ||
        !yvex_sha256_update_u64(&hash, r->compute_minor) ||
        !yvex_sha256_update_u64(&hash, r->device_count) ||
        !yvex_sha256_update_u64(&hash, r->context_tokens) ||
        !yvex_sha256_update_u64(&hash, r->prefill_tokens) ||
        !yvex_sha256_update_u64(&hash, r->concurrent_sequences) ||
        !yvex_sha256_update_u64(&hash, r->system_memory_bytes) ||
        !yvex_sha256_update_u64(&hash, r->memory_limit_bytes) ||
        !yvex_sha256_update_u64(&hash, c->reserve_bytes) ||
        !yvex_sha256_update_u64(&hash, (unsigned int)r->allow_approximation) ||
        !yvex_sha256_update_u64(&hash, (unsigned int)r->require_routed_matrix) ||
        !yvex_sha256_final(&hash, digest))
        return refuse(err, YVEX_ERR_STATE, "candidate identity failed");
    yvex_sha256_hex(digest, c->candidate_identity);
    return YVEX_OK;
}

int yvex_optimization_candidate_assess(const yvex_optimization_request *r,
                                      yvex_optimization_candidate *c,
                                      yvex_error *err)
{
    int rc = request_validate(r, err);
    const char *reason;
    if (rc != YVEX_OK) return rc;
    if (!c || c->schema_version != YVEX_OPTIMIZATION_SCHEMA_V1 ||
        !yvex_sha256_hex_valid(c->physical_variant_identity) ||
        !yvex_sha256_hex_valid(c->source_identity) || !c->encoded_bytes ||
        !c->maximum_tensor_bytes || c->maximum_tensor_bytes > c->encoded_bytes ||
        c->artifact_bytes < c->encoded_bytes ||
        c->matrix_incompatible_tensors > c->routed_tensors || c->artifact_catalog_compatibility > 3u ||
        (c->operand_codecs_available != 0 && c->operand_codecs_available != 1))
        return refuse(err, YVEX_ERR_INVALID_ARG, "complete compiler candidate facts required");
    c->reserve_bytes = r->system_reserve_bytes ? r->system_reserve_bytes :
                      yvex_execution_system_reserve(r->system_memory_bytes);
    c->memory_budget_bytes = r->available_memory_bytes;
    if (r->memory_limit_bytes && r->memory_limit_bytes < c->memory_budget_bytes)
        c->memory_budget_bytes = r->memory_limit_bytes;
    if (!yvex_core_u64_add(c->encoded_bytes, c->maximum_tensor_bytes,
                           &c->initial_required_bytes) ||
        !yvex_core_u64_add(c->initial_required_bytes, c->reserve_bytes,
                           &c->initial_required_bytes))
        return refuse(err, YVEX_ERR_BOUNDS, "initial resource envelope overflow");
    c->missing_evidence = YVEX_OPTIMIZATION_MISSING_WORKSPACE |
        YVEX_OPTIMIZATION_MISSING_STATE | YVEX_OPTIMIZATION_MISSING_MODEL_EXECUTION |
        YVEX_OPTIMIZATION_MISSING_QUALITY | YVEX_OPTIMIZATION_MISSING_PERFORMANCE |
        YVEX_OPTIMIZATION_MISSING_LIFECYCLE;
    c->state = YVEX_OPTIMIZATION_NEEDS_QUALIFICATION;
    reason = c->artifact_catalog_compatibility == 3u ?
        "outside static catalog; complete production proof required before binding; execution and quality unqualified" :
        "initial bounds only; full deployment, independent quality and performance unqualified";
    if (r->device_count != 1u || r->backend == YVEX_BACKEND_KIND_METAL ||
        !c->operand_codecs_available || c->artifact_catalog_compatibility == 2u ||
        (r->require_routed_matrix && c->matrix_incompatible_tensors)) {
        c->state = YVEX_OPTIMIZATION_IMPLEMENTATION_UNSUPPORTED;
        reason = r->device_count != 1u ? "multi-device model execution is not admitted" :
                 r->backend == YVEX_BACKEND_KIND_METAL ? "Metal primitive support is not full-model admission" :
                 !c->operand_codecs_available ? "one or more operand codecs have no executable consumer" :
                 c->artifact_catalog_compatibility == 2u ?
                 "physical recipe is outside the family's static artifact admission catalog" :
                 "required routed matrix class is incompatible with the physical operands";
    } else if (!r->allow_approximation && c->approximate_tensors) {
        c->state = YVEX_OPTIMIZATION_NUMERICAL_INCOMPATIBLE;
        reason = "recipe approximates weights under a source-preserving request";
    } else if (c->initial_required_bytes > c->memory_budget_bytes) {
        c->state = YVEX_OPTIMIZATION_RESOURCE_INFEASIBLE;
        reason = "weights plus largest-tensor transient and unchanged reserve already exceed budget";
    }
    yvex_core_text_copy(c->reason, sizeof(c->reason), reason);
    rc = assess_identity(r, c, err);
    if (rc == YVEX_OK) yvex_error_clear(err);
    return rc;
}

static int routed_pair_compatible(const yvex_quant_plan *plan,
                                  const yvex_quant_decision *decision)
{
    yvex_tensor_role peer_role;
    const yvex_quant_plan_summary *summary = yvex_quant_plan_summary_get(plan);
    unsigned int peers = 0u;
    if (decision->role == YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN) return 1;
    peer_role = decision->role == YVEX_TENSOR_ROLE_MOE_EXPERT_GATE ?
        YVEX_TENSOR_ROLE_MOE_EXPERT_UP : YVEX_TENSOR_ROLE_MOE_EXPERT_GATE;
    for (unsigned long long i = 0ull; summary && i < summary->decision_count; ++i) {
        const yvex_quant_decision *peer = yvex_quant_plan_decision_at(plan, i);
        if (!peer || peer->role != peer_role || peer->scope != decision->scope ||
            peer->logical_key.layer_index != decision->logical_key.layer_index ||
            peer->logical_key.auxiliary_index != decision->logical_key.auxiliary_index) continue;
        if (peer->qtype != decision->qtype || peer->row_width != decision->row_width ||
            peer->physical_expert_count != decision->physical_expert_count) return 0;
        peers++;
    }
    /* Current paired implementation admits homogeneous gate/up operands.
     * Individual codec availability must not hide a mixed or missing pair. */
    return peers == 1u;
}

static int collect(const yvex_physical_variant_view *view,
                   const yvex_optimization_request *r,
                   const yvex_family_compiler_adapter *adapter,
                   yvex_optimization_candidate *c, yvex_error *err)
{
    const yvex_quant_plan_summary *plan = view ? yvex_quant_plan_summary_get(view->plan) : NULL;
    const yvex_gguf_writer_plan_summary *writer = view ?
        yvex_gguf_writer_plan_summary_get(view->writer) : NULL;
    if (!plan || !plan->complete || !writer || !writer->complete)
        return refuse(err, YVEX_ERR_STATE, "compiler did not publish complete physical plans");
    yvex_core_text_copy(c->physical_variant_identity, sizeof(c->physical_variant_identity),
                        plan->physical_variant_identity);
    yvex_core_text_copy(c->source_identity, sizeof(c->source_identity), plan->required_payload_identity);
    yvex_core_text_copy(c->transform_identity, sizeof(c->transform_identity), plan->transform_identity);
    yvex_core_text_copy(c->policy_identity, sizeof(c->policy_identity), plan->policy_identity);
    c->encoded_bytes = plan->encoded_bytes;
    c->artifact_bytes = writer->final_file_bytes;
    c->operand_codecs_available = 1;
    const yvex_complete_artifact_admission *catalog = adapter->artifact_constraint ?
        adapter->artifact_constraint() : NULL;
    if (catalog) {
        int match = !strcmp(catalog->profile_identity, plan->physical_variant_identity) &&
            !strcmp(catalog->payload_identity, plan->required_payload_identity) &&
            !strcmp(catalog->transform_identity, plan->transform_identity) &&
            !strcmp(catalog->writer_plan_identity, writer->writer_plan_identity) &&
            catalog->file_bytes == writer->final_file_bytes;
        c->artifact_catalog_compatibility = match ? 1u :
            adapter->binding_compile == yvex_family_binding_compile ? 3u : 2u;
    }
    for (unsigned long long i = 0ull; i < plan->decision_count; ++i) {
        const yvex_quant_decision *d = yvex_quant_plan_decision_at(view->plan, i);
        if (!d) return refuse(err, YVEX_ERR_STATE, "missing physical decision");
        if (d->encoded_bytes > c->maximum_tensor_bytes) c->maximum_tensor_bytes = d->encoded_bytes;
        c->approximate_tensors += d->approximation != 0;
        c->operand_codecs_available &= r->backend == YVEX_BACKEND_KIND_CPU
            ? d->cpu_compute_available != 0 : d->cuda_compute_available != 0;
        if (d->role == YVEX_TENSOR_ROLE_MOE_EXPERT_GATE ||
            d->role == YVEX_TENSOR_ROLE_MOE_EXPERT_UP ||
            d->role == YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN) {
            c->routed_tensors++;
            if (!yvex_execution_routed_matrix_operand_admitted(r->backend, r->compute_major,
                                                               d->role, d->qtype, d->row_width) ||
                !routed_pair_compatible(view->plan, d))
                c->matrix_incompatible_tensors++;
        }
    }
    return yvex_optimization_candidate_assess(r, c, err);
}

static int synthesize_policy(const yvex_optimization_request *request,
                             unsigned int index, yvex_quant_policy **out,
                             yvex_error *err)
{
    const yvex_quant_numeric_capability *capability =
        yvex_quant_numeric_capability_by_name(synthesis[index].codec);
    yvex_quant_policy_rule rules[4] = {{0}};
    yvex_quant_policy_rule *rule = &rules[0];
    unsigned long long count = 1ull;
    yvex_quant_policy_definition definition;
    *out = NULL;
    if (!capability || !capability->encoder_available || !capability->deterministic_encoding ||
        !capability->reference_decoder_available ||
        (request->backend == YVEX_BACKEND_KIND_CPU && !capability->dedicated_cpu_compute_available) ||
        (request->backend == YVEX_BACKEND_KIND_CUDA && !capability->dedicated_cuda_compute_available))
        return refuse(err, YVEX_ERR_UNSUPPORTED, "candidate codec lacks a deterministic executable producer");
    rule->schema_version = YVEX_QUANT_POLICY_SCHEMA_VERSION;
    rule->match_mask = YVEX_QUANT_MATCH_PHYSICAL_CLASS;
    rule->physical_class = YVEX_QUANT_POLICY_PHYSICAL_QUANTIZABLE;
    rule->qtype = synthesis[index].qtype;
    rule->requires_imatrix = capability->calibration == YVEX_QUANT_CALIBRATION_REQUIRED;
    rule->priority = 10u;
    rule->label = "bounded goal-driven candidate; independent quality unqualified";
    if (synthesis[index].routed_policy) {
        static const yvex_tensor_role roles[] = {YVEX_TENSOR_ROLE_MOE_EXPERT_GATE,
            YVEX_TENSOR_ROLE_MOE_EXPERT_UP, YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN};
        int matrix = synthesis[index].routed_policy == 1u;
        if (matrix && (request->backend != YVEX_BACKEND_KIND_CUDA || request->compute_major < 8u))
            return refuse(err, YVEX_ERR_UNSUPPORTED, "routed matrix candidate has no consumer on selected backend");
        if (matrix && !request->imatrix_path)
            return refuse(err, YVEX_ERR_UNSUPPORTED,
                "routed matrix recipe requires checkpoint-matched imatrix; no calibration invented");
        for (unsigned int i = 0u; i < 3u; ++i) {
            rules[i + 1u] = *rule;
            rules[i + 1u].match_mask |= YVEX_QUANT_MATCH_ROLE;
            rules[i + 1u].role = roles[i];
            rules[i + 1u].priority = 20u;
            unsigned int qtype = matrix ? yvex_execution_routed_matrix_qtype(roles[i]) :
                YVEX_GGUF_QTYPE_Q2_K;
            if (qtype == YVEX_GGUF_QTYPE_IQ2_XXS) rules[i + 1u].qtype = YVEX_QUANT_QTYPE_IQ2_XXS;
            else if (qtype == YVEX_GGUF_QTYPE_Q2_K) rules[i + 1u].qtype = YVEX_QUANT_QTYPE_Q2_K;
            else return refuse(err, YVEX_ERR_UNSUPPORTED, "routed matrix codec has no policy representation");
            rules[i + 1u].requires_imatrix = qtype == YVEX_GGUF_QTYPE_IQ2_XXS;
        }
        count = 4ull;
    }
    definition = (yvex_quant_policy_definition){synthesis[index].name, request->target_id,
        "goal-driven-search-v1", rules, count};
    return yvex_quant_policy_create_definition(out, &definition, err);
}

int yvex_optimization_search_open(yvex_optimization_search **out,
                                 const yvex_optimization_request *r, yvex_error *err)
{
    const yvex_graph_execution_binding *execution;
    const yvex_physical_variant_api *api;
    yvex_optimization_search *search;
    unsigned long long count, presets;
    int rc;
    if (out) *out = NULL;
    rc = request_validate(r, err);
    if (rc != YVEX_OK) return rc;
    if (!out || !r->target_id || !r->source_path || !r->models_root || !r->source_manifest_path)
        return refuse(err, YVEX_ERR_INVALID_ARG, "exact target and authenticated source inputs required");
    execution = yvex_graph_execution_find(0ull, 0ull, r->target_id);
    api = execution && execution->compiler &&
        execution->compiler->schema_version == YVEX_FAMILY_COMPILER_SCHEMA_V3 && execution->compiler->physical_variant
        ? execution->compiler->physical_variant() : NULL;
    if (!api || api->schema_version != YVEX_PHYSICAL_VARIANT_API_SCHEMA_V2 ||
        !api->open || !api->view || !api->close)
        return refuse(err, YVEX_ERR_UNSUPPORTED, "target has no admitted physical compiler");
    presets = r->policy_path ? 1ull : yvex_quant_policy_target_preset_count(r->target_id);
    count = presets + (r->policy_path ? 0ull : sizeof(synthesis) / sizeof(synthesis[0]));
    if (!presets)
        return refuse(err, YVEX_ERR_UNSUPPORTED, "target has no canonical recipe population");
    if (!count || count > r->maximum_candidates)
        return refuse(err, YVEX_ERR_BOUNDS, "recipe population is empty or exceeds explicit search budget");
    search = calloc(1u, sizeof(*search));
    if (!search) return refuse(err, YVEX_ERR_NOMEM, "bounded search allocation failed");
    rc = yvex_optimization_context_resolve(r, &search->context, err);
    if (rc != YVEX_OK) { free(search); return rc; }
    for (unsigned int i = 0u; i < count; ++i) {
        yvex_physical_variant_session *session = NULL;
        yvex_quant_policy *policy = NULL;
        yvex_optimization_candidate *c = &search->candidates[i];
        const char *preset = r->policy_path || i >= presets ? NULL :
            yvex_quant_policy_target_preset_name(r->target_id, i);
        yvex_physical_variant_request variant = {
            .target_id = r->target_id, .source_path = r->source_path, .models_root = r->models_root,
            .source_manifest_path = r->source_manifest_path, .quant_preset_name = preset,
            .quant_policy_path = r->policy_path, .imatrix_path = r->imatrix_path,
            .backend = NULL, .worker_count = 1u
        };
        c->schema_version = YVEX_OPTIMIZATION_SCHEMA_V1;
        c->missing_evidence = YVEX_OPTIMIZATION_MISSING_WORKSPACE |
            YVEX_OPTIMIZATION_MISSING_STATE | YVEX_OPTIMIZATION_MISSING_MODEL_EXECUTION |
            YVEX_OPTIMIZATION_MISSING_QUALITY | YVEX_OPTIMIZATION_MISSING_PERFORMANCE |
            YVEX_OPTIMIZATION_MISSING_LIFECYCLE;
        yvex_core_text_copy(c->recipe, sizeof(c->recipe), i >= presets ?
            synthesis[i - presets].name : preset ? preset : "explicit-policy");
        rc = i >= presets ? synthesize_policy(r, (unsigned int)(i - presets), &policy, err) :
            r->policy_path ? yvex_quant_policy_open(&policy, r->policy_path, err) :
            yvex_quant_policy_preset_open(&policy, preset, err);
        variant.quant_preset_name = NULL;
        variant.quant_policy_path = NULL;
        variant.quant_policy = policy;
        if (rc == YVEX_OK) rc = api->open(&session, &variant, err);
        if (rc == YVEX_OK) rc = collect(api->view(session), r, execution->compiler, c, err);
        api->close(&session);
        if (rc != YVEX_OK) {
            yvex_quant_policy_close(policy);
            /* Never hide integrity/IO/state errors as a merely lower-ranked recipe. */
            if (rc != YVEX_ERR_UNSUPPORTED) {
                yvex_optimization_search_close(&search);
                return rc;
            }
            c->state = YVEX_OPTIMIZATION_COMPILATION_REFUSED;
            c->failure_status = rc;
            yvex_core_text_copy(c->reason, sizeof(c->reason), err ? err->message :
                "recipe prerequisites are not admitted");
        } else search->policies[i] = policy;
        search->count++;
    }
    prioritize(search, r->goal);
    *out = search;
    yvex_error_clear(err);
    return YVEX_OK;
}

typedef struct {
    yvex_optimization_allocation_group groups[YVEX_OPTIMIZATION_MAX_GROUPS];
    unsigned int role_group[YVEX_TENSOR_ROLE_COUNT], count;
    unsigned long long fixed_bytes;
} role_allocation;

static yvex_tensor_role coupled_role(yvex_tensor_role role)
{
    return role == YVEX_TENSOR_ROLE_MOE_EXPERT_UP ? YVEX_TENSOR_ROLE_MOE_EXPERT_GATE : role;
}

static int allocation_collect(const yvex_quant_plan *low, const yvex_quant_plan *high,
    const yvex_optimization_request *r, role_allocation *allocation, yvex_error *err)
{
    const yvex_quant_plan_summary *a = yvex_quant_plan_summary_get(low);
    const yvex_quant_plan_summary *b = yvex_quant_plan_summary_get(high);
    if (!a || !b || !a->complete || !b->complete || a->decision_count != b->decision_count ||
        strcmp(a->transform_identity, b->transform_identity) ||
        strcmp(a->required_payload_identity, b->required_payload_identity))
        return refuse(err, YVEX_ERR_STATE, "allocation plans must share exact terminal lineage");
    memset(allocation, 0, sizeof(*allocation));
    for (unsigned int i = 0u; i < YVEX_TENSOR_ROLE_COUNT; ++i)
        allocation->role_group[i] = YVEX_OPTIMIZATION_MAX_GROUPS;
    for (unsigned long long i = 0ull; i < a->decision_count; ++i) {
        const yvex_quant_decision *lo = yvex_quant_plan_decision_at(low, i);
        const yvex_quant_decision *hi = yvex_quant_plan_decision_at(high, i);
        if (!lo || !hi || lo->role != hi->role || lo->terminal_value_id != hi->terminal_value_id ||
            lo->element_count != hi->element_count || (unsigned int)lo->role >= YVEX_TENSOR_ROLE_COUNT)
            return refuse(err, YVEX_ERR_STATE, "allocation terminal geometry changed");
        int routed = lo->role == YVEX_TENSOR_ROLE_MOE_EXPERT_GATE ||
            lo->role == YVEX_TENSOR_ROLE_MOE_EXPERT_UP || lo->role == YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN;
        /* A required matrix class cannot acquire incompatible source operands.
         * Keep those groups fixed at the lower plan; final collect independently
         * validates the complete consumer, including homogeneous paired codecs. */
        if (!lo->approximation || hi->approximation || lo->qtype == hi->qtype ||
            (r->require_routed_matrix && routed)) {
            if (!yvex_core_u64_add(allocation->fixed_bytes, lo->encoded_bytes, &allocation->fixed_bytes))
                return refuse(err, YVEX_ERR_BOUNDS, "fixed terminal bytes overflow");
            continue;
        }
        yvex_tensor_role role = coupled_role(lo->role);
        unsigned int group = allocation->role_group[role];
        if (group == YVEX_OPTIMIZATION_MAX_GROUPS) {
            if (allocation->count == YVEX_OPTIMIZATION_MAX_GROUPS)
                return refuse(err, YVEX_ERR_BOUNDS, "model role population exceeds allocation group budget");
            group = allocation->count++;
            allocation->role_group[role] = group;
        }
        allocation->role_group[lo->role] = group;
        yvex_optimization_allocation_group *g = &allocation->groups[group];
        if (!yvex_core_u64_add(g->encoded_bytes[0], lo->encoded_bytes, &g->encoded_bytes[0]) ||
            !yvex_core_u64_add(g->encoded_bytes[1], hi->encoded_bytes, &g->encoded_bytes[1]) ||
            !yvex_core_u64_add(g->source_elements[1], hi->element_count, &g->source_elements[1]))
            return refuse(err, YVEX_ERR_BOUNDS, "role allocation geometry overflow");
    }
    return allocation->count ? YVEX_OK :
        refuse(err, YVEX_ERR_UNSUPPORTED, "no legal source-retention allocation groups in this model");
}

static int allocation_policy(const yvex_optimization_request *r, const role_allocation *a,
    unsigned long long choices, yvex_quant_policy **out, yvex_error *err)
{
    yvex_quant_policy_rule rules[YVEX_TENSOR_ROLE_COUNT + 1u] = {{0}};
    unsigned int count = 1u;
    char name[64];
    rules[0].schema_version = YVEX_QUANT_POLICY_SCHEMA_VERSION;
    rules[0].match_mask = YVEX_QUANT_MATCH_PHYSICAL_CLASS;
    rules[0].physical_class = YVEX_QUANT_POLICY_PHYSICAL_QUANTIZABLE;
    rules[0].qtype = YVEX_QUANT_QTYPE_Q2_K;
    rules[0].priority = 10u;
    rules[0].label = "source-retention-allocation-v1";
    for (unsigned int role = 0u; role < YVEX_TENSOR_ROLE_COUNT; ++role) {
        unsigned int group = a->role_group[role];
        if (group >= a->count || !(choices & (1ull << group))) continue;
        rules[count] = rules[0];
        rules[count].match_mask |= YVEX_QUANT_MATCH_ROLE;
        rules[count].role = (yvex_tensor_role)role;
        rules[count].qtype = YVEX_QUANT_QTYPE_SOURCE;
        rules[count++].priority = 20u;
    }
    snprintf(name, sizeof(name), "source-retention-v1-%016llx", choices);
    yvex_quant_policy_definition definition = {name, r->target_id,
        "source-retention-allocation-v1", rules, count};
    return yvex_quant_policy_create_definition(out, &definition, err);
}

int yvex_optimization_search_allocate(yvex_optimization_search **out,
    const yvex_optimization_request *r, unsigned long long weight_budget,
    unsigned int maximum_states, yvex_error *err)
{
    yvex_optimization_search *search = NULL;
    yvex_physical_variant_session *sessions[2] = {NULL, NULL};
    yvex_quant_policy *policies[2] = {NULL, NULL};
    role_allocation allocation;
    yvex_optimization_allocation frontier[YVEX_OPTIMIZATION_MAX_CANDIDATES];
    unsigned int count = 0u;
    int rc = request_validate(r, err);
    if (out) *out = NULL;
    if (rc != YVEX_OK) return rc;
    if (!out || !r->target_id || !r->source_path || !r->models_root ||
        !r->source_manifest_path || !weight_budget || !maximum_states || maximum_states > 65536u ||
        r->policy_path || !r->allow_approximation)
        return refuse(err, YVEX_ERR_INVALID_ARG,
            "allocation requires weight/state budgets, explicit approximation and no conflicting policy");
    const yvex_graph_execution_binding *execution = yvex_graph_execution_find(0ull, 0ull, r->target_id);
    const yvex_family_compiler_adapter *adapter = execution ? execution->compiler : NULL;
    const yvex_physical_variant_api *api = adapter && adapter->schema_version == YVEX_FAMILY_COMPILER_SCHEMA_V3 &&
        adapter->physical_variant ? adapter->physical_variant() : NULL;
    if (!api || api->schema_version != YVEX_PHYSICAL_VARIANT_API_SCHEMA_V2 ||
        !api->open || !api->view || !api->close)
        return refuse(err, YVEX_ERR_UNSUPPORTED, "target has no admitted allocation producer");
    search = calloc(1u, sizeof(*search));
    if (!search) return refuse(err, YVEX_ERR_NOMEM, "allocation search storage unavailable");
    rc = yvex_optimization_context_resolve(r, &search->context, err);
    yvex_physical_variant_request variant = {.target_id = r->target_id, .source_path = r->source_path,
        .models_root = r->models_root, .source_manifest_path = r->source_manifest_path,
        .imatrix_path = r->imatrix_path, .worker_count = 1u};
    for (unsigned int i = 0u; i < 2u && rc == YVEX_OK; ++i) {
        rc = synthesize_policy(r, i ? 0u : 2u, &policies[i], err);
        variant.quant_policy = policies[i];
        if (rc == YVEX_OK) rc = api->open(&sessions[i], &variant, err);
    }
    if (rc == YVEX_OK) rc = allocation_collect(api->view(sessions[0])->plan,
        api->view(sessions[1])->plan, r, &allocation, err);
    if (rc == YVEX_OK) {
        yvex_optimization_allocation_request allocation_request = {
            YVEX_OPTIMIZATION_ALLOCATION_SCHEMA_V1, allocation.count, maximum_states,
            allocation.fixed_bytes, weight_budget, allocation.groups};
        rc = yvex_optimization_allocate(&allocation_request, frontier, r->maximum_candidates, &count, err);
    }
    for (unsigned int i = 0u; i < 2u; ++i) {
        api->close(&sessions[i]);
        yvex_quant_policy_close(policies[i]);
    }
    for (unsigned int i = 0u; i < count && rc == YVEX_OK; ++i) {
        yvex_physical_variant_session *session = NULL;
        yvex_optimization_candidate *c = &search->candidates[i];
        c->schema_version = YVEX_OPTIMIZATION_SCHEMA_V1;
        rc = allocation_policy(r, &allocation, frontier[i].choices, &search->policies[i], err);
        variant.quant_policy = search->policies[i];
        if (rc == YVEX_OK) rc = api->open(&session, &variant, err);
        if (rc == YVEX_OK) rc = collect(api->view(session), r, adapter, c, err);
        api->close(&session);
        if (rc == YVEX_OK && c->encoded_bytes != frontier[i].encoded_bytes)
            rc = refuse(err, YVEX_ERR_STATE, "allocated cost disagrees with canonical compiled bytes");
        snprintf(c->recipe, sizeof(c->recipe), "source-retention-v1-%016llx", frontier[i].choices);
        search->count++;
    }
    if (rc != YVEX_OK) { yvex_optimization_search_close(&search); return rc; }
    prioritize(search, r->goal);
    *out = search;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_optimization_search_close(yvex_optimization_search **search)
{
    if (search && *search) {
        for (unsigned int i = 0u; i < YVEX_OPTIMIZATION_MAX_CANDIDATES; ++i)
            yvex_quant_policy_close((*search)->policies[i]);
        free(*search); *search = NULL;
    }
}

int yvex_optimization_search_policy(const yvex_optimization_search *search,
                                   const char *identity,
                                   const yvex_quant_policy **out, yvex_error *err)
{
    if (out) *out = NULL;
    if (!search || !out || !yvex_sha256_hex_valid(identity))
        return refuse(err, YVEX_ERR_INVALID_ARG, "exact candidate identity required");
    for (unsigned int i = 0u; i < search->count; ++i) {
        if (strcmp(identity, search->candidates[i].candidate_identity)) continue;
        if (!search->policies[i] || search->candidates[i].state != YVEX_OPTIMIZATION_NEEDS_QUALIFICATION)
            return refuse(err, YVEX_ERR_UNSUPPORTED, "refused candidate cannot be selected for production");
        *out = search->policies[i];
        yvex_error_clear(err);
        return YVEX_OK;
    }
    return refuse(err, YVEX_ERR_STATE, "candidate identity is not in this exact search");
}

unsigned int yvex_optimization_search_count(const yvex_optimization_search *search)
{
    return search ? search->count : 0u;
}

const yvex_optimization_context *yvex_optimization_search_context(
    const yvex_optimization_search *search)
{
    return search ? &search->context : NULL;
}

const yvex_optimization_candidate *yvex_optimization_search_at(
    const yvex_optimization_search *search, unsigned int index)
{
    return search && index < search->count ? &search->candidates[index] : NULL;
}
