/*
 * yvex_model_target_candidates.c - model-target candidate report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   full-runtime candidate, dense candidate, and Qwen/Metal pressure report
 *   facts and builders.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, target catalog storage,
 *   sidecar writing, runtime execution, generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   candidate reports remain blocked/report-only until promoted by separate
 *   implementation proof rows.
 *
 * Boundary:
 *   candidate reporting does not create runtime capability, quantization,
 *   artifact emission, generation, benchmark, or release readiness.
 */
#include "yvex_model_target_candidates.h"

#include "yvex_model_target_catalog.h"
#include "yvex_model_target_private.h"

#include <string.h>

typedef struct {
    const char *id;
    const char *class_name;
    const char *stage;
    const char *eligibility;
    const char *status;
    const char *reason;
    const char *next;
} candidate_fact;

static const candidate_fact candidate_facts[] = {
    {"deepseek4-v4-flash-selected-embed", "selected-runtime-slice",
     "selected-slice", "selected-slice-only", "ineligible-selected-slice",
     "selected-runtime-slice missing full model tensor coverage",
     "V010.GRAPH.DEEPSEEK.TRANSFORMER.0"},
    {"deepseek4-v4-flash-selected-embed-rmsnorm", "selected-runtime-slice",
     "diagnostic-runtime", "selected-slice-only", "ineligible-selected-slice",
     "selected-runtime-slice missing MoE router/expert tensor coverage",
     "V010.GRAPH.DEEPSEEK.TRANSFORMER.0"},
    {"glm-5.2-official-safetensors", "huge-source-pressure", "report-only",
     "source-only", "ineligible-source-only", "source-only target",
     "POST010.GLM.RUNTIME.0"},
    {"qwen3-8b", "source-model-candidate", "source-target-profiled",
     "planned-portability-only", "ineligible-source-model-candidate",
     "source model candidate requires tensor role mapping",
     "V010.MODEL.ARCH.IR.0"},
    {"gemma-4-12b-it", "source-model-candidate", "source-target-profiled",
     "planned-dense-pressure-only", "ineligible-source-model-candidate",
     "source model candidate requires tensor role mapping",
     "V010.MODEL.ARCH.IR.0"},
    {"tests/fixtures/gguf/valid-tokenizer-simple.gguf", "fixture-artifact",
     "fixture", "fixture-only", "ineligible-fixture-only", "fixture only",
     "V010.GGUF.ARTIFACT.ABI.1"},
};

static unsigned long candidate_fact_count(void)
{
    return sizeof(candidate_facts) / sizeof(candidate_facts[0]);
}

static const candidate_fact *candidate_find(const char *id)
{
    unsigned long i;

    if (!id || !id[0]) return NULL;
    for (i = 0; i < candidate_fact_count(); ++i) {
        if (strcmp(candidate_facts[i].id, id) == 0) {
            return &candidate_facts[i];
        }
    }
    return NULL;
}

static const char *candidate_blocker0(const candidate_fact *fact,
                                      const char *prefix)
{
    if (!fact) {
        return "unknown-target";
    }
    if (strcmp(prefix, "dense_candidate") == 0 &&
        strncmp(fact->id, "deepseek", 8) == 0) {
        return "not-dense-target";
    }
    if (strcmp(prefix, "dense_candidate") == 0 &&
        strncmp(fact->id, "glm", 3) == 0) {
        return "moe-target";
    }
    if (strcmp(fact->class_name, "selected-runtime-slice") == 0) {
        return "selected-runtime-slice-only";
    }
    if (strncmp(fact->id, "qwen", 4) == 0) {
        return "planned-portability-only";
    }
    return fact->eligibility;
}

static const char *candidate_eligibility_for_prefix(const candidate_fact *fact,
                                                   const char *prefix)
{
    if (!fact) {
        return "unknown-target";
    }
    if (strcmp(prefix, "dense_candidate") != 0) {
        return fact->eligibility;
    }
    if (strncmp(fact->id, "deepseek", 8) == 0) {
        return "not-dense-target";
    }
    if (strncmp(fact->id, "qwen", 4) == 0 ||
        strncmp(fact->id, "gemma", 5) == 0) {
        return "dense-pressure-only";
    }
    return fact->eligibility;
}

static const char *candidate_blocker1(const candidate_fact *fact,
                                      const char *prefix)
{
    if (!fact || strcmp(prefix, "dense_candidate") != 0) {
        return NULL;
    }
    if (strncmp(fact->id, "deepseek", 8) == 0) {
        return "selected-runtime-slice-only";
    }
    if (strncmp(fact->id, "glm", 3) == 0) {
        return "source-only-target";
    }
    if (strncmp(fact->id, "qwen", 4) == 0) {
        return "missing-qwen-source-path";
    }
    if (strncmp(fact->id, "gemma", 5) == 0) {
        return "missing-gemma-source-path";
    }
    return NULL;
}

static const char *candidate_next_for_prefix(const candidate_fact *fact,
                                             const char *prefix)
{
    if (!fact || strcmp(prefix, "dense_candidate") != 0) {
        return fact ? fact->next : "V010.REBASE.DEEPSEEK.0";
    }
    if (strncmp(fact->id, "deepseek", 8) == 0) {
        return "V010.GRAPH.DEEPSEEK.TRANSFORMER.0";
    }
    if (strncmp(fact->id, "qwen", 4) == 0 ||
        strncmp(fact->id, "gemma", 5) == 0) {
        return "V010.MODEL.ARCH.IR.0";
    }
    return fact->next;
}

static int candidate_bad_release(const yvex_model_target_request *request,
                                 yvex_model_target_report *report,
                                 const char *label)
{
    report->exit_code = 2;
    report->status = "unsupported-release";
    yvex_model_target_report_add_row(report, "%s: %s",
                                     label,
                                     request->release[0] ? request->release : "missing");
    yvex_model_target_report_add_row(report, "status: unsupported-release");
    yvex_model_target_report_common_tail(report);
    return YVEX_OK;
}

static int candidate_emit_help(const yvex_model_target_request *request,
                               yvex_model_target_report *report)
{
    if (request->kind == YVEX_MODEL_TARGET_COMMAND_DENSE_CANDIDATE) {
        yvex_model_target_report_add_row(report, "usage: yvex model-target dense-candidate --release v0.1.0 [options]");
        yvex_model_target_report_add_row(report, "The dense-candidate report preserves Qwen and Gemma engineering evidence without offering an alternate v0.1.0 release target.");
        yvex_model_target_report_add_row(report, "does not download weights, emit artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready");
    } else if (request->kind == YVEX_MODEL_TARGET_COMMAND_QWEN_METAL) {
        yvex_model_target_report_add_row(report, "usage: yvex model-target qwen-metal --release v0.1.0 [options]");
        yvex_model_target_report_add_row(report, "The Qwen/Metal pressure report records a planned reduced-scale Apple Silicon / Metal lane for future full-runtime work.");
        yvex_model_target_report_add_row(report, "does not download weights, implement Metal, emit Qwen artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready");
    } else {
        yvex_model_target_report_add_row(report, "usage: yvex model-target candidate --release v0.1.0 [options]");
        yvex_model_target_report_add_row(report, "The candidate report shows the selected DeepSeek release source and keeps other families or selected slices as non-release engineering evidence.");
        yvex_model_target_report_add_row(report, "target selection does not select a ready model");
    }
    return YVEX_OK;
}

static void candidate_emit_table(yvex_model_target_report *report,
                                 const char *report_name,
                                 const char *status,
                                 const char *next)
{
    yvex_model_target_report_add_row(report, "REPORT  STATUS  SELECTED  ELIGIBLE  NEXT");
    yvex_model_target_report_add_row(report, "%s  %s  none  0  %s",
                                     report_name, status, next);
}

static void candidate_emit_common_normal(yvex_model_target_report *report,
                                         const char *name,
                                         const char *status,
                                         const char *blocker)
{
    yvex_model_target_report_add_row(report, "report: model-target %s", name);
    yvex_model_target_report_add_row(report, "status: %s", status);
    yvex_model_target_report_add_row(report, "release: v0.1.0");
    yvex_model_target_report_add_row(report, "selected: none");
    yvex_model_target_report_add_row(report, "top_blocker: %s", blocker);
    yvex_model_target_report_add_row(report, "next: V010.MODEL.ARCH.IR.0");
    yvex_model_target_report_add_row(report, "boundary: report-only; generation unsupported; benchmark not measured");
}

static int candidate_emit_unknown_target(yvex_model_target_report *report,
                                         const char *status,
                                         const char *target)
{
    report->exit_code = 2;
    yvex_model_target_report_add_row(report, "status: %s", status);
    yvex_model_target_report_add_row(report, "target_requested: %s",
                                     target && target[0] ? target : "unknown");
    return YVEX_OK;
}

static void candidate_emit_full_audit(yvex_model_target_report *report,
                                      const char *prefix,
                                      const char *target)
{
    unsigned long i;

    if (target && target[0]) {
        const candidate_fact *fact = candidate_find(target);
        if (!fact) {
            candidate_emit_unknown_target(report,
                                          strcmp(prefix, "dense_candidate") == 0
                                              ? "dense-candidate-report-fail"
                                              : "full-runtime-candidate-report-fail",
                                          target);
            return;
        }
        yvex_model_target_report_add_row(report, "%s_count: 1", prefix);
        yvex_model_target_report_add_row(report, "%s_0_id: %s", prefix, fact->id);
        yvex_model_target_report_add_row(report, "%s_0_class: %s", prefix, fact->class_name);
        yvex_model_target_report_add_row(report, "%s_0_stage: %s", prefix, fact->stage);
        yvex_model_target_report_add_row(report, "%s_0_eligibility: %s", prefix,
                                         candidate_eligibility_for_prefix(fact, prefix));
        yvex_model_target_report_add_row(report, "%s_0_blocker_0: %s", prefix,
                                         candidate_blocker0(fact, prefix));
        if (candidate_blocker1(fact, prefix)) {
            yvex_model_target_report_add_row(report, "%s_0_blocker_1: %s", prefix,
                                             candidate_blocker1(fact, prefix));
        } else if (strncmp(fact->id, "gemma", 5) == 0) {
            yvex_model_target_report_add_row(report, "%s_0_blocker_1: missing-gemma-source-path", prefix);
        }
        yvex_model_target_report_add_row(report, "%s_0_next_required_rows: %s",
                                         prefix, candidate_next_for_prefix(fact, prefix));
        return;
    }

    yvex_model_target_report_add_row(report, "%s_count: %lu", prefix,
                                     candidate_fact_count());
    for (i = 0; i < candidate_fact_count(); ++i) {
        const candidate_fact *fact = &candidate_facts[i];
        yvex_model_target_report_add_row(report, "%s_%lu_id: %s", prefix, i, fact->id);
        yvex_model_target_report_add_row(report, "%s_%lu_class: %s", prefix, i, fact->class_name);
        yvex_model_target_report_add_row(report, "%s_%lu_stage: %s", prefix, i, fact->stage);
        yvex_model_target_report_add_row(report, "%s_%lu_eligibility: %s", prefix, i,
                                         candidate_eligibility_for_prefix(fact, prefix));
        yvex_model_target_report_add_row(report, "%s_%lu_blocker_0: %s", prefix, i,
                                         candidate_blocker0(fact, prefix));
        if (candidate_blocker1(fact, prefix)) {
            yvex_model_target_report_add_row(report, "%s_%lu_blocker_1: %s", prefix, i,
                                             candidate_blocker1(fact, prefix));
        } else if (strncmp(fact->id, "gemma", 5) == 0) {
            yvex_model_target_report_add_row(report, "%s_%lu_blocker_1: missing-gemma-source-path", prefix, i);
        }
        if (i == 0 && strcmp(prefix, "dense_candidate") == 0) {
            yvex_model_target_report_add_row(report, "dense_candidate_0_required_role_5: dense-mlp");
            yvex_model_target_report_add_row(report, "dense_candidate_0_blocker_1: selected-runtime-slice-only");
        }
    }
}

static int candidate_report_build(const yvex_model_target_request *request,
                                  yvex_model_target_report *report)
{
    if (request->help_requested) return candidate_emit_help(request, report);
    if (strcmp(request->release, "v0.1.0") != 0) {
        return candidate_bad_release(request, report, "full_runtime_candidate");
    }
    if (request->target_id[0] && !candidate_find(request->target_id)) {
        return candidate_emit_unknown_target(report,
                                             "full-runtime-candidate-report-fail",
                                             request->target_id);
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        yvex_model_target_report_add_row(report,
                                         "REPORT  STATUS  SELECTED  ELIGIBLE  NEXT");
        yvex_model_target_report_add_row(report,
                                         "full-runtime-candidate  source-verification-required  %s  0  V010.REBASE.DEEPSEEK.0",
                                         yvex_deepseek_v4_target_id);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        yvex_model_target_report_add_row(report,
                                         "selected_release_target: %s",
                                         yvex_deepseek_v4_target_id);
        yvex_model_target_report_add_row(report, "other_candidate_scope: non-release-engineering-evidence");
        yvex_model_target_report_add_row(report, "next_required_rows: V010.REBASE.DEEPSEEK.0");
        yvex_model_target_report_add_row(report, "post_verification_next: V010.GGUF.QTYPE.ABI.1");
        candidate_emit_full_audit(report, "candidate", request->target_id);
        yvex_model_target_report_common_tail(report);
        return YVEX_OK;
    }
    yvex_model_target_report_add_row(report, "report: model-target candidate");
    yvex_model_target_report_add_row(report, "status: selected-source-verification-required");
    yvex_model_target_report_add_row(report, "release: v0.1.0");
    yvex_model_target_report_add_row(report, "selected: %s",
                                     yvex_deepseek_v4_target_id);
    yvex_model_target_report_add_row(report, "top_blocker: exact source verification");
    yvex_model_target_report_add_row(report, "next: V010.REBASE.DEEPSEEK.0");
    yvex_model_target_report_add_row(report, "boundary: target selected; artifact/runtime/generation unsupported; benchmark not measured");
    return YVEX_OK;
}

static int dense_candidate_report_build(const yvex_model_target_request *request,
                                        yvex_model_target_report *report)
{
    if (request->help_requested) return candidate_emit_help(request, report);
    if (strcmp(request->release, "v0.1.0") != 0) {
        return candidate_bad_release(request, report, "dense_candidate");
    }
    if (request->target_id[0] && !candidate_find(request->target_id)) {
        return candidate_emit_unknown_target(report,
                                             "dense-candidate-report-fail",
                                             request->target_id);
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        candidate_emit_table(report, "dense-candidate", "missing",
                             "V010.MODEL.ARCH.IR.0");
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        yvex_model_target_report_add_row(report, "dense_candidate_status: candidate-incomplete");
        yvex_model_target_report_add_row(report,
                                         "next_required_rows: V010.MODEL.ARCH.IR.0");
        candidate_emit_full_audit(report, "dense_candidate", request->target_id);
        yvex_model_target_report_common_tail(report);
        return YVEX_OK;
    }
    candidate_emit_common_normal(report, "dense-candidate", "dense-candidate-missing",
                                 "no selected dense full-runtime candidate");
    return YVEX_OK;
}

static int qwen_metal_report_build(const yvex_model_target_request *request,
                                   yvex_model_target_report *report)
{
    const char *target;

    if (request->help_requested) return candidate_emit_help(request, report);
    if (strcmp(request->release, "v0.1.0") != 0) {
        return candidate_bad_release(request, report, "qwen_metal");
    }
    if (request->target_id[0] && strcmp(request->target_id, "qwen3-8b") != 0 &&
        strcmp(request->target_id, "qwen-small") != 0 &&
        strcmp(request->target_id, "qwen-medium") != 0) {
        report->exit_code = 2;
        yvex_model_target_report_add_row(report, "status: qwen-metal-pressure-report-fail");
        yvex_model_target_report_add_row(report, "target_requested: %s", request->target_id);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        candidate_emit_table(report, "qwen-metal-pressure", "pressure",
                             "POST010.QWEN.METAL.0");
        return YVEX_OK;
    }
    yvex_model_target_report_add_row(report, "report: model-target qwen-metal");
    yvex_model_target_report_add_row(report, "status: pressure-target-only");
    yvex_model_target_report_add_row(report, "release: v0.1.0");
    yvex_model_target_report_add_row(report, "lane: qwen-metal / apple-silicon-metal");
    target = request->target_id[0] ? request->target_id : "qwen3-8b";
    yvex_model_target_report_add_row(report, "target: %s", target);
    yvex_model_target_report_add_row(report, "candidate: source-target-profiled pressure-target-only");
    yvex_model_target_report_add_row(report, "source_target: profiled");
    yvex_model_target_report_add_row(report, "source: missing");
    yvex_model_target_report_add_row(report, "backend: metal unsupported");
    yvex_model_target_report_add_row(report, "next: POST010.QWEN.METAL.0");
    yvex_model_target_report_add_row(report, "boundary: report-only; generation unsupported; benchmark not measured");
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        if (strcmp(target, "qwen-small") == 0 ||
            strcmp(target, "qwen-medium") == 0) {
            yvex_model_target_report_add_row(report, "qwen_candidate_count: 1");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_id: %s", target);
            yvex_model_target_report_add_row(report, "qwen_candidate_0_class: backend-compatibility-pressure");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_stage: report-only");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_eligibility: pressure-target-only");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_source_target_status: pending");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_backend_status: unsupported");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_runtime_status: unsupported");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_generation_status: unsupported-full-model");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_blocker_0: missing-qwen-source-path");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_blocker_6: missing-metal-backend-feasibility");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_blocker_7: missing-real-prefill");
        } else {
            yvex_model_target_report_add_row(report, "qwen_candidate_count: 3");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_id: qwen-small");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_class: backend-compatibility-pressure");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_stage: report-only");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_eligibility: pressure-target-only");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_source_target_status: pending");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_backend_status: unsupported");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_runtime_status: unsupported");
            yvex_model_target_report_add_row(report, "qwen_candidate_0_generation_status: unsupported-full-model");
            yvex_model_target_report_add_row(report, "qwen_candidate_1_id: qwen-medium");
            yvex_model_target_report_add_row(report, "qwen_candidate_2_id: qwen3-8b");
            yvex_model_target_report_add_row(report, "qwen_candidate_2_stage: source-target-profiled");
            yvex_model_target_report_add_row(report, "qwen_candidate_2_source_target_status: profiled");
        }
        yvex_model_target_report_add_row(report, "candidate_id: %s", target);
        yvex_model_target_report_add_row(report, "candidate_stage: source-target-profiled");
        yvex_model_target_report_add_row(report, "source_target_status: profiled");
        yvex_model_target_report_add_row(report, "hardware_profile_status: planned");
        yvex_model_target_report_add_row(report, "machine_profile_required: true");
        yvex_model_target_report_add_row(report, "unified_memory_report_required: true");
        yvex_model_target_report_add_row(report, "metal_device_report_required: true");
        yvex_model_target_report_add_row(report, "metal_feasibility_status: missing");
        yvex_model_target_report_add_row(report, "metal_allocation_status: unsupported");
        yvex_model_target_report_add_row(report, "metal_graph_primitive_status: unsupported");
        yvex_model_target_report_add_row(report, "cuda_lane_independent: true");
        yvex_model_target_report_add_row(report, "source_family: qwen");
        yvex_model_target_report_add_row(report, "source_manifest_status: missing");
        yvex_model_target_report_add_row(report, "native_tensor_inventory_status: missing");
        yvex_model_target_report_add_row(report, "source_config_status: missing");
        yvex_model_target_report_add_row(report, "model_class_profile_status: command-visible");
        yvex_model_target_report_add_row(report, "blocker_0: missing-qwen-source-path");
        yvex_model_target_report_add_row(report, "blocker_1: missing-qwen-source-manifest");
        yvex_model_target_report_add_row(report, "blocker_9: missing-metal-backend-feasibility");
        yvex_model_target_report_add_row(report, "blocker_16: missing-real-prefill");
        yvex_model_target_report_add_row(report, "blocker_19: missing-real-output-head-logits");
        yvex_model_target_report_add_row(report, "blocker_20: missing-real-vocabulary-sampling");
        yvex_model_target_report_add_row(report,
                                         "next_required_rows: POST010.QWEN.METAL.0");
    }
    return YVEX_OK;
}

int yvex_model_target_candidate_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_target_candidate",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    report->kind = request->kind;
    report->mode = request->mode;
    if (request->kind == YVEX_MODEL_TARGET_COMMAND_DENSE_CANDIDATE) {
        return dense_candidate_report_build(request, report);
    }
    if (request->kind == YVEX_MODEL_TARGET_COMMAND_QWEN_METAL) {
        return qwen_metal_report_build(request, report);
    }
    return candidate_report_build(request, report);
}
