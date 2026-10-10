#include <yvex/optimization.h>
#include <yvex/quant.h>
#include <yvex/qtype.h>
#include <yvex/internal/deployment.h>
#include "tests/test.h"
#include <string.h>
#include <math.h>

#define GIB (1024ull * 1024ull * 1024ull)

static yvex_optimization_request request(void)
{
    yvex_optimization_request r = {0};
    r.schema_version = YVEX_OPTIMIZATION_SCHEMA_V1;
    r.backend = YVEX_BACKEND_KIND_CUDA;
    r.compute_major = 12u; r.compute_minor = 1u; r.device_count = 1u;
    r.maximum_candidates = 8u;
    r.context_tokens = 32768ull; r.prefill_tokens = 512ull; r.concurrent_sequences = 1ull;
    r.system_memory_bytes = 128ull * GIB; r.available_memory_bytes = 120ull * GIB;
    r.allow_approximation = 1;
    return r;
}

static yvex_optimization_candidate candidate(void)
{
    yvex_optimization_candidate c = {0};
    c.schema_version = YVEX_OPTIMIZATION_SCHEMA_V1;
    memset(c.physical_variant_identity, 'a', 64u);
    memset(c.source_identity, 'b', 64u);
    c.artifact_bytes = 100ull * GIB + 4096ull;
    c.encoded_bytes = 100ull * GIB; c.maximum_tensor_bytes = GIB;
    c.approximate_tensors = 129ull; c.routed_tensors = 129ull;
    c.matrix_incompatible_tensors = 86ull; c.operand_codecs_available = 1;
    return c;
}

static int measured_selection(void)
{
    const char *a = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    const char *b = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    const char *c = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
    yvex_optimization_constraints constraints = {
        .schema_version = 1u, .minimum_samples = 3u, .maximum_working_bytes = 120ull * GIB,
        .maximum_quality_loss = .1, .minimum_prefill = 700.0, .minimum_decode = 20.0};
    yvex_optimization_observation rows[3] = {{
        .schema_version = 1u, .samples = 3u, .candidate_identity = a,
        .comparison_identity = c, .quality_reference_identity = c, .evidence_identity = a,
        .prefill_tokens_per_second = 700.0, .decode_tokens_per_second = 20.0,
        .ttft_seconds = 1.0, .preparation_seconds = 60.0, .quality_loss = .05,
        .peak_working_bytes = 100ull * GIB}};
    yvex_optimization_selection out[3], unchanged[3];
    yvex_error err = {0};
    rows[1] = rows[0]; rows[1].candidate_identity = b; rows[1].evidence_identity = b;
    rows[2] = rows[0]; rows[2].candidate_identity = c; rows[2].evidence_identity = c;
    rows[1].decode_tokens_per_second = 24.0;
    rows[2].decode_tokens_per_second = 26.0; rows[2].peak_working_bytes = 110ull * GIB;
    YVEX_TEST_ASSERT(yvex_optimization_select(&constraints, rows, 3u, out, &err) == YVEX_OK &&
        out[0].state == YVEX_OPTIMIZATION_DOMINATED && out[0].dominator == 1u &&
        out[1].state == YVEX_OPTIMIZATION_FRONTIER && out[2].state == YVEX_OPTIMIZATION_FRONTIER,
        "measured faster/heavier tradeoff retains both Pareto candidates");
    rows[1].missing_evidence = YVEX_OPTIMIZATION_MISSING_QUALITY;
    rows[1].quality_reference_identity = NULL;
    rows[1].quality_loss = NAN; /* unavailable, never a zero-error claim */
    rows[2].samples = 1u;
    YVEX_TEST_ASSERT(yvex_optimization_select(&constraints, rows, 3u, out, &err) == YVEX_OK &&
        out[0].state == YVEX_OPTIMIZATION_FRONTIER &&
        out[1].state == YVEX_OPTIMIZATION_EVIDENCE_INCOMPLETE &&
        out[2].state == YVEX_OPTIMIZATION_EVIDENCE_INCOMPLETE,
        "absent independent reference or insufficient repetitions cannot dominate measured evidence");
    rows[0].quality_reference_identity = NULL;
    YVEX_TEST_ASSERT(yvex_optimization_select(&constraints, rows, 3u, out, &err) == YVEX_ERR_INVALID_ARG,
        "known-quality claim without its reference refuses");
    rows[0].quality_reference_identity = c;
    yvex_optimization_observation reordered[] = {rows[1], rows[0], rows[2]};
    reordered[2].quality_reference_identity = b;
    YVEX_TEST_ASSERT(yvex_optimization_select(&constraints, reordered, 3u, out, &err) == YVEX_ERR_INVALID_ARG,
        "an unknown first reference cannot hide disagreement between later known references");
    rows[1].missing_evidence = 0u;
    rows[1].quality_reference_identity = c;
    rows[1].quality_loss = .2;
    rows[2].samples = 3u; rows[2].peak_working_bytes = 121ull * GIB;
    YVEX_TEST_ASSERT(yvex_optimization_select(&constraints, rows, 3u, out, &err) == YVEX_OK &&
        out[1].state == YVEX_OPTIMIZATION_OUTSIDE_CONSTRAINTS &&
        out[2].state == YVEX_OPTIMIZATION_OUTSIDE_CONSTRAINTS,
        "speed does not override quality floor or memory ceiling");
    rows[1] = rows[0]; rows[1].candidate_identity = b;
    YVEX_TEST_ASSERT(yvex_optimization_select(&constraints, rows, 2u, out, &err) == YVEX_OK &&
        out[0].state == YVEX_OPTIMIZATION_FRONTIER && out[1].state == YVEX_OPTIMIZATION_FRONTIER,
        "ties are not silently ranked by candidate order");
    memcpy(unchanged, out, sizeof(out));
    rows[1].comparison_identity = b;
    YVEX_TEST_ASSERT(yvex_optimization_select(&constraints, rows, 2u, out, &err) == YVEX_ERR_INVALID_ARG &&
        !memcmp(out, unchanged, sizeof(out)), "incompatible comparison refuses without partial selection");
    rows[1].comparison_identity = c; rows[1].quality_reference_identity = b;
    YVEX_TEST_ASSERT(yvex_optimization_select(&constraints, rows, 2u, out, &err) == YVEX_ERR_INVALID_ARG,
        "quality references cannot be substituted across candidates");
    rows[1].quality_reference_identity = c; rows[1].decode_tokens_per_second = INFINITY;
    YVEX_TEST_ASSERT(yvex_optimization_select(&constraints, rows, 2u, out, &err) == YVEX_ERR_INVALID_ARG,
        "nonfinite rates cannot win selection");
    rows[1] = rows[0];
    YVEX_TEST_ASSERT(yvex_optimization_select(&constraints, rows, 2u, out, &err) == YVEX_ERR_INVALID_ARG,
        "duplicate candidates do not manufacture evidence population");
    rows[1].candidate_identity = b; rows[1].schema_version++;
    YVEX_TEST_ASSERT(yvex_optimization_select(&constraints, rows, 2u, out, &err) == YVEX_ERR_INVALID_ARG,
        "stale observation schema refuses before interpreting its facts");
    return 0;
}

static int allocation_controls(void)
{
    yvex_optimization_allocation_group groups[5];
    yvex_optimization_allocation rows[32], before[32];
    yvex_optimization_allocation_request r = {
        YVEX_OPTIMIZATION_ALLOCATION_SCHEMA_V1, 5u, 64u, 7ull, 90ull, groups};
    yvex_error err;
    unsigned int count = 0u;
    /* Independent exhaustive oracle, varying costs/retention/ties and budget.
     * The reference enumerates all complete assignments, not the DP recurrence. */
    for (unsigned int seed = 0u; seed < 64u; ++seed) {
        for (unsigned int i = 0u; i < 5u; ++i) {
            groups[i].encoded_bytes[0] = 2ull + (seed + i) % 5u;
            groups[i].encoded_bytes[1] = groups[i].encoded_bytes[0] + 1ull + (seed * 3u + i * 7u) % 11u;
            groups[i].source_elements[0] = 0ull;
            groups[i].source_elements[1] = 1ull + (seed + 13u * i) % 23u;
        }
        r.maximum_encoded_bytes = 30ull + seed;
        YVEX_TEST_ASSERT(yvex_optimization_allocate(&r, rows, 32u, &count, &err) == YVEX_OK,
            "exact role allocation must resolve a bounded feasible population");
        unsigned int expected = 0u;
        for (unsigned int mask = 0u; mask < 32u; ++mask) {
            unsigned long long bytes = r.fixed_bytes, retained = 0ull;
            for (unsigned int i = 0u; i < 5u; ++i) {
                bytes += groups[i].encoded_bytes[(mask >> i) & 1u];
                retained += groups[i].source_elements[(mask >> i) & 1u];
            }
            if (bytes > r.maximum_encoded_bytes) continue;
            int dominated = 0;
            for (unsigned int peer = 0u; peer < 32u; ++peer) {
                unsigned long long cost = r.fixed_bytes, value = 0ull;
                for (unsigned int i = 0u; i < 5u; ++i) {
                    cost += groups[i].encoded_bytes[(peer >> i) & 1u];
                    value += groups[i].source_elements[(peer >> i) & 1u];
                }
                if (cost <= bytes && value >= retained &&
                    (cost < bytes || value > retained || peer < mask)) dominated = 1;
            }
            if (dominated) continue;
            expected++;
            unsigned int matched = 0u;
            for (unsigned int i = 0u; i < count; ++i)
                matched += rows[i].choices == mask && rows[i].encoded_bytes == bytes &&
                    rows[i].source_elements == retained;
            YVEX_TEST_ASSERT(matched == 1u, "native frontier must equal independent exhaustive assignments");
        }
        YVEX_TEST_ASSERT(count == expected, "no extra or missing source-retention alternatives");
    }
    memset(rows, 0x5a, sizeof(rows)); memcpy(before, rows, sizeof(rows)); count = 77u;
    r.maximum_encoded_bytes = 1ull;
    YVEX_TEST_ASSERT(yvex_optimization_allocate(&r, rows, 32u, &count, &err) == YVEX_ERR_NOMEM &&
        count == 77u && !memcmp(rows, before, sizeof(rows)), "infeasible allocation is failure atomic");
    r.maximum_encoded_bytes = 1000ull; r.maximum_states = 1u;
    YVEX_TEST_ASSERT(yvex_optimization_allocate(&r, rows, 32u, &count, &err) == YVEX_ERR_BOUNDS &&
        count == 77u && !memcmp(rows, before, sizeof(rows)), "frontier exhaustion cannot invent exact optimality");
    r.maximum_states = 64u;
    YVEX_TEST_ASSERT(yvex_optimization_allocate(&r, rows, 1u, &count, &err) == YVEX_ERR_BOUNDS &&
        count == 77u && !memcmp(rows, before, sizeof(rows)), "small output never publishes a partial frontier");
    r.fixed_bytes = ~0ull;
    YVEX_TEST_ASSERT(yvex_optimization_allocate(&r, rows, 32u, &count, &err) == YVEX_ERR_BOUNDS,
        "allocation byte arithmetic is checked");
    r.schema_version++;
    YVEX_TEST_ASSERT(yvex_optimization_allocate(&r, rows, 32u, &count, &err) == YVEX_ERR_INVALID_ARG,
        "stale allocation schema refuses before group access");
    return 0;
}

int yvex_test_optimization(void)
{
    YVEX_TEST_ASSERT(allocation_controls() == 0, "bounded allocation controls");
    YVEX_TEST_ASSERT(yvex_optimization_technique_count() == 2u &&
        yvex_optimization_technique_at(2u) == NULL, "only implemented techniques are registered");
    for (unsigned int i = 0u; i < 2u; ++i) {
        const yvex_optimization_technique *method = yvex_optimization_technique_at(i);
        yvex_optimization_search *search = NULL;
        yvex_error failure = {0};
        YVEX_TEST_ASSERT(method && method->schema_version == 1u && method->identity &&
            method->objective && method->unearned_evidence, "versioned executable technique contract");
        YVEX_TEST_ASSERT(yvex_optimization_search_run(&search, NULL, method->identity, 0u, 0u,
            &failure) == YVEX_ERR_INVALID_ARG && !search, "technique dispatch retains native refusal");
        YVEX_TEST_ASSERT(yvex_optimization_search_run(&search, NULL, "future-unimplemented-v1", 0u, 0u,
            &failure) == YVEX_ERR_UNSUPPORTED && !search, "research is not an executable capability");
    }
    YVEX_TEST_ASSERT(measured_selection() == 0, "bounded measured-selection controls");
    yvex_optimization_request r = request();
    yvex_optimization_candidate c = candidate();
    yvex_error err = {0};
    char identity[65];
    YVEX_TEST_ASSERT(yvex_optimization_candidate_assess(&r, &c, &err) == YVEX_OK &&
        c.initial_required_bytes == 117ull * GIB &&
        c.state == YVEX_OPTIMIZATION_NEEDS_QUALIFICATION && c.missing_evidence == 63u,
        "initial fit is not workspace, state, execution, quality or performance qualification");
    yvex_optimization_candidate other = c;
    int order = 99;
    other.initial_required_bytes++;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_priority(YVEX_OPTIMIZATION_MEMORY,
        &c, &other, &order, &err) == YVEX_OK && order == -1,
        "memory experiment order uses known lower bounds, not imagined working sets");
    YVEX_TEST_ASSERT(yvex_optimization_candidate_priority(YVEX_OPTIMIZATION_BALANCED,
        &c, &other, &order, &err) == YVEX_OK && order == 0,
        "balanced order does not invent a speed versus memory score");
    other.approximate_tensors++;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_priority(YVEX_OPTIMIZATION_QUALITY,
        &c, &other, &order, &err) == YVEX_OK && order == 0,
        "approximate tensor count is not a model quality metric");
    other.approximate_tensors = 0ull;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_priority(YVEX_OPTIMIZATION_QUALITY,
        &c, &other, &order, &err) == YVEX_OK && order == 1,
        "source preservation can prioritize an experiment but cannot qualify model quality");
    other.matrix_incompatible_tensors = 0ull;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_priority(YVEX_OPTIMIZATION_THROUGHPUT,
        &c, &other, &order, &err) == YVEX_OK && order == 1,
        "compatible matrix operands are a static experiment hint, not measured throughput");
    other.state = YVEX_OPTIMIZATION_RESOURCE_INFEASIBLE;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_priority(YVEX_OPTIMIZATION_THROUGHPUT,
        &c, &other, &order, &err) == YVEX_OK && order == -1,
        "resource-refused acceleration never outranks eligible experiments");
    other.schema_version++;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_priority(YVEX_OPTIMIZATION_BALANCED,
        &c, &other, &order, &err) == YVEX_ERR_INVALID_ARG && order == 0,
        "stale assessment cannot enter an experiment order");
    memcpy(identity, c.candidate_identity, sizeof(identity));
    r.available_memory_bytes = 116ull * GIB;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_assess(&r, &c, &err) == YVEX_OK &&
        c.state == YVEX_OPTIMIZATION_RESOURCE_INFEASIBLE &&
        strcmp(identity, c.candidate_identity) == 0,
        "ephemeral available memory changes feasibility, not candidate identity");
    r.available_memory_bytes = 120ull * GIB; r.require_routed_matrix = 1;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_assess(&r, &c, &err) == YVEX_OK &&
        c.state == YVEX_OPTIMIZATION_IMPLEMENTATION_UNSUPPORTED &&
        strcmp(identity, c.candidate_identity), "incompatible operands cannot request matrix class");
    r.require_routed_matrix = 0; r.allow_approximation = 0;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_assess(&r, &c, &err) == YVEX_OK &&
        c.state == YVEX_OPTIMIZATION_NUMERICAL_INCOMPATIBLE, "goal cannot relax numerical constraints");
    r = request(); r.backend = YVEX_BACKEND_KIND_METAL;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_assess(&r, &c, &err) == YVEX_OK &&
        c.state == YVEX_OPTIMIZATION_IMPLEMENTATION_UNSUPPORTED, "Metal primitive is not model execution");
    r = request(); r.device_count = 2u;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_assess(&r, &c, &err) == YVEX_OK &&
        c.state == YVEX_OPTIMIZATION_IMPLEMENTATION_UNSUPPORTED, "summed memory is not topology admission");
    r = request(); c.artifact_catalog_compatibility = 2u;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_assess(&r, &c, &err) == YVEX_OK &&
        c.state == YVEX_OPTIMIZATION_IMPLEMENTATION_UNSUPPORTED,
        "valid encoding does not override a static artifact admission catalog");
    c.artifact_catalog_compatibility = 3u;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_assess(&r, &c, &err) == YVEX_OK &&
        c.state == YVEX_OPTIMIZATION_NEEDS_QUALIFICATION && c.missing_evidence == 63u &&
        strstr(c.reason, "complete production proof required"),
        "generic production path permits an experiment, not automatic artifact admission");
    c.artifact_catalog_compatibility = 1u;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_assess(&r, &c, &err) == YVEX_OK &&
        c.state == YVEX_OPTIMIZATION_NEEDS_QUALIFICATION,
        "matching a catalog constraint does not authenticate or qualify an artifact");
    r = request(); r.system_reserve_bytes = 15ull * GIB;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_assess(&r, &c, &err) == YVEX_ERR_INVALID_ARG,
        "refuse reserve weakening");
    r = request(); r.schema_version++;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_assess(&r, &c, &err) == YVEX_ERR_INVALID_ARG,
        "refuse stale layout before field interpretation");
    r = request(); c = candidate(); c.encoded_bytes = ~0ull; c.artifact_bytes = ~0ull;
    YVEX_TEST_ASSERT(yvex_optimization_candidate_assess(&r, &c, &err) == YVEX_ERR_BOUNDS,
        "resource arithmetic never wraps into fit");
    YVEX_TEST_ASSERT(yvex_execution_routed_matrix_operand_admitted(YVEX_BACKEND_KIND_CUDA, 12u,
        YVEX_TENSOR_ROLE_MOE_EXPERT_GATE, YVEX_GGUF_QTYPE_IQ2_XXS, 4096ull) &&
        yvex_execution_routed_matrix_operand_admitted(YVEX_BACKEND_KIND_CUDA, 12u,
        YVEX_TENSOR_ROLE_MOE_EXPERT_GATE, YVEX_GGUF_QTYPE_Q2_K, 4096ull) &&
        !yvex_execution_routed_matrix_operand_admitted(YVEX_BACKEND_KIND_CUDA, 12u,
        YVEX_TENSOR_ROLE_MOE_EXPERT_GATE, YVEX_GGUF_QTYPE_Q8_0, 4096ull) &&
        yvex_execution_routed_matrix_operand_admitted(YVEX_BACKEND_KIND_CUDA, 12u,
        YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN, YVEX_GGUF_QTYPE_Q2_K, 2048ull) &&
        !yvex_execution_routed_matrix_operand_admitted(YVEX_BACKEND_KIND_CPU, 12u,
        YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN, YVEX_GGUF_QTYPE_Q2_K, 2048ull),
        "planner and specialization share operand constraints");
    YVEX_TEST_ASSERT(yvex_quant_policy_target_preset_count("not-a-target") == 0ull &&
        !yvex_quant_policy_target_preset_name("not-a-target", 0ull), "exact target catalog refuses guessing");
    yvex_optimization_search *search = NULL;
    r = request(); r.target_id = "not-a-target"; r.source_path = "/absent";
    r.models_root = "/absent"; r.source_manifest_path = "/absent";
    YVEX_TEST_ASSERT(yvex_optimization_search_open(&search, &r, &err) == YVEX_ERR_UNSUPPORTED &&
        !search && !yvex_optimization_search_count(search) && !yvex_optimization_search_at(search, 0u),
        "no output or source IO when exact compiler is absent");
    yvex_optimization_search_close(&search);
    return 0;
}
