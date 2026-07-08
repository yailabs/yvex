/*
 * yvex_model_target_report.c - model-target report construction.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   model-target report construction, target facts, target profiles, map/gate
 *   facts, qtype facts, sidecar report facts, and captured report text.
 *
 * Does not own:
 *   CLI argv parsing ownership, command dispatch, typed CLI rendering,
 *   stdout/stderr byte emission, runtime graph, generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   reports preserve existing model-target behavior while packaging output as
 *   typed report buffers; domain/report code does not include CLI headers or
 *   write directly to process stdout/stderr.
 *
 * Boundary:
 *   model-target reports expose existing facts; they do not create capability,
 *   prove model support, emit generation-capable artifacts, execute runtime,
 *   generate, evaluate, benchmark, or mark release readiness.
 */

#include "yvex_model_target_report.h"

#include <yvex/dtype.h>
#include <yvex/artifact_integrity.h>
#include <yvex/fs.h>
#include <yvex/model.h>
#include <yvex/model_registry.h>
#include <yvex/native_weights.h>
#include <yvex/qtype_support.h>
#include <yvex/tensor.h>
#include <yvex/weights.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static FILE *model_target_capture_out;
static FILE *model_target_capture_err;

static FILE *model_target_out(void)
{
    return model_target_capture_out;
}

static FILE *model_target_err(void)
{
    return model_target_capture_err;
}

static int model_target_stream_write(FILE *fp, const char *text, size_t len)
{
    if (!fp || !text || len == 0u) {
        return 0;
    }
    return fwrite(text, 1u, len, fp) == len ? (int)len : -1;
}

static int model_target_out_fputs(const char *text, FILE *fp)
{
    size_t len;

    if (!text) {
        text = "";
    }
    len = strlen(text);
    return model_target_stream_write(fp, text, len);
}

static int model_target_file_char(int ch, FILE *fp)
{
    unsigned char c = (unsigned char)ch;

    return model_target_stream_write(fp, (const char *)&c, 1u);
}

static int model_target_out_writef(FILE *fp, const char *fmt, ...)
{
    va_list ap;
    va_list ap_copy;
    char stack_buf[1024];
    char *heap_buf = NULL;
    char *buf = stack_buf;
    int needed;
    int rc;

    if (!fmt) {
        fmt = "";
    }
    va_start(ap, fmt);
    va_copy(ap_copy, ap);
    needed = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap);
    va_end(ap);
    if (needed < 0) {
        va_end(ap_copy);
        return needed;
    }
    if ((size_t)needed >= sizeof(stack_buf)) {
        heap_buf = (char *)malloc((size_t)needed + 1u);
        if (!heap_buf) {
            va_end(ap_copy);
            return -1;
        }
        buf = heap_buf;
        needed = vsnprintf(buf, (size_t)needed + 1u, fmt, ap_copy);
        if (needed < 0) {
            free(heap_buf);
            va_end(ap_copy);
            return needed;
        }
    }
    va_end(ap_copy);
    rc = model_target_stream_write(fp, buf, (size_t)needed);
    free(heap_buf);
    return rc;
}

/* Model pressure target registry */

typedef struct {
    const char *class_id;
    const char *capability_claim;
    const char *runtime_execution;
    const char *generation;
    const char *description;
} yvex_model_target_class_record;

typedef struct {
    const char *target_id;
    const char *family;
    const char *model;
    const char *target_class;
    const char *source_artifact_class;
    const char *target_artifact_class;
    const char *pressure_purpose;
    const char *tensor_set;
    const char *local_path_class;
    const char *source_footprint_class;
    const char *runtime_boundary;
    const char *runtime_execution;
    const char *generation;
    const char *external_reference;
} yvex_model_target_record;

static const yvex_model_target_class_record model_target_classes[] = {
    {
        "selected-runtime-slice",
        "false",
        "partial-boundary-only",
        "unsupported",
        "selected real artifact slice used to prove parser, materialization, backend, graph, reference, and cleanup boundaries",
    },
    {
        "official-source-huge-model",
        "false",
        "unsupported",
        "unsupported",
        "official upstream source tensors used to force source manifest, native tensor inventory, model-class profiling, tensor mapping, quantization policy, and future YVEX-produced artifacts",
    },
    {
        "source-model-candidate",
        "false",
        "unsupported",
        "unsupported",
        "backend-neutral model/source target candidate; backend pressure and runtime compatibility are reported separately",
    },
    {
        "full-runtime-model",
        "false",
        "planned",
        "planned",
        "complete tensor set required for transformer prefill, decode, logits, sampling, and generation after runtime support exists",
    },
    {
        "huge-model-storage-stream",
        "false",
        "planned",
        "unsupported",
        "huge artifact target used to force shard inventory, storage layout, page or chunk planning, staged residency, and cleanup boundaries",
    },
    {
        "external-GGUF-reference",
        "false",
        "external-reference-only",
        "external-reference-only",
        "external GGUF evidence used only to compare artifact layout, qtype choices, deployment constraints, or external behavior",
    },
    {
        "external-runner-reference",
        "false",
        "external-reference-only",
        "external-reference-only",
        "external runtime evidence used only to compare deployment constraints or external behavior",
    },
};

static const yvex_model_target_record model_targets[] = {
    {
        "deepseek4-v4-flash-selected-embed",
        "DeepSeek",
        "DeepSeek-V4-Flash",
        "selected-runtime-slice",
        "official-safetensors",
        "YVEX-produced-selected-GGUF",
        "selected-token-embedding-materialization",
        "token_embd.weight",
        "none",
        "none",
        "selected materialization and selected graph slice only",
        "unsupported",
        "unsupported",
        "false",
    },
    {
        "deepseek4-v4-flash-selected-embed-rmsnorm",
        "DeepSeek",
        "DeepSeek-V4-Flash",
        "selected-runtime-slice",
        "official-safetensors",
        "YVEX-produced-selected-GGUF",
        "selected-embedding-plus-rmsnorm-segment",
        "token_embd.weight,blk.0.attn_norm.weight",
        "none",
        "none",
        "selected segment execution only",
        "unsupported",
        "unsupported",
        "false",
    },
    {
        "glm-5.2-official-safetensors",
        "GLM",
        "GLM-5.2",
        "official-source-huge-model",
        "official-safetensors-huge",
        "future-YVEX-produced-GGUF",
        "huge-source-tensor-intake-moe-storage-stream-planning",
        "none",
        "hf/glm/GLM-5.2",
        "282 safetensors,1.5T-class",
        "source evidence only",
        "unsupported",
        "unsupported",
        "false",
    },
    {
        "qwen3-8b",
        "Qwen",
        "Qwen3-8B",
        "source-model-candidate",
        "official-source-tensors-planned",
        "future-YVEX-produced-GGUF",
        "backend-neutral-qwen-source-model-target",
        "pending-source-config",
        "hf/qwen/qwen3-8b",
        "pending source/config verification",
        "target profile only; no source download/runtime/generation",
        "unsupported",
        "unsupported",
        "false",
    },
    {
        "gemma-4-12b-it",
        "Gemma",
        "Gemma-4-12B-it",
        "source-model-candidate",
        "official-source-tensors-planned",
        "future-YVEX-produced-GGUF",
        "backend-neutral-gemma-source-model-target",
        "pending-source-config",
        "hf/gemma/gemma-4-12b-it",
        "pending source/config verification",
        "target profile only; no source download/runtime/generation",
        "unsupported",
        "unsupported",
        "false",
    },
};

static const unsigned long model_target_class_count =
    sizeof(model_target_classes) / sizeof(model_target_classes[0]);
static const unsigned long model_target_count = sizeof(model_targets) / sizeof(model_targets[0]);

typedef struct {
    const char *id;
    const char *class_name;
    const char *stage;
    const char *eligibility;
    const char *artifact_status;
    const char *source_status;
    const char *tensor_coverage_status;
    const char *runtime_path_status;
    const char *generation_status;
    const char *benchmark_status;
    const char *next_required_rows;
    const char *blockers[16];
    unsigned int blocker_count;
    int pressure_target;
    int fixture_target;
} yvex_full_runtime_candidate_fact;

typedef struct {
    const char *id;
    const char *family;
    const char *class_name;
    const char *stage;
    const char *eligibility;
    const char *source_status;
    const char *artifact_status;
    const char *tensor_map_status;
    const char *tensor_coverage_status;
    const char *tokenizer_status;
    const char *output_head_status;
    const char *runtime_path_status;
    const char *generation_status;
    const char *benchmark_status;
    const char *next_required_rows;
    const char *blockers[24];
    unsigned int blocker_count;
    int dense_pressure_target;
    int fixture_target;
    int eligible;
} yvex_dense_candidate_fact;

typedef struct {
    const char *id;
    const char *class_name;
    const char *stage;
    const char *eligibility;
    const char *source_target_status;
    const char *source_status;
    const char *artifact_status;
    const char *tensor_map_status;
    const char *backend_status;
    const char *runtime_status;
    const char *generation_status;
    const char *benchmark_status;
    const char *blockers[16];
    unsigned int blocker_count;
} yvex_qwen_metal_candidate_fact;

static const yvex_full_runtime_candidate_fact full_runtime_candidate_facts[] = {
    {
        "deepseek4-v4-flash-selected-embed",
        "selected-runtime-slice",
        "selected-slice",
        "selected-slice-only",
        "selected-artifact-known",
        "official-source-pressure",
        "missing-required-tensor-coverage",
        "diagnostic-runtime-only",
        "unsupported-full-model",
        "not-measured",
        "V010.TARGET.3,V010.TARGET.4,V010.MAP.2,V010.FULLMODEL.6",
        {
            "selected-runtime-slice-only",
            "missing-required-tensor-coverage",
            "missing-tokenizer-metadata",
            "missing-output-head",
            "missing-attention-qkv",
            "missing-real-kv-path",
            "missing-real-prefill",
            "missing-real-decode",
            "missing-real-logits",
            "missing-real-sampling",
            "missing-generation-loop-over-real-state",
        },
        11,
        1,
        0,
    },
    {
        "deepseek4-v4-flash-selected-embed-rmsnorm",
        "selected-runtime-slice",
        "diagnostic-runtime",
        "selected-slice-only",
        "selected-segment-artifact-known",
        "official-source-pressure",
        "missing-required-tensor-coverage",
        "bounded-diagnostic-runtime-only",
        "unsupported-full-model",
        "not-measured",
        "V010.TARGET.3,V010.TARGET.4,V010.MAP.2,V010.FULLMODEL.6",
        {
            "selected-runtime-slice-only",
            "missing-required-tensor-coverage",
            "missing-tokenizer-metadata",
            "missing-output-head",
            "missing-attention-qkv",
            "missing-real-kv-path",
            "missing-real-prefill",
            "missing-real-decode",
            "missing-real-logits",
            "missing-real-sampling",
            "missing-generation-loop-over-real-state",
        },
        11,
        1,
        0,
    },
    {
        "glm-5.2-official-safetensors",
        "official-source-huge-model",
        "report-only",
        "source-only",
        "missing-full-artifact",
        "source-storage-pressure",
        "missing-tensor-map",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        "OWI.HUGE.0,V010.SOURCE.8,V010.MAP.4,V010.ARTIFACT.EMIT.2,V010.FULLMODEL.6",
        {
            "source-only-target",
            "missing-source-inventory",
            "missing-tensor-map",
            "missing-full-artifact",
            "missing-required-tensor-coverage",
            "missing-materialization-plan",
            "missing-residency-plan",
            "missing-real-prefill",
            "missing-real-decode",
            "missing-real-logits",
            "missing-real-sampling",
        },
        11,
        1,
        0,
    },
    {
        "qwen3-8b",
        "source-model-candidate",
        "source-target-profiled",
        "planned-portability-only",
        "planned",
        "planned",
        "missing-tensor-map",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        "V010.MAP.8,HARDWARE.PROFILE.MAC.0,COMPUTE.BACKEND.METAL.0",
        {
            "planned-portability-only",
            "missing-qwen-source-path",
            "missing-source-inventory",
            "missing-qwen-tensor-role-map",
            "missing-full-artifact",
            "missing-integrity-gate",
            "missing-real-prefill",
            "missing-real-decode",
            "missing-real-logits",
            "missing-real-sampling",
        },
        10,
        1,
        0,
    },
    {
        "gemma-4-12b-it",
        "source-model-candidate",
        "source-target-profiled",
        "planned-dense-pressure-only",
        "planned",
        "planned",
        "missing-tensor-map",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        "V010.MAP.8",
        {
            "planned-dense-pressure-only",
            "missing-gemma-source-path",
            "missing-gemma-source-manifest",
            "missing-gemma-native-inventory",
            "missing-gemma-source-config",
            "missing-gemma-tensor-role-map",
            "missing-gemma-tensor-map",
            "missing-gemma-yvex-artifact",
            "missing-gemma-real-prefill",
            "missing-gemma-real-decode",
        },
        10,
        1,
        0,
    },
    {
        "tests/fixtures/gguf/valid-tokenizer-simple.gguf",
        "fixture-artifact",
        "fixture",
        "fixture-only",
        "tiny-fixture-present",
        "fixture-only",
        "fixture-only",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        "V010.TARGET.3",
        {
            "fixture-only",
            "missing-full-artifact",
            "missing-required-tensor-coverage",
            "missing-real-kv-path",
            "missing-real-prefill",
            "missing-real-decode",
            "missing-real-logits",
            "missing-real-sampling",
        },
        8,
        0,
        1,
    },
};

static const unsigned long full_runtime_candidate_fact_count =
    sizeof(full_runtime_candidate_facts) / sizeof(full_runtime_candidate_facts[0]);

static const char *dense_candidate_required_roles[] = {
    "embedding",
    "normalization",
    "attention-qkv",
    "attention-output",
    "position",
    "dense-mlp",
    "output-head",
    "tokenizer",
    "kv-runtime",
};

static const unsigned long dense_candidate_required_role_count =
    sizeof(dense_candidate_required_roles) / sizeof(dense_candidate_required_roles[0]);

static const yvex_dense_candidate_fact dense_candidate_facts[] = {
    {
        "deepseek4-v4-flash-selected-embed",
        "DeepSeek",
        "selected-runtime-slice",
        "selected-slice",
        "not-dense-target",
        "official-source-pressure",
        "selected-artifact-known",
        "missing-tensor-map",
        "missing-required-tensor-coverage",
        "missing-tokenizer-metadata",
        "missing-output-head",
        "diagnostic-runtime-only",
        "unsupported-full-model",
        "not-measured",
        "V010.TARGET.7,V010.TARGET.4,V010.MAP.2,V010.FULLMODEL.6",
        {
            "not-dense-target",
            "selected-runtime-slice-only",
            "missing-required-tensor-coverage",
            "missing-tokenizer-metadata",
            "missing-output-head",
            "missing-attention-qkv",
            "missing-dense-mlp",
            "missing-real-kv-path",
            "missing-real-prefill",
            "missing-real-decode",
            "missing-real-logits",
            "missing-real-sampling",
            "missing-generation-loop-over-real-state",
        },
        13,
        0,
        0,
        0,
    },
    {
        "deepseek4-v4-flash-selected-embed-rmsnorm",
        "DeepSeek",
        "selected-runtime-slice",
        "diagnostic-runtime",
        "not-dense-target",
        "official-source-pressure",
        "selected-segment-artifact-known",
        "missing-tensor-map",
        "missing-required-tensor-coverage",
        "missing-tokenizer-metadata",
        "missing-output-head",
        "bounded-diagnostic-runtime-only",
        "unsupported-full-model",
        "not-measured",
        "V010.TARGET.7,V010.TARGET.4,V010.MAP.2,V010.FULLMODEL.6",
        {
            "not-dense-target",
            "selected-runtime-slice-only",
            "missing-required-tensor-coverage",
            "missing-tokenizer-metadata",
            "missing-output-head",
            "missing-attention-qkv",
            "missing-dense-mlp",
            "missing-real-kv-path",
            "missing-real-prefill",
            "missing-real-decode",
            "missing-real-logits",
            "missing-real-sampling",
            "missing-generation-loop-over-real-state",
        },
        13,
        0,
        0,
        0,
    },
    {
        "glm-5.2-official-safetensors",
        "GLM",
        "official-source-huge-model",
        "report-only",
        "unsupported",
        "source-storage-pressure",
        "missing-dense-artifact",
        "missing-tensor-map",
        "missing-required-tensor-coverage",
        "missing-tokenizer-metadata",
        "missing-output-head",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        "V010.TARGET.7,V010.TARGET.4,OWI.HUGE.0,V010.SOURCE.8,V010.MAP.4",
        {
            "moe-target",
            "source-only-target",
            "missing-dense-artifact",
            "missing-source-manifest",
            "missing-native-inventory",
            "missing-tensor-map",
            "missing-required-tensor-coverage",
            "missing-tokenizer-metadata",
            "missing-output-head",
            "missing-dense-mlp",
            "missing-real-kv-path",
            "missing-real-prefill",
            "missing-real-decode",
            "missing-real-logits",
            "missing-real-sampling",
            "missing-generation-loop-over-real-state",
        },
        16,
        0,
        0,
        0,
    },
    {
        "qwen3-8b",
        "Qwen",
        "source-model-candidate",
        "source-target-profiled",
        "dense-pressure-only",
        "missing-dense-source",
        "missing-dense-artifact",
        "missing-tensor-map",
        "missing-required-tensor-coverage",
        "missing-tokenizer-metadata",
        "missing-output-head",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        "V010.TARGET.7,V010.MAP.8,COMPUTE.BACKEND.METAL.0",
        {
            "planned-portability-only",
            "missing-qwen-source-path",
            "missing-dense-source",
            "missing-dense-artifact",
            "missing-source-manifest",
            "missing-native-inventory",
            "missing-qwen-tensor-role-map",
            "missing-required-tensor-coverage",
            "missing-tokenizer-metadata",
            "missing-output-head",
            "missing-attention-qkv",
            "missing-dense-mlp",
            "missing-materialization-plan",
            "missing-residency-plan",
            "missing-integrity-gate",
            "missing-real-kv-path",
            "missing-real-prefill",
            "missing-real-decode",
            "missing-real-logits",
            "missing-real-sampling",
            "missing-generation-loop-over-real-state",
            "missing-eval-path",
            "missing-benchmark-path",
        },
        23,
        1,
        0,
        0,
    },
    {
        "gemma-4-12b-it",
        "Gemma",
        "source-model-candidate",
        "source-target-profiled",
        "dense-pressure-only",
        "missing-dense-source",
        "missing-dense-artifact",
        "missing-tensor-map",
        "missing-required-tensor-coverage",
        "missing-tokenizer-metadata",
        "missing-output-head",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        "V010.TARGET.7,V010.MAP.8",
        {
            "planned-dense-pressure-only",
            "missing-gemma-source-path",
            "missing-gemma-source-manifest",
            "missing-gemma-native-inventory",
            "missing-gemma-source-config",
            "missing-gemma-tokenizer-files",
            "missing-gemma-tensor-role-map",
            "missing-gemma-tensor-map",
            "missing-gemma-yvex-artifact",
            "missing-gemma-artifact-identity",
            "missing-gemma-real-prefill",
            "missing-gemma-real-kv-path",
            "missing-gemma-real-decode",
            "missing-gemma-real-output-head-logits",
            "missing-gemma-real-vocabulary-sampling",
            "missing-gemma-generation-loop-over-real-state",
            "missing-gemma-eval-path",
            "missing-gemma-benchmark-path",
        },
        18,
        1,
        0,
        0,
    },
    {
        "tests/fixtures/gguf/valid-tokenizer-simple.gguf",
        "fixture",
        "fixture-artifact",
        "fixture",
        "fixture-only",
        "fixture-only",
        "tiny-fixture-present",
        "fixture-only",
        "missing-required-tensor-coverage",
        "fixture-tokenizer-metadata",
        "missing-output-head",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        "V010.TARGET.7",
        {
            "fixture-only",
            "missing-dense-source",
            "missing-dense-artifact",
            "missing-required-tensor-coverage",
            "missing-attention-qkv",
            "missing-dense-mlp",
            "missing-output-head",
            "missing-real-kv-path",
            "missing-real-prefill",
            "missing-real-decode",
            "missing-real-logits",
            "missing-real-sampling",
        },
        12,
        0,
        1,
        0,
    },
};

static const unsigned long dense_candidate_fact_count =
    sizeof(dense_candidate_facts) / sizeof(dense_candidate_facts[0]);

static const char *qwen_metal_blockers[] = {
    "missing-qwen-source-path",
    "missing-qwen-source-manifest",
    "missing-qwen-native-inventory",
    "missing-qwen-source-config",
    "missing-qwen-tensor-role-map",
    "missing-qwen-tensor-map",
    "missing-qwen-yvex-artifact",
    "missing-qwen-artifact-identity",
    "missing-metal-hardware-profile",
    "missing-metal-backend-feasibility",
    "missing-metal-allocation-boundary",
    "missing-metal-transfer-boundary",
    "missing-metal-graph-primitive-parity",
    "missing-unified-memory-residency-plan",
    "missing-qwen-fullmodel-report",
    "missing-qwen-materialization-plan",
    "missing-real-prefill",
    "missing-real-kv-path",
    "missing-real-decode",
    "missing-real-output-head-logits",
    "missing-real-vocabulary-sampling",
    "missing-generation-loop-over-real-state",
    "missing-eval-path",
    "missing-benchmark-path",
};

static const unsigned long qwen_metal_blocker_count =
    sizeof(qwen_metal_blockers) / sizeof(qwen_metal_blockers[0]);

static const yvex_qwen_metal_candidate_fact qwen_metal_candidate_facts[] = {
    {
        "qwen-small",
        "backend-compatibility-pressure",
        "report-only",
        "pressure-target-only",
        "pending",
        "missing",
        "missing",
        "missing",
        "unsupported",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        {
            "missing-qwen-source-path",
            "missing-qwen-source-manifest",
            "missing-qwen-native-inventory",
            "missing-qwen-tensor-role-map",
            "missing-qwen-tensor-map",
            "missing-qwen-yvex-artifact",
            "missing-metal-backend-feasibility",
            "missing-real-prefill",
            "missing-real-output-head-logits",
        },
        9,
    },
    {
        "qwen-medium",
        "backend-compatibility-pressure",
        "report-only",
        "pressure-target-only",
        "pending",
        "missing",
        "missing",
        "missing",
        "unsupported",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        {
            "missing-qwen-source-path",
            "missing-qwen-source-manifest",
            "missing-qwen-native-inventory",
            "missing-qwen-tensor-role-map",
            "missing-qwen-tensor-map",
            "missing-qwen-yvex-artifact",
            "missing-metal-hardware-profile",
            "missing-metal-backend-feasibility",
            "missing-unified-memory-residency-plan",
            "missing-real-decode",
        },
        10,
    },
    {
        "qwen3-8b",
        "source-model-candidate",
        "source-target-profiled",
        "pressure-target-only",
        "profiled",
        "missing",
        "missing",
        "missing",
        "unsupported",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        {
            "missing-qwen-source-path",
            "missing-qwen-source-manifest",
            "missing-qwen-native-inventory",
            "missing-qwen-source-config",
            "missing-qwen-tensor-role-map",
            "missing-metal-backend-feasibility",
            "missing-metal-allocation-boundary",
            "missing-metal-graph-primitive-parity",
            "missing-real-kv-path",
            "missing-generation-loop-over-real-state",
        },
        10,
    },
};

static const unsigned long qwen_metal_candidate_fact_count =
    sizeof(qwen_metal_candidate_facts) / sizeof(qwen_metal_candidate_facts[0]);

static const yvex_model_target_record *find_model_target(const char *target_id)
{
    unsigned long i;

    if (!target_id) {
        return NULL;
    }
    for (i = 0; i < model_target_count; ++i) {
        if (strcmp(model_targets[i].target_id, target_id) == 0) {
            return &model_targets[i];
        }
    }
    return NULL;
}

static void print_model_target_decision_usage(FILE *fp)
{
    model_target_out_writef(fp, "usage: " "yvex model-target decision --release v0.1.0 [options]\n");
    model_target_out_writef(fp, "       yvex model-target decision --help\n");
    model_target_out_writef(fp, "\noptions:\n");
    model_target_out_writef(fp, "  --candidate TARGET             report decision facts for one target\n");
    model_target_out_writef(fp, "  --include-candidates           include target candidate classifications\n");
    model_target_out_writef(fp, "  --include-pressure-targets     include pressure-lane status fields\n");
    model_target_out_writef(fp, "  --include-blockers             include blocker row fields\n");
    model_target_out_writef(fp, "  --include-critical-path        include release-critical track fields\n");
    model_target_out_writef(fp, "  --include-next                 include deterministic next row fields\n");
    model_target_out_writef(fp, "  --strict                       keep invalid usage fatal; honest blocked decisions still pass\n");
    model_target_out_writef(fp, "  --audit | --output normal|table|audit\n");
}

static void print_model_target_decision_help(FILE *fp)
{
    print_model_target_decision_usage(fp);
    model_target_out_writef(fp, "\nThis command records the v0.1.0 target decision. It does not download models, emit artifacts, materialize tensors, execute graph work, run prefill, decode, logits, sampling, generation, evaluation, or benchmarks.\n");
    model_target_out_writef(fp, "v0.1.0 requires an honest full-runtime-candidate target before runtime graph, prefill, KV, decode, logits, sampling, and generation rows can advance.\n");
    model_target_out_writef(fp, "Selected runtime slices, source-only pressure targets, external references, and fixture-only targets are ineligible for full-runtime closure.\n");
}

static void print_model_target_candidate_usage(FILE *fp)
{
    model_target_out_writef(fp, "usage: " "yvex model-target candidate --release v0.1.0 [options]\n");
    model_target_out_writef(fp, "       yvex model-target candidate --help\n");
    model_target_out_writef(fp, "\noptions:\n");
    model_target_out_writef(fp, "  --target TARGET                report one candidate target\n");
    model_target_out_writef(fp, "  --include-candidates           include candidate classification blocks\n");
    model_target_out_writef(fp, "  --include-pressure-targets     include pressure target count fields\n");
    model_target_out_writef(fp, "  --include-blockers             include stable blocker fields\n");
    model_target_out_writef(fp, "  --include-next                 include next required row fields\n");
    model_target_out_writef(fp, "  --audit | --output normal|table|audit\n");
}

static void print_model_target_candidate_help(FILE *fp)
{
    print_model_target_candidate_usage(fp);
    model_target_out_writef(fp, "\nThe candidate report evaluates full-runtime target eligibility for a release. It does not select a ready model, download weights, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
}

static void print_model_target_dense_candidate_usage(FILE *fp)
{
    model_target_out_writef(fp, "usage: " "yvex model-target dense-candidate --release v0.1.0 [options]\n");
    model_target_out_writef(fp, "       yvex model-target dense-candidate --help\n");
    model_target_out_writef(fp, "\noptions:\n");
    model_target_out_writef(fp, "  --target TARGET                report one dense or dense-adjacent target\n");
    model_target_out_writef(fp, "  --include-candidates           include dense candidate classification blocks\n");
    model_target_out_writef(fp, "  --include-requirements         include required dense runtime role groups\n");
    model_target_out_writef(fp, "  --include-blockers             include stable blocker fields\n");
    model_target_out_writef(fp, "  --include-next                 include next required row fields\n");
    model_target_out_writef(fp, "  --audit | --output normal|table|audit\n");
}

static void print_model_target_dense_candidate_help(FILE *fp)
{
    print_model_target_dense_candidate_usage(fp);
    model_target_out_writef(fp, "\nThe dense-candidate report evaluates whether a dense model target can become the first v0.1.0 full-runtime candidate. It does not download weights, emit artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
}

static void print_model_target_qwen_metal_usage(FILE *fp)
{
    model_target_out_writef(fp, "usage: " "yvex model-target qwen-metal --release v0.1.0 [options]\n");
    model_target_out_writef(fp, "       yvex model-target qwen-metal --help\n");
    model_target_out_writef(fp, "\noptions:\n");
    model_target_out_writef(fp, "  --target TARGET                report one planned Qwen/Metal candidate slot\n");
    model_target_out_writef(fp, "  --include-candidates           include planned candidate slot blocks\n");
    model_target_out_writef(fp, "  --include-hardware             include Apple Silicon / Metal hardware pressure fields\n");
    model_target_out_writef(fp, "  --include-backend              include Metal backend pressure fields\n");
    model_target_out_writef(fp, "  --include-source               include Qwen source/config pressure fields\n");
    model_target_out_writef(fp, "  --include-blockers             include stable blocker fields\n");
    model_target_out_writef(fp, "  --include-next                 include next required row fields\n");
    model_target_out_writef(fp, "  --audit | --output normal|table|audit\n");
}

static void print_model_target_qwen_metal_help(FILE *fp)
{
    print_model_target_qwen_metal_usage(fp);
    model_target_out_writef(fp, "\nThe Qwen/Metal pressure report records a planned reduced-scale Apple Silicon / Metal lane for future full-runtime work. It does not download weights, implement Metal, emit Qwen artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
}

static int target_decision_is_full_runtime_candidate(const yvex_model_target_record *record)
{
    if (!record || !record->target_class) return 0;
    if (strcmp(record->target_class, "full-runtime-model") == 0) return 1;
    if (strcmp(record->target_class, "full-runtime-candidate") == 0) return 1;
    return 0;
}

typedef enum {
    YVEX_OUTPUT_CONTRACT_TENSOR_MAP = 0,
    YVEX_OUTPUT_CONTRACT_OUTPUT_HEAD,
    YVEX_OUTPUT_CONTRACT_MISSING_ROLES,
    YVEX_OUTPUT_CONTRACT_MAPPING_GATE,
    YVEX_OUTPUT_CONTRACT_QTYPE_POLICY
} yvex_output_contract_report;

#define YVEX_MODEL_CLASS_NEXT_ROW "V010.MAP.8"
#define YVEX_TENSOR_COLLECTION_NEXT_ROW "V010.MAP.8"
#define YVEX_TENSOR_NAMING_NEXT_ROW "V010.MAP.8"
#define YVEX_OUTPUT_HEAD_MAP_NEXT_ROW "V010.MAP.8"
#define YVEX_TOKENIZER_MISSING_NEXT_ROW "V010.MAP.7"
#define YVEX_TOKENIZER_MAP_NEXT_ROW "V010.QUANT.1"
#define YVEX_MISSING_ROLES_PORCELAIN_NEXT_ROW "V010.MAP.8"
#define YVEX_MISSING_ROLE_REPORT_NEXT_ROW "V010.MAP.9"
#define YVEX_TENSOR_MAPPING_GATE_NEXT_ROW "V010.QUANT.0"
#define YVEX_QTYPE_POLICY_NEXT_ROW "V010.QUANT.1"
#define YVEX_QTYPE_POLICY_BACK_ROW "V010.MAP.9"
#define YVEX_QTYPE_ROLE_SUPPORT_NEXT_ROW "V010.QUANT.2"
#define YVEX_TENSOR_COLLECTION_LAYER_CAP 512u
#define YVEX_TENSOR_NAMING_ENTRY_CAP 1024u
#define YVEX_TENSOR_NAMING_TEXT_CAP 192u
#define YVEX_TOKENIZER_MAP_JSON_CAP 65536u

static int parse_model_target_output_mode(const char *value,
                                          yvex_model_target_output_mode *mode)
{
    if (!value || !mode) {
        return 0;
    }
    if (strcmp(value, "normal") == 0) {
        *mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        return 1;
    }
    if (strcmp(value, "table") == 0) {
        *mode = YVEX_MODEL_TARGET_OUTPUT_TABLE;
        return 1;
    }
    if (strcmp(value, "audit") == 0) {
        *mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
        return 1;
    }
    return 0;
}

static const char *model_target_output_mode_name(
    yvex_model_target_output_mode mode)
{
    if (mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) return "table";
    if (mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) return "audit";
    return "normal";
}

typedef struct {
    const char *target_id;
    const char *family_key;
    const char *source_family;
    const char *class_name;
    const char *runtime_shape;
    const char *backend_pressure;
    const char *missing_source_blocker;
    const char *malformed_header_blocker;
    const char *missing_role_map_blocker;
} yvex_model_class_profile_spec;

static const yvex_model_class_profile_spec model_class_profile_specs[] = {
    {
        "qwen3-8b",
        "qwen",
        "qwen",
        "qwen-source-model-class-profile",
        "causal-decoder-candidate-pending-config",
        "metal-planned",
        "missing-qwen-source-path",
        "malformed-qwen-safetensors-header",
        "missing-qwen-tensor-role-map",
    },
    {
        "gemma-4-12b-it",
        "gemma",
        "gemma",
        "gemma-source-model-class-profile",
        "dense-causal-decoder-candidate-pending-config",
        "cpu-cuda-baseline-planned",
        "missing-gemma-source-path",
        "malformed-gemma-safetensors-header",
        "missing-gemma-tensor-role-map",
    },
};

static const unsigned long model_class_profile_spec_count =
    sizeof(model_class_profile_specs) / sizeof(model_class_profile_specs[0]);

typedef struct {
    const yvex_model_target_record *record;
    const yvex_model_class_profile_spec *spec;
    const char *status;
    const char *source_metadata_status;
    const char *top_blocker;
    char source_path[YVEX_PATH_CAP];
    char source_path_source[32];
    int source_exists;
    int config_present;
    int tokenizer_present;
    unsigned long long tensor_count;
    unsigned long long embedding_pattern_count;
    unsigned long long attention_q_pattern_count;
    unsigned long long attention_k_pattern_count;
    unsigned long long attention_v_pattern_count;
    unsigned long long attention_o_pattern_count;
    unsigned long long attention_self_pattern_count;
    unsigned long long mlp_gate_pattern_count;
    unsigned long long mlp_up_pattern_count;
    unsigned long long mlp_down_pattern_count;
    unsigned long long mlp_generic_pattern_count;
    unsigned long long norm_pattern_count;
    unsigned long long output_head_pattern_count;
    unsigned long long moe_router_pattern_count;
    unsigned long long moe_expert_pattern_count;
    unsigned long long other_pattern_count;
} yvex_model_class_profile;

typedef struct {
    int seen;
    int q;
    int k;
    int v;
    int o;
    int gate;
    int up;
    int down;
} yvex_tensor_collection_layer_flags;

typedef struct {
    const yvex_model_target_record *record;
    const yvex_model_class_profile_spec *spec;
    const char *status;
    const char *source_metadata_status;
    const char *top_blocker;
    char source_path[YVEX_PATH_CAP];
    char source_path_source[32];
    int source_exists;
    int config_present;
    int tokenizer_present;
    unsigned long long tensor_count;
    unsigned long long layer_count_observed;
    unsigned long long embedding_tensor_count;
    unsigned long long attention_q_count;
    unsigned long long attention_k_count;
    unsigned long long attention_v_count;
    unsigned long long attention_o_count;
    unsigned long long attention_complete_qkvo_layer_count;
    unsigned long long mlp_gate_count;
    unsigned long long mlp_up_count;
    unsigned long long mlp_down_count;
    unsigned long long mlp_complete_gud_layer_count;
    unsigned long long norm_tensor_count;
    unsigned long long output_head_tensor_count;
    unsigned long long moe_router_count;
    unsigned long long moe_expert_count;
    unsigned long long other_tensor_count;
    yvex_tensor_collection_layer_flags layers[YVEX_TENSOR_COLLECTION_LAYER_CAP];
} yvex_tensor_collection_profile;

typedef struct {
    char native_name[YVEX_TENSOR_NAMING_TEXT_CAP];
    char canonical_role[YVEX_TENSOR_NAMING_TEXT_CAP];
    char family[16];
    char target_id[32];
    char collection[32];
    char layer_index[32];
    char expert_index[32];
    char dtype[24];
    char rank[16];
    char shape[128];
    char source_file[YVEX_TENSOR_NAMING_TEXT_CAP];
    const char *mapping_status;
} yvex_tensor_naming_entry;

typedef struct {
    const yvex_model_target_record *record;
    const yvex_model_class_profile_spec *spec;
    const char *status;
    const char *source_metadata_status;
    const char *top_blocker;
    char source_path[YVEX_PATH_CAP];
    char source_path_source[32];
    int source_exists;
    int config_present;
    int tokenizer_present;
    unsigned long long tensor_count;
    unsigned long long mapped_total_count;
    unsigned long long unmapped_unknown_count;
    unsigned long long ambiguous_count;
    unsigned long long layer_count_observed;
    unsigned long long embedding_count;
    unsigned long long attention_count;
    unsigned long long attention_q_count;
    unsigned long long attention_k_count;
    unsigned long long attention_v_count;
    unsigned long long attention_o_count;
    unsigned long long mlp_count;
    unsigned long long mlp_gate_count;
    unsigned long long mlp_up_count;
    unsigned long long mlp_down_count;
    unsigned long long norm_count;
    unsigned long long output_head_count;
    unsigned long long qwen_linear_attn_count;
    unsigned long long moe_router_count;
    unsigned long long moe_expert_count;
    unsigned long long moe_shared_count;
    unsigned long long entry_count;
    yvex_tensor_collection_layer_flags layers[YVEX_TENSOR_COLLECTION_LAYER_CAP];
    yvex_tensor_naming_entry entries[YVEX_TENSOR_NAMING_ENTRY_CAP];
} yvex_tensor_naming_profile;

typedef struct {
    int present;
    char native_name[YVEX_TENSOR_NAMING_TEXT_CAP];
    char canonical_role[YVEX_TENSOR_NAMING_TEXT_CAP];
    char dtype[24];
    char rank[16];
    char shape[128];
    char vocab_dim_candidate[32];
    char hidden_dim_candidate[32];
    char source_file[YVEX_TENSOR_NAMING_TEXT_CAP];
    const char *mapping_status;
} yvex_output_head_map_entry;

typedef struct {
    const yvex_model_target_record *record;
    const yvex_model_class_profile_spec *spec;
    const char *status;
    const char *source_metadata_status;
    const char *top_blocker;
    const char *tie_policy_status;
    const char *config_tie_word_embeddings_status;
    const char *shape_relation_status;
    const char *output_head_missing_status;
    char source_path[YVEX_PATH_CAP];
    char source_path_source[32];
    int source_exists;
    int config_present;
    int tokenizer_present;
    unsigned long long tensor_count;
    unsigned long long output_head_candidate_count;
    unsigned long long output_head_ambiguous_count;
    yvex_output_head_map_entry output_head;
    yvex_output_head_map_entry embedding;
    yvex_output_head_map_entry final_norm;
} yvex_output_head_map_profile;

typedef struct {
    const char *file_name;
    const char *canonical_role;
    const char *status;
    char path[YVEX_PATH_CAP];
} yvex_tokenizer_map_sidecar;

typedef struct {
    const yvex_model_target_record *record;
    const yvex_model_class_profile_spec *spec;
    const char *status;
    const char *top_blocker;
    char source_path[YVEX_PATH_CAP];
    char source_path_source[32];
    int source_exists;
    yvex_tokenizer_map_sidecar tokenizer_json;
    yvex_tokenizer_map_sidecar tokenizer_config;
    yvex_tokenizer_map_sidecar special_tokens_map;
    yvex_tokenizer_map_sidecar generation_config;
    yvex_tokenizer_map_sidecar config_json;
    yvex_tokenizer_map_sidecar vocab_json;
    yvex_tokenizer_map_sidecar merges_txt;
    yvex_tokenizer_map_sidecar tokenizer_model;
    yvex_tokenizer_map_sidecar chat_template_file;
    char tokenizer_class[128];
    char model_type[64];
    char tokenizer_backend_type[64];
    const char *vocab_size_status;
    char vocab_size[32];
    char config_vocab_size[32];
    char tokenizer_vocab_size[32];
    char output_head_vocab_dim_candidate[32];
    const char *output_head_vocab_relation_status;
    const char *bos_token_id_status;
    char bos_token_id[32];
    const char *bos_token_status;
    char bos_token[64];
    const char *eos_token_id_status;
    char eos_token_id[32];
    const char *eos_token_status;
    char eos_token[64];
    const char *pad_token_id_status;
    char pad_token_id[32];
    const char *pad_token_status;
    char pad_token[64];
    const char *unk_token_id_status;
    char unk_token_id[32];
    const char *unk_token_status;
    char unk_token[64];
    const char *sep_token_id_status;
    char sep_token_id[32];
    const char *sep_token_status;
    char sep_token[64];
    const char *added_tokens_status;
    char added_tokens_count[32];
    const char *special_tokens_status;
    const char *additional_special_tokens_status;
    char additional_special_tokens_count[32];
    const char *stop_token_candidate_status;
    char stop_token_candidate_count[32];
    char stop_token_candidate_0_id[32];
    char stop_token_candidate_0_text[64];
    const char *chat_template_status;
    const char *chat_template_present;
    const char *chat_template_source;
    const char *prompt_template_status;
} yvex_tokenizer_map_profile;

typedef struct {
    const char *name;
    const char *status;
    const char *blocker_class;
} yvex_missing_role_entry;

typedef struct {
    const yvex_model_target_record *record;
    const yvex_model_class_profile_spec *spec;
    const char *status;
    const char *top_blocker;
    char top_blocker_storage[96];
    char source_path[YVEX_PATH_CAP];
    char source_path_source[32];
    int source_exists;
    const char *embedding_status;
    const char *attention_norm_status;
    const char *attention_q_status;
    const char *attention_k_status;
    const char *attention_v_status;
    const char *attention_o_status;
    const char *mlp_norm_status;
    const char *mlp_gate_status;
    const char *mlp_up_status;
    const char *mlp_down_status;
    const char *final_norm_status;
    const char *output_head_status;
    const char *tied_head_policy_status;
    const char *qwen_linear_attn_status;
    const char *moe_router_status;
    const char *moe_expert_status;
    const char *moe_shared_status;
    const char *unknown_tensor_status;
    const char *tokenizer_metadata_status;
    const char *config_metadata_status;
    const char *generation_metadata_status;
    const char *special_tokens_status;
    unsigned long long tensor_count;
    unsigned long long mapped_total_count;
    unsigned long long unmapped_unknown_count;
    unsigned long long layer_count_observed;
    unsigned long long embedding_count;
    unsigned long long attention_count;
    unsigned long long mlp_count;
    unsigned long long norm_count;
    unsigned long long output_head_count;
    unsigned long long qwen_linear_attn_count;
    unsigned long long moe_router_count;
    unsigned long long moe_expert_count;
    unsigned long long moe_shared_count;
    unsigned long long source_role_observed_count;
    unsigned long long source_role_missing_count;
    unsigned long long source_role_ambiguous_count;
    unsigned long long metadata_observed_count;
    unsigned long long metadata_missing_count;
    unsigned long long metadata_ambiguous_count;
    int qwen_extra_required;
    unsigned long long missing_entry_count;
    yvex_missing_role_entry missing_entries[32];
} yvex_missing_role_report_profile;

typedef struct {
    const yvex_model_target_record *record;
    const yvex_model_class_profile_spec *spec;
    const char *status;
    const char *gate_result;
    const char *top_blocker;
    const char *next_required_row;
    char missing_roles[256];
    char missing_source_roles[192];
    char missing_metadata_roles[128];
    char ambiguous_roles[192];
    yvex_model_class_profile model_class;
    yvex_tensor_collection_profile tensor_collection;
    yvex_tensor_naming_profile tensor_naming;
    yvex_output_head_map_profile output_head;
    yvex_tokenizer_map_profile tokenizer;
    yvex_missing_role_report_profile missing_role;
} yvex_tensor_mapping_gate_profile;

static int model_class_name_contains_ci(const char *name, const char *needle)
{
    size_t needle_len;
    size_t i;

    if (!name || !needle) {
        return 0;
    }
    needle_len = strlen(needle);
    if (needle_len == 0) {
        return 1;
    }
    for (i = 0; name[i] != '\0'; ++i) {
        size_t j;
        for (j = 0; j < needle_len; ++j) {
            if (name[i + j] == '\0' ||
                tolower((unsigned char)name[i + j]) !=
                    tolower((unsigned char)needle[j])) {
                break;
            }
        }
        if (j == needle_len) {
            return 1;
        }
    }
    return 0;
}

static int model_class_path_join(char *out, size_t cap,
                                 const char *dir, const char *name)
{
    int n;

    if (!out || cap == 0 || !dir || !name) {
        return 0;
    }
    n = snprintf(out, cap, "%s/%s", dir, name);
    if (n < 0 || (size_t)n >= cap) {
        return 0;
    }
    return 1;
}

static int model_class_file_present(const char *dir, const char *name)
{
    char path[YVEX_PATH_CAP];
    struct stat st;

    if (!model_class_path_join(path, sizeof(path), dir, name)) {
        return 0;
    }
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int model_class_dir_exists(const char *path)
{
    struct stat st;

    return path && path[0] != '\0' &&
           stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static const yvex_model_class_profile_spec *find_model_class_profile_spec(
    const char *target_id)
{
    unsigned long i;

    if (!target_id) {
        return NULL;
    }
    for (i = 0; i < model_class_profile_spec_count; ++i) {
        if (strcmp(model_class_profile_specs[i].target_id, target_id) == 0) {
            return &model_class_profile_specs[i];
        }
    }
    return NULL;
}

typedef struct {
    int found;
    char target_id[128];
    char family_key[16];
    char family_display[32];
    char model_name[128];
    char local_path_class[192];
    char source_path[YVEX_PATH_CAP];
    char registry_path[YVEX_PATH_CAP];
    char download_report_path[YVEX_PATH_CAP];
    char tensor_map_path[YVEX_PATH_CAP];
    char output_head_map_path[YVEX_PATH_CAP];
    char tokenizer_map_path[YVEX_PATH_CAP];
    yvex_model_target_record record;
    yvex_model_class_profile_spec spec;
} yvex_dynamic_source_target;

static int model_target_file_exists(const char *path)
{
    struct stat st;

    return path && path[0] &&
           stat(path, &st) == 0 &&
           S_ISREG(st.st_mode);
}

static int model_target_path_format(char *out,
                                    size_t cap,
                                    const char *fmt,
                                    const char *a,
                                    const char *b,
                                    const char *c)
{
    int n;

    if (!out || cap == 0 || !fmt) return 0;
    n = snprintf(out, cap, fmt, a ? a : "", b ? b : "", c ? c : "");
    if (n < 0 || (size_t)n >= cap) {
        if (cap > 0) out[0] = '\0';
        return 0;
    }
    return 1;
}

static int model_target_read_small_json(const char *path, char **out)
{
    FILE *fp;
    long size;
    char *buf;

    if (!path || !out) return 0;
    *out = NULL;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    size = ftell(fp);
    if (size < 0 || size > 1024L * 1024L) {
        fclose(fp);
        return 0;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 0;
    }
    buf = (char *)malloc((size_t)size + 1u);
    if (!buf) {
        fclose(fp);
        return 0;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    buf[size] = '\0';
    *out = buf;
    return 1;
}

static int model_target_json_string_field(const char *json,
                                          const char *field,
                                          char *out,
                                          size_t cap)
{
    char pattern[128];
    const char *p;
    const char *q;
    size_t len;

    if (!json || !field || !out || cap == 0) return 0;
    out[0] = '\0';
    if (snprintf(pattern, sizeof(pattern), "\"%s\"", field) < 0) return 0;
    p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    while (*p && isspace((unsigned char)*p)) ++p;
    if (*p != ':') return 0;
    ++p;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (*p != '"') return 0;
    ++p;
    q = p;
    while (*q && *q != '"') {
        if (*q == '\\' && q[1]) ++q;
        ++q;
    }
    if (*q != '"') return 0;
    len = (size_t)(q - p);
    if (len >= cap) len = cap - 1u;
    memcpy(out, p, len);
    out[len] = '\0';
    return 1;
}

static int model_target_json_bool_field(const char *json,
                                        const char *field,
                                        int *out)
{
    char pattern[128];
    const char *p;

    if (!json || !field || !out) return 0;
    if (snprintf(pattern, sizeof(pattern), "\"%s\"", field) < 0) return 0;
    p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    while (*p && isspace((unsigned char)*p)) ++p;
    if (*p != ':') return 0;
    ++p;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (strncmp(p, "true", 4) == 0) {
        *out = 1;
        return 1;
    }
    if (strncmp(p, "false", 5) == 0) {
        *out = 0;
        return 1;
    }
    return 0;
}

static const char *model_target_repo_basename(const char *repo_id)
{
    const char *slash;

    if (!repo_id || !repo_id[0]) return NULL;
    slash = strrchr(repo_id, '/');
    return slash && slash[1] ? slash + 1 : repo_id;
}

static void model_target_copy_dynamic_model_name(yvex_dynamic_source_target *target,
                                                 const char *model_name)
{
    if (!target || !model_name || !model_name[0]) return;
    snprintf(target->model_name, sizeof(target->model_name), "%s", model_name);
    if (strcmp(target->family_key, "gemma") == 0 &&
        strncmp(target->model_name, "gemma-", 6) == 0) {
        target->model_name[0] = 'G';
    }
}

static int model_target_dynamic_family_from_target(const char *target_id,
                                                   const char **family_key,
                                                   const char **family_display)
{
    if (!target_id || !target_id[0]) return 0;
    if (strncmp(target_id, "qwen", 4) == 0) {
        *family_key = "qwen";
        *family_display = "Qwen";
        return 1;
    }
    if (strncmp(target_id, "gemma", 5) == 0) {
        *family_key = "gemma";
        *family_display = "Gemma";
        return 1;
    }
    return 0;
}

static void model_target_dynamic_seed_spec(yvex_dynamic_source_target *target)
{
    int is_gemma;

    if (!target) return;
    is_gemma = strcmp(target->family_key, "gemma") == 0;
    target->spec.target_id = target->target_id;
    target->spec.family_key = target->family_key;
    target->spec.source_family = target->family_key;
    target->spec.class_name = is_gemma
                                  ? "gemma-source-model-class-profile"
                                  : "qwen-source-model-class-profile";
    target->spec.runtime_shape = is_gemma
                                     ? "dense-causal-decoder-candidate-pending-config"
                                     : "causal-decoder-candidate-pending-config";
    target->spec.backend_pressure = is_gemma
                                        ? "cpu-cuda-baseline-planned"
                                        : "metal-planned";
    target->spec.missing_source_blocker = is_gemma
                                              ? "missing-gemma-source-path"
                                              : "missing-qwen-source-path";
    target->spec.malformed_header_blocker = is_gemma
                                                ? "malformed-gemma-safetensors-header"
                                                : "malformed-qwen-safetensors-header";
    target->spec.missing_role_map_blocker = is_gemma
                                                ? "missing-gemma-tensor-role-map"
                                                : "missing-qwen-tensor-role-map";
}

static void model_target_dynamic_seed_record(yvex_dynamic_source_target *target)
{
    if (!target) return;
    target->record.target_id = target->target_id;
    target->record.family = target->family_display;
    target->record.model = target->model_name[0] ? target->model_name : target->target_id;
    target->record.target_class = "source-model-candidate";
    target->record.source_artifact_class = "official-source-tensors-downloaded";
    target->record.target_artifact_class = "future-YVEX-produced-GGUF";
    target->record.pressure_purpose = "dynamic-downloaded-source-target";
    target->record.tensor_set = "source-safetensors-header-map";
    target->record.local_path_class = target->local_path_class;
    target->record.source_footprint_class = "downloaded-source-sidecar";
    target->record.runtime_boundary = "source/map handoff only; no runtime/generation";
    target->record.runtime_execution = "unsupported";
    target->record.generation = "unsupported";
    target->record.external_reference = "false";
}

static int model_target_probe_download_identity(const char *path,
                                                yvex_dynamic_source_target *target)
{
    char *json = NULL;
    char value[YVEX_PATH_CAP];

    if (!path || !target || !model_target_read_small_json(path, &json)) {
        return 0;
    }
    if (model_target_json_string_field(json, "target_id", value, sizeof(value)) &&
        strcmp(value, target->target_id) != 0) {
        free(json);
        return 0;
    }
    if (model_target_json_string_field(json, "family", value, sizeof(value)) &&
        strcmp(value, target->family_key) != 0) {
        free(json);
        return 0;
    }
    if (model_target_json_string_field(json, "repo_id", value, sizeof(value))) {
        const char *base = model_target_repo_basename(value);
        if (base && base[0]) {
            model_target_copy_dynamic_model_name(target, base);
        }
    }
    if (model_target_json_string_field(json, "local_source_dir",
                                       value, sizeof(value))) {
        snprintf(target->source_path, sizeof(target->source_path), "%s", value);
    }
    free(json);
    return 1;
}

static int model_target_resolve_dynamic_source_target(
    const char *target_id,
    const char *models_root_override,
    yvex_dynamic_source_target *target)
{
    const char *family_key = NULL;
    const char *family_display = NULL;
    yvex_paths paths;
    yvex_operator_paths operator_paths;
    yvex_error err;
    int rc;
    int has_identity = 0;

    if (!target_id || !target || !model_target_dynamic_family_from_target(
            target_id, &family_key, &family_display)) {
        return 0;
    }
    memset(target, 0, sizeof(*target));
    snprintf(target->target_id, sizeof(target->target_id), "%s", target_id);
    snprintf(target->family_key, sizeof(target->family_key), "%s", family_key);
    snprintf(target->family_display, sizeof(target->family_display), "%s",
             family_display);

    yvex_error_clear(&err);
    rc = yvex_paths_default(&paths, &err);
    if (rc != YVEX_OK) return 0;
    rc = yvex_operator_paths_resolve(&paths, models_root_override,
                                     &operator_paths, &err);
    if (rc != YVEX_OK) return 0;

    (void)model_target_path_format(target->registry_path,
                                   sizeof(target->registry_path),
                                   "%s/%s/%s.download.json",
                                   operator_paths.registry_root,
                                   target->family_key,
                                   target->target_id);
    (void)model_target_path_format(target->download_report_path,
                                   sizeof(target->download_report_path),
                                   "%s/%s/%s.download-report.json",
                                   operator_paths.reports_root,
                                   target->family_key,
                                   target->target_id);
    (void)model_target_path_format(target->tensor_map_path,
                                   sizeof(target->tensor_map_path),
                                   "%s/%s/%s.tensor-map.json",
                                   operator_paths.reports_root,
                                   target->family_key,
                                   target->target_id);
    (void)model_target_path_format(target->output_head_map_path,
                                   sizeof(target->output_head_map_path),
                                   "%s/%s/%s.output-head-map.json",
                                   operator_paths.reports_root,
                                   target->family_key,
                                   target->target_id);
    (void)model_target_path_format(target->tokenizer_map_path,
                                   sizeof(target->tokenizer_map_path),
                                   "%s/%s/%s.tokenizer-map.json",
                                   operator_paths.reports_root,
                                   target->family_key,
                                   target->target_id);
    (void)model_target_path_format(target->source_path,
                                   sizeof(target->source_path),
                                   "%s/hf/%s/%s",
                                   operator_paths.models_root,
                                   target->family_key,
                                   target->target_id);
    (void)model_target_path_format(target->local_path_class,
                                   sizeof(target->local_path_class),
                                   "hf/%s/%s",
                                   target->family_key,
                                   target->target_id,
                                   NULL);

    if (model_target_file_exists(target->registry_path)) {
        has_identity = model_target_probe_download_identity(target->registry_path,
                                                           target);
    }
    if (!has_identity && model_target_file_exists(target->download_report_path)) {
        has_identity = model_target_probe_download_identity(target->download_report_path,
                                                           target);
    }
    if (!has_identity) {
        return 0;
    }

    target->found = 1;
    if (!target->model_name[0]) {
        model_target_copy_dynamic_model_name(target, target->target_id);
    }
    model_target_dynamic_seed_spec(target);
    model_target_dynamic_seed_record(target);
    return 1;
}

static int model_target_mkdir_parent(const char *path)
{
    char buf[YVEX_PATH_CAP];
    char *slash;
    char *p;

    if (!path || strlen(path) >= sizeof(buf)) return 0;
    strcpy(buf, path);
    slash = strrchr(buf, '/');
    if (!slash) return 1;
    *slash = '\0';
    if (!buf[0]) return 1;
    for (p = buf + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(buf, 0775) != 0 && errno != EEXIST) return 0;
            *p = '/';
        }
    }
    return mkdir(buf, 0775) == 0 || errno == EEXIST;
}

static void model_target_json_write_escaped(FILE *fp, const char *s)
{
    if (!s) s = "";
    model_target_file_char('"', fp);
    while (*s) {
        unsigned char ch = (unsigned char)*s++;
        if (ch == '"' || ch == '\\') {
            model_target_file_char('\\', fp);
            model_target_file_char((int)ch, fp);
        } else if (ch == '\n') {
            model_target_out_fputs("\\n", fp);
        } else if (ch == '\r') {
            model_target_out_fputs("\\r", fp);
        } else if (ch == '\t') {
            model_target_out_fputs("\\t", fp);
        } else {
            model_target_file_char((int)ch, fp);
        }
    }
    model_target_file_char('"', fp);
}

static void model_target_json_field(FILE *fp,
                                    const char *key,
                                    const char *value,
                                    int comma)
{
    model_target_out_writef(fp, "  \"%s\": ", key);
    model_target_json_write_escaped(fp, value);
    model_target_out_writef(fp, "%s\n", comma ? "," : "");
}

static void model_target_json_u64_field(FILE *fp,
                                        const char *key,
                                        unsigned long long value,
                                        int comma)
{
    model_target_out_writef(fp, "  \"%s\": %llu%s\n", key, value, comma ? "," : "");
}

static int model_target_json_open_tmp(const char *path,
                                      char *tmp,
                                      size_t tmp_cap,
                                      FILE **out)
{
    int n;

    if (!path || !tmp || tmp_cap == 0 || !out) return 0;
    *out = NULL;
    if (!model_target_mkdir_parent(path)) return 0;
    n = snprintf(tmp, tmp_cap, "%s.tmp", path);
    if (n < 0 || (size_t)n >= tmp_cap) return 0;
    *out = fopen(tmp, "wb");
    return *out != NULL;
}

static int model_target_json_close_tmp(FILE *fp,
                                       const char *tmp,
                                       const char *path)
{
    if (!fp || !tmp || !path) return 0;
    if (fclose(fp) != 0) {
        remove(tmp);
        return 0;
    }
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return 0;
    }
    return 1;
}

static int tensor_naming_required_groups_present(
    const yvex_tensor_naming_profile *profile);

/*
 * write_tensor_map_sidecar()
 *
 * Purpose:
 *   write the report-only tensor naming map sidecar for a dynamic source target.
 *
 * Inputs:
 *   path is the target sidecar path; profile is a borrowed header-derived
 *   report profile.
 *
 * Effects:
 *   creates parent directories as needed, writes a temporary JSON file, and
 *   renames it into place; it does not read tensor payload bytes or emit GGUF.
 *
 * Failure:
 *   returns false when directory creation, temporary open, write close, or
 *   rename fails.
 *
 * Boundary:
 *   a tensor-map sidecar is report evidence only and does not create runtime
 *   descriptor readiness, artifact readiness, or generation capability.
 */
static int write_tensor_map_sidecar(const char *path,
                                    const yvex_tensor_naming_profile *profile)
{
    char tmp[YVEX_PATH_CAP];
    FILE *fp;

    if (!path || !path[0] || !profile) return 1;
    if (!model_target_json_open_tmp(path, tmp, sizeof(tmp), &fp)) return 0;
    model_target_out_writef(fp, "{\n");
    model_target_json_field(fp, "schema", "yvex.source.tensor_map.v1", 1);
    model_target_json_field(fp, "row", "MODELS.SOURCE.MAP.HANDOFF.0", 1);
    model_target_json_field(fp, "status", "present-report-only", 1);
    model_target_json_field(fp, "target_id", profile->record->target_id, 1);
    model_target_json_field(fp, "family", profile->spec->family_key, 1);
    model_target_json_field(fp, "map_kind", "tensor-naming", 1);
    model_target_json_field(fp, "source_path", profile->source_path, 1);
    model_target_json_field(fp, "evidence_basis", "header-metadata-only", 1);
    model_target_json_u64_field(fp, "tensor_count", profile->tensor_count, 1);
    model_target_json_u64_field(fp, "mapped_total_count",
                                profile->mapped_total_count, 1);
    model_target_json_u64_field(fp, "unmapped_unknown_count",
                                profile->unmapped_unknown_count, 1);
    model_target_json_u64_field(fp, "output_head_count",
                                profile->output_head_count, 1);
    model_target_json_u64_field(fp, "qwen_linear_attn_count",
                                profile->qwen_linear_attn_count, 1);
    model_target_json_u64_field(fp, "moe_router_count",
                                profile->moe_router_count, 1);
    model_target_json_u64_field(fp, "moe_expert_count",
                                profile->moe_expert_count, 1);
    model_target_json_u64_field(fp, "moe_shared_count",
                                profile->moe_shared_count, 1);
    model_target_json_field(fp, "required_role_coverage_status",
                            tensor_naming_required_groups_present(profile)
                                ? "required-groups-present"
                                : "required-groups-missing", 1);
    model_target_json_field(fp, "tokenizer_map_status", "missing", 1);
    model_target_json_field(fp, "runtime_claim", "unsupported", 1);
    model_target_json_field(fp, "generation", "unsupported-full-model", 1);
    model_target_json_field(fp, "benchmark_status", "not-measured", 0);
    model_target_out_writef(fp, "}\n");
    return model_target_json_close_tmp(fp, tmp, path);
}

static int write_output_head_map_sidecar(
    const char *path,
    const yvex_output_head_map_profile *profile)
{
    char tmp[YVEX_PATH_CAP];
    FILE *fp;

    if (!path || !path[0] || !profile) return 1;
    if (!model_target_json_open_tmp(path, tmp, sizeof(tmp), &fp)) return 0;
    model_target_out_writef(fp, "{\n");
    model_target_json_field(fp, "schema", "yvex.source.output_head_map.v1", 1);
    model_target_json_field(fp, "row", "MODELS.SOURCE.MAP.HANDOFF.0", 1);
    model_target_json_field(fp, "status", "present-report-only", 1);
    model_target_json_field(fp, "target_id", profile->record->target_id, 1);
    model_target_json_field(fp, "family", profile->spec->family_key, 1);
    model_target_json_field(fp, "map_kind", "output-head", 1);
    model_target_json_field(fp, "source_path", profile->source_path, 1);
    model_target_json_field(fp, "output_head_map_status", profile->status, 1);
    model_target_json_field(fp, "output_head_status",
                            profile->output_head.present ? "present" : "missing", 1);
    model_target_json_field(fp, "output_head_native_name",
                            profile->output_head.native_name, 1);
    model_target_json_field(fp, "output_head_canonical_role",
                            profile->output_head.canonical_role, 1);
    model_target_json_field(fp, "output_head_mapping_status",
                            profile->output_head.mapping_status, 1);
    model_target_json_field(fp, "tie_policy_status",
                            profile->tie_policy_status, 1);
    model_target_json_field(fp, "config_tie_word_embeddings_status",
                            profile->config_tie_word_embeddings_status, 1);
    model_target_json_field(fp, "final_norm_status",
                            profile->final_norm.present ? "present" : "missing", 1);
    model_target_json_field(fp, "embedding_status",
                            profile->embedding.present ? "present" : "missing", 1);
    model_target_json_field(fp, "shape_relation_status",
                            profile->shape_relation_status, 1);
    model_target_json_field(fp, "runtime_claim", "unsupported", 1);
    model_target_json_field(fp, "generation", "unsupported-full-model", 1);
    model_target_json_field(fp, "benchmark_status", "not-measured", 0);
    model_target_out_writef(fp, "}\n");
    return model_target_json_close_tmp(fp, tmp, path);
}

static const char *tokenizer_map_next_row(
    const yvex_tokenizer_map_profile *profile);
static const char *tokenizer_map_vocab_status(
    const yvex_tokenizer_map_profile *profile);
static const char *tokenizer_map_merges_status(
    const yvex_tokenizer_map_profile *profile);

static int write_tokenizer_map_sidecar(
    const char *path,
    const yvex_tokenizer_map_profile *profile)
{
    char tmp[YVEX_PATH_CAP];
    FILE *fp;

    if (!path || !path[0] || !profile) return 1;
    if (!model_target_json_open_tmp(path, tmp, sizeof(tmp), &fp)) return 0;
    model_target_out_writef(fp, "{\n");
    model_target_json_field(fp, "schema_version", "yvex.source.tokenizer_map.v1", 1);
    model_target_json_field(fp, "row", "V010.MAP.7", 1);
    model_target_json_field(fp, "status", profile->status, 1);
    model_target_json_field(fp, "target_id", profile->record->target_id, 1);
    model_target_json_field(fp, "family", profile->spec->family_key, 1);
    model_target_json_field(fp, "source_path", profile->source_path, 1);
    model_target_json_field(fp, "tokenizer_map_status", profile->status, 1);
    model_target_json_field(fp, "evidence_basis", "sidecar-json-only", 1);
    model_target_json_field(fp, "tokenizer_json_status",
                            profile->tokenizer_json.status, 1);
    model_target_json_field(fp, "tokenizer_config_status",
                            profile->tokenizer_config.status, 1);
    model_target_json_field(fp, "special_tokens_map_status",
                            profile->special_tokens_map.status, 1);
    model_target_json_field(fp, "generation_config_status",
                            profile->generation_config.status, 1);
    model_target_json_field(fp, "config_json_status",
                            profile->config_json.status, 1);
    model_target_json_field(fp, "vocab_status",
                            tokenizer_map_vocab_status(profile), 1);
    model_target_json_field(fp, "merges_status",
                            tokenizer_map_merges_status(profile), 1);
    model_target_json_field(fp, "tokenizer_model_type",
                            profile->model_type, 1);
    model_target_json_field(fp, "tokenizer_backend_type",
                            profile->tokenizer_backend_type, 1);
    model_target_json_field(fp, "vocab_size_status",
                            profile->vocab_size_status, 1);
    model_target_json_field(fp, "vocab_size", profile->vocab_size, 1);
    model_target_json_field(fp, "added_tokens_count",
                            profile->added_tokens_count, 1);
    model_target_json_field(fp, "special_tokens_status",
                            profile->special_tokens_status, 1);
    model_target_json_field(fp, "bos_token_id", profile->bos_token_id, 1);
    model_target_json_field(fp, "bos_token", profile->bos_token, 1);
    model_target_json_field(fp, "eos_token_id", profile->eos_token_id, 1);
    model_target_json_field(fp, "eos_token", profile->eos_token, 1);
    model_target_json_field(fp, "pad_token_id", profile->pad_token_id, 1);
    model_target_json_field(fp, "pad_token", profile->pad_token, 1);
    model_target_json_field(fp, "unk_token_id", profile->unk_token_id, 1);
    model_target_json_field(fp, "unk_token", profile->unk_token, 1);
    model_target_json_field(fp, "additional_special_tokens_count",
                            profile->additional_special_tokens_count, 1);
    model_target_json_field(fp, "stop_token_candidate_count",
                            profile->stop_token_candidate_count, 1);
    model_target_json_field(fp, "stop_token_candidate.0.id",
                            profile->stop_token_candidate_0_id, 1);
    model_target_json_field(fp, "stop_token_candidate.0.text",
                            profile->stop_token_candidate_0_text, 1);
    model_target_json_field(fp, "chat_template_status",
                            profile->chat_template_status, 1);
    model_target_json_field(fp, "chat_template_source",
                            profile->chat_template_source, 1);
    model_target_json_field(fp, "chat_template_hash_status",
                            "not-computed", 1);
    model_target_json_field(fp, "prompt_template_status",
                            profile->prompt_template_status, 1);
    model_target_json_field(fp, "tokenizer_runtime_status",
                            "not-implemented", 1);
    model_target_json_field(fp, "detokenization_status",
                            "not-implemented", 1);
    model_target_json_field(fp, "gguf_tokenizer_contract_status",
                            "planned", 1);
    model_target_json_field(fp, "runtime_claim", "unsupported", 1);
    model_target_json_field(fp, "generation", "unsupported-full-model", 1);
    model_target_json_field(fp, "benchmark_status", "not-measured", 1);
    model_target_json_field(fp, "next", tokenizer_map_next_row(profile), 0);
    model_target_out_writef(fp, "}\n");
    return model_target_json_close_tmp(fp, tmp, path);
}

static int model_class_download_source(
    char *out,
    size_t cap,
    const yvex_operator_paths *operator_paths,
    const yvex_model_class_profile_spec *spec)
{
    int n;

    if (!out || cap == 0 || !operator_paths || !spec) {
        return 0;
    }

    n = snprintf(out, cap, "%s/hf/%s/%s",
                 operator_paths->models_root,
                 spec->source_family,
                 spec->target_id);
    return n >= 0 && (size_t)n < cap;
}

static void model_class_count_tensor(yvex_model_class_profile *profile,
                                     const char *name)
{
    int matched = 0;
    int has_attention_projection = 0;
    int has_mlp_projection = 0;
    int has_gate_proj;
    int has_norm_pattern = 0;
    unsigned long long qwen_norm_count = 0;

    if (!profile || !name) {
        return;
    }
    if (model_class_name_contains_ci(name, "embed_tokens") ||
        model_class_name_contains_ci(name, "token_embd") ||
        model_class_name_contains_ci(name, "embeddings")) {
        profile->embedding_pattern_count++;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "q_proj")) {
        profile->attention_q_pattern_count++;
        matched = 1;
        has_attention_projection = 1;
    }
    if (model_class_name_contains_ci(name, "k_proj")) {
        profile->attention_k_pattern_count++;
        matched = 1;
        has_attention_projection = 1;
    }
    if (model_class_name_contains_ci(name, "v_proj")) {
        profile->attention_v_pattern_count++;
        matched = 1;
        has_attention_projection = 1;
    }
    if (model_class_name_contains_ci(name, "o_proj")) {
        profile->attention_o_pattern_count++;
        matched = 1;
        has_attention_projection = 1;
    }
    if (model_class_name_contains_ci(name, "self_attn") &&
        !has_attention_projection) {
        profile->attention_self_pattern_count++;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "gate_proj")) {
        profile->mlp_gate_pattern_count++;
        matched = 1;
        has_mlp_projection = 1;
    }
    if (model_class_name_contains_ci(name, "up_proj")) {
        profile->mlp_up_pattern_count++;
        matched = 1;
        has_mlp_projection = 1;
    }
    if (model_class_name_contains_ci(name, "down_proj")) {
        profile->mlp_down_pattern_count++;
        matched = 1;
        has_mlp_projection = 1;
    }
    if (model_class_name_contains_ci(name, "mlp") && !has_mlp_projection) {
        profile->mlp_generic_pattern_count++;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "input_layernorm")) {
        has_norm_pattern = 1;
        qwen_norm_count++;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "post_attention_layernorm")) {
        has_norm_pattern = 1;
        qwen_norm_count++;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "pre_feedforward_layernorm")) {
        has_norm_pattern = 1;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "post_feedforward_layernorm")) {
        has_norm_pattern = 1;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "layernorm")) {
        has_norm_pattern = 1;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "norm")) {
        has_norm_pattern = 1;
        qwen_norm_count++;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "rms")) {
        has_norm_pattern = 1;
        qwen_norm_count++;
        matched = 1;
    }
    if (profile->spec && strcmp(profile->spec->family_key, "qwen") == 0) {
        profile->norm_pattern_count += qwen_norm_count;
    } else if (has_norm_pattern) {
        profile->norm_pattern_count++;
    }
    if (model_class_name_contains_ci(name, "lm_head") ||
        model_class_name_contains_ci(name, "output")) {
        profile->output_head_pattern_count++;
        matched = 1;
    }
    has_gate_proj = model_class_name_contains_ci(name, "gate_proj");
    if (model_class_name_contains_ci(name, "router") ||
        (model_class_name_contains_ci(name, "gate") && !has_gate_proj)) {
        profile->moe_router_pattern_count++;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "experts")) {
        profile->moe_expert_pattern_count++;
        matched = 1;
    }
    if (!matched) {
        profile->other_pattern_count++;
    }
}

static unsigned long long model_class_profile_attention_count(
    const yvex_model_class_profile *profile)
{
    if (!profile) {
        return 0;
    }
    return profile->attention_q_pattern_count +
           profile->attention_k_pattern_count +
           profile->attention_v_pattern_count +
           profile->attention_o_pattern_count +
           profile->attention_self_pattern_count;
}

static unsigned long long model_class_profile_mlp_count(
    const yvex_model_class_profile *profile)
{
    if (!profile) {
        return 0;
    }
    return profile->mlp_gate_pattern_count +
           profile->mlp_up_pattern_count +
           profile->mlp_down_pattern_count +
           profile->mlp_generic_pattern_count;
}

static unsigned long long model_class_profile_moe_count(
    const yvex_model_class_profile *profile)
{
    if (!profile) {
        return 0;
    }
    return profile->moe_router_pattern_count +
           profile->moe_expert_pattern_count;
}

static int model_class_resolve_source(
    const char *models_root_override,
    const char *source_override,
    yvex_model_class_profile *profile)
{
    if (!profile) {
        return 2;
    }
    if (source_override) {
        int n;
        n = snprintf(profile->source_path, sizeof(profile->source_path),
                     "%s", source_override);
        if (n < 0 || (size_t)n >= sizeof(profile->source_path)) {
            model_target_out_writef(model_target_err(), "model-target class-profile: source path is too long\n");
            return 2;
        }
        snprintf(profile->source_path_source, sizeof(profile->source_path_source),
                 "%s", "explicit-source");
        return 0;
    }
    {
        yvex_paths paths;
        yvex_operator_paths operator_paths;
        yvex_error err;
        int exists = 0;
        int rc;

        memset(&paths, 0, sizeof(paths));
        yvex_error_clear(&err);
        rc = yvex_operator_paths_resolve(&paths, models_root_override,
                                         &operator_paths, &err);
        if (rc != YVEX_OK) {
            model_target_out_writef(model_target_err(), "yvex: %s: %s\n",
                    yvex_error_where(&err), yvex_error_message(&err));
            return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
        }
        rc = yvex_operator_paths_resolve_target(&operator_paths,
                                                profile->spec->source_family,
                                                "source",
                                                profile->source_path,
                                                sizeof(profile->source_path),
                                                &exists, &err);
        if (rc != YVEX_OK) {
            model_target_out_writef(model_target_err(), "yvex: %s: %s\n",
                    yvex_error_where(&err), yvex_error_message(&err));
            return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
        }
        (void)exists;
        {
            char download_source[YVEX_PATH_CAP];

            if (!model_class_download_source(download_source,
                                             sizeof(download_source),
                                             &operator_paths,
                                             profile->spec)) {
                model_target_out_writef(model_target_err(), "model-target class-profile: source path is too long\n");
                return 2;
            }
            if (model_class_dir_exists(download_source)) {
                snprintf(profile->source_path, sizeof(profile->source_path),
                         "%s", download_source);
            }
        }
        snprintf(profile->source_path_source, sizeof(profile->source_path_source),
                 "%s", operator_paths.models_root_source);
    }
    return 0;
}

/*
 * model_class_build_profile()
 *
 * Purpose:
 *   build a header-metadata-only model-class profile for a model-target report.
 *
 * Inputs:
 *   record/spec and optional path overrides are borrowed; profile receives
 *   stack-owned report fields.
 *
 * Effects:
 *   resolves source paths, checks config/tokenizer presence, opens safetensors
 *   headers through native inventory, and counts lexical patterns. It does not
 *   load tensor payloads, emit artifacts, or execute model code.
 *
 * Failure:
 *   returns parser-style exit status for invalid arguments or allocation/header
 *   inventory failures while recording report blockers where possible.
 *
 * Boundary:
 *   model-class profiling is report-only and does not create model support,
 *   runtime support, generation, eval, benchmark, or release readiness.
 */
static int model_class_build_profile(
    const yvex_model_target_record *record,
    const yvex_model_class_profile_spec *spec,
    const char *models_root_override,
    const char *source_override,
    yvex_model_class_profile *profile)
{
    yvex_native_weight_table *table = NULL;
    yvex_native_weight_options options;
    yvex_error err;
    unsigned long long i;
    int rc;

    if (!record || !spec || !profile) {
        return 2;
    }
    memset(profile, 0, sizeof(*profile));
    profile->record = record;
    profile->spec = spec;
    profile->status = "source-missing";
    profile->source_metadata_status = "missing";
    profile->top_blocker = spec->missing_source_blocker;

    rc = model_class_resolve_source(models_root_override,
                                    source_override,
                                    profile);
    if (rc != 0) {
        return rc;
    }

    profile->source_exists = model_class_dir_exists(profile->source_path);
    if (!profile->source_exists) {
        return 0;
    }

    profile->config_present = model_class_file_present(profile->source_path, "config.json");
    profile->tokenizer_present =
        model_class_file_present(profile->source_path, "tokenizer.json") ||
        model_class_file_present(profile->source_path, "tokenizer_config.json");

    memset(&options, 0, sizeof(options));
    options.source_dir = profile->source_path;
    options.recursive = 0;
    options.include_metadata = 0;
    yvex_error_clear(&err);
    rc = yvex_native_weight_table_open(&table, &options, &err);
    if (rc != YVEX_OK) {
        profile->status = "metadata-error";
        profile->source_metadata_status = "header-error";
        profile->top_blocker = spec->malformed_header_blocker;
        return rc == YVEX_ERR_NOMEM ? 3 : 0;
    }

    profile->tensor_count = yvex_native_weight_table_count(table);
    for (i = 0; i < profile->tensor_count; ++i) {
        const yvex_native_weight_info *info =
            yvex_native_weight_table_at(table, i);
        model_class_count_tensor(profile, info ? info->name : NULL);
    }
    yvex_native_weight_table_close(table);

    profile->status = "metadata-profiled";
    profile->source_metadata_status =
        profile->tensor_count > 0 ? "header-only" : "no-safetensors";
    profile->top_blocker = spec->missing_role_map_blocker;
    return 0;
}

static void print_model_class_profile_normal(
    const yvex_model_class_profile *profile)
{
    model_target_out_writef(model_target_out(), "model-class: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "target: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "class: %s\n", profile->spec->class_name);
    model_target_out_writef(model_target_out(), "evidence: header-metadata-only\n");
    model_target_out_writef(model_target_out(), "patterns: tensors=%llu attn=%llu mlp=%llu norm=%llu head=%llu moe=%llu\n",
           profile->tensor_count,
           model_class_profile_attention_count(profile),
           model_class_profile_mlp_count(profile),
           profile->norm_pattern_count,
           profile->output_head_pattern_count,
           model_class_profile_moe_count(profile));
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    model_target_out_writef(model_target_out(), "boundary: model-class profile only; no tensor role mapping/runtime/generation\n");
}

static void print_model_class_profile_table(
    const yvex_model_class_profile *profile)
{
    model_target_out_writef(model_target_out(), "MODEL CLASS PROFILE\n\n");
    model_target_out_writef(model_target_out(), "%-6s  %-24s  %-16s  %7s  %4s  %3s  %4s  %4s  %3s  %s\n",
           "FAMILY", "TARGET", "STATUS", "TENSORS",
           "ATTN", "MLP", "NORM", "HEAD", "MOE", "NEXT");
    model_target_out_writef(model_target_out(), "%-6s  %-24s  %-16s  %7llu  %4llu  %3llu  %4llu  %4llu  %3llu  %s\n",
           profile->spec->family_key,
           profile->record->target_id,
           profile->status,
           profile->tensor_count,
           model_class_profile_attention_count(profile),
           model_class_profile_mlp_count(profile),
           profile->norm_pattern_count,
           profile->output_head_pattern_count,
           model_class_profile_moe_count(profile),
           YVEX_MODEL_CLASS_NEXT_ROW);
}

static void print_model_class_profile_audit(
    const yvex_model_class_profile *profile)
{
    model_target_out_writef(model_target_out(), "model_class_profile_status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "model_class_family: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "model_class_target_id: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "model_class_name: %s\n", profile->spec->class_name);
    model_target_out_writef(model_target_out(), "model_class_runtime_shape: %s\n", profile->spec->runtime_shape);
    model_target_out_writef(model_target_out(), "model_class_evidence_basis: header-metadata-only\n");
    model_target_out_writef(model_target_out(), "model_class_config_status: %s\n",
           profile->config_present ? "present" : "missing");
    model_target_out_writef(model_target_out(), "model_class_tokenizer_status: %s\n",
           profile->tokenizer_present ? "present" : "missing");
    model_target_out_writef(model_target_out(), "model_class_source_metadata_status: %s\n",
           profile->source_metadata_status);
    model_target_out_writef(model_target_out(), "model_class_tensor_count: %llu\n", profile->tensor_count);
    model_target_out_writef(model_target_out(), "model_class_embedding_pattern_count: %llu\n",
           profile->embedding_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_attention_q_pattern_count: %llu\n",
           profile->attention_q_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_attention_k_pattern_count: %llu\n",
           profile->attention_k_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_attention_v_pattern_count: %llu\n",
           profile->attention_v_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_attention_o_pattern_count: %llu\n",
           profile->attention_o_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_mlp_gate_pattern_count: %llu\n",
           profile->mlp_gate_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_mlp_up_pattern_count: %llu\n",
           profile->mlp_up_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_mlp_down_pattern_count: %llu\n",
           profile->mlp_down_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_norm_pattern_count: %llu\n",
           profile->norm_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_output_head_pattern_count: %llu\n",
           profile->output_head_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_moe_router_pattern_count: %llu\n",
           profile->moe_router_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_moe_expert_pattern_count: %llu\n",
           profile->moe_expert_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_other_pattern_count: %llu\n",
           profile->other_pattern_count);
    model_target_out_writef(model_target_out(), "model_class_pattern_status: lexical-only\n");
    model_target_out_writef(model_target_out(), "model_class_role_mapping_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "model_class_runtime_status: unsupported\n");
    model_target_out_writef(model_target_out(), "backend_selection: deferred\n");
    model_target_out_writef(model_target_out(), "backend_pressure: %s\n", profile->spec->backend_pressure);
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    model_target_out_writef(model_target_out(), "source_path: %s\n", profile->source_path);
    model_target_out_writef(model_target_out(), "source_path_source: %s\n", profile->source_path_source);
    model_target_out_writef(model_target_out(), "source_exists: %s\n", profile->source_exists ? "true" : "false");
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next_required_rows: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    model_target_out_writef(model_target_out(), "boundary: model-class profile only; no tensor role mapping/runtime/generation\n");
}

static void print_model_class_audit_hint(const yvex_model_target_record *record)
{
    const yvex_model_class_profile_spec *spec;

    if (!record) {
        return;
    }
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) {
        return;
    }
    model_target_out_writef(model_target_out(), "model_class_profile_status: command-visible\n");
    model_target_out_writef(model_target_out(), "model_class_family: %s\n", spec->family_key);
    model_target_out_writef(model_target_out(), "model_class_target_id: %s\n", spec->target_id);
    model_target_out_writef(model_target_out(), "model_class_name: %s\n", spec->class_name);
    model_target_out_writef(model_target_out(), "model_class_runtime_shape: %s\n", spec->runtime_shape);
    model_target_out_writef(model_target_out(), "model_class_evidence_basis: header-metadata-only\n");
    model_target_out_writef(model_target_out(), "model_class_pattern_status: lexical-only\n");
    model_target_out_writef(model_target_out(), "model_class_role_mapping_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "model_class_runtime_status: unsupported\n");
}

static int tensor_collection_layer_index(const char *name,
                                              unsigned long *layer_index)
{
    const char *patterns[] = {
        "model.language_model.layers.",
        "language_model.layers.",
        "model.layers.",
        "layers.",
        "blk.",
    };
    unsigned long p;

    if (!name || !layer_index) {
        return 0;
    }
    for (p = 0; p < sizeof(patterns) / sizeof(patterns[0]); ++p) {
        const char *pos = strstr(name, patterns[p]);
        const char *digits;
        char *end = NULL;
        unsigned long value;

        if (!pos) {
            continue;
        }
        digits = pos + strlen(patterns[p]);
        if (!isdigit((unsigned char)digits[0])) {
            continue;
        }
        errno = 0;
        value = strtoul(digits, &end, 10);
        if (errno != 0 || end == digits || !end || *end != '.') {
            continue;
        }
        *layer_index = value;
        return 1;
    }
    return 0;
}

static void tensor_collection_note_layer(
    yvex_tensor_collection_profile *profile,
    unsigned long layer_index,
    const char *kind)
{
    yvex_tensor_collection_layer_flags *flags;

    if (!profile || !kind || layer_index >= YVEX_TENSOR_COLLECTION_LAYER_CAP) {
        return;
    }
    flags = &profile->layers[layer_index];
    if (!flags->seen) {
        flags->seen = 1;
        profile->layer_count_observed++;
    }
    if (strcmp(kind, "q") == 0) flags->q = 1;
    else if (strcmp(kind, "k") == 0) flags->k = 1;
    else if (strcmp(kind, "v") == 0) flags->v = 1;
    else if (strcmp(kind, "o") == 0) flags->o = 1;
    else if (strcmp(kind, "gate") == 0) flags->gate = 1;
    else if (strcmp(kind, "up") == 0) flags->up = 1;
    else if (strcmp(kind, "down") == 0) flags->down = 1;
}

static void tensor_collection_count_tensor(
    yvex_tensor_collection_profile *profile,
    const char *name)
{
    int matched = 0;
    int has_norm_pattern = 0;
    unsigned long layer_index = 0;
    int has_layer = 0;

    if (!profile || !name) {
        return;
    }
    has_layer = tensor_collection_layer_index(name, &layer_index);
    if (model_class_name_contains_ci(name, "embed_tokens") ||
        model_class_name_contains_ci(name, "token_embd") ||
        model_class_name_contains_ci(name, "embeddings")) {
        profile->embedding_tensor_count++;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "q_proj")) {
        profile->attention_q_count++;
        if (has_layer) tensor_collection_note_layer(profile, layer_index, "q");
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "k_proj")) {
        profile->attention_k_count++;
        if (has_layer) tensor_collection_note_layer(profile, layer_index, "k");
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "v_proj")) {
        profile->attention_v_count++;
        if (has_layer) tensor_collection_note_layer(profile, layer_index, "v");
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "o_proj")) {
        profile->attention_o_count++;
        if (has_layer) tensor_collection_note_layer(profile, layer_index, "o");
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "gate_proj")) {
        profile->mlp_gate_count++;
        if (has_layer) tensor_collection_note_layer(profile, layer_index, "gate");
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "up_proj")) {
        profile->mlp_up_count++;
        if (has_layer) tensor_collection_note_layer(profile, layer_index, "up");
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "down_proj")) {
        profile->mlp_down_count++;
        if (has_layer) tensor_collection_note_layer(profile, layer_index, "down");
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "input_layernorm") ||
        model_class_name_contains_ci(name, "post_attention_layernorm") ||
        model_class_name_contains_ci(name, "pre_feedforward_layernorm") ||
        model_class_name_contains_ci(name, "post_feedforward_layernorm") ||
        model_class_name_contains_ci(name, "layernorm") ||
        model_class_name_contains_ci(name, "rms") ||
        model_class_name_contains_ci(name, "norm")) {
        has_norm_pattern = 1;
        matched = 1;
    }
    if (has_norm_pattern) {
        profile->norm_tensor_count++;
    }
    if (model_class_name_contains_ci(name, "lm_head") ||
        model_class_name_contains_ci(name, "output")) {
        profile->output_head_tensor_count++;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "router")) {
        profile->moe_router_count++;
        matched = 1;
    }
    if (model_class_name_contains_ci(name, "experts") ||
        model_class_name_contains_ci(name, "expert")) {
        profile->moe_expert_count++;
        matched = 1;
    }
    if (!matched) {
        profile->other_tensor_count++;
    }
}

static void tensor_collection_finalize(
    yvex_tensor_collection_profile *profile)
{
    unsigned long i;

    if (!profile) {
        return;
    }
    for (i = 0; i < YVEX_TENSOR_COLLECTION_LAYER_CAP; ++i) {
        yvex_tensor_collection_layer_flags *flags = &profile->layers[i];
        if (!flags->seen) {
            continue;
        }
        if (flags->q && flags->k && flags->v && flags->o) {
            profile->attention_complete_qkvo_layer_count++;
        }
        if (flags->gate && flags->up && flags->down) {
            profile->mlp_complete_gud_layer_count++;
        }
    }
}

static const char *tensor_collection_present_status(unsigned long long count)
{
    return count > 0 ? "candidate" : "missing";
}

static const char *tensor_collection_attention_status(
    const yvex_tensor_collection_profile *profile)
{
    if (!profile || (profile->attention_q_count +
                    profile->attention_k_count +
                    profile->attention_v_count +
                    profile->attention_o_count) == 0) {
        return "missing";
    }
    return profile->attention_complete_qkvo_layer_count > 0 ? "candidate" : "incomplete";
}

static const char *tensor_collection_mlp_status(
    const yvex_tensor_collection_profile *profile)
{
    if (!profile || (profile->mlp_gate_count +
                    profile->mlp_up_count +
                    profile->mlp_down_count) == 0) {
        return "missing";
    }
    return profile->mlp_complete_gud_layer_count > 0 ? "candidate" : "incomplete";
}

static int tensor_collection_has_dense_profile(
    const yvex_tensor_collection_profile *profile)
{
    return profile &&
           profile->embedding_tensor_count > 0 &&
           profile->attention_complete_qkvo_layer_count > 0 &&
           profile->mlp_complete_gud_layer_count > 0 &&
           profile->norm_tensor_count > 0 &&
           profile->output_head_tensor_count > 0;
}

static int tensor_collection_has_collection_candidate(
    const yvex_tensor_collection_profile *profile)
{
    if (!profile) {
        return 0;
    }
    return profile->embedding_tensor_count > 0 &&
           profile->attention_q_count > 0 &&
           profile->attention_k_count > 0 &&
           profile->attention_v_count > 0 &&
           profile->attention_o_count > 0 &&
           profile->mlp_gate_count > 0 &&
           profile->mlp_up_count > 0 &&
           profile->mlp_down_count > 0 &&
           profile->norm_tensor_count > 0 &&
           profile->output_head_tensor_count > 0;
}

static const char *tensor_collection_header_blocker(
    const yvex_model_class_profile_spec *spec)
{
    if (!spec) return "missing-source-header-metadata";
    if (strcmp(spec->family_key, "gemma") == 0) {
        return "missing-gemma-header-metadata";
    }
    return "missing-qwen-header-metadata";
}

static const char *tensor_collection_complete_layer_blocker(
    const yvex_model_class_profile_spec *spec)
{
    if (!spec) return "missing-complete-layer-collection";
    if (strcmp(spec->family_key, "gemma") == 0) {
        return "missing-gemma-complete-layer-collection";
    }
    return "missing-qwen-complete-layer-collection";
}

static const char *tensor_collection_incomplete_blocker(
    const yvex_model_class_profile_spec *spec)
{
    if (!spec) return "incomplete-tensor-collection-candidates";
    if (strcmp(spec->family_key, "gemma") == 0) {
        return "incomplete-gemma-tensor-collection-candidates";
    }
    return "incomplete-qwen-tensor-collection-candidates";
}

static int build_tensor_collection_profile(
    const yvex_model_target_record *record,
    const yvex_model_class_profile_spec *spec,
    const char *models_root_override,
    const char *source_override,
    yvex_tensor_collection_profile *profile)
{
    yvex_model_class_profile source_profile;
    yvex_native_weight_table *table = NULL;
    yvex_native_weight_options options;
    yvex_error err;
    unsigned long long i;
    int rc;

    if (!record || !profile) {
        return 2;
    }
    if (!spec) {
        return 2;
    }
    memset(profile, 0, sizeof(*profile));
    profile->record = record;
    profile->spec = spec;
    profile->status = "source-missing";
    profile->source_metadata_status = "missing";
    profile->top_blocker = spec->missing_source_blocker;

    rc = model_class_build_profile(record, spec, models_root_override,
                                   source_override, &source_profile);
    if (rc != 0) {
        return rc;
    }
    snprintf(profile->source_path, sizeof(profile->source_path), "%s",
             source_profile.source_path);
    snprintf(profile->source_path_source, sizeof(profile->source_path_source), "%s",
             source_profile.source_path_source);
    profile->source_exists = source_profile.source_exists;
    profile->config_present = source_profile.config_present;
    profile->tokenizer_present = source_profile.tokenizer_present;
    if (!profile->source_exists) {
        return 0;
    }

    memset(&options, 0, sizeof(options));
    options.source_dir = profile->source_path;
    options.recursive = 0;
    options.include_metadata = 0;
    yvex_error_clear(&err);
    rc = yvex_native_weight_table_open(&table, &options, &err);
    if (rc != YVEX_OK) {
        profile->status = "metadata-missing";
        profile->source_metadata_status = "header-error";
        profile->top_blocker = tensor_collection_header_blocker(spec);
        return rc == YVEX_ERR_NOMEM ? 3 : 0;
    }
    profile->tensor_count = yvex_native_weight_table_count(table);
    for (i = 0; i < profile->tensor_count; ++i) {
        const yvex_native_weight_info *info =
            yvex_native_weight_table_at(table, i);
        tensor_collection_count_tensor(profile, info ? info->name : NULL);
    }
    yvex_native_weight_table_close(table);
    tensor_collection_finalize(profile);

    profile->source_metadata_status =
        profile->tensor_count > 0 ? "header-only" : "no-safetensors";
    if (profile->tensor_count == 0) {
        profile->status = "metadata-missing";
        profile->top_blocker = tensor_collection_header_blocker(spec);
    } else if (tensor_collection_has_dense_profile(profile)) {
        profile->status = "collection-profiled";
        profile->top_blocker = spec->missing_role_map_blocker;
    } else if (tensor_collection_has_collection_candidate(profile)) {
        profile->status = "collection-candidate";
        profile->top_blocker = tensor_collection_complete_layer_blocker(spec);
    } else {
        profile->status = "collection-incomplete";
        profile->top_blocker = tensor_collection_incomplete_blocker(spec);
    }
    return 0;
}

static void print_tensor_collection_normal(
    const yvex_tensor_collection_profile *profile)
{
    model_target_out_writef(model_target_out(), "tensor-collection: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "target: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "stage: header-collection-inventory\n");
    model_target_out_writef(model_target_out(), "evidence: header-metadata-only\n");
    model_target_out_writef(model_target_out(), "collections: embedding=%llu attention_qkvo=%llu mlp_gud=%llu norm=%llu head=%llu moe=%llu\n",
           profile->embedding_tensor_count,
           profile->attention_complete_qkvo_layer_count,
           profile->mlp_complete_gud_layer_count,
           profile->norm_tensor_count,
           profile->output_head_tensor_count,
           profile->moe_router_count + profile->moe_expert_count);
    if (profile->source_exists && profile->tensor_count > 0) {
        model_target_out_writef(model_target_out(), "layers_observed: %llu\n", profile->layer_count_observed);
    }
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next: %s\n", YVEX_TENSOR_COLLECTION_NEXT_ROW);
    model_target_out_writef(model_target_out(), "boundary: tensor collection inventory only; no role mapping/runtime/generation\n");
}

static void print_tensor_collection_table(
    const yvex_tensor_collection_profile *profile)
{
    model_target_out_writef(model_target_out(), "TENSOR COLLECTION INVENTORY\n\n");
    model_target_out_writef(model_target_out(), "%-6s  %-10s  %-19s  %5s  %9s  %7s  %4s  %4s  %3s  %6s  %s\n",
           "FAMILY", "TARGET", "STATUS", "EMBED", "ATTN_QKVO",
           "MLP_GUD", "NORM", "HEAD", "MOE", "LAYERS", "NEXT");
    model_target_out_writef(model_target_out(), "%-6s  %-10s  %-19s  %5llu  %9llu  %7llu  %4llu  %4llu  %3llu  %6llu  %s\n",
           profile->spec->family_key,
           profile->record->target_id,
           profile->status,
           profile->embedding_tensor_count,
           profile->attention_complete_qkvo_layer_count,
           profile->mlp_complete_gud_layer_count,
           profile->norm_tensor_count,
           profile->output_head_tensor_count,
           profile->moe_router_count + profile->moe_expert_count,
           profile->layer_count_observed,
           YVEX_TENSOR_COLLECTION_NEXT_ROW);
}

static void print_tensor_collection_audit(
    const yvex_tensor_collection_profile *profile)
{
    model_target_out_writef(model_target_out(), "tensor_collection_status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "tensor_collection_family: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "tensor_collection_target_id: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "tensor_collection_stage: header-collection-inventory\n");
    model_target_out_writef(model_target_out(), "tensor_collection_evidence_basis: header-metadata-only\n");
    model_target_out_writef(model_target_out(), "tensor_collection_source_status: %s\n",
           profile->source_exists ? "present" : "missing");
    model_target_out_writef(model_target_out(), "tensor_collection_source_path: %s\n", profile->source_path);
    model_target_out_writef(model_target_out(), "tensor_collection_manifest_status: not-checked\n");
    model_target_out_writef(model_target_out(), "tensor_collection_config_status: %s\n",
           profile->config_present ? "present" : "missing");
    model_target_out_writef(model_target_out(), "tensor_collection_tokenizer_status: %s\n",
           profile->tokenizer_present ? "present" : "missing");
    model_target_out_writef(model_target_out(), "tensor_collection_tensor_count: %llu\n", profile->tensor_count);
    model_target_out_writef(model_target_out(), "tensor_collection_layer_count_observed: %llu\n",
           profile->layer_count_observed);
    model_target_out_writef(model_target_out(), "tensor_collection_embedding_status: %s\n",
           tensor_collection_present_status(profile->embedding_tensor_count));
    model_target_out_writef(model_target_out(), "tensor_collection_embedding_tensor_count: %llu\n",
           profile->embedding_tensor_count);
    model_target_out_writef(model_target_out(), "tensor_collection_attention_status: %s\n",
           tensor_collection_attention_status(profile));
    model_target_out_writef(model_target_out(), "tensor_collection_attention_q_count: %llu\n", profile->attention_q_count);
    model_target_out_writef(model_target_out(), "tensor_collection_attention_k_count: %llu\n", profile->attention_k_count);
    model_target_out_writef(model_target_out(), "tensor_collection_attention_v_count: %llu\n", profile->attention_v_count);
    model_target_out_writef(model_target_out(), "tensor_collection_attention_o_count: %llu\n", profile->attention_o_count);
    model_target_out_writef(model_target_out(), "tensor_collection_attention_complete_qkvo_layer_count: %llu\n",
           profile->attention_complete_qkvo_layer_count);
    model_target_out_writef(model_target_out(), "tensor_collection_mlp_status: %s\n",
           tensor_collection_mlp_status(profile));
    model_target_out_writef(model_target_out(), "tensor_collection_mlp_gate_count: %llu\n", profile->mlp_gate_count);
    model_target_out_writef(model_target_out(), "tensor_collection_mlp_up_count: %llu\n", profile->mlp_up_count);
    model_target_out_writef(model_target_out(), "tensor_collection_mlp_down_count: %llu\n", profile->mlp_down_count);
    model_target_out_writef(model_target_out(), "tensor_collection_mlp_complete_gud_layer_count: %llu\n",
           profile->mlp_complete_gud_layer_count);
    model_target_out_writef(model_target_out(), "tensor_collection_norm_status: %s\n",
           tensor_collection_present_status(profile->norm_tensor_count));
    model_target_out_writef(model_target_out(), "tensor_collection_norm_tensor_count: %llu\n",
           profile->norm_tensor_count);
    model_target_out_writef(model_target_out(), "tensor_collection_output_head_status: %s\n",
           tensor_collection_present_status(profile->output_head_tensor_count));
    model_target_out_writef(model_target_out(), "tensor_collection_output_head_tensor_count: %llu\n",
           profile->output_head_tensor_count);
    model_target_out_writef(model_target_out(), "tensor_collection_moe_status: %s\n",
           (profile->moe_router_count + profile->moe_expert_count) > 0
               ? "observed"
               : "not-observed");
    model_target_out_writef(model_target_out(), "tensor_collection_moe_router_count: %llu\n", profile->moe_router_count);
    model_target_out_writef(model_target_out(), "tensor_collection_moe_expert_count: %llu\n", profile->moe_expert_count);
    model_target_out_writef(model_target_out(), "tensor_collection_tokenizer_collection_status: %s\n",
           profile->tokenizer_present ? "sidecar-observed" : "missing");
    model_target_out_writef(model_target_out(), "tensor_collection_kv_runtime_state_status: runtime-state-required-not-implemented\n");
    model_target_out_writef(model_target_out(), "tensor_collection_validation_status: lexical-and-header-only\n");
    model_target_out_writef(model_target_out(), "tensor_collection_role_mapping_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "tensor_collection_runtime_descriptor_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "tensor_collection_graph_consumer_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next_required_rows: %s\n", YVEX_TENSOR_COLLECTION_NEXT_ROW);
    model_target_out_writef(model_target_out(), "boundary: tensor collection inventory only; no role mapping/runtime/generation\n");
}

static void print_tensor_collection_audit_hint(
    const yvex_model_target_record *record)
{
    const yvex_model_class_profile_spec *spec;

    if (!record) {
        return;
    }
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) return;
    model_target_out_writef(model_target_out(), "tensor_collection_status: command-visible\n");
    model_target_out_writef(model_target_out(), "tensor_collection_family: %s\n", spec->family_key);
    model_target_out_writef(model_target_out(), "tensor_collection_target_id: %s\n", spec->target_id);
    model_target_out_writef(model_target_out(), "tensor_collection_stage: header-collection-inventory\n");
    model_target_out_writef(model_target_out(), "tensor_collection_evidence_basis: header-metadata-only\n");
    model_target_out_writef(model_target_out(), "tensor_collection_validation_status: lexical-and-header-only\n");
    model_target_out_writef(model_target_out(), "tensor_collection_role_mapping_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "tensor_collection_runtime_descriptor_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "tensor_collection_graph_consumer_status: not-implemented\n");
}

static void tensor_naming_copy(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) return;
    if (!src) src = "";
    snprintf(dst, cap, "%s", src);
}

static int tensor_naming_ends_with(const char *s, const char *suffix)
{
    size_t s_len;
    size_t suffix_len;

    if (!s || !suffix) return 0;
    s_len = strlen(s);
    suffix_len = strlen(suffix);
    if (suffix_len > s_len) return 0;
    return strcmp(s + s_len - suffix_len, suffix) == 0;
}

static void tensor_naming_shape_string(const yvex_native_weight_info *info,
                                       char *out,
                                       size_t cap)
{
    unsigned int i;
    size_t used = 0;

    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!info || info->rank == 0) {
        snprintf(out, cap, "%s", "scalar");
        return;
    }
    for (i = 0; i < info->rank && i < YVEX_NATIVE_WEIGHT_MAX_DIMS; ++i) {
        int n = snprintf(out + used, cap - used, "%s%llu",
                         i == 0 ? "" : "x",
                         info->dims[i]);
        if (n < 0) {
            out[0] = '\0';
            return;
        }
        if ((size_t)n >= cap - used) {
            out[cap - 1] = '\0';
            return;
        }
        used += (size_t)n;
    }
}

static int tensor_naming_expert_index(const char *name,
                                      unsigned long *expert_index)
{
    const char *patterns[] = {
        ".experts.",
        ".expert.",
    };
    unsigned long p;

    if (!name || !expert_index) return 0;
    for (p = 0; p < sizeof(patterns) / sizeof(patterns[0]); ++p) {
        const char *pos = strstr(name, patterns[p]);
        const char *digits;
        char *end = NULL;
        unsigned long value;

        if (!pos) continue;
        digits = pos + strlen(patterns[p]);
        if (!isdigit((unsigned char)digits[0])) continue;
        errno = 0;
        value = strtoul(digits, &end, 10);
        if (errno != 0 || end == digits || !end || *end != '.') continue;
        *expert_index = value;
        return 1;
    }
    return 0;
}

static void tensor_naming_note_layer(yvex_tensor_naming_profile *profile,
                                     unsigned long layer_index)
{
    yvex_tensor_collection_layer_flags *flags;

    if (!profile || layer_index >= YVEX_TENSOR_COLLECTION_LAYER_CAP) return;
    flags = &profile->layers[layer_index];
    if (!flags->seen) {
        flags->seen = 1;
        profile->layer_count_observed++;
    }
}

static void tensor_naming_set_mapped(yvex_tensor_naming_entry *entry,
                                     const char *collection,
                                     const char *canonical)
{
    tensor_naming_copy(entry->collection, sizeof(entry->collection), collection);
    tensor_naming_copy(entry->canonical_role, sizeof(entry->canonical_role),
                       canonical);
    entry->mapping_status = "mapped-candidate";
}

static void dense_tensor_naming_map_native(yvex_tensor_naming_entry *entry,
                                           const char *name)
{
    unsigned long layer_index = 0;
    unsigned long expert_index = 0;
    int has_layer;
    char canonical[YVEX_TENSOR_NAMING_TEXT_CAP];

    if (!entry || !name) return;
    has_layer = tensor_collection_layer_index(name, &layer_index);
    if (has_layer) {
        snprintf(entry->layer_index, sizeof(entry->layer_index), "%lu",
                 layer_index);
    }

    if (!has_layer &&
        (strcmp(name, "model.embed_tokens.weight") == 0 ||
         strcmp(name, "model.language_model.embed_tokens.weight") == 0 ||
         strcmp(name, "embed_tokens.weight") == 0 ||
         tensor_naming_ends_with(name, ".embed_tokens.weight") ||
         model_class_name_contains_ci(name, "token_embd"))) {
        tensor_naming_set_mapped(entry, "embedding",
                                 "model.embedding.token.weight");
        return;
    }
    if (!has_layer &&
        (strcmp(name, "model.norm.weight") == 0 ||
         strcmp(name, "model.language_model.norm.weight") == 0 ||
         strcmp(name, "model.language_model.final_layernorm.weight") == 0 ||
         strcmp(name, "final_layernorm.weight") == 0 ||
         tensor_naming_ends_with(name, ".final_layernorm.weight") ||
         strcmp(name, "norm.weight") == 0)) {
        tensor_naming_set_mapped(entry, "norm", "model.final_norm.weight");
        return;
    }
    if (!has_layer &&
        (strcmp(name, "lm_head.weight") == 0 ||
         strcmp(name, "model.language_model.lm_head.weight") == 0 ||
         tensor_naming_ends_with(name, ".lm_head.weight") ||
         strcmp(name, "output.weight") == 0 ||
         tensor_naming_ends_with(name, ".output.weight"))) {
        tensor_naming_set_mapped(entry, "output-head",
                                 "model.output_head.weight");
        return;
    }
    if (!has_layer) return;

    if (strstr(name, ".mlp.shared_expert_gate.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.moe.shared_expert_gate.weight",
                 layer_index);
        tensor_naming_set_mapped(entry, "moe", canonical);
        return;
    }
    if (strstr(name, ".mlp.shared_expert.gate_proj.weight") ||
        strstr(name, ".mlp.shared_expert.up_proj.weight") ||
        strstr(name, ".mlp.shared_expert.down_proj.weight")) {
        const char *proj = strstr(name, ".mlp.shared_expert.gate_proj.weight")
                               ? "gate_proj"
                               : (strstr(name, ".mlp.shared_expert.up_proj.weight")
                                      ? "up_proj"
                                      : "down_proj");
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.moe.shared_expert.%s.weight",
                 layer_index, proj);
        tensor_naming_set_mapped(entry, "moe", canonical);
        return;
    }
    if (strstr(name, ".mlp.gate.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.moe.router.weight", layer_index);
        tensor_naming_set_mapped(entry, "moe", canonical);
        return;
    }
    if (strstr(name, ".mlp.experts.gate_up_proj") ||
        strstr(name, ".mlp.experts.down_proj")) {
        const char *proj = strstr(name, ".mlp.experts.gate_up_proj")
                               ? "gate_up_proj"
                               : "down_proj";
        snprintf(entry->expert_index, sizeof(entry->expert_index), "all");
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.moe.experts.all.%s.weight",
                 layer_index, proj);
        tensor_naming_set_mapped(entry, "moe", canonical);
        return;
    }
    if (strstr(name, ".linear_attn.")) {
        const char *suffix = strstr(name, ".linear_attn.");
        suffix += strlen(".linear_attn.");
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.qwen_linear_attn.%s",
                 layer_index, suffix);
        tensor_naming_set_mapped(entry, "qwen-linear-attn", canonical);
        return;
    }
    if ((strstr(name, ".experts.") || strstr(name, ".expert.")) &&
        tensor_naming_expert_index(name, &expert_index)) {
        const char *proj = NULL;

        snprintf(entry->expert_index, sizeof(entry->expert_index), "%lu",
                 expert_index);
        if (strstr(name, ".gate_proj.weight")) proj = "gate_proj";
        else if (strstr(name, ".up_proj.weight")) proj = "up_proj";
        else if (strstr(name, ".down_proj.weight")) proj = "down_proj";
        if (proj) {
            snprintf(canonical, sizeof(canonical),
                     "model.layers.%lu.moe.experts.%lu.%s.weight",
                     layer_index, expert_index, proj);
            tensor_naming_set_mapped(entry, "moe", canonical);
            return;
        }
    }
    if (strstr(name, "router")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.moe.router.weight", layer_index);
        tensor_naming_set_mapped(entry, "moe", canonical);
        return;
    }
    if (strstr(name, ".self_attn.q_proj.weight") ||
        strstr(name, ".attention.q_proj.weight") ||
        strstr(name, ".attn.q_proj.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.attention.q_proj.weight", layer_index);
        tensor_naming_set_mapped(entry, "attention", canonical);
        return;
    }
    if (strstr(name, ".self_attn.k_proj.weight") ||
        strstr(name, ".attention.k_proj.weight") ||
        strstr(name, ".attn.k_proj.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.attention.k_proj.weight", layer_index);
        tensor_naming_set_mapped(entry, "attention", canonical);
        return;
    }
    if (strstr(name, ".self_attn.v_proj.weight") ||
        strstr(name, ".attention.v_proj.weight") ||
        strstr(name, ".attn.v_proj.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.attention.v_proj.weight", layer_index);
        tensor_naming_set_mapped(entry, "attention", canonical);
        return;
    }
    if (strstr(name, ".self_attn.o_proj.weight") ||
        strstr(name, ".attention.o_proj.weight") ||
        strstr(name, ".attn.o_proj.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.attention.o_proj.weight", layer_index);
        tensor_naming_set_mapped(entry, "attention", canonical);
        return;
    }
    if (strstr(name, ".self_attn.q_norm.weight") ||
        strstr(name, ".attention.q_norm.weight") ||
        strstr(name, ".attn.q_norm.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.attention.q_norm.weight", layer_index);
        tensor_naming_set_mapped(entry, "attention", canonical);
        return;
    }
    if (strstr(name, ".self_attn.k_norm.weight") ||
        strstr(name, ".attention.k_norm.weight") ||
        strstr(name, ".attn.k_norm.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.attention.k_norm.weight", layer_index);
        tensor_naming_set_mapped(entry, "attention", canonical);
        return;
    }
    if (strstr(name, ".input_layernorm.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.attention.norm.weight", layer_index);
        tensor_naming_set_mapped(entry, "norm", canonical);
        return;
    }
    if (strstr(name, ".layer_scalar")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.layer_scalar", layer_index);
        tensor_naming_set_mapped(entry, "norm", canonical);
        return;
    }
    if (strstr(name, ".post_attention_layernorm.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.mlp.norm.weight", layer_index);
        tensor_naming_set_mapped(entry, "norm", canonical);
        return;
    }
    if (strstr(name, ".pre_feedforward_layernorm.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.mlp.norm.weight", layer_index);
        tensor_naming_set_mapped(entry, "norm", canonical);
        return;
    }
    if (strstr(name, ".post_feedforward_layernorm.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.mlp.norm.weight", layer_index);
        tensor_naming_set_mapped(entry, "norm", canonical);
        return;
    }
    if (strstr(name, ".mlp.gate_proj.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.mlp.gate_proj.weight", layer_index);
        tensor_naming_set_mapped(entry, "mlp", canonical);
        return;
    }
    if (strstr(name, ".mlp.up_proj.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.mlp.up_proj.weight", layer_index);
        tensor_naming_set_mapped(entry, "mlp", canonical);
        return;
    }
    if (strstr(name, ".mlp.down_proj.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.mlp.down_proj.weight", layer_index);
        tensor_naming_set_mapped(entry, "mlp", canonical);
        return;
    }
}

static void tensor_naming_count_entry(yvex_tensor_naming_profile *profile,
                                      const yvex_tensor_naming_entry *entry)
{
    if (!profile || !entry) return;
    if (strcmp(entry->layer_index, "none") != 0) {
        unsigned long layer_index = strtoul(entry->layer_index, NULL, 10);
        tensor_naming_note_layer(profile, layer_index);
    }
    if (strcmp(entry->mapping_status, "mapped-candidate") != 0) {
        if (strcmp(entry->mapping_status, "unmapped-unknown") == 0) {
            profile->unmapped_unknown_count++;
        } else if (strcmp(entry->mapping_status, "ambiguous") == 0) {
            profile->ambiguous_count++;
        }
        return;
    }

    profile->mapped_total_count++;
    if (strcmp(entry->collection, "embedding") == 0) {
        profile->embedding_count++;
    } else if (strcmp(entry->collection, "attention") == 0) {
        profile->attention_count++;
        if (strstr(entry->canonical_role, ".q_proj.weight")) profile->attention_q_count++;
        else if (strstr(entry->canonical_role, ".k_proj.weight")) profile->attention_k_count++;
        else if (strstr(entry->canonical_role, ".v_proj.weight")) profile->attention_v_count++;
        else if (strstr(entry->canonical_role, ".o_proj.weight")) profile->attention_o_count++;
    } else if (strcmp(entry->collection, "qwen-linear-attn") == 0) {
        profile->attention_count++;
        profile->qwen_linear_attn_count++;
    } else if (strcmp(entry->collection, "mlp") == 0) {
        profile->mlp_count++;
        if (strstr(entry->canonical_role, ".gate_proj.weight")) profile->mlp_gate_count++;
        else if (strstr(entry->canonical_role, ".up_proj.weight")) profile->mlp_up_count++;
        else if (strstr(entry->canonical_role, ".down_proj.weight")) profile->mlp_down_count++;
    } else if (strcmp(entry->collection, "norm") == 0) {
        profile->norm_count++;
    } else if (strcmp(entry->collection, "output-head") == 0) {
        profile->output_head_count++;
    } else if (strcmp(entry->collection, "moe") == 0) {
        profile->mlp_count++;
        if (strstr(entry->canonical_role, ".router.weight")) {
            profile->moe_router_count++;
            profile->mlp_gate_count++;
        } else if (strstr(entry->canonical_role, ".shared_expert")) {
            profile->moe_shared_count++;
            if (strstr(entry->canonical_role, ".gate_proj.weight") ||
                strstr(entry->canonical_role, "shared_expert_gate.weight")) {
                profile->mlp_gate_count++;
            } else if (strstr(entry->canonical_role, ".up_proj.weight")) {
                profile->mlp_up_count++;
            } else if (strstr(entry->canonical_role, ".down_proj.weight")) {
                profile->mlp_down_count++;
            }
        } else {
            profile->moe_expert_count++;
            if (strstr(entry->canonical_role, ".gate_up_proj.weight")) {
                profile->mlp_gate_count++;
                profile->mlp_up_count++;
            } else if (strstr(entry->canonical_role, ".gate_proj.weight")) {
                profile->mlp_gate_count++;
            } else if (strstr(entry->canonical_role, ".up_proj.weight")) {
                profile->mlp_up_count++;
            } else if (strstr(entry->canonical_role, ".down_proj.weight")) {
                profile->mlp_down_count++;
            }
        }
    }
}

static int tensor_naming_requires_qwen_extra_roles(
    const yvex_tensor_naming_profile *profile)
{
    return profile &&
           profile->spec &&
           strcmp(profile->spec->family_key, "qwen") == 0 &&
           ((profile->record && strstr(profile->record->target_id, "a3b")) ||
            profile->qwen_linear_attn_count > 0 ||
            profile->moe_router_count > 0 ||
            profile->moe_expert_count > 0 ||
            profile->moe_shared_count > 0);
}

static int tensor_naming_required_groups_present(
    const yvex_tensor_naming_profile *profile)
{
    if (!profile ||
        profile->embedding_count == 0 ||
        profile->attention_q_count == 0 ||
        profile->attention_k_count == 0 ||
        profile->attention_v_count == 0 ||
        profile->attention_o_count == 0 ||
        profile->mlp_gate_count == 0 ||
        profile->mlp_up_count == 0 ||
        profile->mlp_down_count == 0 ||
        profile->norm_count == 0) {
        return 0;
    }
    if (tensor_naming_requires_qwen_extra_roles(profile)) {
        return profile->qwen_linear_attn_count > 0 &&
               profile->moe_router_count > 0 &&
               profile->moe_expert_count > 0 &&
               profile->moe_shared_count > 0;
    }
    return 1;
}

static const char *tensor_naming_header_blocker(
    const yvex_model_class_profile_spec *spec)
{
    if (spec && strcmp(spec->family_key, "gemma") == 0) {
        return "missing-gemma-header-metadata";
    }
    return "missing-qwen-header-metadata";
}

static const char *tensor_naming_incomplete_blocker(
    const yvex_model_class_profile_spec *spec)
{
    if (spec && strcmp(spec->family_key, "gemma") == 0) {
        return "incomplete-dense-tensor-naming-map";
    }
    return "incomplete-qwen-tensor-naming-map";
}

static const char *tensor_naming_runtime_role_blocker(
    const yvex_model_class_profile_spec *spec)
{
    if (spec && strcmp(spec->family_key, "gemma") == 0) {
        return "missing-dense-runtime-role-validation";
    }
    return "missing-qwen-runtime-role-validation";
}

static int build_tensor_naming_profile(
    const yvex_model_target_record *record,
    const yvex_model_class_profile_spec *spec,
    const char *models_root_override,
    const char *source_override,
    yvex_tensor_naming_profile *profile)
{
    yvex_model_class_profile source_profile;
    yvex_native_weight_table *table = NULL;
    yvex_native_weight_options options;
    yvex_error err;
    unsigned long long i;
    int rc;

    if (!record || !profile) return 2;
    if (!spec) return 2;

    memset(profile, 0, sizeof(*profile));
    profile->record = record;
    profile->spec = spec;
    profile->status = "source-missing";
    profile->source_metadata_status = "missing";
    profile->top_blocker = spec->missing_source_blocker;

    rc = model_class_build_profile(record, spec, models_root_override,
                                   source_override, &source_profile);
    if (rc != 0) return rc;
    snprintf(profile->source_path, sizeof(profile->source_path), "%s",
             source_profile.source_path);
    snprintf(profile->source_path_source, sizeof(profile->source_path_source),
             "%s", source_profile.source_path_source);
    profile->source_exists = source_profile.source_exists;
    profile->config_present = source_profile.config_present;
    profile->tokenizer_present = source_profile.tokenizer_present;
    if (!profile->source_exists) return 0;

    memset(&options, 0, sizeof(options));
    options.source_dir = profile->source_path;
    options.recursive = 0;
    options.include_metadata = 0;
    yvex_error_clear(&err);
    rc = yvex_native_weight_table_open(&table, &options, &err);
    if (rc != YVEX_OK) {
        profile->status = "metadata-missing";
        profile->source_metadata_status = "header-error";
        profile->top_blocker = tensor_naming_header_blocker(spec);
        return rc == YVEX_ERR_NOMEM ? 3 : 0;
    }

    profile->tensor_count = yvex_native_weight_table_count(table);
    for (i = 0; i < profile->tensor_count; ++i) {
        const yvex_native_weight_info *info =
            yvex_native_weight_table_at(table, i);
        yvex_tensor_naming_entry *entry;

        if (!info || profile->entry_count >= YVEX_TENSOR_NAMING_ENTRY_CAP) {
            continue;
        }
        entry = &profile->entries[profile->entry_count++];
        tensor_naming_copy(entry->native_name, sizeof(entry->native_name),
                           info->name);
        tensor_naming_copy(entry->canonical_role, sizeof(entry->canonical_role),
                           "none");
        tensor_naming_copy(entry->family, sizeof(entry->family), spec->family_key);
        tensor_naming_copy(entry->target_id, sizeof(entry->target_id),
                           record->target_id);
        tensor_naming_copy(entry->collection, sizeof(entry->collection), "unknown");
        tensor_naming_copy(entry->layer_index, sizeof(entry->layer_index), "none");
        tensor_naming_copy(entry->expert_index, sizeof(entry->expert_index), "none");
        tensor_naming_copy(entry->dtype, sizeof(entry->dtype),
                           info->dtype_name ? info->dtype_name : "unknown");
        snprintf(entry->rank, sizeof(entry->rank), "%u", info->rank);
        tensor_naming_shape_string(info, entry->shape, sizeof(entry->shape));
        tensor_naming_copy(entry->source_file, sizeof(entry->source_file),
                           info->shard_path);
        entry->mapping_status = "unmapped-unknown";
        dense_tensor_naming_map_native(entry, info->name);
        tensor_naming_count_entry(profile, entry);
    }
    yvex_native_weight_table_close(table);

    profile->source_metadata_status =
        profile->tensor_count > 0 ? "header-only" : "no-safetensors";
    if (profile->tensor_count == 0) {
        profile->status = "metadata-missing";
        profile->top_blocker = tensor_naming_header_blocker(spec);
    } else if (!tensor_naming_required_groups_present(profile)) {
        profile->status = "naming-map-incomplete";
        profile->top_blocker = tensor_naming_incomplete_blocker(spec);
    } else if (profile->unmapped_unknown_count > 0 || profile->ambiguous_count > 0) {
        profile->status = "naming-map-candidate";
        profile->top_blocker = tensor_naming_runtime_role_blocker(spec);
    } else {
        profile->status = "naming-map-profiled";
        profile->top_blocker = tensor_naming_runtime_role_blocker(spec);
    }
    return 0;
}

static int tensor_naming_is_dense_family(
    const yvex_tensor_naming_profile *profile)
{
    return profile &&
           profile->spec &&
           strcmp(profile->spec->family_key, "gemma") == 0;
}

static unsigned long long tensor_naming_moe_count(
    const yvex_tensor_naming_profile *profile)
{
    if (!profile) return 0;
    return profile->moe_router_count + profile->moe_expert_count +
           profile->moe_shared_count;
}

static const char *compact_status_bracket(const char *status)
{
    if (!status || !status[0]) return "unknown";
    if (strstr(status, "pass") ||
        strstr(status, "profiled") ||
        strstr(status, "reported")) {
        return "reported";
    }
    if (strstr(status, "blocked") ||
        strstr(status, "incomplete") ||
        strstr(status, "missing") ||
        strstr(status, "ambiguous")) {
        return "blocked";
    }
    if (strstr(status, "unsupported")) return "unsupported";
    return status;
}

static void print_tensor_naming_normal(
    const yvex_tensor_naming_profile *profile)
{
    model_target_out_writef(model_target_out(), "tensor-map: %s [%s]\n",
           profile->record->target_id,
           compact_status_bracket(profile->status));
    model_target_out_writef(model_target_out(), "family: %s  stage: header-naming-map  evidence: header-only\n",
           profile->spec->family_key);
    model_target_out_writef(model_target_out(), "roles: total=%llu embedding=%llu attention=%llu mlp=%llu norm=%llu head=%llu moe=%llu unknown=%llu\n",
           profile->mapped_total_count,
           profile->embedding_count,
           profile->attention_count,
           profile->mlp_count,
           profile->norm_count,
           profile->output_head_count,
           tensor_naming_moe_count(profile),
           profile->unmapped_unknown_count);
    if (profile->source_exists && profile->tensor_count > 0) {
        model_target_out_writef(model_target_out(), "layers: %llu\n", profile->layer_count_observed);
    }
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next: %s\n", YVEX_TENSOR_NAMING_NEXT_ROW);
    model_target_out_writef(model_target_out(), "boundary: report-only; use --audit for tensor entries\n");
}

static void print_tensor_naming_table(
    const yvex_tensor_naming_profile *profile)
{
    model_target_out_writef(model_target_out(), "TENSOR NAMING MAP\n\n");
    model_target_out_writef(model_target_out(), "%-6s  %-20s  %-24s  %7s  %6s  %6s  %6s  %6s  %6s  %6s  %8s  %7s  %s\n",
           "FAMILY", "TARGET", "STATUS", "TOTAL", "EMBED", "ATTN",
           "MLP", "NORM", "HEAD", "MOE", "UNKNOWN", "LAYERS", "NEXT");
    model_target_out_writef(model_target_out(), "%-6s  %-20s  %-24s  %7llu  %6llu  %6llu  %6llu  %6llu  %6llu  %6llu  %8llu  %7llu  %s\n",
           profile->spec->family_key,
           profile->record->target_id,
           profile->status,
           profile->mapped_total_count,
           profile->embedding_count,
           profile->attention_count,
           profile->mlp_count,
           profile->norm_count,
           profile->output_head_count,
           tensor_naming_moe_count(profile),
           profile->unmapped_unknown_count,
           profile->layer_count_observed,
           YVEX_TENSOR_NAMING_NEXT_ROW);
}

static void print_tensor_naming_audit(
    const yvex_tensor_naming_profile *profile)
{
    unsigned long long i;

    model_target_out_writef(model_target_out(), "tensor_map_status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "tensor_map_family: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "tensor_map_target_id: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "tensor_map_stage: header-naming-map\n");
    model_target_out_writef(model_target_out(), "tensor_map_evidence_basis: header-metadata-only\n");
    model_target_out_writef(model_target_out(), "tensor_map_source_status: %s\n",
           profile->source_exists ? "present" : "missing");
    model_target_out_writef(model_target_out(), "tensor_map_source_path: %s\n", profile->source_path);
    model_target_out_writef(model_target_out(), "tensor_map_manifest_status: not-checked\n");
    model_target_out_writef(model_target_out(), "tensor_map_config_status: %s\n",
           profile->config_present ? "present" : "missing");
    model_target_out_writef(model_target_out(), "tensor_map_tokenizer_status: %s\n",
           profile->tokenizer_present ? "present" : "missing");
    model_target_out_writef(model_target_out(), "tensor_map_tensor_count: %llu\n", profile->tensor_count);
    model_target_out_writef(model_target_out(), "tensor_map_mapped_total_count: %llu\n", profile->mapped_total_count);
    model_target_out_writef(model_target_out(), "tensor_map_unmapped_unknown_count: %llu\n",
           profile->unmapped_unknown_count);
    model_target_out_writef(model_target_out(), "tensor_map_ambiguous_count: %llu\n", profile->ambiguous_count);
    model_target_out_writef(model_target_out(), "tensor_map_layer_count_observed: %llu\n",
           profile->layer_count_observed);
    model_target_out_writef(model_target_out(), "tensor_map_embedding_count: %llu\n", profile->embedding_count);
    model_target_out_writef(model_target_out(), "tensor_map_attention_count: %llu\n", profile->attention_count);
    model_target_out_writef(model_target_out(), "tensor_map_attention_q_count: %llu\n", profile->attention_q_count);
    model_target_out_writef(model_target_out(), "tensor_map_attention_k_count: %llu\n", profile->attention_k_count);
    model_target_out_writef(model_target_out(), "tensor_map_attention_v_count: %llu\n", profile->attention_v_count);
    model_target_out_writef(model_target_out(), "tensor_map_attention_o_count: %llu\n", profile->attention_o_count);
    model_target_out_writef(model_target_out(), "tensor_map_mlp_count: %llu\n", profile->mlp_count);
    model_target_out_writef(model_target_out(), "tensor_map_mlp_gate_count: %llu\n", profile->mlp_gate_count);
    model_target_out_writef(model_target_out(), "tensor_map_mlp_up_count: %llu\n", profile->mlp_up_count);
    model_target_out_writef(model_target_out(), "tensor_map_mlp_down_count: %llu\n", profile->mlp_down_count);
    model_target_out_writef(model_target_out(), "tensor_map_norm_count: %llu\n", profile->norm_count);
    model_target_out_writef(model_target_out(), "tensor_map_output_head_count: %llu\n", profile->output_head_count);
    model_target_out_writef(model_target_out(), "tensor_map_qwen_linear_attn_count: %llu\n",
           profile->qwen_linear_attn_count);
    model_target_out_writef(model_target_out(), "tensor_map_moe_router_count: %llu\n", profile->moe_router_count);
    model_target_out_writef(model_target_out(), "tensor_map_moe_expert_count: %llu\n", profile->moe_expert_count);
    model_target_out_writef(model_target_out(), "tensor_map_moe_shared_count: %llu\n", profile->moe_shared_count);
    model_target_out_writef(model_target_out(), "tensor_map_required_role_coverage_status: %s\n",
           tensor_naming_required_groups_present(profile)
               ? "required-groups-present"
               : "required-groups-missing");
    model_target_out_writef(model_target_out(), "tensor_map_tokenizer_sidecar_status: %s\n",
           profile->tokenizer_present ? "sidecar-observed" : "missing");
    model_target_out_writef(model_target_out(), "tensor_map_config_sidecar_status: %s\n",
           profile->config_present ? "sidecar-observed" : "missing");
    model_target_out_writef(model_target_out(), "tensor_map_validation_status: lexical-and-header-only\n");
    model_target_out_writef(model_target_out(), "tensor_map_canonical_role_status: mapped-candidates\n");
    model_target_out_writef(model_target_out(), "tensor_map_runtime_role_coverage_status: report-only\n");
    model_target_out_writef(model_target_out(), "tensor_map_artifact_contract_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "tensor_map_runtime_descriptor_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "tensor_map_graph_consumer_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next_required_rows: %s\n", YVEX_TENSOR_NAMING_NEXT_ROW);
    for (i = 0; i < profile->entry_count; ++i) {
        const yvex_tensor_naming_entry *entry = &profile->entries[i];
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.native_name: %s\n", i, entry->native_name);
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.canonical_role: %s\n", i, entry->canonical_role);
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.family: %s\n", i, entry->family);
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.target_id: %s\n", i, entry->target_id);
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.collection: %s\n", i, entry->collection);
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.layer_index: %s\n", i, entry->layer_index);
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.expert_index: %s\n", i, entry->expert_index);
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.dtype: %s\n", i, entry->dtype);
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.rank: %s\n", i, entry->rank);
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.shape: %s\n", i,
               entry->shape[0] ? entry->shape : "unknown");
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.source_file: %s\n", i, entry->source_file);
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.mapping_status: %s\n",
               i, entry->mapping_status);
        model_target_out_writef(model_target_out(), "tensor_map.entry.%llu.mapping: %s -> %s\n",
               i, entry->native_name, entry->canonical_role);
    }
    model_target_out_writef(model_target_out(), "boundary: %stensor naming map only; no runtime descriptor/graph/runtime/generation\n",
           tensor_naming_is_dense_family(profile) ? "dense " : "");
}

static void print_tensor_map_audit_hint(const yvex_model_target_record *record)
{
    const yvex_model_class_profile_spec *spec;

    if (!record) return;
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) return;
    model_target_out_writef(model_target_out(), "tensor_map_status: not-run\n");
    model_target_out_writef(model_target_out(), "tensor_map_family: %s\n", spec->family_key);
    model_target_out_writef(model_target_out(), "tensor_map_target_id: %s\n", spec->target_id);
    model_target_out_writef(model_target_out(), "tensor_map_stage: header-naming-map\n");
    model_target_out_writef(model_target_out(), "tensor_map_evidence_basis: header-metadata-only\n");
    model_target_out_writef(model_target_out(), "tensor_map_next: %s\n", YVEX_TENSOR_NAMING_NEXT_ROW);
}

static void print_output_head_map_audit_hint(const yvex_model_target_record *record)
{
    const yvex_model_class_profile_spec *spec;

    if (!record) return;
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) return;
    model_target_out_writef(model_target_out(), "output_head_map_status: not-run\n");
    model_target_out_writef(model_target_out(), "output_head_map_family: %s\n", spec->family_key);
    model_target_out_writef(model_target_out(), "output_head_map_target_id: %s\n", spec->target_id);
    model_target_out_writef(model_target_out(), "output_head_map_stage: header-output-head-map\n");
    model_target_out_writef(model_target_out(), "output_head_map_evidence_basis: header-metadata-only\n");
    model_target_out_writef(model_target_out(), "output_head_map_next: %s\n", YVEX_OUTPUT_HEAD_MAP_NEXT_ROW);
}

static void output_head_map_entry_init(yvex_output_head_map_entry *entry)
{
    if (!entry) return;
    memset(entry, 0, sizeof(*entry));
    tensor_naming_copy(entry->native_name, sizeof(entry->native_name), "none");
    tensor_naming_copy(entry->canonical_role, sizeof(entry->canonical_role), "none");
    tensor_naming_copy(entry->dtype, sizeof(entry->dtype), "unknown");
    tensor_naming_copy(entry->rank, sizeof(entry->rank), "unknown");
    tensor_naming_copy(entry->shape, sizeof(entry->shape), "unknown");
    tensor_naming_copy(entry->vocab_dim_candidate,
                       sizeof(entry->vocab_dim_candidate), "unknown");
    tensor_naming_copy(entry->hidden_dim_candidate,
                       sizeof(entry->hidden_dim_candidate), "unknown");
    tensor_naming_copy(entry->source_file, sizeof(entry->source_file), "none");
    entry->mapping_status = "missing";
}

static void output_head_map_entry_from_info(
    yvex_output_head_map_entry *entry,
    const yvex_native_weight_info *info,
    const char *canonical_role,
    const char *mapping_status)
{
    if (!entry || !info) return;
    entry->present = 1;
    tensor_naming_copy(entry->native_name, sizeof(entry->native_name), info->name);
    tensor_naming_copy(entry->canonical_role, sizeof(entry->canonical_role),
                       canonical_role ? canonical_role : "none");
    tensor_naming_copy(entry->dtype, sizeof(entry->dtype),
                       info->dtype_name ? info->dtype_name : "unknown");
    snprintf(entry->rank, sizeof(entry->rank), "%u", info->rank);
    tensor_naming_shape_string(info, entry->shape, sizeof(entry->shape));
    tensor_naming_copy(entry->source_file, sizeof(entry->source_file),
                       info->shard_path ? info->shard_path : "none");
    if (info->rank >= 1) {
        snprintf(entry->vocab_dim_candidate,
                 sizeof(entry->vocab_dim_candidate), "%llu", info->dims[0]);
    }
    if (info->rank >= 2) {
        snprintf(entry->hidden_dim_candidate,
                 sizeof(entry->hidden_dim_candidate), "%llu", info->dims[1]);
    }
    entry->mapping_status = mapping_status ? mapping_status : "mapped-candidate";
}

static int output_head_map_name_is_output(const char *name)
{
    return name &&
           (strcmp(name, "lm_head.weight") == 0 ||
            strcmp(name, "model.lm_head.weight") == 0 ||
            strcmp(name, "model.language_model.lm_head.weight") == 0 ||
            tensor_naming_ends_with(name, ".lm_head.weight") ||
            strcmp(name, "output.weight") == 0 ||
            strcmp(name, "model.output.weight") == 0 ||
            strcmp(name, "output_head.weight") == 0 ||
            strcmp(name, "model.output_head.weight") == 0);
}

static int output_head_map_name_is_embedding(const char *name)
{
    return name &&
           (strcmp(name, "model.embed_tokens.weight") == 0 ||
            strcmp(name, "model.language_model.embed_tokens.weight") == 0 ||
            tensor_naming_ends_with(name, ".embed_tokens.weight") ||
            strcmp(name, "embed_tokens.weight") == 0 ||
            strcmp(name, "token_embd.weight") == 0 ||
            strcmp(name, "tok_embeddings.weight") == 0 ||
            strcmp(name, "embeddings.weight") == 0);
}

static int output_head_map_name_is_final_norm(const char *name)
{
    return name &&
           (strcmp(name, "model.norm.weight") == 0 ||
            strcmp(name, "model.language_model.norm.weight") == 0 ||
            strcmp(name, "model.language_model.final_layernorm.weight") == 0 ||
            tensor_naming_ends_with(name, ".final_layernorm.weight") ||
            strcmp(name, "norm.weight") == 0 ||
            strcmp(name, "final_norm.weight") == 0 ||
            strcmp(name, "model.final_norm.weight") == 0);
}

static void output_head_map_probe_tie_policy(
    yvex_output_head_map_profile *profile)
{
    char path[YVEX_PATH_CAP];
    char *json = NULL;
    int tied = 0;

    if (!profile || !profile->source_path[0]) return;
    if (!model_class_path_join(path, sizeof(path), profile->source_path,
                               "config.json")) {
        return;
    }
    if (!model_target_read_small_json(path, &json)) {
        profile->config_tie_word_embeddings_status =
            profile->config_present ? "unreadable" : "missing";
        return;
    }
    if (model_target_json_bool_field(json, "tie_word_embeddings", &tied)) {
        profile->config_tie_word_embeddings_status = tied ? "true" : "false";
        if (tied) {
            profile->tie_policy_status = "tied-output-head-candidate";
        } else {
            profile->tie_policy_status = "not-proven";
        }
    } else {
        profile->config_tie_word_embeddings_status = "missing";
        profile->tie_policy_status = "not-proven";
    }
    free(json);
}

static int output_head_map_same_shape(const yvex_output_head_map_entry *a,
                                      const yvex_output_head_map_entry *b)
{
    return a && b && a->present && b->present &&
           strcmp(a->rank, b->rank) == 0 &&
           strcmp(a->shape, b->shape) == 0;
}

static int output_head_map_transposed_shape(
    const yvex_output_head_map_entry *a,
    const yvex_output_head_map_entry *b)
{
    unsigned long long a0;
    unsigned long long a1;
    unsigned long long b0;
    unsigned long long b1;
    char extra;

    if (!a || !b || !a->present || !b->present) return 0;
    if (strcmp(a->rank, "2") != 0 || strcmp(b->rank, "2") != 0) return 0;
    if (sscanf(a->shape, "%llux%llu%c", &a0, &a1, &extra) != 2) return 0;
    if (sscanf(b->shape, "%llux%llu%c", &b0, &b1, &extra) != 2) return 0;
    return a0 == b1 && a1 == b0;
}

static const char *output_head_map_shape_relation(
    const yvex_output_head_map_profile *profile)
{
    if (!profile || !profile->output_head.present || !profile->embedding.present) {
        return "unknown";
    }
    if (output_head_map_same_shape(&profile->output_head, &profile->embedding)) {
        return "compatible-same-shape";
    }
    if (output_head_map_transposed_shape(&profile->output_head,
                                         &profile->embedding)) {
        return "compatible-transposed";
    }
    return "mismatch";
}

static int build_output_head_map_profile(
    const yvex_model_target_record *record,
    const yvex_model_class_profile_spec *spec,
    const char *models_root_override,
    const char *source_override,
    yvex_output_head_map_profile *profile)
{
    yvex_model_class_profile source_profile;
    yvex_native_weight_table *table = NULL;
    yvex_native_weight_options options;
    yvex_error err;
    unsigned long long i;
    int rc;

    if (!record || !profile) return 2;
    if (!spec) return 2;

    memset(profile, 0, sizeof(*profile));
    profile->record = record;
    profile->spec = spec;
    profile->status = "source-missing";
    profile->source_metadata_status = "missing";
    profile->top_blocker = spec->missing_source_blocker;
    profile->tie_policy_status = "unknown";
    profile->config_tie_word_embeddings_status = "unknown";
    profile->shape_relation_status = "unknown";
    profile->output_head_missing_status = "missing";
    output_head_map_entry_init(&profile->output_head);
    output_head_map_entry_init(&profile->embedding);
    output_head_map_entry_init(&profile->final_norm);

    rc = model_class_build_profile(record, spec, models_root_override,
                                   source_override, &source_profile);
    if (rc != 0) return rc;
    snprintf(profile->source_path, sizeof(profile->source_path), "%s",
             source_profile.source_path);
    snprintf(profile->source_path_source, sizeof(profile->source_path_source),
             "%s", source_profile.source_path_source);
    profile->source_exists = source_profile.source_exists;
    profile->config_present = source_profile.config_present;
    profile->tokenizer_present = source_profile.tokenizer_present;
    if (!profile->source_exists) return 0;

    memset(&options, 0, sizeof(options));
    options.source_dir = profile->source_path;
    options.recursive = 0;
    options.include_metadata = 0;
    yvex_error_clear(&err);
    rc = yvex_native_weight_table_open(&table, &options, &err);
    if (rc != YVEX_OK) {
        profile->status = "metadata-missing";
        profile->source_metadata_status = "header-error";
        profile->top_blocker = tensor_naming_header_blocker(spec);
        return rc == YVEX_ERR_NOMEM ? 3 : 0;
    }

    profile->tensor_count = yvex_native_weight_table_count(table);
    for (i = 0; i < profile->tensor_count; ++i) {
        const yvex_native_weight_info *info =
            yvex_native_weight_table_at(table, i);

        if (!info || !info->name) continue;
        if (output_head_map_name_is_output(info->name)) {
            profile->output_head_candidate_count++;
            if (!profile->output_head.present) {
                output_head_map_entry_from_info(
                    &profile->output_head,
                    info,
                    "model.output_head.weight",
                    "mapped-candidate");
            }
        } else if (!profile->embedding.present &&
                   output_head_map_name_is_embedding(info->name)) {
            output_head_map_entry_from_info(
                &profile->embedding,
                info,
                "model.embedding.token.weight",
                "mapped-candidate");
        } else if (!profile->final_norm.present &&
                   output_head_map_name_is_final_norm(info->name)) {
            output_head_map_entry_from_info(
                &profile->final_norm,
                info,
                "model.final_norm.weight",
                "mapped-candidate");
        }
    }
    yvex_native_weight_table_close(table);

    profile->source_metadata_status =
        profile->tensor_count > 0 ? "header-only" : "no-safetensors";
    output_head_map_probe_tie_policy(profile);
    if (profile->tensor_count == 0) {
        profile->status = "metadata-missing";
        profile->top_blocker = tensor_naming_header_blocker(spec);
    } else if (profile->output_head_candidate_count == 0) {
        if (profile->embedding.present &&
            strcmp(profile->tie_policy_status,
                   "tied-output-head-candidate") == 0) {
            profile->output_head = profile->embedding;
            tensor_naming_copy(profile->output_head.canonical_role,
                               sizeof(profile->output_head.canonical_role),
                               "model.output_head.tied_embedding");
            profile->output_head.mapping_status =
                "tied-to-token-embedding-candidate";
            profile->output_head_candidate_count = 1;
            profile->status = "tied-output-head-report-only";
            profile->top_blocker = "missing-output-head-runtime-consumer";
            profile->output_head_missing_status = "present";
            profile->shape_relation_status = "tied-to-embedding";
        } else {
            profile->status = "output-head-missing";
            profile->top_blocker = "missing-output-head-tensor";
            profile->output_head_missing_status = "missing";
            if (strcmp(profile->tie_policy_status, "unknown") == 0) {
                profile->tie_policy_status = "not-proven";
            }
        }
    } else if (profile->output_head_candidate_count > 1) {
        profile->status = "output-head-ambiguous";
        profile->top_blocker = "ambiguous-output-head-tensor";
        profile->output_head_ambiguous_count = 1;
        profile->output_head.mapping_status = "ambiguous";
        profile->output_head_missing_status = "ambiguous";
        profile->shape_relation_status = output_head_map_shape_relation(profile);
    } else {
        profile->status = "output-head-profiled";
        profile->top_blocker = "missing-output-head-runtime-consumer";
        profile->output_head_missing_status = "present";
        profile->tie_policy_status = "separate-output-head-candidate";
        profile->shape_relation_status = output_head_map_shape_relation(profile);
    }
    return 0;
}

static const char *output_head_map_present_label(
    const yvex_output_head_map_entry *entry)
{
    return entry && entry->present ? "yes" : "no";
}

static const char *output_head_map_normal_role(
    const yvex_output_head_map_entry *entry)
{
    return entry && entry->present ? entry->canonical_role : "missing";
}

static void print_output_head_map_normal(
    const yvex_output_head_map_profile *profile)
{
    model_target_out_writef(model_target_out(), "output-head-map: %s [%s]\n",
           profile->record->target_id,
           compact_status_bracket(profile->status));
    model_target_out_writef(model_target_out(), "family: %s  evidence: header-only\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "head: %s  final_norm: %s  embedding: %s  tie: %s\n",
           output_head_map_normal_role(&profile->output_head),
           output_head_map_normal_role(&profile->final_norm),
           output_head_map_normal_role(&profile->embedding),
           profile->tie_policy_status);
    model_target_out_writef(model_target_out(), "shape: %s\n", profile->shape_relation_status);
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next: %s\n", YVEX_OUTPUT_HEAD_MAP_NEXT_ROW);
    model_target_out_writef(model_target_out(), "boundary: mapping only; no logits/runtime/generation\n");
}

static void print_output_head_map_table(
    const yvex_output_head_map_profile *profile)
{
    model_target_out_writef(model_target_out(), "OUTPUT HEAD TENSOR MAP\n\n");
    model_target_out_writef(model_target_out(), "%-6s  %-20s  %-31s  %-4s  %-10s  %-5s  %-34s  %-24s  %s\n",
           "FAMILY", "TARGET", "STATUS", "HEAD", "FINAL_NORM", "EMBED",
           "TIE_POLICY", "SHAPE_RELATION", "NEXT");
    model_target_out_writef(model_target_out(), "%-6s  %-20s  %-31s  %-4s  %-10s  %-5s  %-34s  %-24s  %s\n",
           profile->spec->family_key,
           profile->record->target_id,
           profile->status,
           output_head_map_present_label(&profile->output_head),
           output_head_map_present_label(&profile->final_norm),
           output_head_map_present_label(&profile->embedding),
           profile->tie_policy_status,
           profile->shape_relation_status,
           YVEX_OUTPUT_HEAD_MAP_NEXT_ROW);
}

static void print_output_head_entry_audit(
    const char *name,
    const yvex_output_head_map_entry *entry)
{
    model_target_out_writef(model_target_out(), "output_head.entry.%s.native_name: %s\n", name, entry->native_name);
    model_target_out_writef(model_target_out(), "output_head.entry.%s.canonical_role: %s\n",
           name, entry->canonical_role);
    model_target_out_writef(model_target_out(), "output_head.entry.%s.mapping_status: %s\n",
           name, entry->mapping_status);
    model_target_out_writef(model_target_out(), "output_head.entry.%s.dtype: %s\n", name, entry->dtype);
    model_target_out_writef(model_target_out(), "output_head.entry.%s.rank: %s\n", name, entry->rank);
    model_target_out_writef(model_target_out(), "output_head.entry.%s.shape: %s\n", name, entry->shape);
    model_target_out_writef(model_target_out(), "output_head.entry.%s.source_file: %s\n", name, entry->source_file);
}

static void print_output_head_map_audit(
    const yvex_output_head_map_profile *profile)
{
    model_target_out_writef(model_target_out(), "output_head_map_status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "output_head_map_family: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "output_head_map_target_id: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "output_head_map_stage: header-output-head-map\n");
    model_target_out_writef(model_target_out(), "output_head_map_evidence_basis: header-metadata-only\n");
    model_target_out_writef(model_target_out(), "output_head_map_source_status: %s\n",
           profile->source_exists ? "present" : "missing");
    model_target_out_writef(model_target_out(), "output_head_map_source_path: %s\n", profile->source_path);
    model_target_out_writef(model_target_out(), "output_head_map_manifest_status: not-checked\n");
    model_target_out_writef(model_target_out(), "output_head_map_config_status: %s\n",
           profile->config_present ? "present" : "missing");
    model_target_out_writef(model_target_out(), "output_head_map_tokenizer_status: %s\n",
           profile->tokenizer_present ? "present" : "missing");
    model_target_out_writef(model_target_out(), "output_head_native_name: %s\n", profile->output_head.native_name);
    model_target_out_writef(model_target_out(), "output_head_canonical_role: %s\n",
           profile->output_head.canonical_role);
    model_target_out_writef(model_target_out(), "output_head_mapping_status: %s\n",
           profile->output_head.mapping_status);
    model_target_out_writef(model_target_out(), "output_head_candidate_count: %llu\n",
           profile->output_head_candidate_count);
    model_target_out_writef(model_target_out(), "output_head_ambiguous_count: %llu\n",
           profile->output_head_ambiguous_count);
    model_target_out_writef(model_target_out(), "output_head_missing_status: %s\n",
           profile->output_head_missing_status);
    model_target_out_writef(model_target_out(), "output_head_dtype: %s\n", profile->output_head.dtype);
    model_target_out_writef(model_target_out(), "output_head_rank: %s\n", profile->output_head.rank);
    model_target_out_writef(model_target_out(), "output_head_shape: %s\n", profile->output_head.shape);
    model_target_out_writef(model_target_out(), "output_head_vocab_dim_candidate: %s\n",
           profile->output_head.vocab_dim_candidate);
    model_target_out_writef(model_target_out(), "output_head_hidden_dim_candidate: %s\n",
           profile->output_head.hidden_dim_candidate);
    model_target_out_writef(model_target_out(), "embedding_native_name: %s\n", profile->embedding.native_name);
    model_target_out_writef(model_target_out(), "embedding_canonical_role: %s\n", profile->embedding.canonical_role);
    model_target_out_writef(model_target_out(), "embedding_dtype: %s\n", profile->embedding.dtype);
    model_target_out_writef(model_target_out(), "embedding_rank: %s\n", profile->embedding.rank);
    model_target_out_writef(model_target_out(), "embedding_shape: %s\n", profile->embedding.shape);
    model_target_out_writef(model_target_out(), "final_norm_native_name: %s\n", profile->final_norm.native_name);
    model_target_out_writef(model_target_out(), "final_norm_canonical_role: %s\n",
           profile->final_norm.canonical_role);
    model_target_out_writef(model_target_out(), "final_norm_dtype: %s\n", profile->final_norm.dtype);
    model_target_out_writef(model_target_out(), "final_norm_rank: %s\n", profile->final_norm.rank);
    model_target_out_writef(model_target_out(), "final_norm_shape: %s\n", profile->final_norm.shape);
    model_target_out_writef(model_target_out(), "tie_policy_status: %s\n", profile->tie_policy_status);
    model_target_out_writef(model_target_out(), "config_tie_word_embeddings_status: %s\n",
           profile->config_tie_word_embeddings_status);
    model_target_out_writef(model_target_out(), "shape_relation_status: %s\n", profile->shape_relation_status);
    model_target_out_writef(model_target_out(), "output_head_runtime_consumer_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "output_head_logits_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "output_head_artifact_contract_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "output_head_runtime_descriptor_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "output_head_graph_consumer_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next_required_rows: %s\n", YVEX_OUTPUT_HEAD_MAP_NEXT_ROW);
    print_output_head_entry_audit("output", &profile->output_head);
    print_output_head_entry_audit("embedding", &profile->embedding);
    print_output_head_entry_audit("final_norm", &profile->final_norm);
    model_target_out_writef(model_target_out(), "boundary: output-head tensor mapping only; no logits/runtime/generation\n");
}

static void tokenizer_map_sidecar_init(yvex_tokenizer_map_sidecar *sidecar,
                                       const char *source_path,
                                       const char *file_name,
                                       const char *canonical_role)
{
    if (!sidecar) return;
    memset(sidecar, 0, sizeof(*sidecar));
    sidecar->file_name = file_name;
    sidecar->canonical_role = canonical_role;
    sidecar->status = "missing";
    if (source_path && source_path[0] && file_name) {
        (void)model_class_path_join(sidecar->path, sizeof(sidecar->path),
                                    source_path, file_name);
    } else {
        snprintf(sidecar->path, sizeof(sidecar->path), "unknown");
    }
}

static int tokenizer_map_file_readable(const char *path)
{
    FILE *fp;

    if (!path || !path[0]) return 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

static int tokenizer_map_read_json(const char *path, char *buf, size_t cap)
{
    FILE *fp;
    long size;
    size_t nread;

    if (!path || !buf || cap == 0u) return 0;
    buf[0] = '\0';
    fp = fopen(path, "rb");
    if (!fp) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    size = ftell(fp);
    if (size < 0 || (unsigned long)size >= cap) {
        fclose(fp);
        return 0;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 0;
    }
    nread = fread(buf, 1u, (size_t)size, fp);
    fclose(fp);
    if (nread != (size_t)size) return 0;
    buf[nread] = '\0';
    return 1;
}

static int tokenizer_map_json_valid(const char *json)
{
    const char *p;
    const char *end;
    int depth = 0;
    int in_string = 0;
    int escape = 0;

    if (!json) return 0;
    p = json;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '{') return 0;
    end = json + strlen(json);
    while (end > p && isspace((unsigned char)end[-1])) end--;
    if (end <= p || end[-1] != '}') return 0;
    for (; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (in_string) {
            if (escape) {
                escape = 0;
            } else if (c == '\\') {
                escape = 1;
            } else if (c == '"') {
                in_string = 0;
            }
            continue;
        }
        if (c == '"') {
            in_string = 1;
        } else if (c == '{' || c == '[') {
            depth++;
        } else if (c == '}' || c == ']') {
            depth--;
            if (depth < 0) return 0;
        }
    }
    return depth == 0 && !in_string && !escape;
}

static const char *tokenizer_map_json_value(const char *json, const char *key)
{
    char pattern[96];
    const char *p;
    const char *colon;
    int n;

    if (!json || !key) return NULL;
    n = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (n < 0 || (size_t)n >= sizeof(pattern)) return NULL;
    p = strstr(json, pattern);
    if (!p) return NULL;
    colon = strchr(p + strlen(pattern), ':');
    if (!colon) return NULL;
    p = colon + 1;
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static int tokenizer_map_json_has_key(const char *json, const char *key)
{
    return tokenizer_map_json_value(json, key) != NULL;
}

static int tokenizer_map_json_string(const char *json,
                                     const char *key,
                                     char *out,
                                     size_t out_cap)
{
    const char *p;
    size_t len = 0u;
    int escape = 0;

    if (!out || out_cap == 0u) return 0;
    out[0] = '\0';
    p = tokenizer_map_json_value(json, key);
    if (!p || *p != '"') return 0;
    p++;
    while (*p) {
        unsigned char c = (unsigned char)*p++;
        if (escape) {
            if (len + 1u < out_cap) out[len++] = (char)c;
            escape = 0;
        } else if (c == '\\') {
            escape = 1;
        } else if (c == '"') {
            out[len] = '\0';
            return 1;
        } else if (len + 1u < out_cap) {
            out[len++] = (char)c;
        }
    }
    out[len] = '\0';
    return 0;
}

static int tokenizer_map_json_uint(const char *json,
                                   const char *key,
                                   char *out,
                                   size_t out_cap)
{
    const char *p;
    char *end = NULL;
    unsigned long long value;

    if (!out || out_cap == 0u) return 0;
    out[0] = '\0';
    p = tokenizer_map_json_value(json, key);
    if (!p) return 0;
    if (*p == '"') p++;
    if (!isdigit((unsigned char)*p)) return 0;
    errno = 0;
    value = strtoull(p, &end, 10);
    if (errno != 0 || end == p) return 0;
    snprintf(out, out_cap, "%llu", value);
    return 1;
}

static unsigned long tokenizer_map_json_string_array_count(const char *json,
                                                           const char *key)
{
    const char *p;
    unsigned long count = 0u;
    int in_string = 0;
    int escape = 0;

    p = tokenizer_map_json_value(json, key);
    if (!p) return 0u;
    while (*p && *p != '[') p++;
    if (*p != '[') return 0u;
    p++;
    for (; *p && *p != ']'; ++p) {
        unsigned char c = (unsigned char)*p;
        if (in_string) {
            if (escape) {
                escape = 0;
            } else if (c == '\\') {
                escape = 1;
            } else if (c == '"') {
                in_string = 0;
                count++;
            }
        } else if (c == '"') {
            in_string = 1;
        }
    }
    return count;
}

static unsigned long tokenizer_map_json_object_array_count(const char *json,
                                                           const char *key)
{
    const char *p;
    unsigned long count = 0u;
    int depth = 0;
    int in_string = 0;
    int escape = 0;

    p = tokenizer_map_json_value(json, key);
    if (!p) return 0u;
    while (*p && *p != '[') p++;
    if (*p != '[') return 0u;
    p++;
    for (; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (in_string) {
            if (escape) {
                escape = 0;
            } else if (c == '\\') {
                escape = 1;
            } else if (c == '"') {
                in_string = 0;
            }
            continue;
        }
        if (c == '"') {
            in_string = 1;
        } else if (c == '{') {
            if (depth == 0) count++;
            depth++;
        } else if (c == '}') {
            if (depth > 0) depth--;
        } else if (c == ']' && depth == 0) {
            break;
        }
    }
    return count;
}

static void tokenizer_map_set_id(char *id,
                                 size_t id_cap,
                                 const char **status,
                                 const char *candidate)
{
    if (!id || id_cap == 0u || !status || !candidate || !candidate[0]) return;
    if (strcmp(*status, "present") == 0) return;
    snprintf(id, id_cap, "%s", candidate);
    *status = "present";
}

static void tokenizer_map_set_text(char *text,
                                   size_t text_cap,
                                   const char **status,
                                   const char *candidate)
{
    if (!text || text_cap == 0u || !status || !candidate || !candidate[0]) return;
    if (strcmp(*status, "present") == 0) return;
    snprintf(text, text_cap, "%s", candidate);
    *status = "present";
}

static void tokenizer_map_probe_json_sidecar(yvex_tokenizer_map_sidecar *sidecar,
                                             char *json,
                                             size_t json_cap)
{
    struct stat st;

    if (!sidecar || !json || json_cap == 0u) return;
    json[0] = '\0';
    if (stat(sidecar->path, &st) != 0 || !S_ISREG(st.st_mode)) {
        sidecar->status = "missing";
        return;
    }
    if (!tokenizer_map_read_json(sidecar->path, json, json_cap)) {
        sidecar->status = tokenizer_map_file_readable(sidecar->path)
            ? "malformed"
            : "unreadable";
        json[0] = '\0';
        return;
    }
    if (!tokenizer_map_json_valid(json)) {
        sidecar->status = "malformed";
        json[0] = '\0';
        return;
    }
    sidecar->status = "present";
}

static void tokenizer_map_probe_plain_sidecar(yvex_tokenizer_map_sidecar *sidecar)
{
    struct stat st;

    if (!sidecar) return;
    if (stat(sidecar->path, &st) != 0 || !S_ISREG(st.st_mode)) {
        sidecar->status = "missing";
    } else if (tokenizer_map_file_readable(sidecar->path)) {
        sidecar->status = "present";
    } else {
        sidecar->status = "unreadable";
    }
}

static int tokenizer_map_sidecar_present(const yvex_tokenizer_map_sidecar *sidecar)
{
    return sidecar && strcmp(sidecar->status, "present") == 0;
}

static int tokenizer_map_sidecar_malformed(const yvex_tokenizer_map_sidecar *sidecar)
{
    return sidecar && strcmp(sidecar->status, "malformed") == 0;
}

static const char *tokenizer_map_yes_no(const yvex_tokenizer_map_sidecar *sidecar)
{
    return tokenizer_map_sidecar_present(sidecar) ? "yes" : "no";
}

static const char *tokenizer_map_normal_sidecar(
    const yvex_tokenizer_map_sidecar *sidecar,
    const char *present_text)
{
    if (!sidecar) return "unknown";
    if (strcmp(sidecar->status, "present") == 0) return present_text;
    return sidecar->status;
}

static void tokenizer_map_parse_id_fields(yvex_tokenizer_map_profile *profile,
                                          const char *json)
{
    char value[32];

    if (!profile || !json || !json[0]) return;
    if (tokenizer_map_json_uint(json, "bos_token_id", value, sizeof(value))) {
        tokenizer_map_set_id(profile->bos_token_id, sizeof(profile->bos_token_id),
                             &profile->bos_token_id_status, value);
    }
    if (tokenizer_map_json_uint(json, "eos_token_id", value, sizeof(value))) {
        tokenizer_map_set_id(profile->eos_token_id, sizeof(profile->eos_token_id),
                             &profile->eos_token_id_status, value);
    }
    if (tokenizer_map_json_uint(json, "pad_token_id", value, sizeof(value))) {
        tokenizer_map_set_id(profile->pad_token_id, sizeof(profile->pad_token_id),
                             &profile->pad_token_id_status, value);
    }
    if (tokenizer_map_json_uint(json, "unk_token_id", value, sizeof(value))) {
        tokenizer_map_set_id(profile->unk_token_id, sizeof(profile->unk_token_id),
                             &profile->unk_token_id_status, value);
    }
    if (tokenizer_map_json_uint(json, "sep_token_id", value, sizeof(value))) {
        tokenizer_map_set_id(profile->sep_token_id, sizeof(profile->sep_token_id),
                             &profile->sep_token_id_status, value);
    }
}

static void tokenizer_map_parse_text_fields(yvex_tokenizer_map_profile *profile,
                                            const char *json)
{
    char value[64];

    if (!profile || !json || !json[0]) return;
    if (tokenizer_map_json_string(json, "bos_token", value, sizeof(value))) {
        tokenizer_map_set_text(profile->bos_token, sizeof(profile->bos_token),
                               &profile->bos_token_status, value);
    }
    if (tokenizer_map_json_string(json, "eos_token", value, sizeof(value))) {
        tokenizer_map_set_text(profile->eos_token, sizeof(profile->eos_token),
                               &profile->eos_token_status, value);
    }
    if (tokenizer_map_json_string(json, "pad_token", value, sizeof(value))) {
        tokenizer_map_set_text(profile->pad_token, sizeof(profile->pad_token),
                               &profile->pad_token_status, value);
    }
    if (tokenizer_map_json_string(json, "unk_token", value, sizeof(value))) {
        tokenizer_map_set_text(profile->unk_token, sizeof(profile->unk_token),
                               &profile->unk_token_status, value);
    }
    if (tokenizer_map_json_string(json, "sep_token", value, sizeof(value))) {
        tokenizer_map_set_text(profile->sep_token, sizeof(profile->sep_token),
                               &profile->sep_token_status, value);
    }
}

static int tokenizer_map_status_complete(const yvex_tokenizer_map_profile *profile)
{
    return profile && strcmp(profile->status, "present-report-only") == 0;
}

static const char *tokenizer_map_next_row(const yvex_tokenizer_map_profile *profile)
{
    return tokenizer_map_status_complete(profile)
        ? YVEX_TOKENIZER_MAP_NEXT_ROW
        : YVEX_TOKENIZER_MISSING_NEXT_ROW;
}

static const char *tokenizer_map_vocab_status(
    const yvex_tokenizer_map_profile *profile)
{
    if (!profile) return "unknown";
    if (strcmp(profile->vocab_size_status, "present") != 0) return "missing";
    if (tokenizer_map_sidecar_present(&profile->vocab_json)) return "present";
    if (tokenizer_map_sidecar_present(&profile->tokenizer_json)) return "embedded-or-tokenizer-json";
    return "config-only";
}

static const char *tokenizer_map_merges_status(
    const yvex_tokenizer_map_profile *profile)
{
    if (!profile) return "unknown";
    if (tokenizer_map_sidecar_present(&profile->merges_txt)) return "present";
    if (strcmp(profile->spec->family_key, "gemma") == 0) return "not-required-or-absent";
    return "missing";
}

static const char *tokenizer_map_specials_status(
    const yvex_tokenizer_map_profile *profile)
{
    if (!profile) return "missing";
    return strcmp(profile->bos_token_id_status, "present") == 0 &&
           strcmp(profile->eos_token_id_status, "present") == 0 &&
           strcmp(profile->pad_token_id_status, "present") == 0 &&
           strcmp(profile->unk_token_id_status, "present") == 0
               ? "present"
               : "missing";
}

static void tokenizer_map_choose_vocab(yvex_tokenizer_map_profile *profile)
{
    if (!profile) return;
    if (strcmp(profile->config_vocab_size, "unknown") != 0) {
        snprintf(profile->vocab_size, sizeof(profile->vocab_size), "%s",
                 profile->config_vocab_size);
        profile->vocab_size_status = "present";
    } else if (strcmp(profile->tokenizer_vocab_size, "unknown") != 0) {
        snprintf(profile->vocab_size, sizeof(profile->vocab_size), "%s",
                 profile->tokenizer_vocab_size);
        profile->vocab_size_status = "present";
    }
}

static void tokenizer_map_output_head_relation(
    yvex_tokenizer_map_profile *profile,
    const char *models_root_override,
    const char *source_override)
{
    yvex_output_head_map_profile output_profile;
    int rc;

    if (!profile) return;
    rc = build_output_head_map_profile(profile->record, profile->spec,
                                       models_root_override, source_override,
                                       &output_profile);
    if (rc != 0) {
        profile->output_head_vocab_relation_status = "unknown";
        return;
    }
    snprintf(profile->output_head_vocab_dim_candidate,
             sizeof(profile->output_head_vocab_dim_candidate), "%s",
             output_profile.output_head.vocab_dim_candidate);
    if (!output_profile.source_exists ||
        strcmp(profile->vocab_size_status, "present") != 0) {
        profile->output_head_vocab_relation_status = "unknown";
    } else if (!output_profile.output_head.present) {
        profile->output_head_vocab_relation_status = "output-head-missing";
    } else if (strcmp(output_profile.output_head.vocab_dim_candidate,
                      profile->vocab_size) == 0) {
        profile->output_head_vocab_relation_status =
            "vocab-size-matches-output-head";
    } else {
        profile->output_head_vocab_relation_status =
            "vocab-size-mismatch-output-head";
    }
}

static void tokenizer_map_profile_defaults(yvex_tokenizer_map_profile *profile)
{
    snprintf(profile->tokenizer_class, sizeof(profile->tokenizer_class), "unknown");
    snprintf(profile->model_type, sizeof(profile->model_type), "unknown");
    snprintf(profile->tokenizer_backend_type, sizeof(profile->tokenizer_backend_type),
             "unknown");
    profile->vocab_size_status = "missing";
    snprintf(profile->vocab_size, sizeof(profile->vocab_size), "unknown");
    snprintf(profile->config_vocab_size, sizeof(profile->config_vocab_size), "unknown");
    snprintf(profile->tokenizer_vocab_size, sizeof(profile->tokenizer_vocab_size), "unknown");
    snprintf(profile->output_head_vocab_dim_candidate,
             sizeof(profile->output_head_vocab_dim_candidate), "unknown");
    profile->output_head_vocab_relation_status = "unknown";
    profile->bos_token_id_status = "missing";
    profile->eos_token_id_status = "missing";
    profile->pad_token_id_status = "missing";
    profile->unk_token_id_status = "missing";
    profile->sep_token_id_status = "missing";
    profile->bos_token_status = "missing";
    profile->eos_token_status = "missing";
    profile->pad_token_status = "missing";
    profile->unk_token_status = "missing";
    profile->sep_token_status = "missing";
    snprintf(profile->bos_token_id, sizeof(profile->bos_token_id), "unknown");
    snprintf(profile->eos_token_id, sizeof(profile->eos_token_id), "unknown");
    snprintf(profile->pad_token_id, sizeof(profile->pad_token_id), "unknown");
    snprintf(profile->unk_token_id, sizeof(profile->unk_token_id), "unknown");
    snprintf(profile->sep_token_id, sizeof(profile->sep_token_id), "unknown");
    snprintf(profile->bos_token, sizeof(profile->bos_token), "unknown");
    snprintf(profile->eos_token, sizeof(profile->eos_token), "unknown");
    snprintf(profile->pad_token, sizeof(profile->pad_token), "unknown");
    snprintf(profile->unk_token, sizeof(profile->unk_token), "unknown");
    snprintf(profile->sep_token, sizeof(profile->sep_token), "unknown");
    profile->added_tokens_status = "missing";
    snprintf(profile->added_tokens_count, sizeof(profile->added_tokens_count), "0");
    profile->special_tokens_status = "missing";
    profile->additional_special_tokens_status = "missing";
    snprintf(profile->additional_special_tokens_count,
             sizeof(profile->additional_special_tokens_count), "0");
    profile->stop_token_candidate_status = "missing";
    snprintf(profile->stop_token_candidate_count,
             sizeof(profile->stop_token_candidate_count), "0");
    snprintf(profile->stop_token_candidate_0_id,
             sizeof(profile->stop_token_candidate_0_id), "unknown");
    snprintf(profile->stop_token_candidate_0_text,
             sizeof(profile->stop_token_candidate_0_text), "unknown");
    profile->chat_template_status = "unknown";
    profile->chat_template_present = "unknown";
    profile->chat_template_source = "none";
    profile->prompt_template_status = "missing";
}

static int build_tokenizer_map_profile(
    const yvex_model_target_record *record,
    const yvex_model_class_profile_spec *spec_override,
    const char *models_root_override,
    const char *source_override,
    yvex_tokenizer_map_profile *profile)
{
    const yvex_model_class_profile_spec *spec;
    yvex_model_class_profile source_profile;
    char json[YVEX_TOKENIZER_MAP_JSON_CAP];
    int rc;
    int sidecar_count;
    int malformed_count;

    if (!record || !profile) return 2;
    spec = spec_override ? spec_override :
           find_model_class_profile_spec(record->target_id);
    if (!spec) return 2;

    memset(profile, 0, sizeof(*profile));
    profile->record = record;
    profile->spec = spec;
    profile->status = "source-missing";
    profile->top_blocker = spec->missing_source_blocker;
    tokenizer_map_profile_defaults(profile);

    memset(&source_profile, 0, sizeof(source_profile));
    source_profile.record = record;
    source_profile.spec = spec;
    rc = model_class_resolve_source(models_root_override, source_override,
                                    &source_profile);
    if (rc != 0) return rc;
    snprintf(profile->source_path, sizeof(profile->source_path), "%s",
             source_profile.source_path);
    snprintf(profile->source_path_source, sizeof(profile->source_path_source),
             "%s", source_profile.source_path_source);
    profile->source_exists = model_class_dir_exists(profile->source_path);

    tokenizer_map_sidecar_init(&profile->tokenizer_json, profile->source_path,
                               "tokenizer.json",
                               "model.tokenizer.sidecar.tokenizer_json");
    tokenizer_map_sidecar_init(&profile->tokenizer_config, profile->source_path,
                               "tokenizer_config.json",
                               "model.tokenizer.sidecar.tokenizer_config");
    tokenizer_map_sidecar_init(&profile->special_tokens_map, profile->source_path,
                               "special_tokens_map.json",
                               "model.tokenizer.sidecar.special_tokens_map");
    tokenizer_map_sidecar_init(&profile->generation_config, profile->source_path,
                               "generation_config.json",
                               "model.tokenizer.sidecar.generation_config");
    tokenizer_map_sidecar_init(&profile->config_json, profile->source_path,
                               "config.json",
                               "model.config.sidecar.config_json");
    tokenizer_map_sidecar_init(&profile->vocab_json, profile->source_path,
                               "vocab.json",
                               "model.tokenizer.sidecar.vocab_json");
    tokenizer_map_sidecar_init(&profile->merges_txt, profile->source_path,
                               "merges.txt",
                               "model.tokenizer.sidecar.merges_txt");
    tokenizer_map_sidecar_init(&profile->tokenizer_model, profile->source_path,
                               "tokenizer.model",
                               "model.tokenizer.sidecar.tokenizer_model");
    tokenizer_map_sidecar_init(&profile->chat_template_file, profile->source_path,
                               "chat_template.jinja",
                               "model.tokenizer.sidecar.chat_template");

    if (!profile->source_exists) return 0;

    tokenizer_map_probe_json_sidecar(&profile->config_json, json, sizeof(json));
    if (tokenizer_map_sidecar_present(&profile->config_json)) {
        (void)tokenizer_map_json_string(json, "model_type",
                                        profile->model_type,
                                        sizeof(profile->model_type));
        if (tokenizer_map_json_uint(json, "vocab_size",
                                    profile->config_vocab_size,
                                    sizeof(profile->config_vocab_size))) {
            profile->vocab_size_status = "present";
        }
        tokenizer_map_parse_id_fields(profile, json);
    }

    tokenizer_map_probe_json_sidecar(&profile->tokenizer_config, json,
                                     sizeof(json));
    if (tokenizer_map_sidecar_present(&profile->tokenizer_config)) {
        (void)tokenizer_map_json_string(json, "tokenizer_class",
                                        profile->tokenizer_class,
                                        sizeof(profile->tokenizer_class));
        tokenizer_map_parse_id_fields(profile, json);
        tokenizer_map_parse_text_fields(profile, json);
        if (tokenizer_map_json_has_key(json, "chat_template")) {
            profile->chat_template_status = "present";
            profile->chat_template_present = "true";
            profile->chat_template_source = "tokenizer_config.json";
            profile->prompt_template_status = "present-report-only";
        }
    }

    tokenizer_map_probe_json_sidecar(&profile->generation_config, json,
                                     sizeof(json));
    if (tokenizer_map_sidecar_present(&profile->generation_config)) {
        tokenizer_map_parse_id_fields(profile, json);
    }

    tokenizer_map_probe_json_sidecar(&profile->special_tokens_map, json,
                                     sizeof(json));
    if (tokenizer_map_sidecar_present(&profile->special_tokens_map)) {
        unsigned long count =
            tokenizer_map_json_string_array_count(json,
                                                  "additional_special_tokens");
        tokenizer_map_parse_text_fields(profile, json);
        if (tokenizer_map_json_has_key(json, "additional_special_tokens")) {
            profile->additional_special_tokens_status = "present";
            snprintf(profile->additional_special_tokens_count,
                     sizeof(profile->additional_special_tokens_count),
                     "%lu", count);
        }
    }

    tokenizer_map_probe_json_sidecar(&profile->tokenizer_json, json,
                                     sizeof(json));
    if (tokenizer_map_sidecar_present(&profile->tokenizer_json)) {
        char value[32];

        if (tokenizer_map_json_uint(json, "vocab_size", value, sizeof(value))) {
            snprintf(profile->tokenizer_vocab_size,
                     sizeof(profile->tokenizer_vocab_size), "%s", value);
        }
        if (tokenizer_map_json_string(json, "type",
                                      profile->tokenizer_backend_type,
                                      sizeof(profile->tokenizer_backend_type))) {
            profile->tokenizer_backend_type[sizeof(profile->tokenizer_backend_type) - 1u] = '\0';
        }
        if (tokenizer_map_json_has_key(json, "added_tokens")) {
            unsigned long count =
                tokenizer_map_json_object_array_count(json, "added_tokens");
            profile->added_tokens_status = "present";
            snprintf(profile->added_tokens_count,
                     sizeof(profile->added_tokens_count), "%lu", count);
        }
    }

    tokenizer_map_probe_json_sidecar(&profile->vocab_json, json, sizeof(json));
    tokenizer_map_probe_plain_sidecar(&profile->merges_txt);
    tokenizer_map_probe_plain_sidecar(&profile->tokenizer_model);
    tokenizer_map_probe_plain_sidecar(&profile->chat_template_file);
    if (tokenizer_map_sidecar_present(&profile->chat_template_file)) {
        profile->chat_template_status = "present";
        profile->chat_template_present = "true";
        profile->chat_template_source = "chat_template.jinja";
        profile->prompt_template_status = "present-report-only";
    }

    tokenizer_map_choose_vocab(profile);
    tokenizer_map_output_head_relation(profile, models_root_override,
                                       source_override);
    profile->special_tokens_status = tokenizer_map_specials_status(profile);
    if (strcmp(profile->eos_token_id_status, "present") == 0) {
        profile->stop_token_candidate_status = "present";
        snprintf(profile->stop_token_candidate_count,
                 sizeof(profile->stop_token_candidate_count), "1");
        snprintf(profile->stop_token_candidate_0_id,
                 sizeof(profile->stop_token_candidate_0_id), "%s",
                 profile->eos_token_id);
        snprintf(profile->stop_token_candidate_0_text,
                 sizeof(profile->stop_token_candidate_0_text), "%s",
                 strcmp(profile->eos_token_status, "present") == 0
                     ? profile->eos_token
                     : "unknown");
    }

    sidecar_count =
        tokenizer_map_sidecar_present(&profile->tokenizer_json) +
        tokenizer_map_sidecar_present(&profile->tokenizer_config) +
        tokenizer_map_sidecar_present(&profile->special_tokens_map) +
        tokenizer_map_sidecar_present(&profile->generation_config) +
        tokenizer_map_sidecar_present(&profile->config_json) +
        tokenizer_map_sidecar_present(&profile->vocab_json) +
        tokenizer_map_sidecar_present(&profile->merges_txt) +
        tokenizer_map_sidecar_present(&profile->tokenizer_model) +
        tokenizer_map_sidecar_present(&profile->chat_template_file);
    malformed_count =
        tokenizer_map_sidecar_malformed(&profile->tokenizer_json) +
        tokenizer_map_sidecar_malformed(&profile->tokenizer_config) +
        tokenizer_map_sidecar_malformed(&profile->special_tokens_map) +
        tokenizer_map_sidecar_malformed(&profile->generation_config) +
        tokenizer_map_sidecar_malformed(&profile->config_json) +
        tokenizer_map_sidecar_malformed(&profile->vocab_json);

    if (malformed_count > 0) {
        profile->status = "tokenizer-metadata-malformed";
        profile->top_blocker = "malformed-tokenizer-sidecar";
    } else if (sidecar_count == 0) {
        profile->status = "metadata-missing";
        profile->top_blocker = "missing-tokenizer-sidecars";
    } else if (strcmp(profile->output_head_vocab_relation_status,
                      "vocab-size-mismatch-output-head") == 0) {
        profile->status = "tokenizer-metadata-ambiguous";
        profile->top_blocker = "tokenizer-vocab-output-head-mismatch";
    } else if (tokenizer_map_sidecar_present(&profile->tokenizer_json) &&
               tokenizer_map_sidecar_present(&profile->config_json) &&
               strcmp(profile->vocab_size_status, "present") == 0 &&
               strcmp(profile->bos_token_id_status, "present") == 0 &&
               strcmp(profile->eos_token_id_status, "present") == 0 &&
               strcmp(profile->pad_token_id_status, "present") == 0 &&
               strcmp(profile->unk_token_id_status, "present") == 0) {
        profile->status = "present-report-only";
        profile->top_blocker = "quant-policy-or-artifact-emitter";
    } else {
        profile->status = "tokenizer-metadata-incomplete";
        profile->top_blocker = "incomplete-tokenizer-metadata";
    }
    return 0;
}

static void print_tokenizer_map_normal(
    const yvex_tokenizer_map_profile *profile)
{
    model_target_out_writef(model_target_out(), "tokenizer-map: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "family: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "tokenizer: %s\n",
           tokenizer_map_normal_sidecar(&profile->tokenizer_json, "present"));
    model_target_out_writef(model_target_out(), "vocab: %s\n", tokenizer_map_vocab_status(profile));
    model_target_out_writef(model_target_out(), "merges: %s\n", tokenizer_map_merges_status(profile));
    model_target_out_writef(model_target_out(), "chat_template: %s\n",
           strcmp(profile->chat_template_status, "present") == 0
               ? "present"
               : profile->chat_template_status);
    model_target_out_writef(model_target_out(), "specials: %s\n", profile->special_tokens_status);
    model_target_out_writef(model_target_out(), "runtime: unsupported\n");
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next: %s\n", tokenizer_map_next_row(profile));
    model_target_out_writef(model_target_out(), "boundary: tokenizer metadata mapping only; no tokenization/detokenization/runtime/generation\n");
}

static void print_tokenizer_map_table(
    const yvex_tokenizer_map_profile *profile)
{
    model_target_out_writef(model_target_out(), "TOKENIZER METADATA MAP\n\n");
    model_target_out_writef(model_target_out(), "%-20s  %-6s  %-19s  %-9s  %-28s  %-22s  %-13s  %-8s  %s\n",
           "TARGET", "FAMILY", "STATUS", "TOKENIZER", "VOCAB", "MERGES",
           "CHAT_TEMPLATE", "SPECIALS", "NEXT");
    model_target_out_writef(model_target_out(), "%-20s  %-6s  %-19s  %-9s  %-28s  %-22s  %-13s  %-8s  %s\n",
           profile->record->target_id,
           profile->spec->family_key,
           profile->status,
           tokenizer_map_yes_no(&profile->tokenizer_json),
           tokenizer_map_vocab_status(profile),
           tokenizer_map_merges_status(profile),
           strcmp(profile->chat_template_status, "present") == 0
               ? "present"
               : profile->chat_template_status,
           profile->special_tokens_status,
           tokenizer_map_next_row(profile));
}

static void print_tokenizer_map_audit(
    const yvex_tokenizer_map_profile *profile)
{
    model_target_out_writef(model_target_out(), "schema_version: yvex.source.tokenizer_map.v1\n");
    model_target_out_writef(model_target_out(), "tokenizer_map_status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "tokenizer_map_family: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "tokenizer_map_target_id: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "target_id: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "family: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "tokenizer_map_stage: metadata-tokenizer-map\n");
    model_target_out_writef(model_target_out(), "tokenizer_map_evidence_basis: sidecar-json-only\n");
    model_target_out_writef(model_target_out(), "evidence_basis: sidecar-json-only\n");
    model_target_out_writef(model_target_out(), "tokenizer_map_source_status: %s\n",
           profile->source_exists ? "present" : "missing");
    model_target_out_writef(model_target_out(), "tokenizer_map_source_path: %s\n", profile->source_path);
    model_target_out_writef(model_target_out(), "source_path: %s\n", profile->source_path);
    model_target_out_writef(model_target_out(), "tokenizer_json_status: %s\n", profile->tokenizer_json.status);
    model_target_out_writef(model_target_out(), "tokenizer_json_path: %s\n", profile->tokenizer_json.path);
    model_target_out_writef(model_target_out(), "tokenizer_config_status: %s\n", profile->tokenizer_config.status);
    model_target_out_writef(model_target_out(), "tokenizer_config_path: %s\n", profile->tokenizer_config.path);
    model_target_out_writef(model_target_out(), "special_tokens_map_status: %s\n",
           profile->special_tokens_map.status);
    model_target_out_writef(model_target_out(), "special_tokens_map_path: %s\n", profile->special_tokens_map.path);
    model_target_out_writef(model_target_out(), "generation_config_status: %s\n",
           profile->generation_config.status);
    model_target_out_writef(model_target_out(), "generation_config_path: %s\n", profile->generation_config.path);
    model_target_out_writef(model_target_out(), "config_json_status: %s\n", profile->config_json.status);
    model_target_out_writef(model_target_out(), "config_json_path: %s\n", profile->config_json.path);
    model_target_out_writef(model_target_out(), "vocab_json_status: %s\n", profile->vocab_json.status);
    model_target_out_writef(model_target_out(), "vocab_status: %s\n", tokenizer_map_vocab_status(profile));
    model_target_out_writef(model_target_out(), "merges_txt_status: %s\n", profile->merges_txt.status);
    model_target_out_writef(model_target_out(), "merges_status: %s\n", tokenizer_map_merges_status(profile));
    model_target_out_writef(model_target_out(), "tokenizer_model_status: %s\n", profile->tokenizer_model.status);
    model_target_out_writef(model_target_out(), "chat_template_file_status: %s\n",
           profile->chat_template_file.status);
    model_target_out_writef(model_target_out(), "tokenizer_class: %s\n", profile->tokenizer_class);
    model_target_out_writef(model_target_out(), "model_type: %s\n", profile->model_type);
    model_target_out_writef(model_target_out(), "tokenizer_model_type: %s\n", profile->model_type);
    model_target_out_writef(model_target_out(), "tokenizer_backend_type: %s\n", profile->tokenizer_backend_type);
    model_target_out_writef(model_target_out(), "vocab_size_status: %s\n", profile->vocab_size_status);
    model_target_out_writef(model_target_out(), "vocab_size: %s\n", profile->vocab_size);
    model_target_out_writef(model_target_out(), "config_vocab_size: %s\n", profile->config_vocab_size);
    model_target_out_writef(model_target_out(), "tokenizer_vocab_size: %s\n", profile->tokenizer_vocab_size);
    model_target_out_writef(model_target_out(), "output_head_vocab_dim_candidate: %s\n",
           profile->output_head_vocab_dim_candidate);
    model_target_out_writef(model_target_out(), "output_head_vocab_relation_status: %s\n",
           profile->output_head_vocab_relation_status);
    model_target_out_writef(model_target_out(), "bos_token_id_status: %s\n", profile->bos_token_id_status);
    model_target_out_writef(model_target_out(), "bos_token_id: %s\n", profile->bos_token_id);
    model_target_out_writef(model_target_out(), "bos_token_status: %s\n", profile->bos_token_status);
    model_target_out_writef(model_target_out(), "bos_token: %s\n", profile->bos_token);
    model_target_out_writef(model_target_out(), "eos_token_id_status: %s\n", profile->eos_token_id_status);
    model_target_out_writef(model_target_out(), "eos_token_id: %s\n", profile->eos_token_id);
    model_target_out_writef(model_target_out(), "eos_token_status: %s\n", profile->eos_token_status);
    model_target_out_writef(model_target_out(), "eos_token: %s\n", profile->eos_token);
    model_target_out_writef(model_target_out(), "pad_token_id_status: %s\n", profile->pad_token_id_status);
    model_target_out_writef(model_target_out(), "pad_token_id: %s\n", profile->pad_token_id);
    model_target_out_writef(model_target_out(), "pad_token_status: %s\n", profile->pad_token_status);
    model_target_out_writef(model_target_out(), "pad_token: %s\n", profile->pad_token);
    model_target_out_writef(model_target_out(), "unk_token_id_status: %s\n", profile->unk_token_id_status);
    model_target_out_writef(model_target_out(), "unk_token_id: %s\n", profile->unk_token_id);
    model_target_out_writef(model_target_out(), "unk_token_status: %s\n", profile->unk_token_status);
    model_target_out_writef(model_target_out(), "unk_token: %s\n", profile->unk_token);
    model_target_out_writef(model_target_out(), "sep_token_id_status: %s\n", profile->sep_token_id_status);
    model_target_out_writef(model_target_out(), "sep_token_id: %s\n", profile->sep_token_id);
    model_target_out_writef(model_target_out(), "sep_token_status: %s\n", profile->sep_token_status);
    model_target_out_writef(model_target_out(), "sep_token: %s\n", profile->sep_token);
    model_target_out_writef(model_target_out(), "added_tokens_status: %s\n", profile->added_tokens_status);
    model_target_out_writef(model_target_out(), "added_tokens_count: %s\n", profile->added_tokens_count);
    model_target_out_writef(model_target_out(), "special_tokens_status: %s\n", profile->special_tokens_status);
    model_target_out_writef(model_target_out(), "additional_special_tokens_status: %s\n",
           profile->additional_special_tokens_status);
    model_target_out_writef(model_target_out(), "additional_special_tokens_count: %s\n",
           profile->additional_special_tokens_count);
    model_target_out_writef(model_target_out(), "stop_token_candidate_status: %s\n",
           profile->stop_token_candidate_status);
    model_target_out_writef(model_target_out(), "stop_token_candidate_count: %s\n",
           profile->stop_token_candidate_count);
    model_target_out_writef(model_target_out(), "stop_token_candidate.0.id: %s\n",
           profile->stop_token_candidate_0_id);
    model_target_out_writef(model_target_out(), "stop_token_candidate.0.text: %s\n",
           profile->stop_token_candidate_0_text);
    model_target_out_writef(model_target_out(), "chat_template_status: %s\n", profile->chat_template_status);
    model_target_out_writef(model_target_out(), "chat_template_present: %s\n", profile->chat_template_present);
    model_target_out_writef(model_target_out(), "chat_template_source: %s\n", profile->chat_template_source);
    model_target_out_writef(model_target_out(), "chat_template_hash_status: not-computed\n");
    model_target_out_writef(model_target_out(), "prompt_template_status: %s\n", profile->prompt_template_status);
    model_target_out_writef(model_target_out(), "chat_template_runtime_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "tokenizer_runtime_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "tokenization_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "detokenization_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "gguf_tokenizer_contract_status: planned\n");
    model_target_out_writef(model_target_out(), "eos_stop_policy_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "stop_token_policy_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "prompt_template_runtime_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next_required_rows: %s\n", tokenizer_map_next_row(profile));
    model_target_out_writef(model_target_out(), "boundary: tokenizer metadata mapping only; no tokenization/runtime/generation\n");
}

static void print_tokenizer_map_json(
    const yvex_tokenizer_map_profile *profile)
{
    if (!profile) return;
    model_target_out_writef(model_target_out(), "{\"schema_version\":\"yvex.source.tokenizer_map.v1\",");
    model_target_out_writef(model_target_out(), "\"status\":");
    model_target_json_write_escaped(model_target_out(), profile->status);
    model_target_out_writef(model_target_out(), ",\"target_id\":");
    model_target_json_write_escaped(model_target_out(), profile->record->target_id);
    model_target_out_writef(model_target_out(), ",\"family\":");
    model_target_json_write_escaped(model_target_out(), profile->spec->family_key);
    model_target_out_writef(model_target_out(), ",\"source_path\":");
    model_target_json_write_escaped(model_target_out(), profile->source_path);
    model_target_out_writef(model_target_out(), ",\"tokenizer_json_status\":");
    model_target_json_write_escaped(model_target_out(), profile->tokenizer_json.status);
    model_target_out_writef(model_target_out(), ",\"vocab_status\":");
    model_target_json_write_escaped(model_target_out(), tokenizer_map_vocab_status(profile));
    model_target_out_writef(model_target_out(), ",\"merges_status\":");
    model_target_json_write_escaped(model_target_out(), tokenizer_map_merges_status(profile));
    model_target_out_writef(model_target_out(), ",\"chat_template_status\":");
    model_target_json_write_escaped(model_target_out(), profile->chat_template_status);
    model_target_out_writef(model_target_out(), ",\"special_tokens_status\":");
    model_target_json_write_escaped(model_target_out(), profile->special_tokens_status);
    model_target_out_writef(model_target_out(), ",\"tokenizer_runtime_status\":\"not-implemented\",");
    model_target_out_writef(model_target_out(), "\"generation\":\"unsupported-full-model\",");
    model_target_out_writef(model_target_out(), "\"benchmark_status\":\"not-measured\",");
    model_target_out_writef(model_target_out(), "\"next\":");
    model_target_json_write_escaped(model_target_out(), tokenizer_map_next_row(profile));
    model_target_out_writef(model_target_out(), "}\n");
}

static void print_tokenizer_map_audit_hint(const yvex_model_target_record *record)
{
    const yvex_model_class_profile_spec *spec;

    if (!record) return;
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) return;
    model_target_out_writef(model_target_out(), "tokenizer_map_status: not-run\n");
    model_target_out_writef(model_target_out(), "tokenizer_map_family: %s\n", spec->family_key);
    model_target_out_writef(model_target_out(), "tokenizer_map_target_id: %s\n", spec->target_id);
    model_target_out_writef(model_target_out(), "tokenizer_map_stage: metadata-tokenizer-map\n");
    model_target_out_writef(model_target_out(), "tokenizer_map_evidence_basis: sidecar-metadata-only\n");
    model_target_out_writef(model_target_out(), "tokenizer_runtime_status: not-implemented\n");
    model_target_out_writef(model_target_out(), "tokenizer_map_next: %s\n", YVEX_TOKENIZER_MISSING_NEXT_ROW);
}

static const char *missing_role_many_status(unsigned long long count)
{
    return count > 0 ? "present" : "missing";
}

static const char *missing_role_singleton_status(unsigned long long count)
{
    if (count == 0) return "missing";
    if (count > 1) return "ambiguous";
    return "present";
}

static unsigned long long missing_role_count_canonical(
    const yvex_tensor_naming_profile *profile,
    const char *needle)
{
    unsigned long long i;
    unsigned long long count = 0;

    if (!profile || !needle) return 0;
    for (i = 0; i < profile->entry_count; ++i) {
        const yvex_tensor_naming_entry *entry = &profile->entries[i];
        if (strcmp(entry->mapping_status, "mapped-candidate") == 0 &&
            strstr(entry->canonical_role, needle)) {
            count++;
        }
    }
    return count;
}

static void missing_role_tally(const char *status,
                               unsigned long long *observed,
                               unsigned long long *missing,
                               unsigned long long *ambiguous)
{
    if (!status) return;
    if (strcmp(status, "present") == 0) {
        if (observed) (*observed)++;
    } else if (strcmp(status, "ambiguous") == 0) {
        if (ambiguous) (*ambiguous)++;
    } else {
        if (missing) (*missing)++;
    }
}

static const char *missing_role_metadata_status(
    const yvex_tokenizer_map_sidecar *sidecar)
{
    return tokenizer_map_sidecar_present(sidecar) ? "present" : "missing";
}

static unsigned int missing_role_source_required_count(
    const yvex_missing_role_report_profile *profile)
{
    return 12u + (profile && profile->qwen_extra_required ? 4u : 0u);
}

static void missing_role_add_entry(yvex_missing_role_report_profile *profile,
                                   const char *name,
                                   const char *status,
                                   const char *blocker_class)
{
    yvex_missing_role_entry *entry;

    if (!profile || !name || !status || strcmp(status, "present") == 0) return;
    if (profile->missing_entry_count >=
        sizeof(profile->missing_entries) / sizeof(profile->missing_entries[0])) {
        return;
    }
    entry = &profile->missing_entries[profile->missing_entry_count++];
    entry->name = name;
    entry->status = status;
    entry->blocker_class = blocker_class;
}

static const char *missing_role_first_missing_source(
    const yvex_missing_role_report_profile *profile)
{
    if (!profile) return "unknown";
    if (strcmp(profile->embedding_status, "missing") == 0) return "embedding";
    if (strcmp(profile->attention_norm_status, "missing") == 0) return "attention-norm";
    if (strcmp(profile->attention_q_status, "missing") == 0) return "attention-q";
    if (strcmp(profile->attention_k_status, "missing") == 0) return "attention-k";
    if (strcmp(profile->attention_v_status, "missing") == 0) return "attention-v";
    if (strcmp(profile->attention_o_status, "missing") == 0) return "attention-o";
    if (strcmp(profile->mlp_norm_status, "missing") == 0) return "mlp-norm";
    if (strcmp(profile->mlp_gate_status, "missing") == 0) return "mlp-gate";
    if (strcmp(profile->mlp_up_status, "missing") == 0) return "mlp-up";
    if (strcmp(profile->mlp_down_status, "missing") == 0) return "mlp-down";
    if (strcmp(profile->final_norm_status, "missing") == 0) return "final-norm";
    if (profile->qwen_extra_required &&
        strcmp(profile->qwen_linear_attn_status, "missing") == 0) {
        return "qwen-linear-attn";
    }
    if (profile->qwen_extra_required &&
        strcmp(profile->moe_router_status, "missing") == 0) return "moe-router";
    if (profile->qwen_extra_required &&
        strcmp(profile->moe_expert_status, "missing") == 0) return "moe-experts";
    if (profile->qwen_extra_required &&
        strcmp(profile->moe_shared_status, "missing") == 0) return "shared-expert";
    if (strcmp(profile->output_head_status, "missing") == 0) return "output-head";
    return "unknown";
}

static const char *missing_role_first_ambiguous_source(
    const yvex_missing_role_report_profile *profile)
{
    if (!profile) return "unknown";
    if (strcmp(profile->embedding_status, "ambiguous") == 0) return "embedding";
    if (strcmp(profile->final_norm_status, "ambiguous") == 0) return "final-norm";
    if (strcmp(profile->output_head_status, "ambiguous") == 0) return "output-head";
    return "unknown";
}

static int build_missing_role_report_profile(
    const yvex_model_target_record *record,
    const yvex_model_class_profile_spec *spec_override,
    const char *models_root_override,
    const char *source_override,
    yvex_missing_role_report_profile *profile)
{
    yvex_tensor_naming_profile naming_profile;
    yvex_output_head_map_profile output_profile;
    yvex_tokenizer_map_profile tokenizer_profile;
    const char *first_source;
    int rc;

    if (!record || !profile) return 2;
    memset(profile, 0, sizeof(*profile));
    profile->record = record;
    profile->spec = spec_override ? spec_override :
                    find_model_class_profile_spec(record->target_id);
    if (!profile->spec) return 2;
    profile->status = "source-missing";
    profile->top_blocker = profile->spec->missing_source_blocker;

    rc = build_tensor_naming_profile(record, profile->spec,
                                     models_root_override, source_override,
                                     &naming_profile);
    if (rc != 0) return rc;
    rc = build_output_head_map_profile(record, profile->spec,
                                       models_root_override, source_override,
                                       &output_profile);
    if (rc != 0) return rc;
    rc = build_tokenizer_map_profile(record, profile->spec, models_root_override,
                                     source_override, &tokenizer_profile);
    if (rc != 0) return rc;

    snprintf(profile->source_path, sizeof(profile->source_path), "%s",
             naming_profile.source_path);
    snprintf(profile->source_path_source, sizeof(profile->source_path_source),
             "%s", naming_profile.source_path_source);
    profile->source_exists = naming_profile.source_exists;
    profile->tensor_count = naming_profile.tensor_count;
    profile->mapped_total_count = naming_profile.mapped_total_count;
    profile->unmapped_unknown_count = naming_profile.unmapped_unknown_count;
    profile->layer_count_observed = naming_profile.layer_count_observed;
    profile->embedding_count = naming_profile.embedding_count;
    profile->attention_count = naming_profile.attention_count;
    profile->mlp_count = naming_profile.mlp_count;
    profile->norm_count = naming_profile.norm_count;
    profile->output_head_count = output_profile.output_head_candidate_count;
    profile->qwen_linear_attn_count = naming_profile.qwen_linear_attn_count;
    profile->moe_router_count = naming_profile.moe_router_count;
    profile->moe_expert_count = naming_profile.moe_expert_count;
    profile->moe_shared_count = naming_profile.moe_shared_count;
    profile->qwen_extra_required =
        tensor_naming_requires_qwen_extra_roles(&naming_profile);

    if (!profile->source_exists) {
        profile->embedding_status = "missing";
        profile->attention_norm_status = "missing";
        profile->attention_q_status = "missing";
        profile->attention_k_status = "missing";
        profile->attention_v_status = "missing";
        profile->attention_o_status = "missing";
        profile->mlp_norm_status = "missing";
        profile->mlp_gate_status = "missing";
        profile->mlp_up_status = "missing";
        profile->mlp_down_status = "missing";
        profile->final_norm_status = "missing";
        profile->output_head_status = "missing";
        profile->tied_head_policy_status = "unknown";
        profile->qwen_linear_attn_status =
            profile->qwen_extra_required ? "missing" : "not-required";
        profile->moe_router_status =
            profile->qwen_extra_required ? "missing" : "not-required";
        profile->moe_expert_status =
            profile->qwen_extra_required ? "missing" : "not-required";
        profile->moe_shared_status =
            profile->qwen_extra_required ? "missing" : "not-required";
        profile->unknown_tensor_status = "none";
        profile->tokenizer_metadata_status = "missing";
        profile->config_metadata_status = "missing";
        profile->generation_metadata_status = "missing";
        profile->special_tokens_status = "missing";
    } else {
        profile->embedding_status =
            missing_role_singleton_status(naming_profile.embedding_count);
        profile->attention_norm_status =
            missing_role_many_status(missing_role_count_canonical(
                &naming_profile, ".attention.norm.weight"));
        profile->attention_q_status =
            missing_role_many_status(naming_profile.attention_q_count);
        profile->attention_k_status =
            missing_role_many_status(naming_profile.attention_k_count);
        profile->attention_v_status =
            missing_role_many_status(naming_profile.attention_v_count);
        profile->attention_o_status =
            missing_role_many_status(naming_profile.attention_o_count);
        profile->mlp_norm_status =
            missing_role_many_status(missing_role_count_canonical(
                &naming_profile, ".mlp.norm.weight"));
        profile->mlp_gate_status =
            missing_role_many_status(naming_profile.mlp_gate_count);
        profile->mlp_up_status =
            missing_role_many_status(naming_profile.mlp_up_count);
        profile->mlp_down_status =
            missing_role_many_status(naming_profile.mlp_down_count);
        profile->final_norm_status =
            missing_role_singleton_status(missing_role_count_canonical(
                &naming_profile, "model.final_norm.weight"));
        profile->output_head_status =
            output_profile.output_head_ambiguous_count > 0
                ? "ambiguous"
                : (output_profile.output_head.present ? "present" : "missing");
        profile->tied_head_policy_status = output_profile.tie_policy_status;
        profile->qwen_linear_attn_status =
            profile->qwen_extra_required
                ? missing_role_many_status(naming_profile.qwen_linear_attn_count)
                : (naming_profile.qwen_linear_attn_count ? "present" : "not-required");
        profile->moe_router_status =
            profile->qwen_extra_required
                ? missing_role_many_status(naming_profile.moe_router_count)
                : (naming_profile.moe_router_count ? "present" : "not-required");
        profile->moe_expert_status =
            profile->qwen_extra_required
                ? missing_role_many_status(naming_profile.moe_expert_count)
                : (naming_profile.moe_expert_count ? "present" : "not-required");
        profile->moe_shared_status =
            profile->qwen_extra_required
                ? missing_role_many_status(naming_profile.moe_shared_count)
                : (naming_profile.moe_shared_count ? "present" : "not-required");
        profile->unknown_tensor_status =
            naming_profile.unmapped_unknown_count > 0
                ? "unclassified-header-name"
                : "none";
        profile->tokenizer_metadata_status =
            missing_role_metadata_status(&tokenizer_profile.tokenizer_json);
        profile->config_metadata_status =
            missing_role_metadata_status(&tokenizer_profile.config_json);
        profile->generation_metadata_status =
            missing_role_metadata_status(&tokenizer_profile.generation_config);
        profile->special_tokens_status =
            missing_role_metadata_status(&tokenizer_profile.special_tokens_map);
        if (strcmp(tokenizer_profile.status, "tokenizer-metadata-ambiguous") == 0) {
            profile->tokenizer_metadata_status = "ambiguous";
        }
    }

    missing_role_tally(profile->embedding_status,
                       &profile->source_role_observed_count,
                       &profile->source_role_missing_count,
                       &profile->source_role_ambiguous_count);
    missing_role_tally(profile->attention_norm_status,
                       &profile->source_role_observed_count,
                       &profile->source_role_missing_count,
                       &profile->source_role_ambiguous_count);
    missing_role_tally(profile->attention_q_status,
                       &profile->source_role_observed_count,
                       &profile->source_role_missing_count,
                       &profile->source_role_ambiguous_count);
    missing_role_tally(profile->attention_k_status,
                       &profile->source_role_observed_count,
                       &profile->source_role_missing_count,
                       &profile->source_role_ambiguous_count);
    missing_role_tally(profile->attention_v_status,
                       &profile->source_role_observed_count,
                       &profile->source_role_missing_count,
                       &profile->source_role_ambiguous_count);
    missing_role_tally(profile->attention_o_status,
                       &profile->source_role_observed_count,
                       &profile->source_role_missing_count,
                       &profile->source_role_ambiguous_count);
    missing_role_tally(profile->mlp_norm_status,
                       &profile->source_role_observed_count,
                       &profile->source_role_missing_count,
                       &profile->source_role_ambiguous_count);
    missing_role_tally(profile->mlp_gate_status,
                       &profile->source_role_observed_count,
                       &profile->source_role_missing_count,
                       &profile->source_role_ambiguous_count);
    missing_role_tally(profile->mlp_up_status,
                       &profile->source_role_observed_count,
                       &profile->source_role_missing_count,
                       &profile->source_role_ambiguous_count);
    missing_role_tally(profile->mlp_down_status,
                       &profile->source_role_observed_count,
                       &profile->source_role_missing_count,
                       &profile->source_role_ambiguous_count);
    missing_role_tally(profile->final_norm_status,
                       &profile->source_role_observed_count,
                       &profile->source_role_missing_count,
                       &profile->source_role_ambiguous_count);
    if (profile->qwen_extra_required) {
        missing_role_tally(profile->qwen_linear_attn_status,
                           &profile->source_role_observed_count,
                           &profile->source_role_missing_count,
                           &profile->source_role_ambiguous_count);
        missing_role_tally(profile->moe_router_status,
                           &profile->source_role_observed_count,
                           &profile->source_role_missing_count,
                           &profile->source_role_ambiguous_count);
        missing_role_tally(profile->moe_expert_status,
                           &profile->source_role_observed_count,
                           &profile->source_role_missing_count,
                           &profile->source_role_ambiguous_count);
        missing_role_tally(profile->moe_shared_status,
                           &profile->source_role_observed_count,
                           &profile->source_role_missing_count,
                           &profile->source_role_ambiguous_count);
    }
    missing_role_tally(profile->output_head_status,
                       &profile->source_role_observed_count,
                       &profile->source_role_missing_count,
                       &profile->source_role_ambiguous_count);
    missing_role_tally(profile->tokenizer_metadata_status,
                       &profile->metadata_observed_count,
                       &profile->metadata_missing_count,
                       &profile->metadata_ambiguous_count);
    missing_role_tally(profile->config_metadata_status,
                       &profile->metadata_observed_count,
                       &profile->metadata_missing_count,
                       &profile->metadata_ambiguous_count);
    missing_role_tally(profile->generation_metadata_status,
                       &profile->metadata_observed_count,
                       &profile->metadata_missing_count,
                       &profile->metadata_ambiguous_count);
    missing_role_tally(profile->special_tokens_status,
                       &profile->metadata_observed_count,
                       &profile->metadata_missing_count,
                       &profile->metadata_ambiguous_count);

    missing_role_add_entry(profile, "embedding", profile->embedding_status,
                           strcmp(profile->embedding_status, "ambiguous") == 0
                               ? "source-role-ambiguous" : "source-role-missing");
    missing_role_add_entry(profile, "attention_norm",
                           profile->attention_norm_status,
                           "source-role-missing");
    missing_role_add_entry(profile, "attention_q", profile->attention_q_status,
                           "source-role-missing");
    missing_role_add_entry(profile, "attention_k", profile->attention_k_status,
                           "source-role-missing");
    missing_role_add_entry(profile, "attention_v", profile->attention_v_status,
                           "source-role-missing");
    missing_role_add_entry(profile, "attention_o", profile->attention_o_status,
                           "source-role-missing");
    missing_role_add_entry(profile, "mlp_norm", profile->mlp_norm_status,
                           "source-role-missing");
    missing_role_add_entry(profile, "mlp_gate", profile->mlp_gate_status,
                           "source-role-missing");
    missing_role_add_entry(profile, "mlp_up", profile->mlp_up_status,
                           "source-role-missing");
    missing_role_add_entry(profile, "mlp_down", profile->mlp_down_status,
                           "source-role-missing");
    missing_role_add_entry(profile, "final_norm", profile->final_norm_status,
                           strcmp(profile->final_norm_status, "ambiguous") == 0
                               ? "source-role-ambiguous" : "source-role-missing");
    if (profile->qwen_extra_required || profile->qwen_linear_attn_count > 0) {
        missing_role_add_entry(profile, "qwen_linear_attn",
                               profile->qwen_linear_attn_status,
                               "source-role-missing");
    }
    if (profile->qwen_extra_required || profile->moe_router_count > 0) {
        missing_role_add_entry(profile, "moe_router",
                               profile->moe_router_status,
                               "source-role-missing");
    }
    if (profile->qwen_extra_required || profile->moe_expert_count > 0) {
        missing_role_add_entry(profile, "moe_experts",
                               profile->moe_expert_status,
                               "source-role-missing");
    }
    if (profile->qwen_extra_required || profile->moe_shared_count > 0) {
        missing_role_add_entry(profile, "shared_expert",
                               profile->moe_shared_status,
                               "source-role-missing");
    }
    missing_role_add_entry(profile, "output_head", profile->output_head_status,
                           strcmp(profile->output_head_status, "ambiguous") == 0
                               ? "source-role-ambiguous" : "source-role-missing");
    if (strcmp(profile->spec->family_key, "gemma") == 0 &&
        strcmp(profile->output_head_status, "present") != 0) {
        missing_role_add_entry(profile, "tied_head_policy",
                               profile->tied_head_policy_status,
                               "source-role-missing");
    }
    if (profile->unmapped_unknown_count > 0) {
        missing_role_add_entry(profile, "unknown_tensors",
                               profile->unknown_tensor_status,
                               "unclassified-header-name");
    }
    missing_role_add_entry(profile, "tokenizer_metadata",
                           profile->tokenizer_metadata_status,
                           strcmp(profile->tokenizer_metadata_status, "ambiguous") == 0
                               ? "metadata-ambiguous" : "metadata-incomplete");
    missing_role_add_entry(profile, "config_metadata",
                           profile->config_metadata_status,
                           "metadata-incomplete");
    missing_role_add_entry(profile, "generation_metadata",
                           profile->generation_metadata_status,
                           "metadata-incomplete");
    missing_role_add_entry(profile, "special_tokens",
                           profile->special_tokens_status,
                           "metadata-incomplete");

    if (!profile->source_exists) {
        profile->status = "source-missing";
        profile->top_blocker = profile->spec->missing_source_blocker;
    } else if (profile->source_role_ambiguous_count > 0) {
        first_source = missing_role_first_ambiguous_source(profile);
        snprintf(profile->top_blocker_storage,
                 sizeof(profile->top_blocker_storage),
                 "ambiguous-source-role-%s", first_source);
        profile->status = "missing-role-report-ambiguous";
        profile->top_blocker = profile->top_blocker_storage;
    } else if (profile->metadata_ambiguous_count > 0) {
        profile->status = "missing-role-report-ambiguous";
        profile->top_blocker = "ambiguous-tokenizer-metadata";
    } else if (profile->source_role_missing_count > 0) {
        first_source = missing_role_first_missing_source(profile);
        snprintf(profile->top_blocker_storage,
                 sizeof(profile->top_blocker_storage),
                 "missing-source-role-%s", first_source);
        profile->status = "missing-role-report-incomplete";
        profile->top_blocker = profile->top_blocker_storage;
    } else if (profile->metadata_missing_count > 0) {
        profile->status = "missing-role-report-incomplete";
        profile->top_blocker = "missing-tokenizer-metadata";
    } else {
        profile->status = "missing-role-report-blocked";
        profile->top_blocker = "missing-artifact-contract";
    }

    return 0;
}

static void print_missing_role_list(const char *label,
                                    const yvex_missing_role_report_profile *profile,
                                    int metadata)
{
    unsigned long long i;
    int first = 1;

    if (!profile) return;
    model_target_out_writef(model_target_out(), "%s: ", label);
    for (i = 0; i < profile->missing_entry_count; ++i) {
        const yvex_missing_role_entry *entry = &profile->missing_entries[i];
        int is_metadata = strstr(entry->blocker_class, "metadata") != NULL;
        if (metadata != is_metadata || strcmp(entry->status, "missing") != 0) {
            continue;
        }
        model_target_out_writef(model_target_out(), "%s%s", first ? "" : ",", entry->name);
        first = 0;
    }
    if (first) model_target_out_writef(model_target_out(), "none");
    model_target_out_writef(model_target_out(), "\n");
}

static void print_missing_role_report_normal(
    const yvex_missing_role_report_profile *profile)
{
    model_target_out_writef(model_target_out(), "missing-roles: %s [%s]\n",
           profile->record->target_id,
           compact_status_bracket(profile->status));
    model_target_out_writef(model_target_out(), "family: %s  evidence: header+sidecar-only\n",
           profile->spec->family_key);
    model_target_out_writef(model_target_out(), "source_roles: %llu/%u present, %llu missing, %llu ambiguous\n",
           profile->source_role_observed_count,
           missing_role_source_required_count(profile),
           profile->source_role_missing_count,
           profile->source_role_ambiguous_count);
    model_target_out_writef(model_target_out(), "metadata_roles: %llu/4 present, %llu missing, %llu ambiguous\n",
           profile->metadata_observed_count,
           profile->metadata_missing_count,
           profile->metadata_ambiguous_count);
    if (profile->source_role_missing_count > 0) {
        print_missing_role_list("missing_source", profile, 0);
    }
    if (profile->metadata_missing_count > 0) {
        print_missing_role_list("missing_metadata", profile, 1);
    }
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next: %s\n", YVEX_MISSING_ROLE_REPORT_NEXT_ROW);
    model_target_out_writef(model_target_out(), "boundary: report-only; use --audit for role details\n");
}

static void print_missing_role_report_table(
    const yvex_missing_role_report_profile *profile)
{
    model_target_out_writef(model_target_out(), "MISSING ROLE BLOCKER REPORT\n\n");
    model_target_out_writef(model_target_out(), "%-6s  %-15s  %-29s  %7s  %8s  %9s  %8s  %9s  %-30s  %s\n",
           "FAMILY", "TARGET", "STATUS", "OBS_SRC", "MISS_SRC",
           "AMBIG_SRC", "OBS_META", "MISS_META", "TOP_BLOCKER", "NEXT");
    model_target_out_writef(model_target_out(), "%-6s  %-15s  %-29s  %7llu  %8llu  %9llu  %8llu  %9llu  %-30s  %s\n",
           profile->spec->family_key,
           profile->record->target_id,
           profile->status,
           profile->source_role_observed_count,
           profile->source_role_missing_count,
           profile->source_role_ambiguous_count,
           profile->metadata_observed_count,
           profile->metadata_missing_count,
           profile->top_blocker,
           YVEX_MISSING_ROLE_REPORT_NEXT_ROW);
}

static void print_missing_role_report_audit(
    const yvex_missing_role_report_profile *profile)
{
    unsigned long long i;

    model_target_out_writef(model_target_out(), "missing_role_report_status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "missing_role_report_family: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "missing_role_report_target_id: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "missing_role_report_stage: missing-role-blocker-report\n");
    model_target_out_writef(model_target_out(), "missing_role_report_evidence_basis: header-and-sidecar-metadata-only\n");
    model_target_out_writef(model_target_out(), "missing_role_report_source_status: %s\n",
           profile->source_exists ? "present" : "missing");
    model_target_out_writef(model_target_out(), "missing_role_report_source_path: %s\n", profile->source_path);
    model_target_out_writef(model_target_out(), "missing_role_source_role_required_count: %u\n",
           missing_role_source_required_count(profile));
    model_target_out_writef(model_target_out(), "missing_role_source_role_observed_count: %llu\n",
           profile->source_role_observed_count);
    model_target_out_writef(model_target_out(), "missing_role_source_role_missing_count: %llu\n",
           profile->source_role_missing_count);
    model_target_out_writef(model_target_out(), "missing_role_source_role_ambiguous_count: %llu\n",
           profile->source_role_ambiguous_count);
    model_target_out_writef(model_target_out(), "missing_role_metadata_required_count: 4\n");
    model_target_out_writef(model_target_out(), "missing_role_metadata_observed_count: %llu\n",
           profile->metadata_observed_count);
    model_target_out_writef(model_target_out(), "missing_role_metadata_missing_count: %llu\n",
           profile->metadata_missing_count);
    model_target_out_writef(model_target_out(), "missing_role_metadata_ambiguous_count: %llu\n",
           profile->metadata_ambiguous_count);
    model_target_out_writef(model_target_out(), "missing_role_embedding_status: %s\n", profile->embedding_status);
    model_target_out_writef(model_target_out(), "missing_role_attention_norm_status: %s\n",
           profile->attention_norm_status);
    model_target_out_writef(model_target_out(), "missing_role_attention_q_status: %s\n",
           profile->attention_q_status);
    model_target_out_writef(model_target_out(), "missing_role_attention_k_status: %s\n",
           profile->attention_k_status);
    model_target_out_writef(model_target_out(), "missing_role_attention_v_status: %s\n",
           profile->attention_v_status);
    model_target_out_writef(model_target_out(), "missing_role_attention_o_status: %s\n",
           profile->attention_o_status);
    model_target_out_writef(model_target_out(), "missing_role_mlp_norm_status: %s\n", profile->mlp_norm_status);
    model_target_out_writef(model_target_out(), "missing_role_mlp_gate_status: %s\n", profile->mlp_gate_status);
    model_target_out_writef(model_target_out(), "missing_role_mlp_up_status: %s\n", profile->mlp_up_status);
    model_target_out_writef(model_target_out(), "missing_role_mlp_down_status: %s\n", profile->mlp_down_status);
    model_target_out_writef(model_target_out(), "missing_role_final_norm_status: %s\n", profile->final_norm_status);
    model_target_out_writef(model_target_out(), "missing_role_output_head_status: %s\n",
           profile->output_head_status);
    model_target_out_writef(model_target_out(), "missing_role_tied_head_policy_status: %s\n",
           profile->tied_head_policy_status);
    model_target_out_writef(model_target_out(), "missing_role_qwen_linear_attn_status: %s\n",
           profile->qwen_linear_attn_status);
    model_target_out_writef(model_target_out(), "missing_role_qwen_linear_attn_count: %llu\n",
           profile->qwen_linear_attn_count);
    model_target_out_writef(model_target_out(), "missing_role_moe_router_status: %s\n",
           profile->moe_router_status);
    model_target_out_writef(model_target_out(), "missing_role_moe_router_count: %llu\n",
           profile->moe_router_count);
    model_target_out_writef(model_target_out(), "missing_role_moe_expert_status: %s\n",
           profile->moe_expert_status);
    model_target_out_writef(model_target_out(), "missing_role_moe_expert_count: %llu\n",
           profile->moe_expert_count);
    model_target_out_writef(model_target_out(), "missing_role_moe_shared_status: %s\n",
           profile->moe_shared_status);
    model_target_out_writef(model_target_out(), "missing_role_moe_shared_count: %llu\n",
           profile->moe_shared_count);
    model_target_out_writef(model_target_out(), "missing_role_unknown_tensor_status: %s\n",
           profile->unknown_tensor_status);
    model_target_out_writef(model_target_out(), "missing_role_unknown_tensor_count: %llu\n",
           profile->unmapped_unknown_count);
    model_target_out_writef(model_target_out(), "missing_role_tokenizer_metadata_status: %s\n",
           profile->tokenizer_metadata_status);
    model_target_out_writef(model_target_out(), "missing_role_config_metadata_status: %s\n",
           profile->config_metadata_status);
    model_target_out_writef(model_target_out(), "missing_role_generation_metadata_status: %s\n",
           profile->generation_metadata_status);
    model_target_out_writef(model_target_out(), "missing_role_special_tokens_status: %s\n",
           profile->special_tokens_status);
    model_target_out_writef(model_target_out(), "missing_role_artifact_contract_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_runtime_descriptor_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_graph_consumer_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_backend_residency_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_attention_runtime_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_kv_runtime_state_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_prefill_runtime_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_decode_runtime_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_logits_runtime_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_tokenizer_runtime_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_sampling_runtime_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_generation_runtime_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_eval_benchmark_status: missing\n");
    model_target_out_writef(model_target_out(), "missing_role_top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "missing_role_next_required_row: %s\n",
           YVEX_MISSING_ROLE_REPORT_NEXT_ROW);
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    for (i = 0; i < profile->missing_entry_count; ++i) {
        const yvex_missing_role_entry *entry = &profile->missing_entries[i];
        model_target_out_writef(model_target_out(), "missing_role.entry.%llu.role: %s\n", i, entry->name);
        model_target_out_writef(model_target_out(), "missing_role.entry.%llu.blocker_class: %s\n",
               i, entry->blocker_class);
        model_target_out_writef(model_target_out(), "missing_role.entry.%llu.status: %s\n", i, entry->status);
    }
    model_target_out_writef(model_target_out(), "boundary: missing-role report only; no artifact/runtime descriptor/graph/runtime/generation\n");
}

static void print_missing_role_report_audit_hint(
    const yvex_model_target_record *record)
{
    const yvex_model_class_profile_spec *spec;

    if (!record) return;
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) return;
    model_target_out_writef(model_target_out(), "missing_role_report_status: not-run\n");
    model_target_out_writef(model_target_out(), "missing_role_report_family: %s\n", spec->family_key);
    model_target_out_writef(model_target_out(), "missing_role_report_target_id: %s\n", spec->target_id);
    model_target_out_writef(model_target_out(), "missing_role_report_stage: missing-role-blocker-report\n");
    model_target_out_writef(model_target_out(), "missing_role_top_blocker: not-run\n");
    model_target_out_writef(model_target_out(), "missing_role_next_required_row: %s\n",
           YVEX_MISSING_ROLE_REPORT_NEXT_ROW);
}

typedef struct {
    char expected_artifact_path[YVEX_PATH_CAP];
    char tensor_map_path[YVEX_PATH_CAP];
    char output_head_map_path[YVEX_PATH_CAP];
    char tokenizer_map_path[YVEX_PATH_CAP];
    char top_blocker[96];
    char tensor_map_status[64];
    char output_head_map_status[64];
    char tokenizer_map_status[64];
    char artifact_status[32];
    char artifact_identity_status[32];
    unsigned int blocker_count;
} yvex_missing_roles_porcelain_context;

static int missing_roles_has_incomplete_tensor_map(
    const yvex_missing_role_report_profile *profile)
{
    if (!profile || !profile->source_exists) return 1;
    if (strcmp(profile->embedding_status, "present") != 0 ||
        strcmp(profile->attention_norm_status, "present") != 0 ||
        strcmp(profile->attention_q_status, "present") != 0 ||
        strcmp(profile->attention_k_status, "present") != 0 ||
        strcmp(profile->attention_v_status, "present") != 0 ||
        strcmp(profile->attention_o_status, "present") != 0 ||
        strcmp(profile->mlp_norm_status, "present") != 0 ||
        strcmp(profile->mlp_gate_status, "present") != 0 ||
        strcmp(profile->mlp_up_status, "present") != 0 ||
        strcmp(profile->mlp_down_status, "present") != 0 ||
        strcmp(profile->final_norm_status, "present") != 0) {
        return 1;
    }
    if (profile->qwen_extra_required &&
        (strcmp(profile->qwen_linear_attn_status, "present") != 0 ||
         strcmp(profile->moe_router_status, "present") != 0 ||
         strcmp(profile->moe_expert_status, "present") != 0 ||
         strcmp(profile->moe_shared_status, "present") != 0)) {
        return 1;
    }
    return 0;
}

static void build_missing_roles_porcelain_context(
    const yvex_operator_paths *operator_paths,
    const yvex_missing_role_report_profile *profile,
    yvex_missing_roles_porcelain_context *ctx)
{
    const char *family;
    const char *target;
    int tensor_map_present;
    int output_head_present;
    int tokenizer_present;
    int artifact_present;
    int incomplete_map;
    int missing_output_head;

    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    snprintf(ctx->top_blocker, sizeof(ctx->top_blocker), "unknown");
    snprintf(ctx->tensor_map_status, sizeof(ctx->tensor_map_status), "missing");
    snprintf(ctx->output_head_map_status, sizeof(ctx->output_head_map_status), "missing");
    snprintf(ctx->tokenizer_map_status, sizeof(ctx->tokenizer_map_status), "missing");
    snprintf(ctx->artifact_status, sizeof(ctx->artifact_status), "missing");
    snprintf(ctx->artifact_identity_status, sizeof(ctx->artifact_identity_status), "missing");
    if (!operator_paths || !profile || !profile->record || !profile->spec) return;

    family = profile->spec->family_key;
    target = profile->record->target_id;
    (void)model_target_path_format(ctx->expected_artifact_path,
                                   sizeof(ctx->expected_artifact_path),
                                   "%s/%s/%s.gguf",
                                   operator_paths->gguf_root,
                                   family,
                                   target);
    (void)model_target_path_format(ctx->tensor_map_path,
                                   sizeof(ctx->tensor_map_path),
                                   "%s/%s/%s.tensor-map.json",
                                   operator_paths->reports_root,
                                   family,
                                   target);
    (void)model_target_path_format(ctx->output_head_map_path,
                                   sizeof(ctx->output_head_map_path),
                                   "%s/%s/%s.output-head-map.json",
                                   operator_paths->reports_root,
                                   family,
                                   target);
    (void)model_target_path_format(ctx->tokenizer_map_path,
                                   sizeof(ctx->tokenizer_map_path),
                                   "%s/%s/%s.tokenizer-map.json",
                                   operator_paths->reports_root,
                                   family,
                                   target);

    tensor_map_present = model_target_file_exists(ctx->tensor_map_path);
    output_head_present = model_target_file_exists(ctx->output_head_map_path);
    tokenizer_present = model_target_file_exists(ctx->tokenizer_map_path);
    artifact_present = model_target_file_exists(ctx->expected_artifact_path);
    incomplete_map = missing_roles_has_incomplete_tensor_map(profile);
    missing_output_head = strcmp(profile->output_head_status, "present") != 0;

    snprintf(ctx->tensor_map_status, sizeof(ctx->tensor_map_status), "%s",
             tensor_map_present
                 ? (incomplete_map ? "incomplete-report-only" : "present-report-only")
                 : (profile->source_exists ? "not-written" : "missing"));
    snprintf(ctx->output_head_map_status, sizeof(ctx->output_head_map_status), "%s",
             output_head_present
                 ? (missing_output_head ? "missing-in-report" : "present-report-only")
                 : (missing_output_head ? "missing" : "not-written"));
    snprintf(ctx->tokenizer_map_status, sizeof(ctx->tokenizer_map_status), "%s",
             tokenizer_present ? "present-report-only" : "missing");
    snprintf(ctx->artifact_status, sizeof(ctx->artifact_status), "%s",
             artifact_present ? "present" : "missing");
    snprintf(ctx->artifact_identity_status, sizeof(ctx->artifact_identity_status), "%s",
             artifact_present ? "not-checked" : "missing");

    if (!profile->source_exists) ctx->blocker_count++;
    if (incomplete_map) ctx->blocker_count++;
    if (missing_output_head) ctx->blocker_count++;
    if (!tokenizer_present) ctx->blocker_count++;
    if (!artifact_present) ctx->blocker_count++;
    ctx->blocker_count++; /* full GGUF emitter/identity remains unsupported. */

    if (!profile->source_exists) {
        snprintf(ctx->top_blocker, sizeof(ctx->top_blocker), "%s",
                 profile->spec->missing_source_blocker);
    } else if (missing_output_head) {
        snprintf(ctx->top_blocker, sizeof(ctx->top_blocker),
                 "missing-output-head-map");
    } else if (incomplete_map) {
        snprintf(ctx->top_blocker, sizeof(ctx->top_blocker),
                 "incomplete-tensor-map");
    } else if (!tokenizer_present) {
        snprintf(ctx->top_blocker, sizeof(ctx->top_blocker),
                 "missing-tokenizer-map");
    } else if (!artifact_present) {
        snprintf(ctx->top_blocker, sizeof(ctx->top_blocker),
                 "quant-policy-or-artifact-emitter");
    } else {
        snprintf(ctx->top_blocker, sizeof(ctx->top_blocker),
                 "missing-artifact-identity");
    }
}

static const char *missing_roles_porcelain_next_row(
    const yvex_missing_roles_porcelain_context *ctx)
{
    if (!ctx) return YVEX_MISSING_ROLES_PORCELAIN_NEXT_ROW;
    if (strcmp(ctx->top_blocker, "missing-tokenizer-map") == 0) {
        return YVEX_TOKENIZER_MISSING_NEXT_ROW;
    }
    if (strcmp(ctx->top_blocker, "quant-policy-or-artifact-emitter") == 0) {
        return YVEX_TOKENIZER_MAP_NEXT_ROW;
    }
    return YVEX_MISSING_ROLES_PORCELAIN_NEXT_ROW;
}

static const char *missing_roles_group_status(unsigned long long count)
{
    return count > 0 ? "present" : "missing";
}

static void print_missing_roles_row(const char *group,
                                    const char *status,
                                    unsigned long long found,
                                    const char *detail)
{
    model_target_out_writef(model_target_out(), "%-16s  %-10s  %5llu  %s\n",
           group,
           status ? status : "missing",
           found,
           detail ? detail : "");
}

static void print_missing_roles_porcelain_normal(
    const yvex_missing_role_report_profile *profile,
    const yvex_missing_roles_porcelain_context *ctx)
{
    int is_gemma;

    if (!profile || !ctx) return;
    is_gemma = strcmp(profile->spec->family_key, "gemma") == 0;
    model_target_out_writef(model_target_out(), "missing-roles: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "family: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "status: blocked\n");
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", ctx->top_blocker);
    model_target_out_writef(model_target_out(), "next: %s\n\n", missing_roles_porcelain_next_row(ctx));
    model_target_out_writef(model_target_out(), "%-16s  %-10s  %5s  %s\n",
           "ROLE GROUP", "STATUS", "FOUND", "DETAIL");
    if (is_gemma) {
        print_missing_roles_row("embedding",
                                profile->embedding_count ? "present" : "missing",
                                profile->embedding_count,
                                profile->embedding_count
                                    ? "token embedding mapped report-only"
                                    : "token embedding not mapped");
        print_missing_roles_row("attention",
                                missing_roles_group_status(profile->attention_count),
                                profile->attention_count,
                                "attention roles mapped report-only");
        print_missing_roles_row("dense-mlp",
                                missing_roles_group_status(profile->mlp_count),
                                profile->mlp_count,
                                "dense MLP roles mapped report-only");
        print_missing_roles_row("norm",
                                missing_roles_group_status(profile->norm_count),
                                profile->norm_count,
                                "norm/scalar roles mapped report-only");
        print_missing_roles_row("output-head",
                                profile->output_head_status,
                                profile->output_head_count,
                                strcmp(profile->output_head_status, "present") == 0
                                    ? "report-only, runtime consumer not implemented"
                                    : "separate head absent or tied-head policy unknown");
        print_missing_roles_row("tied-head-policy",
                                profile->tied_head_policy_status,
                                strcmp(profile->tied_head_policy_status,
                                       "tied-output-head-candidate") == 0 ? 1 : 0,
                                "config/header evidence only");
        print_missing_roles_row("tokenizer", ctx->tokenizer_map_status,
                                strcmp(ctx->tokenizer_map_status,
                                       "present-report-only") == 0 ? 1 : 0,
                                strcmp(ctx->tokenizer_map_status,
                                       "present-report-only") == 0
                                    ? "tokenizer metadata mapped report-only"
                                    : "tokenizer metadata map missing");
        print_missing_roles_row("unknown-tensors",
                                profile->unknown_tensor_status,
                                profile->unmapped_unknown_count,
                                "unclassified header names remain non-blocking");
        print_missing_roles_row("artifact", "missing", 0,
                                "GGUF emission blocked");
    } else {
        print_missing_roles_row("embedding",
                                profile->embedding_count ? "present" : "missing",
                                profile->embedding_count,
                                profile->embedding_count
                                    ? "token embedding mapped report-only"
                                    : "token embedding not mapped");
        print_missing_roles_row("attention",
                                missing_roles_group_status(profile->attention_count),
                                profile->attention_count,
                                "attention roles mapped report-only");
        print_missing_roles_row("qwen-linear-attn",
                                profile->qwen_linear_attn_status,
                                profile->qwen_linear_attn_count,
                                "linear_attn roles mapped report-only");
        print_missing_roles_row("mlp",
                                missing_roles_group_status(profile->mlp_count),
                                profile->mlp_count,
                                "MLP/MoE roles mapped report-only");
        print_missing_roles_row("moe-router",
                                profile->moe_router_status,
                                profile->moe_router_count,
                                "router gate mapped report-only");
        print_missing_roles_row("moe-experts",
                                profile->moe_expert_status,
                                profile->moe_expert_count,
                                "expert tensors mapped report-only");
        print_missing_roles_row("shared-expert",
                                profile->moe_shared_status,
                                profile->moe_shared_count,
                                "shared expert tensors mapped report-only");
        print_missing_roles_row("output-head",
                                profile->output_head_status,
                                profile->output_head_count,
                                strcmp(profile->output_head_status, "present") == 0
                                    ? "report-only, runtime consumer not implemented"
                                    : "output head not mapped");
        print_missing_roles_row("tokenizer", ctx->tokenizer_map_status,
                                strcmp(ctx->tokenizer_map_status,
                                       "present-report-only") == 0 ? 1 : 0,
                                strcmp(ctx->tokenizer_map_status,
                                       "present-report-only") == 0
                                    ? "tokenizer metadata mapped report-only"
                                    : "tokenizer metadata map missing");
        print_missing_roles_row("unknown-tensors",
                                profile->unknown_tensor_status,
                                profile->unmapped_unknown_count,
                                "unclassified header names remain non-blocking");
        print_missing_roles_row("artifact", "missing", 0,
                                "GGUF emission blocked");
    }
    model_target_out_writef(model_target_out(), "\nboundary: missing-role report only; no GGUF/runtime/generation\n");
}

static void print_missing_roles_porcelain_table(
    const yvex_missing_role_report_profile *profile,
    const yvex_missing_roles_porcelain_context *ctx)
{
    unsigned long long moe_total;

    if (!profile || !ctx) return;
    moe_total = profile->moe_router_count + profile->moe_expert_count +
                profile->moe_shared_count;
    model_target_out_writef(model_target_out(), "%-18s  %-6s  %-7s  %-24s  %5s  %5s  %5s  %5s  %5s  %3s  %7s  %-9s  %-8s  %s\n",
           "TARGET", "FAMILY", "STATUS", "TOP_BLOCKER", "EMBED", "ATTN",
           "MLP", "NORM", "HEAD", "MOE", "UNKNOWN", "TOKENIZER",
           "ARTIFACT", "NEXT");
    model_target_out_writef(model_target_out(), "%-18s  %-6s  %-7s  %-24s  %5llu  %5llu  %5llu  %5llu  %5llu  %3llu  %7llu  %-9s  %-8s  %s\n",
           profile->record->target_id,
           profile->spec->family_key,
           "blocked",
           ctx->top_blocker,
           profile->embedding_count,
           profile->attention_count,
           profile->mlp_count,
           profile->norm_count,
           profile->output_head_count,
           moe_total,
           profile->unmapped_unknown_count,
           ctx->tokenizer_map_status,
           ctx->artifact_status,
           missing_roles_porcelain_next_row(ctx));
}

static void print_missing_roles_porcelain_audit(
    const yvex_missing_role_report_profile *profile,
    const yvex_missing_roles_porcelain_context *ctx)
{
    unsigned long long i;

    if (!profile || !ctx) return;
    model_target_out_writef(model_target_out(), "target_id: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "family: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "source_status: %s\n", profile->source_exists ? "present" : "missing");
    model_target_out_writef(model_target_out(), "model_class_status: %s\n", profile->source_exists ? "present" : "missing");
    model_target_out_writef(model_target_out(), "tensor_map_status: %s\n", ctx->tensor_map_status);
    model_target_out_writef(model_target_out(), "tensor_map_path: %s\n", ctx->tensor_map_path);
    model_target_out_writef(model_target_out(), "tensor_map_mapped_total_count: %llu\n", profile->mapped_total_count);
    model_target_out_writef(model_target_out(), "tensor_map_unmapped_unknown_count: %llu\n",
           profile->unmapped_unknown_count);
    model_target_out_writef(model_target_out(), "tensor_map_role_counts: embed=%llu attention=%llu qwen_linear_attn=%llu mlp=%llu norm=%llu head=%llu moe_router=%llu moe_expert=%llu moe_shared=%llu unknown=%llu\n",
           profile->embedding_count,
           profile->attention_count,
           profile->qwen_linear_attn_count,
           profile->mlp_count,
           profile->norm_count,
           profile->output_head_count,
           profile->moe_router_count,
           profile->moe_expert_count,
           profile->moe_shared_count,
           profile->unmapped_unknown_count);
    model_target_out_writef(model_target_out(), "role_group.embedding.status: %s\n", profile->embedding_status);
    model_target_out_writef(model_target_out(), "role_group.attention.status: %s\n",
           profile->attention_count ? "present" : "missing");
    model_target_out_writef(model_target_out(), "role_group.qwen_linear_attn.status: %s\n",
           profile->qwen_linear_attn_status);
    model_target_out_writef(model_target_out(), "role_group.mlp.status: %s\n",
           profile->mlp_count ? "present" : "missing");
    model_target_out_writef(model_target_out(), "role_group.moe_router.status: %s\n",
           profile->moe_router_status);
    model_target_out_writef(model_target_out(), "role_group.moe_experts.status: %s\n",
           profile->moe_expert_status);
    model_target_out_writef(model_target_out(), "role_group.shared_expert.status: %s\n",
           profile->moe_shared_status);
    model_target_out_writef(model_target_out(), "role_group.output_head.status: %s\n",
           profile->output_head_status);
    model_target_out_writef(model_target_out(), "role_group.tied_head_policy.status: %s\n",
           profile->tied_head_policy_status);
    model_target_out_writef(model_target_out(), "role_group.unknown_tensors.status: %s\n",
           profile->unknown_tensor_status);
    model_target_out_writef(model_target_out(), "output_head_map_status: %s\n", ctx->output_head_map_status);
    model_target_out_writef(model_target_out(), "output_head_map_path: %s\n", ctx->output_head_map_path);
    model_target_out_writef(model_target_out(), "tokenizer_map_status: %s\n", ctx->tokenizer_map_status);
    model_target_out_writef(model_target_out(), "artifact_status: %s\n", ctx->artifact_status);
    model_target_out_writef(model_target_out(), "expected_artifact_path: %s\n", ctx->expected_artifact_path);
    model_target_out_writef(model_target_out(), "artifact_plan_status: planned-full-gguf\n");
    model_target_out_writef(model_target_out(), "artifact_emission_status: not-performed\n");
    model_target_out_writef(model_target_out(), "artifact_identity_status: %s\n", ctx->artifact_identity_status);
    model_target_out_writef(model_target_out(), "prepare_blocker_count: %u\n", ctx->blocker_count);
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", ctx->top_blocker);
    model_target_out_writef(model_target_out(), "missing_role_count: %llu\n", profile->missing_entry_count);
    for (i = 0; i < profile->missing_entry_count; ++i) {
        const yvex_missing_role_entry *entry = &profile->missing_entries[i];
        model_target_out_writef(model_target_out(), "missing_role.%llu.name: %s\n", i, entry->name);
        model_target_out_writef(model_target_out(), "missing_role.%llu.status: %s\n", i, entry->status);
        model_target_out_writef(model_target_out(), "missing_role.%llu.detail: %s\n", i, entry->blocker_class);
    }
    model_target_out_writef(model_target_out(), "next: %s\n", missing_roles_porcelain_next_row(ctx));
    model_target_out_writef(model_target_out(), "runtime_execution: not-performed\n");
    model_target_out_writef(model_target_out(), "generation: unsupported\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "boundary: missing-role report only; no GGUF/runtime/generation\n");
}

static void print_missing_roles_porcelain_json(
    const yvex_missing_role_report_profile *profile,
    const yvex_missing_roles_porcelain_context *ctx)
{
    if (!profile || !ctx) return;
    model_target_out_writef(model_target_out(), "{\"status\":\"blocked\",\"target_id\":");
    model_target_json_write_escaped(model_target_out(), profile->record->target_id);
    model_target_out_writef(model_target_out(), ",\"family\":");
    model_target_json_write_escaped(model_target_out(), profile->spec->family_key);
    model_target_out_writef(model_target_out(), ",\"top_blocker\":");
    model_target_json_write_escaped(model_target_out(), ctx->top_blocker);
    model_target_out_writef(model_target_out(), ",\"next\":");
    model_target_json_write_escaped(model_target_out(), missing_roles_porcelain_next_row(ctx));
    model_target_out_writef(model_target_out(), ",\"runtime_execution\":\"not-performed\",");
    model_target_out_writef(model_target_out(), "\"role_groups\":{");
    model_target_out_writef(model_target_out(), "\"embedding\":");
    model_target_json_write_escaped(model_target_out(), profile->embedding_status);
    model_target_out_writef(model_target_out(), ",\"attention\":");
    model_target_json_write_escaped(model_target_out(),
                                    profile->attention_count ? "present" : "missing");
    model_target_out_writef(model_target_out(), ",\"qwen_linear_attn\":");
    model_target_json_write_escaped(model_target_out(), profile->qwen_linear_attn_status);
    model_target_out_writef(model_target_out(), ",\"mlp\":");
    model_target_json_write_escaped(model_target_out(),
                                    profile->mlp_count ? "present" : "missing");
    model_target_out_writef(model_target_out(), ",\"moe_router\":");
    model_target_json_write_escaped(model_target_out(), profile->moe_router_status);
    model_target_out_writef(model_target_out(), ",\"moe_experts\":");
    model_target_json_write_escaped(model_target_out(), profile->moe_expert_status);
    model_target_out_writef(model_target_out(), ",\"shared_expert\":");
    model_target_json_write_escaped(model_target_out(), profile->moe_shared_status);
    model_target_out_writef(model_target_out(), ",\"output_head\":");
    model_target_json_write_escaped(model_target_out(), profile->output_head_status);
    model_target_out_writef(model_target_out(), ",\"tied_head_policy\":");
    model_target_json_write_escaped(model_target_out(), profile->tied_head_policy_status);
    model_target_out_writef(model_target_out(), ",\"unknown_tensors\":");
    model_target_json_write_escaped(model_target_out(), profile->unknown_tensor_status);
    model_target_out_writef(model_target_out(), ",\"tokenizer\":");
    model_target_json_write_escaped(model_target_out(), ctx->tokenizer_map_status);
    model_target_out_writef(model_target_out(), "},");
    model_target_out_writef(model_target_out(), "\"generation\":\"unsupported\",\"benchmark_status\":\"not-measured\"}\n");
}

static void tensor_mapping_gate_append_csv(char *out, size_t cap,
                                           const char *value)
{
    size_t len;

    if (!out || cap == 0 || !value || !value[0]) return;
    len = strlen(out);
    if (len + 1 >= cap) return;
    if (len > 0) {
        snprintf(out + len, cap - len, ",");
        len = strlen(out);
    }
    if (len + 1 >= cap) return;
    snprintf(out + len, cap - len, "%s", value);
}

static void tensor_mapping_gate_role_list(
    const yvex_missing_role_report_profile *profile,
    const char *status,
    int metadata_filter,
    char *out,
    size_t cap)
{
    unsigned long long i;

    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!profile || !status) {
        snprintf(out, cap, "none");
        return;
    }
    for (i = 0; i < profile->missing_entry_count; ++i) {
        const yvex_missing_role_entry *entry = &profile->missing_entries[i];
        int is_metadata;

        if (!entry || strcmp(entry->status, status) != 0) continue;
        is_metadata = strstr(entry->blocker_class, "metadata") != NULL;
        if (metadata_filter == 0 && is_metadata) continue;
        if (metadata_filter == 1 && !is_metadata) continue;
        tensor_mapping_gate_append_csv(out, cap, entry->name);
    }
    if (out[0] == '\0') {
        snprintf(out, cap, "none");
    }
}

static int tensor_mapping_gate_source_ready(
    const yvex_tensor_mapping_gate_profile *profile)
{
    return profile &&
           profile->model_class.source_exists &&
           profile->tensor_collection.source_exists &&
           profile->tensor_naming.source_exists &&
           profile->output_head.source_exists &&
           profile->tokenizer.source_exists &&
           profile->missing_role.source_exists;
}

static int tensor_mapping_gate_tokenizer_ready(
    const yvex_tokenizer_map_profile *profile)
{
    return profile &&
           strcmp(profile->status, "present-report-only") == 0;
}

/*
 * build_tensor_mapping_gate_profile()
 *
 * Purpose:
 *   aggregate model-class, collection, tensor-map, output-head, tokenizer, and
 *   missing-role report facts for the v0.1.0 mapping gate.
 *
 * Inputs:
 *   record and optional path overrides are borrowed; profile receives the
 *   composed report state.
 *
 * Effects:
 *   runs report builders, records blocker strings, and fills summary fields; it
 *   performs no quantization, GGUF emission, runtime execution, or benchmark.
 *
 * Failure:
 *   propagates the first report-builder failure that cannot be represented as a
 *   blocked gate profile.
 *
 * Boundary:
 *   mapping-gate reports are control evidence only and do not complete artifact
 *   contracts, runtime descriptors, graph routes, or generation capability.
 */
static int build_tensor_mapping_gate_profile(
    const yvex_model_target_record *record,
    const char *models_root_override,
    const char *source_override,
    yvex_tensor_mapping_gate_profile *profile)
{
    const yvex_model_class_profile_spec *spec;
    int rc;

    if (!record || !profile) return 2;
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) return 2;

    memset(profile, 0, sizeof(*profile));
    profile->record = record;
    profile->spec = spec;
    profile->status = "blocked-missing-source";
    profile->gate_result = "blocked";
    profile->top_blocker = spec->missing_source_blocker;
    profile->next_required_row = YVEX_MISSING_ROLE_REPORT_NEXT_ROW;

    rc = model_class_build_profile(record, spec, models_root_override,
                                   source_override, &profile->model_class);
    if (rc != 0) return rc;
    rc = build_tensor_collection_profile(record, spec, models_root_override,
                                         source_override,
                                         &profile->tensor_collection);
    if (rc != 0) return rc;
    rc = build_tensor_naming_profile(record, spec, models_root_override,
                                     source_override, &profile->tensor_naming);
    if (rc != 0) return rc;
    rc = build_output_head_map_profile(record, spec, models_root_override,
                                       source_override, &profile->output_head);
    if (rc != 0) return rc;
    rc = build_tokenizer_map_profile(record, spec, models_root_override,
                                     source_override, &profile->tokenizer);
    if (rc != 0) return rc;
    rc = build_missing_role_report_profile(record, spec, models_root_override,
                                           source_override,
                                           &profile->missing_role);
    if (rc != 0) return rc;

    tensor_mapping_gate_role_list(&profile->missing_role, "missing", -1,
                                  profile->missing_roles,
                                  sizeof(profile->missing_roles));
    tensor_mapping_gate_role_list(&profile->missing_role, "missing", 0,
                                  profile->missing_source_roles,
                                  sizeof(profile->missing_source_roles));
    tensor_mapping_gate_role_list(&profile->missing_role, "missing", 1,
                                  profile->missing_metadata_roles,
                                  sizeof(profile->missing_metadata_roles));
    tensor_mapping_gate_role_list(&profile->missing_role, "ambiguous", -1,
                                  profile->ambiguous_roles,
                                  sizeof(profile->ambiguous_roles));

    if (!tensor_mapping_gate_source_ready(profile)) {
        profile->status = "blocked-missing-source";
        profile->top_blocker = spec->missing_source_blocker;
    } else if (strcmp(profile->tensor_naming.status, "metadata-missing") == 0) {
        profile->status = "blocked-missing-map";
        profile->top_blocker = profile->tensor_naming.top_blocker;
    } else if (strcmp(profile->output_head.status, "output-head-missing") == 0) {
        profile->status = "blocked-missing-output-head";
        profile->top_blocker = profile->output_head.top_blocker;
    } else if (strcmp(profile->output_head.status, "output-head-ambiguous") == 0 ||
               profile->missing_role.source_role_ambiguous_count > 0) {
        profile->status = "blocked-ambiguous-output-head";
        profile->top_blocker = profile->output_head.top_blocker;
    } else if (!tensor_mapping_gate_tokenizer_ready(&profile->tokenizer) ||
               profile->missing_role.metadata_missing_count > 0 ||
               profile->missing_role.metadata_ambiguous_count > 0) {
        profile->status = "blocked-missing-tokenizer-metadata";
        profile->top_blocker =
            profile->missing_role.metadata_ambiguous_count > 0
                ? profile->missing_role.top_blocker
                : profile->tokenizer.top_blocker;
    } else if (profile->missing_role.source_role_missing_count > 0) {
        profile->status = "blocked-missing-runtime-roles";
        profile->top_blocker = profile->missing_role.top_blocker;
    } else if (strcmp(profile->tensor_naming.status, "naming-map-profiled") != 0 &&
               strcmp(profile->tensor_naming.status, "naming-map-candidate") != 0) {
        profile->status = "blocked-missing-map";
        profile->top_blocker = profile->tensor_naming.top_blocker;
    } else {
        profile->status = "passed-for-artifact-planning";
        profile->gate_result = "pass";
        profile->top_blocker = "missing-qtype-policy-report";
        profile->next_required_row = YVEX_TENSOR_MAPPING_GATE_NEXT_ROW;
    }

    return 0;
}

static void print_tensor_mapping_gate_boundary(void)
{
    model_target_out_writef(model_target_out(), "boundary: V010.MAP.9 is a report-only tensor mapping gate. It does not load tensor payloads, emit artifacts, complete quantization/artifact contract, construct runtime descriptors, attach backend residency, feed graph consumers, execute prefill/decode/logits/tokenizer/sampling/generation, evaluate, benchmark, claim throughput, or mark v0.1.0 release-ready.\n");
}

static void print_tensor_mapping_gate_normal(
    const yvex_tensor_mapping_gate_profile *profile)
{
    unsigned long long missing_count =
        profile->missing_role.source_role_missing_count +
        profile->missing_role.metadata_missing_count;
    unsigned long long ambiguous_count =
        profile->missing_role.source_role_ambiguous_count +
        profile->missing_role.metadata_ambiguous_count;

    model_target_out_writef(model_target_out(), "tensor-mapping-gate: %s [%s]\n",
           profile->record->target_id,
           compact_status_bracket(profile->status));
    model_target_out_writef(model_target_out(), "gate: v0.1.0  family: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "roles: source %llu/12, metadata %llu/4, missing %llu, ambiguous %llu\n",
           profile->missing_role.source_role_observed_count,
           profile->missing_role.metadata_observed_count,
           missing_count,
           ambiguous_count);
    if (strcmp(profile->missing_roles, "none") != 0) {
        model_target_out_writef(model_target_out(), "missing: %s\n", profile->missing_roles);
    }
    if (strcmp(profile->ambiguous_roles, "none") != 0) {
        model_target_out_writef(model_target_out(), "ambiguous: %s\n", profile->ambiguous_roles);
    }
    model_target_out_writef(model_target_out(), "result: %s\n", profile->gate_result);
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next: %s\n", profile->next_required_row);
    model_target_out_writef(model_target_out(), "boundary: report-only; no artifact/runtime/generation\n");
}

static void print_tensor_mapping_gate_table(
    const yvex_tensor_mapping_gate_profile *profile)
{
    unsigned long long missing_count =
        profile->missing_role.source_role_missing_count +
        profile->missing_role.metadata_missing_count;
    unsigned long long ambiguous_count =
        profile->missing_role.source_role_ambiguous_count +
        profile->missing_role.metadata_ambiguous_count;

    model_target_out_writef(model_target_out(), "TENSOR MAPPING GATE\n\n");
    model_target_out_writef(model_target_out(), "%-15s  %-6s  %-8s  %-12s  %-10s  %7s  %5s  %-34s  %-30s  %s\n",
           "TARGET", "FAMILY", "GATE", "SOURCE_ROLES", "META_ROLES",
           "MISSING", "AMBIG", "TOP_BLOCKER", "STATUS", "NEXT");
    model_target_out_writef(model_target_out(), "%-15s  %-6s  %-8s  %2llu/12         %2llu/4       %7llu  %5llu  %-34s  %-30s  %s\n",
           profile->record->target_id,
           profile->spec->family_key,
           "v0.1.0",
           profile->missing_role.source_role_observed_count,
           profile->missing_role.metadata_observed_count,
           missing_count,
           ambiguous_count,
           profile->top_blocker,
           profile->status,
           profile->next_required_row);
}

static void print_tensor_mapping_gate_audit(
    const yvex_tensor_mapping_gate_profile *profile)
{
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "tensor_mapping_gate: v0.1.0-tensor-mapping\n");
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_result: %s\n", profile->gate_result);
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_target_id: %s\n", profile->record->target_id);
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_family: %s\n", profile->spec->family_key);
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_model_class: %s\n", profile->spec->class_name);
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_source_class: %s\n",
           profile->record->source_artifact_class);
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_source_status: %s\n",
           tensor_mapping_gate_source_ready(profile) ? "present" : "missing");
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_source_path: %s\n",
           profile->missing_role.source_path);
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_source_path_source: %s\n",
           profile->missing_role.source_path_source);
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_source_report_path: not-written\n");
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_tensor_map_sidecar_path: not-written\n");
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_output_head_map_sidecar_path: not-written\n");
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_tokenizer_metadata_source_path: %s\n",
           profile->tokenizer.source_path);
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_missing_role_report_sidecar_path: not-written\n");
    model_target_out_writef(model_target_out(), "map_source.0: source-tensor-metadata-inventory\n");
    model_target_out_writef(model_target_out(), "map_source.1: model-class-profile\n");
    model_target_out_writef(model_target_out(), "map_source.2: tensor-collection-inventory\n");
    model_target_out_writef(model_target_out(), "map_source.3: tensor-naming-map\n");
    model_target_out_writef(model_target_out(), "map_source.4: output-head-tensor-map\n");
    model_target_out_writef(model_target_out(), "map_source.5: tokenizer-metadata-map\n");
    model_target_out_writef(model_target_out(), "map_source.6: missing-role-blocker-report\n");
    model_target_out_writef(model_target_out(), "model_class_profile_status: %s\n", profile->model_class.status);
    model_target_out_writef(model_target_out(), "tensor_collection_status: %s\n", profile->tensor_collection.status);
    model_target_out_writef(model_target_out(), "tensor_naming_map_status: %s\n", profile->tensor_naming.status);
    model_target_out_writef(model_target_out(), "output_head_map_status: %s\n", profile->output_head.status);
    model_target_out_writef(model_target_out(), "tokenizer_metadata_map_status: %s\n", profile->tokenizer.status);
    model_target_out_writef(model_target_out(), "missing_role_report_status: %s\n", profile->missing_role.status);
    model_target_out_writef(model_target_out(), "expected_source_role_count: 12\n");
    model_target_out_writef(model_target_out(), "observed_source_role_count: %llu\n",
           profile->missing_role.source_role_observed_count);
    model_target_out_writef(model_target_out(), "missing_source_role_count: %llu\n",
           profile->missing_role.source_role_missing_count);
    model_target_out_writef(model_target_out(), "ambiguous_source_role_count: %llu\n",
           profile->missing_role.source_role_ambiguous_count);
    model_target_out_writef(model_target_out(), "expected_metadata_role_count: 4\n");
    model_target_out_writef(model_target_out(), "observed_metadata_role_count: %llu\n",
           profile->missing_role.metadata_observed_count);
    model_target_out_writef(model_target_out(), "missing_metadata_role_count: %llu\n",
           profile->missing_role.metadata_missing_count);
    model_target_out_writef(model_target_out(), "ambiguous_metadata_role_count: %llu\n",
           profile->missing_role.metadata_ambiguous_count);
    model_target_out_writef(model_target_out(), "missing_source_roles: %s\n", profile->missing_source_roles);
    model_target_out_writef(model_target_out(), "missing_metadata_roles: %s\n", profile->missing_metadata_roles);
    model_target_out_writef(model_target_out(), "missing_roles: %s\n", profile->missing_roles);
    model_target_out_writef(model_target_out(), "ambiguous_roles: %s\n", profile->ambiguous_roles);
    model_target_out_writef(model_target_out(), "downstream_blockers: artifact_contract=missing qtype_policy=missing runtime_descriptor=missing graph_consumer=missing backend_residency=missing logits_runtime=missing tokenizer_runtime=missing generation_runtime=missing eval_benchmark=missing\n");
    model_target_out_writef(model_target_out(), "artifact_contract_status: missing\n");
    model_target_out_writef(model_target_out(), "qtype_policy_status: missing\n");
    model_target_out_writef(model_target_out(), "runtime_descriptor_status: missing\n");
    model_target_out_writef(model_target_out(), "graph_consumer_status: missing\n");
    model_target_out_writef(model_target_out(), "backend_residency_status: missing\n");
    model_target_out_writef(model_target_out(), "logits_runtime_status: missing\n");
    model_target_out_writef(model_target_out(), "tokenizer_runtime_status: missing\n");
    model_target_out_writef(model_target_out(), "generation_runtime_status: missing\n");
    model_target_out_writef(model_target_out(), "eval_benchmark_status: missing\n");
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next_required_rows: %s\n", profile->next_required_row);
    model_target_out_writef(model_target_out(), "payload_bytes_read: false\n");
    model_target_out_writef(model_target_out(), "artifact_emitted: false\n");
    model_target_out_writef(model_target_out(), "runtime_descriptor_constructed: false\n");
    model_target_out_writef(model_target_out(), "graph_consumer_fed: false\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    print_tensor_mapping_gate_boundary();
}

static void print_tensor_mapping_gate_audit_hint(
    const yvex_model_target_record *record)
{
    const yvex_model_class_profile_spec *spec;

    if (!record) return;
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) return;
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_status: not-run\n");
    model_target_out_writef(model_target_out(), "tensor_mapping_gate: v0.1.0-tensor-mapping\n");
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_family: %s\n", spec->family_key);
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_target_id: %s\n", spec->target_id);
    model_target_out_writef(model_target_out(), "tensor_mapping_gate_next_required_row: %s\n",
           YVEX_TENSOR_MAPPING_GATE_NEXT_ROW);
}

typedef struct {
    const yvex_model_target_record *record;
    const yvex_model_class_profile_spec *spec;
    const char *status;
    const char *mapping_gate_status;
    const char *policy_basis;
    const char *source_dtype_profile_status;
    const char *qtype_policy_status;
    const char *qtype_policy;
    const char *preferred_qtype;
    const char *candidate_qtypes;
    const char *refused_qtypes;
    const char *refusal_reasons;
    const char *calibration_status;
    const char *imatrix_status;
    const char *per_role_qtype_status;
    const char *compute_support_status;
    const char *artifact_emit_status;
    const char *top_blocker;
    const char *next_required_row;
    char target_id[64];
    char family[32];
    char model_class[96];
    char source_class[96];
    char target_artifact_class[96];
    char source_path[YVEX_PATH_CAP];
    char dtype_profile[192];
    char dtype_counts[384];
    unsigned long long source_tensor_count;
    unsigned long long source_declared_data_bytes;
    unsigned long long dtype_f32_count;
    unsigned long long dtype_f16_count;
    unsigned long long dtype_bf16_count;
    unsigned long long dtype_f64_count;
    unsigned long long dtype_i64_count;
    unsigned long long dtype_i32_count;
    unsigned long long dtype_i16_count;
    unsigned long long dtype_i8_count;
    unsigned long long dtype_u8_count;
    unsigned long long dtype_bool_count;
    unsigned long long dtype_f8_e4m3_count;
    unsigned long long dtype_f8_e5m2_count;
    unsigned long long dtype_fp4_count;
    unsigned long long dtype_other_count;
    unsigned long long dtype_unknown_count;
    yvex_tensor_mapping_gate_profile gate;
} yvex_qtype_policy_profile;

static const char *qtype_policy_candidate_qtypes(void)
{
    return "F16,BF16,F32";
}

static const char *qtype_policy_refused_qtypes(void)
{
    return "Q8_0,Q4_K,Q2_K,IQ2_XXS";
}

static const char *qtype_policy_refusal_reasons(void)
{
    return "Q8_0:emit-quantize-compute-deferred;Q4_K:storage-or-emitter-missing;Q2_K:storage-or-emitter-missing;IQ2_XXS:storage-or-emitter-missing";
}

static void qtype_policy_copy(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) return;
    if (!src) src = "";
    snprintf(dst, cap, "%s", src);
}

static void qtype_policy_init(yvex_qtype_policy_profile *profile,
                              const yvex_model_target_record *record,
                              const yvex_model_class_profile_spec *spec,
                              const char *target_id)
{
    memset(profile, 0, sizeof(*profile));
    profile->record = record;
    profile->spec = spec;
    profile->status = "parser-error";
    profile->mapping_gate_status = "not-run";
    profile->policy_basis = "unknown";
    profile->source_dtype_profile_status = "missing";
    profile->qtype_policy_status = "missing";
    profile->qtype_policy = "missing";
    profile->preferred_qtype = "none";
    profile->candidate_qtypes = "none";
    profile->refused_qtypes = "none";
    profile->refusal_reasons = "none";
    profile->calibration_status = "deferred";
    profile->imatrix_status = "deferred";
    profile->per_role_qtype_status = "deferred-to-V010.QUANT.1";
    profile->compute_support_status = "runtime-compute-qtype-deferred";
    profile->artifact_emit_status = "blocked-artifact-emit-not-implemented";
    profile->top_blocker = "parser-error";
    profile->next_required_row = YVEX_QTYPE_POLICY_BACK_ROW;
    qtype_policy_copy(profile->target_id, sizeof(profile->target_id),
                      target_id ? target_id : (record ? record->target_id : "none"));
    qtype_policy_copy(profile->family, sizeof(profile->family),
                      spec ? spec->family_key : (record ? record->family : "unknown"));
    qtype_policy_copy(profile->model_class, sizeof(profile->model_class),
                      spec ? spec->class_name : (record ? record->target_class : "unknown"));
    qtype_policy_copy(profile->source_class, sizeof(profile->source_class),
                      record ? record->source_artifact_class : "unknown");
    qtype_policy_copy(profile->target_artifact_class,
                      sizeof(profile->target_artifact_class),
                      record ? record->target_artifact_class : "unknown");
    qtype_policy_copy(profile->source_path, sizeof(profile->source_path), "none");
    qtype_policy_copy(profile->dtype_profile, sizeof(profile->dtype_profile),
                      "missing");
    qtype_policy_copy(profile->dtype_counts, sizeof(profile->dtype_counts),
                      "none");
}

static void qtype_policy_count_native_dtype(yvex_qtype_policy_profile *profile,
                                            yvex_native_dtype dtype)
{
    switch (dtype) {
    case YVEX_NATIVE_DTYPE_F32: profile->dtype_f32_count++; break;
    case YVEX_NATIVE_DTYPE_F16: profile->dtype_f16_count++; break;
    case YVEX_NATIVE_DTYPE_BF16: profile->dtype_bf16_count++; break;
    case YVEX_NATIVE_DTYPE_F64: profile->dtype_f64_count++; break;
    case YVEX_NATIVE_DTYPE_I64: profile->dtype_i64_count++; break;
    case YVEX_NATIVE_DTYPE_I32: profile->dtype_i32_count++; break;
    case YVEX_NATIVE_DTYPE_I16: profile->dtype_i16_count++; break;
    case YVEX_NATIVE_DTYPE_I8: profile->dtype_i8_count++; break;
    case YVEX_NATIVE_DTYPE_U8: profile->dtype_u8_count++; break;
    case YVEX_NATIVE_DTYPE_BOOL: profile->dtype_bool_count++; break;
    case YVEX_NATIVE_DTYPE_F8_E4M3: profile->dtype_f8_e4m3_count++; break;
    case YVEX_NATIVE_DTYPE_F8_E5M2: profile->dtype_f8_e5m2_count++; break;
    case YVEX_NATIVE_DTYPE_FP4: profile->dtype_fp4_count++; break;
    case YVEX_NATIVE_DTYPE_OTHER: profile->dtype_other_count++; break;
    case YVEX_NATIVE_DTYPE_UNKNOWN:
    default:
        profile->dtype_unknown_count++;
        break;
    }
}

static unsigned long long qtype_policy_unsupported_source_dtype_count(
    const yvex_qtype_policy_profile *profile)
{
    if (!profile) return 0;
    return profile->dtype_f64_count +
           profile->dtype_i64_count +
           profile->dtype_i32_count +
           profile->dtype_i16_count +
           profile->dtype_i8_count +
           profile->dtype_u8_count +
           profile->dtype_bool_count +
           profile->dtype_f8_e4m3_count +
           profile->dtype_f8_e5m2_count +
           profile->dtype_fp4_count +
           profile->dtype_other_count +
           profile->dtype_unknown_count;
}

static void qtype_policy_format_dtype_profile(
    yvex_qtype_policy_profile *profile)
{
    unsigned long long other_count;

    if (!profile) return;
    other_count = qtype_policy_unsupported_source_dtype_count(profile);
    snprintf(profile->dtype_profile, sizeof(profile->dtype_profile),
             "F32=%llu F16=%llu BF16=%llu other=%llu",
             profile->dtype_f32_count,
             profile->dtype_f16_count,
             profile->dtype_bf16_count,
             other_count);
    snprintf(profile->dtype_counts, sizeof(profile->dtype_counts),
             "F32=%llu,F16=%llu,BF16=%llu,F64=%llu,I64=%llu,I32=%llu,I16=%llu,I8=%llu,U8=%llu,BOOL=%llu,F8_E4M3=%llu,F8_E5M2=%llu,FP4=%llu,OTHER=%llu,UNKNOWN=%llu",
             profile->dtype_f32_count,
             profile->dtype_f16_count,
             profile->dtype_bf16_count,
             profile->dtype_f64_count,
             profile->dtype_i64_count,
             profile->dtype_i32_count,
             profile->dtype_i16_count,
             profile->dtype_i8_count,
             profile->dtype_u8_count,
             profile->dtype_bool_count,
             profile->dtype_f8_e4m3_count,
             profile->dtype_f8_e5m2_count,
             profile->dtype_fp4_count,
             profile->dtype_other_count,
             profile->dtype_unknown_count);
}

static int qtype_policy_build_source_dtype_profile(
    yvex_qtype_policy_profile *profile)
{
    yvex_native_weight_table *table = NULL;
    yvex_native_weight_options options;
    yvex_native_weight_summary summary;
    yvex_error err;
    unsigned long long i;
    int rc;

    if (!profile || !profile->source_path[0] ||
        strcmp(profile->source_path, "none") == 0) {
        return 0;
    }

    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));
    options.source_dir = profile->source_path;
    options.recursive = 0;
    options.include_metadata = 0;
    yvex_error_clear(&err);
    rc = yvex_native_weight_table_open(&table, &options, &err);
    if (rc != YVEX_OK) {
        profile->source_dtype_profile_status = "header-error";
        profile->top_blocker = "missing-source-dtype-profile";
        qtype_policy_copy(profile->dtype_profile, sizeof(profile->dtype_profile),
                          "missing");
        return rc == YVEX_ERR_NOMEM ? 3 : 0;
    }

    profile->source_tensor_count = yvex_native_weight_table_count(table);
    (void)yvex_native_weight_table_summary(table, &summary, &err);
    profile->source_declared_data_bytes = summary.total_tensor_bytes;
    for (i = 0; i < profile->source_tensor_count; ++i) {
        const yvex_native_weight_info *info =
            yvex_native_weight_table_at(table, i);
        qtype_policy_count_native_dtype(profile,
                                        info ? info->dtype
                                             : YVEX_NATIVE_DTYPE_UNKNOWN);
    }
    yvex_native_weight_table_close(table);

    qtype_policy_format_dtype_profile(profile);
    if (profile->source_tensor_count == 0) {
        profile->source_dtype_profile_status = "missing";
    } else if (qtype_policy_unsupported_source_dtype_count(profile) > 0) {
        profile->source_dtype_profile_status = "unsupported";
    } else {
        profile->source_dtype_profile_status = "profiled";
    }
    return 0;
}

static int qtype_policy_has_required_table(void)
{
    return yvex_qtype_support_by_name("F16") &&
           yvex_qtype_support_by_name("BF16") &&
           yvex_qtype_support_by_name("F32") &&
           yvex_qtype_support_by_name("Q8_0") &&
           yvex_qtype_support_by_name("Q4_K") &&
           yvex_qtype_support_by_name("Q2_K") &&
           yvex_qtype_support_by_name("IQ2_XXS");
}

static void qtype_policy_mark_planning_reported(
    yvex_qtype_policy_profile *profile)
{
    profile->status = "policy-reported";
    profile->policy_basis =
        "header-only-source-metadata+existing-yvex-policy-table";
    profile->qtype_policy_status = "reported";
    profile->qtype_policy = "artifact-planning-storage-policy";
    profile->preferred_qtype = "F16";
    profile->candidate_qtypes = qtype_policy_candidate_qtypes();
    profile->refused_qtypes = qtype_policy_refused_qtypes();
    profile->refusal_reasons = qtype_policy_refusal_reasons();
    profile->calibration_status = "deferred-to-V010.QUANT.3";
    profile->imatrix_status = "deferred-to-V010.QUANT.3";
    profile->per_role_qtype_status = "deferred-to-V010.QUANT.1";
    profile->compute_support_status = "deferred-to-V010.QUANT.2";
    profile->artifact_emit_status = "blocked-until-artifact-emitter";
    profile->top_blocker = "missing-per-role-qtype-support";
    profile->next_required_row = YVEX_QTYPE_POLICY_NEXT_ROW;
}

static int build_qtype_policy_profile(
    const yvex_model_target_record *record,
    const char *models_root_override,
    const char *source_override,
    yvex_qtype_policy_profile *profile)
{
    const yvex_model_class_profile_spec *spec;
    int rc;

    if (!profile || !record) return 2;
    spec = find_model_class_profile_spec(record->target_id);
    qtype_policy_init(profile, record, spec, record->target_id);

    if (!spec) {
        if (strcmp(record->target_class, "selected-runtime-slice") == 0) {
            profile->status = "blocked-unsupported-target-class";
            profile->top_blocker = "unsupported-target-class";
        } else {
            profile->status = "unsupported-family";
            profile->top_blocker = "unsupported-family";
        }
        profile->next_required_row = "none";
        return 0;
    }
    if (strcmp(spec->family_key, "qwen") != 0 &&
        strcmp(spec->family_key, "gemma") != 0) {
        profile->status = "unsupported-family";
        profile->top_blocker = "unsupported-family";
        profile->next_required_row = "none";
        return 0;
    }
    if (strcmp(record->target_class, "source-model-candidate") != 0) {
        profile->status = "blocked-unsupported-target-class";
        profile->top_blocker = "unsupported-target-class";
        profile->next_required_row = "none";
        return 0;
    }
    if (!record->target_artifact_class ||
        strcmp(record->target_artifact_class, "future-YVEX-produced-GGUF") != 0) {
        profile->status = "blocked-missing-target-artifact-class";
        profile->top_blocker = "missing-target-artifact-class";
        return 0;
    }

    rc = build_tensor_mapping_gate_profile(record, models_root_override,
                                           source_override, &profile->gate);
    if (rc != 0) return rc;

    profile->mapping_gate_status = profile->gate.status;
    qtype_policy_copy(profile->source_path, sizeof(profile->source_path),
                      profile->gate.missing_role.source_path);

    if (!tensor_mapping_gate_source_ready(&profile->gate)) {
        profile->status = "blocked-missing-source";
        profile->top_blocker = profile->gate.top_blocker;
        profile->next_required_row = YVEX_QTYPE_POLICY_BACK_ROW;
        return 0;
    }

    rc = qtype_policy_build_source_dtype_profile(profile);
    if (rc != 0) return rc;
    if (strcmp(profile->source_dtype_profile_status, "missing") == 0 ||
        strcmp(profile->source_dtype_profile_status, "header-error") == 0) {
        profile->status = "blocked-missing-source-dtype-profile";
        profile->top_blocker = "missing-source-dtype-profile";
        profile->next_required_row = YVEX_QTYPE_POLICY_BACK_ROW;
        return 0;
    }
    if (strcmp(profile->source_dtype_profile_status, "unsupported") == 0) {
        profile->status = "blocked-unsupported-source-dtype";
        profile->top_blocker = "unsupported-source-dtype-profile";
        profile->next_required_row = YVEX_QTYPE_POLICY_BACK_ROW;
        return 0;
    }
    if (strcmp(profile->gate.status, "passed-for-artifact-planning") != 0) {
        profile->status = "blocked-mapping-gate";
        profile->top_blocker = profile->gate.top_blocker;
        profile->next_required_row = YVEX_QTYPE_POLICY_BACK_ROW;
        return 0;
    }
    if (!qtype_policy_has_required_table()) {
        profile->status = "blocked-missing-qtype-policy";
        profile->top_blocker = "missing-existing-yvex-qtype-policy-table";
        profile->next_required_row = YVEX_QTYPE_POLICY_NEXT_ROW;
        return 0;
    }

    qtype_policy_mark_planning_reported(profile);
    return 0;
}

static void print_qtype_policy_boundary(void)
{
    model_target_out_writef(model_target_out(), "boundary: report-only; no quantization/artifact/runtime\n");
}

static void print_qtype_policy_normal(
    const yvex_qtype_policy_profile *profile)
{
    model_target_out_writef(model_target_out(), "qtype-policy: %s [%s]\n",
           profile->target_id,
           compact_status_bracket(profile->status));
    model_target_out_writef(model_target_out(), "family: %s  mapping_gate: %s\n",
           profile->family,
           profile->mapping_gate_status);
    model_target_out_writef(model_target_out(), "source_dtype: %s\n", profile->dtype_profile);
    model_target_out_writef(model_target_out(), "policy: %s\n", profile->qtype_policy);
    if (strcmp(profile->preferred_qtype, "none") != 0) {
        model_target_out_writef(model_target_out(), "preferred: %s\n", profile->preferred_qtype);
    }
    if (strcmp(profile->candidate_qtypes, "none") != 0) {
        model_target_out_writef(model_target_out(), "candidates: %s\n", profile->candidate_qtypes);
    }
    if (strcmp(profile->refused_qtypes, "none") != 0) {
        model_target_out_writef(model_target_out(), "refused: %s\n", profile->refused_qtypes);
    }
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next: %s\n", profile->next_required_row);
    print_qtype_policy_boundary();
}

static void print_qtype_policy_table(
    const yvex_qtype_policy_profile *profile)
{
    model_target_out_writef(model_target_out(), "QTYPE POLICY\n\n");
    model_target_out_writef(model_target_out(), "%-16s  %-6s  %-28s  %-32s  %-9s  %-14s  %-25s  %-30s  %s\n",
           "TARGET", "FAMILY", "SOURCE_DTYPE", "POLICY", "PREFERRED",
           "CANDIDATES", "REFUSED", "STATUS", "NEXT");
    model_target_out_writef(model_target_out(), "%-16s  %-6s  %-28s  %-32s  %-9s  %-14s  %-25s  %-30s  %s\n",
           profile->target_id,
           profile->family,
           profile->dtype_profile,
           profile->qtype_policy,
           profile->preferred_qtype,
           profile->candidate_qtypes,
           profile->refused_qtypes,
           profile->status,
           profile->next_required_row);
}

static void print_qtype_policy_audit(
    const yvex_qtype_policy_profile *profile)
{
    model_target_out_writef(model_target_out(), "report: qtype-policy\n");
    model_target_out_writef(model_target_out(), "status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "target_id: %s\n", profile->target_id);
    model_target_out_writef(model_target_out(), "family: %s\n", profile->family);
    model_target_out_writef(model_target_out(), "target_class: %s\n",
           profile->record ? profile->record->target_class : "unknown");
    model_target_out_writef(model_target_out(), "model_class: %s\n", profile->model_class);
    model_target_out_writef(model_target_out(), "source_path: %s\n", profile->source_path);
    model_target_out_writef(model_target_out(), "source_class: %s\n", profile->source_class);
    model_target_out_writef(model_target_out(), "target_artifact_class: %s\n", profile->target_artifact_class);
    model_target_out_writef(model_target_out(), "source_metadata_status: %s\n",
           profile->gate.model_class.source_metadata_status
               ? profile->gate.model_class.source_metadata_status
               : "not-run");
    model_target_out_writef(model_target_out(), "source_dtype_profile_status: %s\n",
           profile->source_dtype_profile_status);
    model_target_out_writef(model_target_out(), "source_dtype_counts: %s\n", profile->dtype_counts);
    model_target_out_writef(model_target_out(), "source_tensor_count: %llu\n", profile->source_tensor_count);
    model_target_out_writef(model_target_out(), "source_declared_data_bytes: %llu\n",
           profile->source_declared_data_bytes);
    model_target_out_writef(model_target_out(), "mapping_gate_status: %s\n", profile->mapping_gate_status);
    model_target_out_writef(model_target_out(), "mapping_gate_report_path: not-written\n");
    model_target_out_writef(model_target_out(), "tensor_map_status: %s\n",
           profile->gate.tensor_naming.status
               ? profile->gate.tensor_naming.status
               : "not-run");
    model_target_out_writef(model_target_out(), "output_head_map_status: %s\n",
           profile->gate.output_head.status
               ? profile->gate.output_head.status
               : "not-run");
    model_target_out_writef(model_target_out(), "tokenizer_metadata_map_status: %s\n",
           profile->gate.tokenizer.status
               ? profile->gate.tokenizer.status
               : "not-run");
    model_target_out_writef(model_target_out(), "missing_role_report_status: %s\n",
           profile->gate.missing_role.status
               ? profile->gate.missing_role.status
               : "not-run");
    model_target_out_writef(model_target_out(), "qtype_policy_basis: %s\n", profile->policy_basis);
    model_target_out_writef(model_target_out(), "qtype_policy_status: %s\n", profile->qtype_policy_status);
    model_target_out_writef(model_target_out(), "preferred_qtype: %s\n", profile->preferred_qtype);
    model_target_out_writef(model_target_out(), "candidate_qtypes: %s\n", profile->candidate_qtypes);
    model_target_out_writef(model_target_out(), "refused_qtypes: %s\n", profile->refused_qtypes);
    model_target_out_writef(model_target_out(), "refusal_reasons: %s\n", profile->refusal_reasons);
    model_target_out_writef(model_target_out(), "per_role_qtype_status: %s\n", profile->per_role_qtype_status);
    model_target_out_writef(model_target_out(), "compute_support_status: %s\n", profile->compute_support_status);
    model_target_out_writef(model_target_out(), "calibration_status: %s\n", profile->calibration_status);
    model_target_out_writef(model_target_out(), "imatrix_status: %s\n", profile->imatrix_status);
    model_target_out_writef(model_target_out(), "artifact_emit_status: %s\n", profile->artifact_emit_status);
    model_target_out_writef(model_target_out(), "artifact_identity_status: missing\n");
    model_target_out_writef(model_target_out(), "materialization_status: unsupported\n");
    model_target_out_writef(model_target_out(), "runtime_descriptor_status: missing\n");
    model_target_out_writef(model_target_out(), "graph_consumer_status: missing\n");
    model_target_out_writef(model_target_out(), "backend_residency_status: missing\n");
    model_target_out_writef(model_target_out(), "downstream_blockers: per_role_qtype=deferred compute_refusal_matrix=deferred calibration_imatrix=deferred artifact_emit=missing artifact_identity=missing runtime_descriptor=missing graph_consumer=missing backend_residency=missing generation_runtime=missing eval_benchmark=missing\n");
    model_target_out_writef(model_target_out(), "next_required_rows: %s\n", profile->next_required_row);
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    print_qtype_policy_boundary();
}

static int print_qtype_policy_unsupported_target(
    const char *target_id,
    yvex_model_target_output_mode output_mode)
{
    yvex_qtype_policy_profile profile;

    qtype_policy_init(&profile, NULL, NULL, target_id);
    profile.status = "unsupported-target";
    profile.top_blocker = "unsupported-target";
    profile.next_required_row = "none";
    if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        print_qtype_policy_table(&profile);
    } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        print_qtype_policy_audit(&profile);
    } else {
        print_qtype_policy_normal(&profile);
    }
    return 2;
}

#define YVEX_QTYPE_ROLE_SUPPORT_ENTRY_CAP 48u

typedef struct {
    const char *role_name;
    const char *role_status;
    char source_dtype[32];
    const char *source_dtype_status;
    unsigned long long source_tensor_count;
    const char *preferred_artifact_qtype;
    const char *candidate_artifact_qtypes;
    const char *refused_artifact_qtypes;
    const char *qtype_policy_status;
    const char *storage_support_status;
    const char *compute_support_status;
    const char *calibration_required;
    const char *imatrix_required;
    const char *artifact_emission_allowed;
    const char *artifact_emission_blocker;
} yvex_qtype_role_support_entry;

typedef struct {
    const char *role_name;
    const char *canonical_needle_a;
    const char *canonical_needle_b;
    const char *native_needle_a;
    const char *native_needle_b;
} yvex_qtype_role_spec;

typedef struct {
    const yvex_model_target_record *record;
    const yvex_model_class_profile_spec *spec;
    const char *status;
    const char *top_blocker;
    const char *next_required_row;
    const char *evidence_basis;
    const char *full_family_artifact_status;
    char target_id[64];
    char family[32];
    char source_path[YVEX_PATH_CAP];
    char source_dtype[32];
    char source_dtype_profile[192];
    unsigned long long source_tensor_count;
    unsigned long long role_count;
    unsigned long long supported_role_count;
    unsigned long long blocked_role_count;
    unsigned int selected_slice_evidence_only;
    yvex_qtype_role_support_entry roles[YVEX_QTYPE_ROLE_SUPPORT_ENTRY_CAP];
} yvex_qtype_role_support_profile;

typedef struct {
    char family[32];
    char target_id[64];
    char status[32];
    unsigned long long role_count;
    unsigned long long blocked_role_count;
    char top_blocker[96];
    char next_required_row[32];
} yvex_qtype_role_support_gate_row;

typedef struct {
    const char *status;
    const char *top_blocker;
    const char *next_required_row;
    yvex_qtype_role_support_gate_row rows[3];
    unsigned long long row_count;
} yvex_qtype_role_support_gate_profile;

static int qtype_role_support_is_present_status(const char *status)
{
    return status &&
           (strcmp(status, "present") == 0 ||
            strcmp(status, "present-report-only") == 0 ||
            strcmp(status, "selected-slice-evidence-only") == 0);
}

static void qtype_role_support_init(
    yvex_qtype_role_support_profile *profile,
    const yvex_model_target_record *record,
    const yvex_model_class_profile_spec *spec,
    const char *target_id)
{
    memset(profile, 0, sizeof(*profile));
    profile->record = record;
    profile->spec = spec;
    profile->status = "blocked";
    profile->top_blocker = "missing-role-support";
    profile->next_required_row = YVEX_QTYPE_ROLE_SUPPORT_NEXT_ROW;
    profile->evidence_basis = "header-and-sidecar-metadata-only";
    profile->full_family_artifact_status = "missing";
    qtype_policy_copy(profile->target_id, sizeof(profile->target_id),
                      target_id ? target_id : (record ? record->target_id : "unknown"));
    qtype_policy_copy(profile->family, sizeof(profile->family),
                      spec ? spec->family_key : (record ? record->family : "unknown"));
    qtype_policy_copy(profile->source_path, sizeof(profile->source_path), "none");
    qtype_policy_copy(profile->source_dtype, sizeof(profile->source_dtype),
                      "missing");
    qtype_policy_copy(profile->source_dtype_profile,
                      sizeof(profile->source_dtype_profile), "missing");
}

static int qtype_role_support_entry_matches(
    const yvex_tensor_naming_entry *entry,
    const yvex_qtype_role_spec *spec)
{
    if (!entry || !spec ||
        strcmp(entry->mapping_status, "mapped-candidate") != 0) {
        return 0;
    }
    if (spec->canonical_needle_a &&
        !strstr(entry->canonical_role, spec->canonical_needle_a)) {
        return 0;
    }
    if (spec->canonical_needle_b &&
        !strstr(entry->canonical_role, spec->canonical_needle_b)) {
        return 0;
    }
    if (spec->native_needle_a &&
        !strstr(entry->native_name, spec->native_needle_a)) {
        return 0;
    }
    if (spec->native_needle_b &&
        !strstr(entry->native_name, spec->native_needle_b)) {
        return 0;
    }
    return 1;
}

static unsigned long long qtype_role_support_count_role(
    const yvex_tensor_naming_profile *naming,
    const yvex_qtype_role_spec *spec,
    char *dtype,
    size_t dtype_cap)
{
    unsigned long long i;
    unsigned long long count = 0;
    char observed[32];

    if (dtype && dtype_cap > 0) dtype[0] = '\0';
    observed[0] = '\0';
    if (!naming || !spec) return 0;
    for (i = 0; i < naming->entry_count; ++i) {
        const yvex_tensor_naming_entry *entry = &naming->entries[i];
        if (!qtype_role_support_entry_matches(entry, spec)) continue;
        count++;
        if (!entry->dtype[0]) continue;
        if (!observed[0]) {
            snprintf(observed, sizeof(observed), "%s", entry->dtype);
        } else if (strcmp(observed, entry->dtype) != 0) {
            snprintf(observed, sizeof(observed), "%s", "mixed");
        }
    }
    if (dtype && dtype_cap > 0) {
        snprintf(dtype, dtype_cap, "%s", count ? (observed[0] ? observed : "unknown") : "missing");
    }
    return count;
}

static void qtype_role_support_add_entry(
    yvex_qtype_role_support_profile *profile,
    const char *role_name,
    const char *role_status,
    const char *source_dtype,
    const char *source_dtype_status,
    unsigned long long source_tensor_count,
    const char *artifact_blocker)
{
    yvex_qtype_role_support_entry *entry;

    if (!profile || !role_name ||
        profile->role_count >= YVEX_QTYPE_ROLE_SUPPORT_ENTRY_CAP) {
        return;
    }
    entry = &profile->roles[profile->role_count++];
    entry->role_name = role_name;
    entry->role_status = role_status ? role_status : "missing";
    snprintf(entry->source_dtype, sizeof(entry->source_dtype), "%s",
             source_dtype && source_dtype[0] ? source_dtype : "missing");
    entry->source_dtype_status = source_dtype_status ? source_dtype_status : "missing";
    entry->source_tensor_count = source_tensor_count;
    entry->preferred_artifact_qtype = "unresolved";
    entry->candidate_artifact_qtypes = qtype_policy_candidate_qtypes();
    entry->refused_artifact_qtypes = qtype_policy_refused_qtypes();
    entry->qtype_policy_status =
        qtype_role_support_is_present_status(entry->role_status)
            ? "role-support-reported"
            : "blocked-missing-role";
    entry->storage_support_status =
        qtype_role_support_is_present_status(entry->role_status)
            ? "header-storage-profiled"
            : "missing-role";
    entry->compute_support_status = "unknown";
    entry->calibration_required = "deferred";
    entry->imatrix_required = "deferred";
    entry->artifact_emission_allowed = "false";
    entry->artifact_emission_blocker =
        artifact_blocker ? artifact_blocker
                         : (qtype_role_support_is_present_status(entry->role_status)
                                ? "qtype-compute-refusal-matrix-missing"
                                : "missing-runtime-role");
    if (qtype_role_support_is_present_status(entry->role_status)) {
        profile->supported_role_count++;
    }
    profile->blocked_role_count++;
}

static void qtype_role_support_add_tensor_role(
    yvex_qtype_role_support_profile *profile,
    const yvex_tensor_naming_profile *naming,
    const yvex_qtype_role_spec *spec)
{
    char dtype[32];
    unsigned long long count;

    count = qtype_role_support_count_role(naming, spec, dtype, sizeof(dtype));
    qtype_role_support_add_entry(profile,
                                 spec->role_name,
                                 count ? "present" : "missing",
                                 dtype,
                                 count ? "profiled" : "missing",
                                 count,
                                 count ? "qtype-compute-refusal-matrix-missing"
                                       : "missing-runtime-role");
}

static void qtype_role_support_add_tokenizer_role(
    yvex_qtype_role_support_profile *profile,
    const yvex_tokenizer_map_profile *tokenizer)
{
    int present;

    present = tokenizer &&
              strcmp(tokenizer->status, "present-report-only") == 0;
    qtype_role_support_add_entry(profile,
                                 "tokenizer_metadata",
                                 present ? "present-report-only" : "missing",
                                 present ? "metadata" : "missing",
                                 present ? "sidecar-profiled" : "missing",
                                 present ? 1ull : 0ull,
                                 present ? "qtype-compute-refusal-matrix-missing"
                                         : "missing-tokenizer-metadata");
}

static void qtype_role_support_finish_dtype_summary(
    yvex_qtype_role_support_profile *profile)
{
    unsigned long long i;
    char dtype[32];

    if (!profile) return;
    dtype[0] = '\0';
    for (i = 0; i < profile->role_count; ++i) {
        const yvex_qtype_role_support_entry *entry = &profile->roles[i];
        if (!qtype_role_support_is_present_status(entry->role_status)) continue;
        if (strcmp(entry->source_dtype, "metadata") == 0 ||
            strcmp(entry->source_dtype, "selected-slice") == 0) {
            continue;
        }
        if (!dtype[0]) {
            snprintf(dtype, sizeof(dtype), "%s", entry->source_dtype);
        } else if (strcmp(dtype, entry->source_dtype) != 0) {
            snprintf(dtype, sizeof(dtype), "%s", "mixed");
        }
    }
    if (!dtype[0]) {
        snprintf(dtype, sizeof(dtype), "%s",
                 profile->selected_slice_evidence_only ? "selected-slice" : "missing");
    }
    qtype_policy_copy(profile->source_dtype, sizeof(profile->source_dtype), dtype);
    snprintf(profile->source_dtype_profile, sizeof(profile->source_dtype_profile),
             "%s", dtype);
}

static const yvex_qtype_role_spec qwen_role_support_specs[] = {
    {"token_embedding", "model.embedding.token.weight", NULL, NULL, NULL},
    {"attention_q_proj", ".attention.q_proj.weight", NULL, NULL, NULL},
    {"attention_k_proj", ".attention.k_proj.weight", NULL, NULL, NULL},
    {"attention_v_proj", ".attention.v_proj.weight", NULL, NULL, NULL},
    {"attention_o_proj", ".attention.o_proj.weight", NULL, NULL, NULL},
    {"attention_q_norm", ".attention.q_norm.weight", NULL, NULL, NULL},
    {"attention_k_norm", ".attention.k_norm.weight", NULL, NULL, NULL},
    {"qwen_linear_attn", ".qwen_linear_attn.", NULL, NULL, NULL},
    {"qwen_linear_attn_A_log", ".qwen_linear_attn.A_log", NULL, NULL, NULL},
    {"qwen_linear_attn_dt_bias", ".qwen_linear_attn.dt_bias", NULL, NULL, NULL},
    {"qwen_linear_attn_conv1d", ".qwen_linear_attn.conv1d", NULL, NULL, NULL},
    {"qwen_linear_attn_in_proj_a", ".qwen_linear_attn.in_proj_a", NULL, NULL, NULL},
    {"qwen_linear_attn_in_proj_b", ".qwen_linear_attn.in_proj_b", NULL, NULL, NULL},
    {"qwen_linear_attn_norm", ".qwen_linear_attn.norm", NULL, NULL, NULL},
    {"input_layernorm", ".attention.norm.weight", NULL, ".input_layernorm.weight", NULL},
    {"post_attention_layernorm", ".mlp.norm.weight", NULL, ".post_attention_layernorm.weight", NULL},
    {"final_norm", "model.final_norm.weight", NULL, NULL, NULL},
    {"moe_router", ".moe.router.weight", NULL, NULL, NULL},
    {"moe_expert_gate_up", ".moe.experts.", ".gate_up_proj.weight", NULL, NULL},
    {"moe_expert_down", ".moe.experts.", ".down_proj.weight", NULL, NULL},
    {"moe_shared_expert", ".moe.shared_expert", NULL, NULL, NULL},
    {"output_head", "model.output_head", NULL, NULL, NULL},
};

static const yvex_qtype_role_spec gemma_role_support_specs[] = {
    {"token_embedding", "model.embedding.token.weight", NULL, NULL, NULL},
    {"attention_q_proj", ".attention.q_proj.weight", NULL, NULL, NULL},
    {"attention_k_proj", ".attention.k_proj.weight", NULL, NULL, NULL},
    {"attention_v_proj", ".attention.v_proj.weight", NULL, NULL, NULL},
    {"attention_o_proj", ".attention.o_proj.weight", NULL, NULL, NULL},
    {"attention_q_norm", ".attention.q_norm.weight", NULL, NULL, NULL},
    {"attention_k_norm", ".attention.k_norm.weight", NULL, NULL, NULL},
    {"dense_mlp_gate", ".mlp.gate_proj.weight", NULL, NULL, NULL},
    {"dense_mlp_up", ".mlp.up_proj.weight", NULL, NULL, NULL},
    {"dense_mlp_down", ".mlp.down_proj.weight", NULL, NULL, NULL},
    {"input_layernorm", ".attention.norm.weight", NULL, ".input_layernorm.weight", NULL},
    {"post_attention_layernorm", ".mlp.norm.weight", NULL, ".post_attention_layernorm.weight", NULL},
    {"pre_feedforward_layernorm", ".mlp.norm.weight", NULL, ".pre_feedforward_layernorm.weight", NULL},
    {"post_feedforward_layernorm", ".mlp.norm.weight", NULL, ".post_feedforward_layernorm.weight", NULL},
    {"layer_scalar", "model.layers.", ".layer_scalar", NULL, NULL},
    {"final_norm", "model.final_norm.weight", NULL, NULL, NULL},
    {"output_head_tied_embedding", "model.output_head", NULL, NULL, NULL},
};

static void qtype_role_support_add_family_roles(
    yvex_qtype_role_support_profile *profile,
    const yvex_tensor_naming_profile *naming,
    const yvex_tokenizer_map_profile *tokenizer)
{
    const yvex_qtype_role_spec *specs;
    size_t count;
    size_t i;

    if (!profile || !profile->spec) return;
    if (strcmp(profile->spec->family_key, "qwen") == 0) {
        specs = qwen_role_support_specs;
        count = sizeof(qwen_role_support_specs) /
                sizeof(qwen_role_support_specs[0]);
    } else {
        specs = gemma_role_support_specs;
        count = sizeof(gemma_role_support_specs) /
                sizeof(gemma_role_support_specs[0]);
    }
    for (i = 0; i < count; ++i) {
        qtype_role_support_add_tensor_role(profile, naming, &specs[i]);
    }
    qtype_role_support_add_tokenizer_role(profile, tokenizer);
}

static void qtype_role_support_finish_status(
    yvex_qtype_role_support_profile *profile)
{
    unsigned long long missing = 0;
    unsigned long long i;

    if (!profile) return;
    for (i = 0; i < profile->role_count; ++i) {
        if (!qtype_role_support_is_present_status(profile->roles[i].role_status)) {
            missing++;
        }
    }
    qtype_role_support_finish_dtype_summary(profile);
    if (profile->selected_slice_evidence_only) {
        profile->status = "blocked";
        profile->top_blocker = "full-family-artifact-missing";
        profile->next_required_row = YVEX_QTYPE_ROLE_SUPPORT_NEXT_ROW;
    } else if (missing > 0) {
        profile->status = "blocked";
        profile->top_blocker = "missing-runtime-role";
        profile->next_required_row = YVEX_QTYPE_POLICY_BACK_ROW;
    } else {
        profile->status = "blocked";
        profile->top_blocker = "qtype-compute-refusal-matrix-missing";
        profile->next_required_row = YVEX_QTYPE_ROLE_SUPPORT_NEXT_ROW;
    }
}

static int build_selected_slice_role_support_profile(
    const yvex_model_target_record *record,
    yvex_qtype_role_support_profile *profile)
{
    qtype_role_support_init(profile, record, NULL,
                            record ? record->target_id : "selected-slice");
    qtype_policy_copy(profile->family, sizeof(profile->family), "deepseek");
    profile->selected_slice_evidence_only = 1u;
    profile->evidence_basis = "selected-slice-artifact-only";
    profile->full_family_artifact_status = "missing";
    qtype_role_support_add_entry(profile, "token_embedding",
                                 "selected-slice-evidence-only",
                                 "selected-slice",
                                 "selected-artifact-profiled",
                                 1ull,
                                 "full-family-artifact-missing");
    if (record && strstr(record->target_id, "rmsnorm")) {
        qtype_role_support_add_entry(profile, "rmsnorm",
                                     "selected-slice-evidence-only",
                                     "selected-slice",
                                     "selected-artifact-profiled",
                                     1ull,
                                     "full-family-artifact-missing");
    }
    qtype_role_support_finish_status(profile);
    return 0;
}

static int build_qtype_role_support_profile(
    const yvex_model_target_record *record,
    const yvex_model_class_profile_spec *spec_override,
    const char *models_root_override,
    const char *source_override,
    yvex_qtype_role_support_profile *profile)
{
    yvex_tensor_naming_profile naming;
    yvex_tokenizer_map_profile tokenizer;
    const yvex_model_class_profile_spec *spec;
    int rc;

    if (!record || !profile) return 2;
    spec = spec_override ? spec_override :
           find_model_class_profile_spec(record->target_id);
    if (strcmp(record->target_class, "selected-runtime-slice") == 0) {
        return build_selected_slice_role_support_profile(record, profile);
    }

    qtype_role_support_init(profile, record, spec, record->target_id);
    if (!spec) {
        profile->status = "unsupported";
        profile->top_blocker = "unsupported-family";
        profile->next_required_row = "none";
        return 0;
    }
    if (strcmp(spec->family_key, "qwen") != 0 &&
        strcmp(spec->family_key, "gemma") != 0) {
        profile->status = "unsupported";
        profile->top_blocker = "unsupported-family";
        profile->next_required_row = "none";
        return 0;
    }
    if (strcmp(record->target_class, "source-model-candidate") != 0) {
        profile->status = "blocked";
        profile->top_blocker = "unsupported-target-class";
        profile->next_required_row = "none";
        return 0;
    }

    rc = build_tensor_naming_profile(record, spec, models_root_override,
                                     source_override, &naming);
    if (rc != 0) return rc;
    rc = build_tokenizer_map_profile(record, spec, models_root_override,
                                     source_override, &tokenizer);
    if (rc != 0) return rc;

    qtype_policy_copy(profile->source_path, sizeof(profile->source_path),
                      naming.source_path);
    profile->source_tensor_count = naming.tensor_count;
    if (!naming.source_exists) {
        profile->status = "blocked";
        profile->top_blocker = spec->missing_source_blocker;
        profile->next_required_row = YVEX_QTYPE_POLICY_BACK_ROW;
        qtype_role_support_add_family_roles(profile, &naming, &tokenizer);
        qtype_role_support_finish_dtype_summary(profile);
        return 0;
    }

    qtype_role_support_add_family_roles(profile, &naming, &tokenizer);
    qtype_role_support_finish_status(profile);
    return 0;
}

static void print_qtype_role_support_boundary(void)
{
    model_target_out_writef(model_target_out(), "boundary: qtype role report only; no quantization/GGUF/runtime/generation\n");
}

static void print_qtype_role_support_normal(
    const yvex_qtype_role_support_profile *profile)
{
    model_target_out_writef(model_target_out(), "qtype-role-support: %s\n", profile->target_id);
    model_target_out_writef(model_target_out(), "family: %s\n", profile->family);
    model_target_out_writef(model_target_out(), "status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "source_dtype: %s\n", profile->source_dtype);
    model_target_out_writef(model_target_out(), "preferred_artifact_qtype: unresolved\n");
    model_target_out_writef(model_target_out(), "supported_roles: %llu\n", profile->supported_role_count);
    model_target_out_writef(model_target_out(), "blocked_roles: %llu\n", profile->blocked_role_count);
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next: %s\n", profile->next_required_row);
    print_qtype_role_support_boundary();
}

static void print_qtype_role_support_table(
    const yvex_qtype_role_support_profile *profile)
{
    unsigned long long i;

    model_target_out_writef(model_target_out(), "QTYPE ROLE SUPPORT\n\n");
    model_target_out_writef(model_target_out(), "%-32s  %-10s  %-14s  %-22s  %-8s  %-11s  %s\n",
           "ROLE", "SRC_DTYPE", "ARTIFACT_QTYPE", "STORAGE",
           "COMPUTE", "CALIBRATION", "STATUS");
    for (i = 0; i < profile->role_count; ++i) {
        const yvex_qtype_role_support_entry *entry = &profile->roles[i];
        model_target_out_writef(model_target_out(), "%-32s  %-10s  %-14s  %-22s  %-8s  %-11s  %s\n",
               entry->role_name,
               entry->source_dtype,
               entry->preferred_artifact_qtype,
               entry->storage_support_status,
               entry->compute_support_status,
               entry->calibration_required,
               entry->role_status);
    }
}

static void print_qtype_role_support_audit(
    const yvex_qtype_role_support_profile *profile)
{
    unsigned long long i;

    model_target_out_writef(model_target_out(), "report: qtype-role-support\n");
    model_target_out_writef(model_target_out(), "status: %s\n", profile->status);
    model_target_out_writef(model_target_out(), "target_id: %s\n", profile->target_id);
    model_target_out_writef(model_target_out(), "family: %s\n", profile->family);
    model_target_out_writef(model_target_out(), "target_class: %s\n",
           profile->record ? profile->record->target_class : "unknown");
    model_target_out_writef(model_target_out(), "source_path: %s\n", profile->source_path);
    model_target_out_writef(model_target_out(), "source_dtype: %s\n", profile->source_dtype);
    model_target_out_writef(model_target_out(), "source_tensor_count: %llu\n", profile->source_tensor_count);
    model_target_out_writef(model_target_out(), "role_count: %llu\n", profile->role_count);
    model_target_out_writef(model_target_out(), "supported_role_count: %llu\n", profile->supported_role_count);
    model_target_out_writef(model_target_out(), "blocked_role_count: %llu\n", profile->blocked_role_count);
    model_target_out_writef(model_target_out(), "evidence_basis: %s\n", profile->evidence_basis);
    model_target_out_writef(model_target_out(), "selected_slice_evidence_only: %s\n",
           profile->selected_slice_evidence_only ? "true" : "false");
    model_target_out_writef(model_target_out(), "full_family_artifact_status: %s\n",
           profile->full_family_artifact_status);
    for (i = 0; i < profile->role_count; ++i) {
        const yvex_qtype_role_support_entry *entry = &profile->roles[i];
        model_target_out_writef(model_target_out(), "role.%llu.target_id: %s\n", i, profile->target_id);
        model_target_out_writef(model_target_out(), "role.%llu.family: %s\n", i, profile->family);
        model_target_out_writef(model_target_out(), "role.%llu.role_name: %s\n", i, entry->role_name);
        model_target_out_writef(model_target_out(), "role.%llu.role_status: %s\n", i, entry->role_status);
        model_target_out_writef(model_target_out(), "role.%llu.source_dtype: %s\n", i, entry->source_dtype);
        model_target_out_writef(model_target_out(), "role.%llu.source_dtype_status: %s\n", i,
               entry->source_dtype_status);
        model_target_out_writef(model_target_out(), "role.%llu.source_tensor_count: %llu\n", i,
               entry->source_tensor_count);
        model_target_out_writef(model_target_out(), "role.%llu.preferred_artifact_qtype: %s\n", i,
               entry->preferred_artifact_qtype);
        model_target_out_writef(model_target_out(), "role.%llu.candidate_artifact_qtypes: %s\n", i,
               entry->candidate_artifact_qtypes);
        model_target_out_writef(model_target_out(), "role.%llu.refused_artifact_qtypes: %s\n", i,
               entry->refused_artifact_qtypes);
        model_target_out_writef(model_target_out(), "role.%llu.qtype_policy_status: %s\n", i,
               entry->qtype_policy_status);
        model_target_out_writef(model_target_out(), "role.%llu.storage_support_status: %s\n", i,
               entry->storage_support_status);
        model_target_out_writef(model_target_out(), "role.%llu.compute_support_status: %s\n", i,
               entry->compute_support_status);
        model_target_out_writef(model_target_out(), "role.%llu.calibration_required: %s\n", i,
               entry->calibration_required);
        model_target_out_writef(model_target_out(), "role.%llu.imatrix_required: %s\n", i,
               entry->imatrix_required);
        model_target_out_writef(model_target_out(), "role.%llu.artifact_emission_allowed: %s\n", i,
               entry->artifact_emission_allowed);
        model_target_out_writef(model_target_out(), "role.%llu.artifact_emission_blocker: %s\n", i,
               entry->artifact_emission_blocker);
    }
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", profile->top_blocker);
    model_target_out_writef(model_target_out(), "next_required_rows: %s\n", profile->next_required_row);
    model_target_out_writef(model_target_out(), "payload_bytes_read: false\n");
    model_target_out_writef(model_target_out(), "quantization_performed: false\n");
    model_target_out_writef(model_target_out(), "gguf_emitted: false\n");
    model_target_out_writef(model_target_out(), "materialization_status: unsupported\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    print_qtype_role_support_boundary();
}

static void qtype_role_support_gate_add_row(
    yvex_qtype_role_support_gate_profile *gate,
    const char *family,
    const char *target_id,
    const yvex_qtype_role_support_profile *profile)
{
    yvex_qtype_role_support_gate_row *row;

    if (!gate || gate->row_count >= 3u) return;
    row = &gate->rows[gate->row_count++];
    snprintf(row->family, sizeof(row->family), "%s", family ? family : "unknown");
    snprintf(row->target_id, sizeof(row->target_id), "%s",
             target_id ? target_id : "unknown");
    snprintf(row->status, sizeof(row->status), "%s",
             profile ? profile->status : "blocked");
    row->role_count = profile ? profile->role_count : 0ull;
    row->blocked_role_count = profile ? profile->blocked_role_count : 0ull;
    snprintf(row->top_blocker, sizeof(row->top_blocker), "%s",
             profile ? profile->top_blocker : "downloaded-source-target-missing");
    snprintf(row->next_required_row, sizeof(row->next_required_row), "%s",
             profile ? profile->next_required_row
                     : YVEX_QTYPE_ROLE_SUPPORT_NEXT_ROW);
}

static int build_qtype_role_support_gate_profile(
    const char *models_root,
    yvex_qtype_role_support_gate_profile *gate)
{
    yvex_qtype_role_support_profile profile;
    yvex_dynamic_source_target dynamic_target;
    const yvex_model_target_record *record;
    int rc;

    if (!gate) return 2;
    memset(gate, 0, sizeof(*gate));
    gate->status = "blocked";
    gate->top_blocker = "qtype-compute-refusal-matrix-missing";
    gate->next_required_row = YVEX_QTYPE_ROLE_SUPPORT_NEXT_ROW;

    record = find_model_target("deepseek4-v4-flash-selected-embed-rmsnorm");
    if (!record) record = find_model_target("deepseek4-v4-flash-selected-embed");
    rc = build_selected_slice_role_support_profile(record, &profile);
    if (rc != 0) return rc;
    qtype_role_support_gate_add_row(gate, "deepseek", "selected-slice", &profile);

    memset(&dynamic_target, 0, sizeof(dynamic_target));
    if (model_target_resolve_dynamic_source_target("qwen3-6-35b-a3b",
                                                   models_root,
                                                   &dynamic_target)) {
        rc = build_qtype_role_support_profile(&dynamic_target.record,
                                              &dynamic_target.spec,
                                              models_root,
                                              dynamic_target.source_path,
                                              &profile);
        if (rc != 0) return rc;
        qtype_role_support_gate_add_row(gate, "qwen",
                                        dynamic_target.record.target_id,
                                        &profile);
    } else {
        qtype_role_support_gate_add_row(gate, "qwen", "qwen3-6-35b-a3b", NULL);
    }

    memset(&dynamic_target, 0, sizeof(dynamic_target));
    if (model_target_resolve_dynamic_source_target("gemma-4-31b-it",
                                                   models_root,
                                                   &dynamic_target)) {
        rc = build_qtype_role_support_profile(&dynamic_target.record,
                                              &dynamic_target.spec,
                                              models_root,
                                              dynamic_target.source_path,
                                              &profile);
        if (rc != 0) return rc;
        qtype_role_support_gate_add_row(gate, "gemma",
                                        dynamic_target.record.target_id,
                                        &profile);
    } else {
        qtype_role_support_gate_add_row(gate, "gemma", "gemma-4-31b-it", NULL);
    }
    return 0;
}

static void print_qtype_role_support_gate_table(
    const yvex_qtype_role_support_gate_profile *gate)
{
    unsigned long long i;

    model_target_out_writef(model_target_out(), "%-9s  %-29s  %-7s  %-5s  %-7s  %-39s  %s\n",
           "FAMILY", "TARGET", "STATUS", "ROLES", "BLOCKED",
           "TOP_BLOCKER", "NEXT");
    for (i = 0; i < gate->row_count; ++i) {
        const yvex_qtype_role_support_gate_row *row = &gate->rows[i];
        model_target_out_writef(model_target_out(), "%-9s  %-29s  %-7s  %-5llu  %-7llu  %-39s  %s\n",
               row->family,
               row->target_id,
               row->status,
               row->role_count,
               row->blocked_role_count,
               row->top_blocker,
               row->next_required_row);
    }
}

static void print_qtype_role_support_gate_normal(
    const yvex_qtype_role_support_gate_profile *gate)
{
    model_target_out_writef(model_target_out(), "qtype-role-support-gate: v0.1.0\n");
    model_target_out_writef(model_target_out(), "status: %s\n", gate->status);
    print_qtype_role_support_gate_table(gate);
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", gate->top_blocker);
    model_target_out_writef(model_target_out(), "next: %s\n", gate->next_required_row);
    print_qtype_role_support_boundary();
}

static void print_qtype_role_support_gate_audit(
    const yvex_qtype_role_support_gate_profile *gate)
{
    unsigned long long i;

    model_target_out_writef(model_target_out(), "report: qtype-role-support-gate\n");
    model_target_out_writef(model_target_out(), "status: %s\n", gate->status);
    model_target_out_writef(model_target_out(), "gate: v0.1.0\n");
    model_target_out_writef(model_target_out(), "family_count: %llu\n", gate->row_count);
    for (i = 0; i < gate->row_count; ++i) {
        const yvex_qtype_role_support_gate_row *row = &gate->rows[i];
        model_target_out_writef(model_target_out(), "family.%llu.family: %s\n", i, row->family);
        model_target_out_writef(model_target_out(), "family.%llu.target_id: %s\n", i, row->target_id);
        model_target_out_writef(model_target_out(), "family.%llu.status: %s\n", i, row->status);
        model_target_out_writef(model_target_out(), "family.%llu.role_count: %llu\n", i, row->role_count);
        model_target_out_writef(model_target_out(), "family.%llu.blocked_role_count: %llu\n", i,
               row->blocked_role_count);
        model_target_out_writef(model_target_out(), "family.%llu.top_blocker: %s\n", i, row->top_blocker);
        model_target_out_writef(model_target_out(), "family.%llu.next_required_rows: %s\n", i,
               row->next_required_row);
    }
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", gate->top_blocker);
    model_target_out_writef(model_target_out(), "next_required_rows: %s\n", gate->next_required_row);
    model_target_out_writef(model_target_out(), "payload_bytes_read: false\n");
    model_target_out_writef(model_target_out(), "quantization_performed: false\n");
    model_target_out_writef(model_target_out(), "gguf_emitted: false\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    print_qtype_role_support_boundary();
}

typedef struct {
    yvex_output_contract_report report;
    yvex_model_target_output_mode mode;
    const void *profile;
} yvex_output_contract_render_context;

typedef struct {
    unsigned long long line_count;
    unsigned long long audit_prefix_hits;
    unsigned long long forbidden_hits;
    unsigned long long detail_dump_hits;
    unsigned long long table_header_hits;
    unsigned long long compact_header_hits;
    unsigned long long top_or_next_hits;
    unsigned long long audit_a_hits;
    unsigned long long audit_b_hits;
    unsigned long long audit_c_hits;
    unsigned long long runtime_claim_hits;
    unsigned long long generation_hits;
    unsigned long long benchmark_hits;
    unsigned long long release_hits;
    unsigned long long positive_claim_hits;
} yvex_output_contract_stats;

static int output_contract_starts_with(const char *line, const char *prefix)
{
    size_t n;

    if (!line || !prefix) return 0;
    n = strlen(prefix);
    return strncmp(line, prefix, n) == 0;
}

static const char *output_contract_report_name(
    yvex_output_contract_report report)
{
    if (report == YVEX_OUTPUT_CONTRACT_OUTPUT_HEAD) {
        return "model-target tensor-map output-head";
    }
    if (report == YVEX_OUTPUT_CONTRACT_MISSING_ROLES) {
        return "model-target tensor-map missing-roles";
    }
    if (report == YVEX_OUTPUT_CONTRACT_MAPPING_GATE) {
        return "model-target tensor-map gate";
    }
    if (report == YVEX_OUTPUT_CONTRACT_QTYPE_POLICY) {
        return "model-target quant-policy";
    }
    return "model-target tensor-map";
}

static const char *output_contract_render_path_name(
    yvex_model_target_output_mode mode)
{
    if (mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) return "table";
    if (mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) return "audit";
    return "compact";
}

static unsigned long long output_contract_line_limit(
    yvex_output_contract_report report,
    yvex_model_target_output_mode mode)
{
    if (mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) return 0;
    if (report == YVEX_OUTPUT_CONTRACT_TENSOR_MAP) return 20;
    if (report == YVEX_OUTPUT_CONTRACT_OUTPUT_HEAD) return 12;
    if (report == YVEX_OUTPUT_CONTRACT_MISSING_ROLES) {
        return mode == YVEX_MODEL_TARGET_OUTPUT_TABLE ? 12 : 14;
    }
    if (report == YVEX_OUTPUT_CONTRACT_MAPPING_GATE) {
        return mode == YVEX_MODEL_TARGET_OUTPUT_TABLE ? 12 : 14;
    }
    return 12;
}

static int output_contract_is_table_header(
    yvex_output_contract_report report,
    const char *line)
{
    if (report == YVEX_OUTPUT_CONTRACT_TENSOR_MAP) {
        return output_contract_starts_with(line, "TENSOR NAMING MAP");
    }
    if (report == YVEX_OUTPUT_CONTRACT_OUTPUT_HEAD) {
        return output_contract_starts_with(line, "OUTPUT HEAD TENSOR MAP");
    }
    if (report == YVEX_OUTPUT_CONTRACT_MISSING_ROLES) {
        return output_contract_starts_with(line, "MISSING ROLE BLOCKER REPORT");
    }
    if (report == YVEX_OUTPUT_CONTRACT_MAPPING_GATE) {
        return output_contract_starts_with(line, "TENSOR MAPPING GATE");
    }
    return output_contract_starts_with(line, "QTYPE POLICY");
}

static int output_contract_is_compact_header(
    yvex_output_contract_report report,
    const char *line)
{
    if (report == YVEX_OUTPUT_CONTRACT_TENSOR_MAP) {
        return output_contract_starts_with(line, "tensor-map:");
    }
    if (report == YVEX_OUTPUT_CONTRACT_OUTPUT_HEAD) {
        return output_contract_starts_with(line, "output-head-map:");
    }
    if (report == YVEX_OUTPUT_CONTRACT_MISSING_ROLES) {
        return output_contract_starts_with(line, "missing-roles:");
    }
    if (report == YVEX_OUTPUT_CONTRACT_MAPPING_GATE) {
        return output_contract_starts_with(line, "tensor-mapping-gate:");
    }
    return output_contract_starts_with(line, "qtype-policy:");
}

static int output_contract_line_has_detail_dump(const char *line)
{
    return strstr(line, "tensor_map.entry.") ||
           strstr(line, "output_head.entry.") ||
           strstr(line, "missing_role.entry.") ||
           strstr(line, "map_source.") ||
           output_contract_starts_with(line, "downstream_blockers:");
}

static int output_contract_line_has_audit_prefix(
    yvex_output_contract_report report,
    const char *line)
{
    if (report == YVEX_OUTPUT_CONTRACT_TENSOR_MAP) {
        return strstr(line, "tensor_map.entry.") != NULL;
    }
    if (report == YVEX_OUTPUT_CONTRACT_OUTPUT_HEAD) {
        return strstr(line, "output_head_map_") ||
               strstr(line, "output_head.entry.");
    }
    if (report == YVEX_OUTPUT_CONTRACT_MISSING_ROLES) {
        return strstr(line, "missing_role.entry.") ||
               output_contract_starts_with(line, "missing_role_report_status:");
    }
    if (report == YVEX_OUTPUT_CONTRACT_MAPPING_GATE) {
        return output_contract_starts_with(line, "tensor_mapping_gate_source_path:") ||
               output_contract_starts_with(line, "tensor_mapping_gate_tensor_map_sidecar_path:") ||
               strstr(line, "map_source.");
    }
    return output_contract_starts_with(line, "qtype_policy_status:") ||
           output_contract_starts_with(line, "source_declared_data_bytes:") ||
           output_contract_starts_with(line, "downstream_blockers:");
}

static int output_contract_line_forbidden(
    yvex_output_contract_report report,
    yvex_model_target_output_mode mode,
    const char *line)
{
    if (output_contract_line_has_audit_prefix(report, line)) return 1;
    if (output_contract_starts_with(line, "runtime_claim:")) return 1;
    if (output_contract_starts_with(line, "generation:")) return 1;
    if (output_contract_starts_with(line, "benchmark_status:")) return 1;
    if (output_contract_starts_with(line, "release_ready:")) return 1;
    if (mode == YVEX_MODEL_TARGET_OUTPUT_NORMAL &&
        output_contract_starts_with(line, "next_required_rows:")) {
        return 1;
    }
    if (mode == YVEX_MODEL_TARGET_OUTPUT_TABLE &&
        output_contract_starts_with(line, "boundary:")) {
        return 1;
    }
    return 0;
}

static void output_contract_count_audit_requirement(
    yvex_output_contract_report report,
    const char *line,
    yvex_output_contract_stats *stats)
{
    if (report == YVEX_OUTPUT_CONTRACT_TENSOR_MAP) {
        if (strstr(line, "tensor_map.entry.")) stats->audit_a_hits++;
    } else if (report == YVEX_OUTPUT_CONTRACT_OUTPUT_HEAD) {
        if (output_contract_starts_with(line, "output_head_map_status:")) {
            stats->audit_a_hits++;
        }
        if (strstr(line, "output_head.entry.")) stats->audit_b_hits++;
    } else if (report == YVEX_OUTPUT_CONTRACT_MISSING_ROLES) {
        if (output_contract_starts_with(line, "missing_role_report_status:")) {
            stats->audit_a_hits++;
        }
        if (strstr(line, "missing_role.entry.")) stats->audit_b_hits++;
    } else if (report == YVEX_OUTPUT_CONTRACT_MAPPING_GATE) {
        if (output_contract_starts_with(line, "tensor_mapping_gate_status:")) {
            stats->audit_a_hits++;
        }
        if (strstr(line, "map_source.")) stats->audit_b_hits++;
    } else {
        if (output_contract_starts_with(line, "qtype_policy_status:")) {
            stats->audit_a_hits++;
        }
        if (output_contract_starts_with(line, "source_dtype_counts:")) {
            stats->audit_b_hits++;
        }
        if (output_contract_starts_with(line, "downstream_blockers:")) {
            stats->audit_c_hits++;
        }
    }
}

static void output_contract_analyze_line(
    yvex_output_contract_report report,
    yvex_model_target_output_mode mode,
    const char *line,
    yvex_output_contract_stats *stats)
{
    stats->line_count++;
    if (output_contract_line_has_audit_prefix(report, line)) {
        stats->audit_prefix_hits++;
    }
    if (output_contract_line_forbidden(report, mode, line)) {
        stats->forbidden_hits++;
    }
    if (output_contract_line_has_detail_dump(line)) {
        stats->detail_dump_hits++;
    }
    if (output_contract_is_table_header(report, line)) {
        stats->table_header_hits++;
    }
    if (output_contract_is_compact_header(report, line)) {
        stats->compact_header_hits++;
    }
    if (output_contract_starts_with(line, "top_blocker:") ||
        output_contract_starts_with(line, "next:")) {
        stats->top_or_next_hits++;
    }
    if (output_contract_starts_with(line, "runtime_claim: unsupported")) {
        stats->runtime_claim_hits++;
    }
    if (output_contract_starts_with(line, "generation: unsupported-full-model")) {
        stats->generation_hits++;
    }
    if (output_contract_starts_with(line, "benchmark_status: not-measured")) {
        stats->benchmark_hits++;
    }
    if (output_contract_starts_with(line, "release_ready: false")) {
        stats->release_hits++;
    }
    if (strstr(line, "generation_ready: " "true") ||
        strstr(line, "release_ready: " "true") ||
        strstr(line, "benchmark_status: " "measured") ||
        strstr(line, "throughput " "achieved") ||
        strstr(line, "runtime_claim: " "supported")) {
        stats->positive_claim_hits++;
    }
    output_contract_count_audit_requirement(report, line, stats);
}

static void output_contract_render_profile(void *opaque)
{
    const yvex_output_contract_render_context *ctx =
        (const yvex_output_contract_render_context *)opaque;

    if (!ctx) return;
    if (ctx->report == YVEX_OUTPUT_CONTRACT_TENSOR_MAP) {
        const yvex_tensor_naming_profile *p =
            (const yvex_tensor_naming_profile *)ctx->profile;
        if (ctx->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_tensor_naming_table(p);
        } else if (ctx->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_tensor_naming_audit(p);
        } else {
            print_tensor_naming_normal(p);
        }
    } else if (ctx->report == YVEX_OUTPUT_CONTRACT_OUTPUT_HEAD) {
        const yvex_output_head_map_profile *p =
            (const yvex_output_head_map_profile *)ctx->profile;
        if (ctx->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_output_head_map_table(p);
        } else if (ctx->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_output_head_map_audit(p);
        } else {
            print_output_head_map_normal(p);
        }
    } else if (ctx->report == YVEX_OUTPUT_CONTRACT_MISSING_ROLES) {
        const yvex_missing_role_report_profile *p =
            (const yvex_missing_role_report_profile *)ctx->profile;
        if (ctx->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_missing_role_report_table(p);
        } else if (ctx->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_missing_role_report_audit(p);
        } else {
            print_missing_role_report_normal(p);
        }
    } else if (ctx->report == YVEX_OUTPUT_CONTRACT_MAPPING_GATE) {
        const yvex_tensor_mapping_gate_profile *p =
            (const yvex_tensor_mapping_gate_profile *)ctx->profile;
        if (ctx->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_tensor_mapping_gate_table(p);
        } else if (ctx->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_tensor_mapping_gate_audit(p);
        } else {
            print_tensor_mapping_gate_normal(p);
        }
    } else {
        const yvex_qtype_policy_profile *p =
            (const yvex_qtype_policy_profile *)ctx->profile;
        if (ctx->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_qtype_policy_table(p);
        } else if (ctx->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_qtype_policy_audit(p);
        } else {
            print_qtype_policy_normal(p);
        }
    }
}

static int output_contract_capture_stats(
    yvex_output_contract_report report,
    yvex_model_target_output_mode mode,
    const void *profile,
    yvex_output_contract_stats *stats)
{
    yvex_output_contract_render_context ctx;
    FILE *tmp;
    char line[8192];
    FILE *previous_out;

    if (!profile || !stats) return 0;
    memset(stats, 0, sizeof(*stats));
    tmp = tmpfile();
    if (!tmp) return 0;
    previous_out = model_target_capture_out;
    model_target_capture_out = tmp;
    ctx.report = report;
    ctx.mode = mode;
    ctx.profile = profile;
    output_contract_render_profile(&ctx);
    fflush(tmp);
    model_target_capture_out = previous_out;

    rewind(tmp);
    while (fgets(line, sizeof(line), tmp)) {
        output_contract_analyze_line(report, mode, line, stats);
    }
    fclose(tmp);
    return 1;
}

static int output_contract_audit_evidence_required(
    yvex_output_contract_report report,
    const void *profile)
{
    if (report == YVEX_OUTPUT_CONTRACT_TENSOR_MAP) {
        const yvex_tensor_naming_profile *p =
            (const yvex_tensor_naming_profile *)profile;
        return p && p->entry_count > 0;
    }
    if (report == YVEX_OUTPUT_CONTRACT_MISSING_ROLES) {
        const yvex_missing_role_report_profile *p =
            (const yvex_missing_role_report_profile *)profile;
        return p && p->missing_entry_count > 0;
    }
    return 1;
}

static int output_contract_audit_evidence_present(
    yvex_output_contract_report report,
    const yvex_output_contract_stats *stats,
    int required)
{
    if (!required) return 1;
    if (report == YVEX_OUTPUT_CONTRACT_TENSOR_MAP) {
        return stats->audit_a_hits > 0;
    }
    if (report == YVEX_OUTPUT_CONTRACT_OUTPUT_HEAD ||
        report == YVEX_OUTPUT_CONTRACT_MISSING_ROLES ||
        report == YVEX_OUTPUT_CONTRACT_MAPPING_GATE) {
        return stats->audit_a_hits > 0 && stats->audit_b_hits > 0;
    }
    return stats->audit_a_hits > 0 &&
           stats->audit_b_hits > 0 &&
           stats->audit_c_hits > 0;
}

static const char *output_contract_status(
    yvex_output_contract_report report,
    yvex_model_target_output_mode mode,
    const yvex_output_contract_stats *stats,
    unsigned long long limit,
    int audit_required,
    int captured)
{
    if (!captured) return "fail-render-path";
    if (stats->positive_claim_hits > 0) return "fail-render-path";
    if (mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        if (!output_contract_audit_evidence_present(report, stats,
                                                    audit_required)) {
            return "fail-audit-evidence-missing";
        }
        if (stats->runtime_claim_hits == 0 ||
            stats->generation_hits == 0 ||
            stats->benchmark_hits == 0 ||
            stats->release_hits == 0) {
            return "fail-audit-evidence-missing";
        }
        return "pass";
    }
    if (mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        if (stats->table_header_hits == 0) return "fail-render-path";
        if (stats->forbidden_hits > 0 || stats->detail_dump_hits > 0) {
            return "fail-table-wall";
        }
        if (limit > 0 && stats->line_count > limit) {
            return "fail-line-budget";
        }
        return "pass";
    }
    if (stats->compact_header_hits == 0 || stats->top_or_next_hits == 0) {
        return "fail-render-path";
    }
    if (stats->forbidden_hits > 0 || stats->detail_dump_hits > 0) {
        return "fail-audit-leak";
    }
    if (limit > 0 && stats->line_count > limit) return "fail-line-budget";
    return "pass";
}

static int output_contract_print_result(
    yvex_output_contract_report report,
    const char *target_id,
    yvex_model_target_output_mode mode,
    const void *profile)
{
    yvex_output_contract_stats stats;
    unsigned long long limit;
    int captured;
    int audit_required;
    const char *status;
    const char *audit_evidence;
    const char *table_only;

    limit = output_contract_line_limit(report, mode);
    captured = output_contract_capture_stats(report, mode, profile, &stats);
    audit_required = output_contract_audit_evidence_required(report, profile);
    status = output_contract_status(report, mode, &stats, limit,
                                    audit_required, captured);
    if (mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        audit_evidence =
            output_contract_audit_evidence_present(report, &stats,
                                                   audit_required)
                ? (audit_required ? "present" : "not-required")
                : "missing";
    } else {
        audit_evidence = "not-required";
    }
    if (mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        table_only = (stats.table_header_hits > 0 &&
                      stats.forbidden_hits == 0 &&
                      stats.detail_dump_hits == 0)
                         ? "true"
                         : "false";
    } else {
        table_only = "not-applicable";
    }

    model_target_out_writef(model_target_out(), "output-contract: %s\n", output_contract_report_name(report));
    model_target_out_writef(model_target_out(), "target: %s\n", target_id && target_id[0] ? target_id : "none");
    model_target_out_writef(model_target_out(), "mode: %s\n", model_target_output_mode_name(mode));
    model_target_out_writef(model_target_out(), "status: %s\n", status);
    model_target_out_writef(model_target_out(), "render_path: %s\n", output_contract_render_path_name(mode));
    model_target_out_writef(model_target_out(), "line_count: %llu\n", stats.line_count);
    if (mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        model_target_out_writef(model_target_out(), "line_limit: unbounded\n");
    } else {
        model_target_out_writef(model_target_out(), "line_limit: %llu\n", limit);
    }
    model_target_out_writef(model_target_out(), "audit_prefix_hits: %llu\n", stats.audit_prefix_hits);
    model_target_out_writef(model_target_out(), "detail_dump: %s\n",
           stats.detail_dump_hits > 0 ? "present" : "suppressed");
    model_target_out_writef(model_target_out(), "table_only: %s\n", table_only);
    model_target_out_writef(model_target_out(), "audit_evidence: %s\n", audit_evidence);
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    model_target_out_writef(model_target_out(), "boundary: output-contract check only; no runtime/generation claim\n");
    return strcmp(status, "pass") == 0 ? 0 : 1;
}

static int output_contract_print_refusal(const char *report,
                                         const char *target_id,
                                         const char *mode,
                                         const char *status)
{
    model_target_out_writef(model_target_out(), "output-contract: %s\n", report ? report : "model-target");
    model_target_out_writef(model_target_out(), "target: %s\n", target_id && target_id[0] ? target_id : "none");
    model_target_out_writef(model_target_out(), "mode: %s\n", mode && mode[0] ? mode : "none");
    model_target_out_writef(model_target_out(), "status: %s\n", status);
    model_target_out_writef(model_target_out(), "render_path: not-run\n");
    model_target_out_writef(model_target_out(), "line_count: 0\n");
    model_target_out_writef(model_target_out(), "line_limit: not-run\n");
    model_target_out_writef(model_target_out(), "audit_prefix_hits: 0\n");
    model_target_out_writef(model_target_out(), "detail_dump: not-run\n");
    model_target_out_writef(model_target_out(), "table_only: not-applicable\n");
    model_target_out_writef(model_target_out(), "audit_evidence: not-run\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    model_target_out_writef(model_target_out(), "boundary: output-contract check only; no runtime/generation claim\n");
    return 2;
}

static const char *target_decision_candidate_class(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strcmp(record->target_id, "glm-5.2-official-safetensors") == 0) {
        return "huge-source-pressure";
    }
    return record->target_class ? record->target_class : "unknown";
}

static const char *target_decision_candidate_status(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (target_decision_is_full_runtime_candidate(record)) return "eligible";
    if (strcmp(record->target_class, "selected-runtime-slice") == 0) {
        return "ineligible-selected-slice";
    }
    if (strcmp(record->target_class, "official-source-huge-model") == 0 ||
        strcmp(record->target_id, "glm-5.2-official-safetensors") == 0) {
        return "ineligible-source-only";
    }
    if (strcmp(record->target_class, "source-model-candidate") == 0) {
        return "ineligible-source-model-candidate";
    }
    if (strcmp(record->target_class, "external-GGUF-reference") == 0 ||
        strcmp(record->target_class, "external-runner-reference") == 0) {
        return "ineligible-external-reference";
    }
    return "unknown";
}

static const char *target_decision_candidate_reason(const yvex_model_target_record *record)
{
    if (!record) return "unknown target";
    if (target_decision_is_full_runtime_candidate(record)) {
        return "full-runtime candidate can feed v0.1.0 planning";
    }
    if (strcmp(record->target_id, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0) {
        return "selected-runtime-slice missing MoE router/expert tensor coverage";
    }
    if (strcmp(record->target_class, "selected-runtime-slice") == 0) {
        return "selected-runtime-slice cannot close full-runtime generation";
    }
    if (strcmp(record->target_id, "glm-5.2-official-safetensors") == 0) {
        return "source-only target has no YVEX-produced artifact/runtime path";
    }
    if (strcmp(record->target_class, "source-model-candidate") == 0) {
        return "source model target remains report-only until tensor role mapping, artifact, and runtime evidence exist";
    }
    if (strcmp(record->target_class, "external-GGUF-reference") == 0 ||
        strcmp(record->target_class, "external-runner-reference") == 0) {
        return "external reference cannot close YVEX runtime";
    }
    return "target is not an implemented full-runtime candidate";
}

static const char *target_decision_candidate_next(const yvex_model_target_record *record)
{
    if (!record) return "none";
    if (target_decision_is_full_runtime_candidate(record)) return "class-and-tensor-gates";
    if (strcmp(record->target_id, "glm-5.2-official-safetensors") == 0) {
        return "source/storage pressure";
    }
    if (strcmp(record->target_class, "source-model-candidate") == 0) {
        return "tensor role mapping";
    }
    if (strcmp(record->target_class, "selected-runtime-slice") == 0) return "pressure-only";
    if (strcmp(record->target_class, "external-GGUF-reference") == 0 ||
        strcmp(record->target_class, "external-runner-reference") == 0) {
        return "reference-only";
    }
    return "target-report";
}

static void print_model_target_decision_candidate(unsigned long index,
                                                  const yvex_model_target_record *record)
{
    const char *status;

    status = target_decision_candidate_status(record);
    model_target_out_writef(model_target_out(), "candidate.%lu.id: %s\n", index, record->target_id);
    model_target_out_writef(model_target_out(), "candidate.%lu.class: %s\n", index, target_decision_candidate_class(record));
    model_target_out_writef(model_target_out(), "candidate.%lu.family: %s\n", index, record->family);
    model_target_out_writef(model_target_out(), "candidate.%lu.model: %s\n", index, record->model);
    model_target_out_writef(model_target_out(), "candidate.%lu.source_class: %s\n", index, record->source_artifact_class);
    model_target_out_writef(model_target_out(), "candidate.%lu.artifact_class: %s\n", index, record->target_artifact_class);
    model_target_out_writef(model_target_out(), "candidate.%lu.status: %s\n", index, status);
    model_target_out_writef(model_target_out(), "candidate.%lu.eligible_for_v010: %s\n", index,
           strcmp(status, "eligible") == 0 ? "true" : "false");
    model_target_out_writef(model_target_out(), "candidate.%lu.reason: %s\n", index, target_decision_candidate_reason(record));
    model_target_out_writef(model_target_out(), "candidate.%lu.next: %s\n", index, target_decision_candidate_next(record));
}

static void print_target_decision_constant_tail(void)
{
    model_target_out_writef(model_target_out(), "release_critical_tracks: TRACK.TARGET,TRACK.SOURCE,TRACK.ARTIFACT,TRACK.INTEGRITY,TRACK.MODEL,TRACK.TENSOR,TRACK.RESIDENCY,TRACK.BACKEND,TRACK.GRAPH,TRACK.PREFILL,TRACK.KV,TRACK.DECODE,TRACK.LOGITS,TRACK.SAMPLING,TRACK.TOKENIZER,TRACK.GENERATION,TRACK.OPERATOR,TRACK.CI,TRACK.RELEASE\n");
    model_target_out_writef(model_target_out(), "blocked_tracks: TRACK.TARGET,TRACK.ARTIFACT,TRACK.MODEL,TRACK.TENSOR,TRACK.GRAPH,TRACK.PREFILL,TRACK.KV,TRACK.DECODE,TRACK.LOGITS,TRACK.SAMPLING,TRACK.GENERATION\n");
    model_target_out_writef(model_target_out(), "excluded_tracks: TRACK.SERVE,TRACK.EVAL,TRACK.BENCH,TRACK.SPEC\n");
    model_target_out_writef(model_target_out(), "post010_tracks: TRACK.SERVE,TRACK.BENCH,TRACK.SPEC,TRACK.POST010\n");
    model_target_out_writef(model_target_out(), "blocking_rows: V010.TARGET.2,V010.TARGET.3,V010.TARGET.4,V010.MAP.2,V010.ARTIFACT.EMIT.2,V010.FULLMODEL.6\n");
    model_target_out_writef(model_target_out(), "next_required_rows: V010.TARGET.2\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
}

static const yvex_full_runtime_candidate_fact *find_full_runtime_candidate_fact(const char *id)
{
    unsigned long i;

    if (!id) return NULL;
    for (i = 0; i < full_runtime_candidate_fact_count; ++i) {
        if (strcmp(full_runtime_candidate_facts[i].id, id) == 0) {
            return &full_runtime_candidate_facts[i];
        }
    }
    return NULL;
}

static int open_candidate_registry(yvex_model_registry **registry)
{
    yvex_model_registry_options options;
    yvex_error err;

    if (!registry) return YVEX_ERR_INVALID_ARG;
    *registry = NULL;
    memset(&options, 0, sizeof(options));
    yvex_error_clear(&err);
    options.create_if_missing = 0;
    return yvex_model_registry_open(registry, &options, &err);
}

static unsigned long candidate_registry_extra_count(const yvex_model_registry *registry)
{
    unsigned long long i;
    unsigned long count = 0;

    if (!registry) return 0;
    for (i = 0; i < yvex_model_registry_count(registry); ++i) {
        const yvex_model_registry_entry *entry = yvex_model_registry_at(registry, i);
        if (!entry || !entry->alias || find_full_runtime_candidate_fact(entry->alias)) continue;
        count++;
    }
    return count;
}

static const char *registered_candidate_tensor_status(const yvex_model_registry_entry *entry)
{
    if (!entry) return "unknown";
    return entry->tensor_count > 0 ? "registered-partial" : "unknown";
}

static void print_full_runtime_candidate_fact(unsigned long index,
                                              const yvex_full_runtime_candidate_fact *fact,
                                              int include_blockers,
                                              int include_next)
{
    unsigned int i;

    if (!fact) return;
    model_target_out_writef(model_target_out(), "candidate_%lu_id: %s\n", index, fact->id);
    model_target_out_writef(model_target_out(), "candidate_%lu_class: %s\n", index, fact->class_name);
    model_target_out_writef(model_target_out(), "candidate_%lu_stage: %s\n", index, fact->stage);
    model_target_out_writef(model_target_out(), "candidate_%lu_eligibility: %s\n", index, fact->eligibility);
    model_target_out_writef(model_target_out(), "candidate_%lu_artifact_status: %s\n", index, fact->artifact_status);
    model_target_out_writef(model_target_out(), "candidate_%lu_source_status: %s\n", index, fact->source_status);
    model_target_out_writef(model_target_out(), "candidate_%lu_tensor_coverage_status: %s\n", index, fact->tensor_coverage_status);
    model_target_out_writef(model_target_out(), "candidate_%lu_runtime_path_status: %s\n", index, fact->runtime_path_status);
    model_target_out_writef(model_target_out(), "candidate_%lu_generation_status: %s\n", index, fact->generation_status);
    model_target_out_writef(model_target_out(), "candidate_%lu_benchmark_status: %s\n", index, fact->benchmark_status);
    model_target_out_writef(model_target_out(), "candidate_%lu_blocker_count: %u\n", index, fact->blocker_count);
    if (include_blockers) {
        for (i = 0; i < fact->blocker_count; ++i) {
            model_target_out_writef(model_target_out(), "candidate_%lu_blocker_%u: %s\n", index, i, fact->blockers[i]);
        }
    }
    if (include_next) {
        model_target_out_writef(model_target_out(), "candidate_%lu_next_required_rows: %s\n", index, fact->next_required_rows);
    }
}

static void print_registered_candidate(unsigned long index,
                                       const yvex_model_registry_entry *entry,
                                       int include_blockers,
                                       int include_next)
{
    const char *alias = entry && entry->alias ? entry->alias : "unknown-registered-alias";

    model_target_out_writef(model_target_out(), "candidate_%lu_id: %s\n", index, alias);
    model_target_out_writef(model_target_out(), "candidate_%lu_class: registered-alias\n", index);
    model_target_out_writef(model_target_out(), "candidate_%lu_stage: report-only\n", index);
    model_target_out_writef(model_target_out(), "candidate_%lu_eligibility: candidate-incomplete\n", index);
    model_target_out_writef(model_target_out(), "candidate_%lu_artifact_status: registered-artifact-not-inspected\n", index);
    model_target_out_writef(model_target_out(), "candidate_%lu_source_status: unknown\n", index);
    model_target_out_writef(model_target_out(), "candidate_%lu_tensor_coverage_status: %s\n", index,
           registered_candidate_tensor_status(entry));
    model_target_out_writef(model_target_out(), "candidate_%lu_runtime_path_status: unsupported\n", index);
    model_target_out_writef(model_target_out(), "candidate_%lu_generation_status: unsupported-full-model\n", index);
    model_target_out_writef(model_target_out(), "candidate_%lu_benchmark_status: not-measured\n", index);
    model_target_out_writef(model_target_out(), "candidate_%lu_blocker_count: 8\n", index);
    if (include_blockers) {
        model_target_out_writef(model_target_out(), "candidate_%lu_blocker_0: missing-source-inventory\n", index);
        model_target_out_writef(model_target_out(), "candidate_%lu_blocker_1: missing-tensor-map\n", index);
        model_target_out_writef(model_target_out(), "candidate_%lu_blocker_2: missing-required-tensor-coverage\n", index);
        model_target_out_writef(model_target_out(), "candidate_%lu_blocker_3: missing-tokenizer-metadata\n", index);
        model_target_out_writef(model_target_out(), "candidate_%lu_blocker_4: missing-output-head\n", index);
        model_target_out_writef(model_target_out(), "candidate_%lu_blocker_5: missing-real-prefill\n", index);
        model_target_out_writef(model_target_out(), "candidate_%lu_blocker_6: missing-real-decode\n", index);
        model_target_out_writef(model_target_out(), "candidate_%lu_blocker_7: missing-real-logits\n", index);
    }
    if (include_next) {
        model_target_out_writef(model_target_out(), "candidate_%lu_next_required_rows: V010.TARGET.3,V010.MAP.*,V010.FULLMODEL.*\n", index);
    }
}

static int print_model_target_candidate_missing(const char *release, const char *target)
{
    model_target_out_writef(model_target_out(), "model-target: candidate\n");
    model_target_out_writef(model_target_out(), "status: full-runtime-candidate-report-fail\n");
    model_target_out_writef(model_target_out(), "release: %s\n", release && release[0] ? release : "v0.1.0");
    model_target_out_writef(model_target_out(), "target_requested: %s\n", target && target[0] ? target : "none");
    model_target_out_writef(model_target_out(), "decision_state: blocked-no-candidate\n");
    model_target_out_writef(model_target_out(), "selected_target_id: none\n");
    model_target_out_writef(model_target_out(), "full_runtime_candidate_status: missing\n");
    model_target_out_writef(model_target_out(), "candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "eligible_candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "pressure_target_count: 0\n");
    model_target_out_writef(model_target_out(), "fixture_target_count: 0\n");
    model_target_out_writef(model_target_out(), "global_blocker: no eligible full-runtime candidate\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    return 2;
}

static int print_model_target_candidate_unsupported_release(const char *release)
{
    model_target_out_writef(model_target_out(), "model-target: candidate\n");
    model_target_out_writef(model_target_out(), "status: unsupported-release\n");
    model_target_out_writef(model_target_out(), "release: %s\n", release && release[0] ? release : "unknown");
    model_target_out_writef(model_target_out(), "decision_state: blocked-no-candidate\n");
    model_target_out_writef(model_target_out(), "selected_target_id: none\n");
    model_target_out_writef(model_target_out(), "full_runtime_candidate_status: missing\n");
    model_target_out_writef(model_target_out(), "candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "eligible_candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "pressure_target_count: 0\n");
    model_target_out_writef(model_target_out(), "fixture_target_count: 0\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    return 2;
}

static int print_model_target_candidate_report(const char *release,
                                               const char *target_id,
                                               int include_candidates,
                                               int include_pressure_targets,
                                               int include_blockers,
                                               int include_next)
{
    const yvex_full_runtime_candidate_fact *target_fact = NULL;
    const yvex_model_registry_entry *target_entry = NULL;
    yvex_model_registry *registry = NULL;
    unsigned long registry_count = 0;
    unsigned long candidate_count = 0;
    unsigned long pressure_count = 0;
    unsigned long fixture_count = 0;
    unsigned long i;

    (void)open_candidate_registry(&registry);
    registry_count = candidate_registry_extra_count(registry);
    if (target_id) {
        target_fact = find_full_runtime_candidate_fact(target_id);
        if (!target_fact && registry) {
            target_entry = yvex_model_registry_find(registry, target_id);
        }
        if (!target_fact && !target_entry) {
            yvex_model_registry_close(registry);
            return print_model_target_candidate_missing(release, target_id);
        }
        candidate_count = 1;
        pressure_count = target_fact && target_fact->pressure_target ? 1 : 0;
        fixture_count = target_fact && target_fact->fixture_target ? 1 : 0;
    } else {
        candidate_count = full_runtime_candidate_fact_count + registry_count;
        for (i = 0; i < full_runtime_candidate_fact_count; ++i) {
            if (full_runtime_candidate_facts[i].pressure_target) pressure_count++;
            if (full_runtime_candidate_facts[i].fixture_target) fixture_count++;
        }
    }

    model_target_out_writef(model_target_out(), "model-target: candidate\n");
    model_target_out_writef(model_target_out(), "status: full-runtime-candidate-report\n");
    model_target_out_writef(model_target_out(), "release: %s\n", release);
    model_target_out_writef(model_target_out(), "decision_state: blocked-no-candidate\n");
    model_target_out_writef(model_target_out(), "selected_target_id: none\n");
    model_target_out_writef(model_target_out(), "full_runtime_candidate_status: missing\n");
    model_target_out_writef(model_target_out(), "candidate_count: %lu\n", candidate_count);
    model_target_out_writef(model_target_out(), "eligible_candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "pressure_target_count: %lu\n", pressure_count);
    model_target_out_writef(model_target_out(), "fixture_target_count: %lu\n", fixture_count);
    model_target_out_writef(model_target_out(), "registered_alias_count: %lu\n", target_id ? (target_entry ? 1ul : 0ul) : registry_count);
    if (include_pressure_targets) {
        model_target_out_writef(model_target_out(), "deepseek_pressure_status: selected-slice-pressure-only\n");
        model_target_out_writef(model_target_out(), "glm_pressure_status: source-storage-pressure-only\n");
        model_target_out_writef(model_target_out(), "qwen_metal_pressure_status: planned-portability-pressure-only\n");
    }
    model_target_out_writef(model_target_out(), "global_blocker: no eligible full-runtime candidate\n");
    if (include_next) {
        model_target_out_writef(model_target_out(), "next_required_rows: V010.TARGET.3\n");
    }
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");

    if (include_candidates || target_id) {
        unsigned long out_index = 0;
        if (target_fact) {
            print_full_runtime_candidate_fact(out_index, target_fact, include_blockers, include_next);
        } else if (target_entry) {
            print_registered_candidate(out_index, target_entry, include_blockers, include_next);
        } else {
            for (i = 0; i < full_runtime_candidate_fact_count; ++i) {
                print_full_runtime_candidate_fact(out_index++,
                                                  &full_runtime_candidate_facts[i],
                                                  include_blockers,
                                                  include_next);
            }
            if (registry) {
                unsigned long long ri;
                for (ri = 0; ri < yvex_model_registry_count(registry); ++ri) {
                    const yvex_model_registry_entry *entry =
                        yvex_model_registry_at(registry, ri);
                    if (!entry || !entry->alias ||
                        find_full_runtime_candidate_fact(entry->alias)) {
                        continue;
                    }
                    print_registered_candidate(out_index++, entry, include_blockers, include_next);
                }
            }
        }
    }

    yvex_model_registry_close(registry);
    return 0;
}

static int print_model_target_candidate_normal(const char *release,
                                               const char *target_id)
{
    const yvex_full_runtime_candidate_fact *target_fact = NULL;
    const yvex_model_registry_entry *target_entry = NULL;
    yvex_model_registry *registry = NULL;
    unsigned long registry_count = 0;
    unsigned long candidate_count = 0;
    unsigned long pressure_count = 0;
    unsigned long fixture_count = 0;
    unsigned long i;

    (void)open_candidate_registry(&registry);
    registry_count = candidate_registry_extra_count(registry);
    if (target_id) {
        target_fact = find_full_runtime_candidate_fact(target_id);
        if (!target_fact && registry) {
            target_entry = yvex_model_registry_find(registry, target_id);
        }
        if (!target_fact && !target_entry) {
            yvex_model_registry_close(registry);
            return print_model_target_candidate_missing(release, target_id);
        }
        candidate_count = 1;
        pressure_count = target_fact && target_fact->pressure_target ? 1 : 0;
        fixture_count = target_fact && target_fact->fixture_target ? 1 : 0;
    } else {
        candidate_count = full_runtime_candidate_fact_count + registry_count;
        for (i = 0; i < full_runtime_candidate_fact_count; ++i) {
            if (full_runtime_candidate_facts[i].pressure_target) pressure_count++;
            if (full_runtime_candidate_facts[i].fixture_target) fixture_count++;
        }
    }

    model_target_out_writef(model_target_out(), "report: model-target candidate\n");
    model_target_out_writef(model_target_out(), "status: blocked-no-candidate\n");
    model_target_out_writef(model_target_out(), "release: %s\n", release);
    model_target_out_writef(model_target_out(), "selected: none\n");
    model_target_out_writef(model_target_out(), "candidates: 0 eligible / %lu known (%lu pressure, %lu fixture)\n",
           candidate_count, pressure_count, fixture_count);
    model_target_out_writef(model_target_out(), "top_blocker: no eligible full-runtime candidate\n");
    model_target_out_writef(model_target_out(), "next: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    model_target_out_writef(model_target_out(), "boundary: report-only; generation unsupported; benchmark not measured\n");
    yvex_model_registry_close(registry);
    return 0;
}

static const yvex_dense_candidate_fact *find_dense_candidate_fact(const char *id)
{
    unsigned long i;

    if (!id) return NULL;
    for (i = 0; i < dense_candidate_fact_count; ++i) {
        if (strcmp(dense_candidate_facts[i].id, id) == 0) {
            return &dense_candidate_facts[i];
        }
    }
    return NULL;
}

static unsigned long dense_candidate_registry_extra_count(const yvex_model_registry *registry)
{
    unsigned long long i;
    unsigned long count = 0;

    if (!registry) return 0;
    for (i = 0; i < yvex_model_registry_count(registry); ++i) {
        const yvex_model_registry_entry *entry = yvex_model_registry_at(registry, i);
        if (!entry || !entry->alias || find_dense_candidate_fact(entry->alias)) continue;
        count++;
    }
    return count;
}

static void print_dense_candidate_requirements(unsigned long index)
{
    unsigned long i;

    for (i = 0; i < dense_candidate_required_role_count; ++i) {
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_required_role_%lu: %s\n",
               index, i, dense_candidate_required_roles[i]);
    }
}

static void print_dense_candidate_fact(unsigned long index,
                                       const yvex_dense_candidate_fact *fact,
                                       int include_requirements,
                                       int include_blockers,
                                       int include_next)
{
    unsigned int i;

    if (!fact) return;
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_id: %s\n", index, fact->id);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_family: %s\n", index, fact->family);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_class: %s\n", index, fact->class_name);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_stage: %s\n", index, fact->stage);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_eligibility: %s\n", index, fact->eligibility);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_source_status: %s\n", index, fact->source_status);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_artifact_status: %s\n", index, fact->artifact_status);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_tensor_map_status: %s\n", index, fact->tensor_map_status);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_tensor_coverage_status: %s\n", index, fact->tensor_coverage_status);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_tokenizer_status: %s\n", index, fact->tokenizer_status);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_output_head_status: %s\n", index, fact->output_head_status);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_runtime_path_status: %s\n", index, fact->runtime_path_status);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_generation_status: %s\n", index, fact->generation_status);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_benchmark_status: %s\n", index, fact->benchmark_status);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_count: %u\n", index, fact->blocker_count);
    if (include_requirements) {
        print_dense_candidate_requirements(index);
    }
    if (include_blockers) {
        for (i = 0; i < fact->blocker_count; ++i) {
            model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_%u: %s\n", index, i, fact->blockers[i]);
        }
    }
    if (include_next) {
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_next_required_rows: %s\n", index, fact->next_required_rows);
    }
}

static void print_registered_dense_candidate(unsigned long index,
                                             const yvex_model_registry_entry *entry,
                                             int include_requirements,
                                             int include_blockers,
                                             int include_next)
{
    const char *alias = entry && entry->alias ? entry->alias : "unknown-registered-alias";

    model_target_out_writef(model_target_out(), "dense_candidate_%lu_id: %s\n", index, alias);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_family: registered\n", index);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_class: registered-alias\n", index);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_stage: report-only\n", index);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_eligibility: candidate-incomplete\n", index);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_source_status: unknown\n", index);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_artifact_status: registered-artifact-not-inspected\n", index);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_tensor_map_status: missing-tensor-map\n", index);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_tensor_coverage_status: %s\n", index,
           registered_candidate_tensor_status(entry));
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_tokenizer_status: unknown\n", index);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_output_head_status: unknown\n", index);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_runtime_path_status: unsupported\n", index);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_generation_status: unsupported-full-model\n", index);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_benchmark_status: not-measured\n", index);
    model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_count: 12\n", index);
    if (include_requirements) {
        print_dense_candidate_requirements(index);
    }
    if (include_blockers) {
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_0: missing-dense-source\n", index);
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_1: missing-dense-artifact\n", index);
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_2: missing-source-manifest\n", index);
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_3: missing-native-inventory\n", index);
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_4: missing-tensor-map\n", index);
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_5: missing-required-tensor-coverage\n", index);
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_6: missing-tokenizer-metadata\n", index);
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_7: missing-output-head\n", index);
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_8: missing-dense-mlp\n", index);
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_9: missing-real-prefill\n", index);
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_10: missing-real-decode\n", index);
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_blocker_11: missing-real-logits\n", index);
    }
    if (include_next) {
        model_target_out_writef(model_target_out(), "dense_candidate_%lu_next_required_rows: V010.TARGET.7,V010.MAP.*\n", index);
    }
}

static int print_model_target_dense_candidate_missing(const char *release, const char *target)
{
    model_target_out_writef(model_target_out(), "model-target: dense-candidate\n");
    model_target_out_writef(model_target_out(), "status: dense-candidate-report-fail\n");
    model_target_out_writef(model_target_out(), "release: %s\n", release && release[0] ? release : "v0.1.0");
    model_target_out_writef(model_target_out(), "target_requested: %s\n", target && target[0] ? target : "none");
    model_target_out_writef(model_target_out(), "decision_state: dense-candidate-missing\n");
    model_target_out_writef(model_target_out(), "selected_dense_candidate_id: none\n");
    model_target_out_writef(model_target_out(), "dense_candidate_status: missing\n");
    model_target_out_writef(model_target_out(), "dense_candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "eligible_dense_candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "dense_pressure_target_count: 0\n");
    model_target_out_writef(model_target_out(), "fixture_target_count: 0\n");
    model_target_out_writef(model_target_out(), "global_blocker: no eligible dense full-runtime candidate\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    return 2;
}

static int print_model_target_dense_candidate_unsupported_release(const char *release)
{
    model_target_out_writef(model_target_out(), "model-target: dense-candidate\n");
    model_target_out_writef(model_target_out(), "status: unsupported-release\n");
    model_target_out_writef(model_target_out(), "release: %s\n", release && release[0] ? release : "unknown");
    model_target_out_writef(model_target_out(), "decision_state: dense-candidate-missing\n");
    model_target_out_writef(model_target_out(), "selected_dense_candidate_id: none\n");
    model_target_out_writef(model_target_out(), "dense_candidate_status: missing\n");
    model_target_out_writef(model_target_out(), "dense_candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "eligible_dense_candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "dense_pressure_target_count: 0\n");
    model_target_out_writef(model_target_out(), "fixture_target_count: 0\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    return 2;
}

static const char *dense_candidate_status_for_target(const yvex_dense_candidate_fact *fact,
                                                     const yvex_model_registry_entry *entry)
{
    if (entry) return "candidate-incomplete";
    if (!fact) return "missing";
    if (fact->eligible) return "candidate-found";
    if (strcmp(fact->eligibility, "dense-pressure-only") == 0 ||
        strcmp(fact->eligibility, "candidate-planned") == 0 ||
        strcmp(fact->eligibility, "candidate-incomplete") == 0) {
        return "candidate-incomplete";
    }
    return "missing";
}

static int print_model_target_dense_candidate_report(const char *release,
                                                     const char *target_id,
                                                     int include_candidates,
                                                     int include_requirements,
                                                     int include_blockers,
                                                     int include_next)
{
    const yvex_dense_candidate_fact *target_fact = NULL;
    const yvex_model_registry_entry *target_entry = NULL;
    yvex_model_registry *registry = NULL;
    unsigned long registry_count = 0;
    unsigned long dense_candidate_count = 0;
    unsigned long dense_pressure_count = 0;
    unsigned long fixture_count = 0;
    unsigned long eligible_count = 0;
    unsigned long i;

    (void)open_candidate_registry(&registry);
    registry_count = dense_candidate_registry_extra_count(registry);
    if (target_id) {
        target_fact = find_dense_candidate_fact(target_id);
        if (!target_fact && registry) {
            target_entry = yvex_model_registry_find(registry, target_id);
        }
        if (!target_fact && !target_entry) {
            yvex_model_registry_close(registry);
            return print_model_target_dense_candidate_missing(release, target_id);
        }
        dense_candidate_count = 1;
        dense_pressure_count = target_fact && target_fact->dense_pressure_target ? 1 : 0;
        fixture_count = target_fact && target_fact->fixture_target ? 1 : 0;
        eligible_count = target_fact && target_fact->eligible ? 1 : 0;
    } else {
        dense_candidate_count = dense_candidate_fact_count + registry_count;
        for (i = 0; i < dense_candidate_fact_count; ++i) {
            if (dense_candidate_facts[i].dense_pressure_target) dense_pressure_count++;
            if (dense_candidate_facts[i].fixture_target) fixture_count++;
            if (dense_candidate_facts[i].eligible) eligible_count++;
        }
    }

    model_target_out_writef(model_target_out(), "model-target: dense-candidate\n");
    model_target_out_writef(model_target_out(), "status: dense-candidate-report\n");
    model_target_out_writef(model_target_out(), "release: %s\n", release);
    model_target_out_writef(model_target_out(), "decision_state: %s\n", eligible_count ? "dense-candidate-found" : "dense-candidate-missing");
    model_target_out_writef(model_target_out(), "selected_dense_candidate_id: none\n");
    model_target_out_writef(model_target_out(), "dense_candidate_status: %s\n",
           target_id ? dense_candidate_status_for_target(target_fact, target_entry)
                     : (eligible_count ? "candidate-found" : "missing"));
    model_target_out_writef(model_target_out(), "dense_candidate_count: %lu\n", dense_candidate_count);
    model_target_out_writef(model_target_out(), "eligible_dense_candidate_count: %lu\n", eligible_count);
    model_target_out_writef(model_target_out(), "dense_pressure_target_count: %lu\n", dense_pressure_count);
    model_target_out_writef(model_target_out(), "fixture_target_count: %lu\n", fixture_count);
    model_target_out_writef(model_target_out(), "registered_alias_count: %lu\n", target_id ? (target_entry ? 1ul : 0ul) : registry_count);
    model_target_out_writef(model_target_out(), "global_blocker: no eligible dense full-runtime candidate\n");
    if (include_next) {
        model_target_out_writef(model_target_out(), "next_required_rows: V010.TARGET.7\n");
    }
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");

    if (include_candidates || target_id) {
        unsigned long out_index = 0;
        if (target_fact) {
            print_dense_candidate_fact(out_index, target_fact,
                                       include_requirements,
                                       include_blockers,
                                       include_next);
        } else if (target_entry) {
            print_registered_dense_candidate(out_index, target_entry,
                                             include_requirements,
                                             include_blockers,
                                             include_next);
        } else {
            for (i = 0; i < dense_candidate_fact_count; ++i) {
                print_dense_candidate_fact(out_index++,
                                           &dense_candidate_facts[i],
                                           include_requirements,
                                           include_blockers,
                                           include_next);
            }
            if (registry) {
                unsigned long long ri;
                for (ri = 0; ri < yvex_model_registry_count(registry); ++ri) {
                    const yvex_model_registry_entry *entry =
                        yvex_model_registry_at(registry, ri);
                    if (!entry || !entry->alias ||
                        find_dense_candidate_fact(entry->alias)) {
                        continue;
                    }
                    print_registered_dense_candidate(out_index++, entry,
                                                     include_requirements,
                                                     include_blockers,
                                                     include_next);
                }
            }
        }
    }

    yvex_model_registry_close(registry);
    return 0;
}

static int print_model_target_dense_candidate_normal(const char *release,
                                                     const char *target_id)
{
    const yvex_dense_candidate_fact *target_fact = NULL;
    const yvex_model_registry_entry *target_entry = NULL;
    yvex_model_registry *registry = NULL;
    unsigned long registry_count = 0;
    unsigned long dense_candidate_count = 0;
    unsigned long dense_pressure_count = 0;
    unsigned long eligible_count = 0;
    unsigned long i;

    (void)open_candidate_registry(&registry);
    registry_count = dense_candidate_registry_extra_count(registry);
    if (target_id) {
        target_fact = find_dense_candidate_fact(target_id);
        if (!target_fact && registry) {
            target_entry = yvex_model_registry_find(registry, target_id);
        }
        if (!target_fact && !target_entry) {
            yvex_model_registry_close(registry);
            return print_model_target_dense_candidate_missing(release, target_id);
        }
        dense_candidate_count = 1;
        dense_pressure_count = target_fact && target_fact->dense_pressure_target ? 1 : 0;
        eligible_count = target_fact && target_fact->eligible ? 1 : 0;
    } else {
        dense_candidate_count = dense_candidate_fact_count + registry_count;
        for (i = 0; i < dense_candidate_fact_count; ++i) {
            if (dense_candidate_facts[i].dense_pressure_target) dense_pressure_count++;
            if (dense_candidate_facts[i].eligible) eligible_count++;
        }
    }

    model_target_out_writef(model_target_out(), "report: model-target dense-candidate\n");
    model_target_out_writef(model_target_out(), "status: %s\n", eligible_count ? "dense-candidate-found" : "dense-candidate-missing");
    model_target_out_writef(model_target_out(), "release: %s\n", release);
    model_target_out_writef(model_target_out(), "selected: none\n");
    model_target_out_writef(model_target_out(), "candidates: %lu eligible / %lu known (%lu dense pressure)\n",
           eligible_count, dense_candidate_count, dense_pressure_count);
    model_target_out_writef(model_target_out(), "top_blocker: no selected dense full-runtime candidate\n");
    model_target_out_writef(model_target_out(), "next: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    model_target_out_writef(model_target_out(), "boundary: report-only; generation unsupported; benchmark not measured\n");
    yvex_model_registry_close(registry);
    return 0;
}

static const yvex_qwen_metal_candidate_fact *find_qwen_metal_candidate_fact(const char *id)
{
    unsigned long i;

    if (!id) return NULL;
    for (i = 0; i < qwen_metal_candidate_fact_count; ++i) {
        if (strcmp(qwen_metal_candidate_facts[i].id, id) == 0) {
            return &qwen_metal_candidate_facts[i];
        }
    }
    return NULL;
}

static void print_qwen_metal_candidate(unsigned long index,
                                       const yvex_qwen_metal_candidate_fact *fact,
                                       int include_blockers)
{
    unsigned int i;

    if (!fact) return;
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_id: %s\n", index, fact->id);
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_class: %s\n", index, fact->class_name);
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_stage: %s\n", index, fact->stage);
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_eligibility: %s\n", index, fact->eligibility);
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_source_target_status: %s\n", index, fact->source_target_status);
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_source_status: %s\n", index, fact->source_status);
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_artifact_status: %s\n", index, fact->artifact_status);
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_tensor_map_status: %s\n", index, fact->tensor_map_status);
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_backend_status: %s\n", index, fact->backend_status);
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_runtime_status: %s\n", index, fact->runtime_status);
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_generation_status: %s\n", index, fact->generation_status);
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_benchmark_status: %s\n", index, fact->benchmark_status);
    model_target_out_writef(model_target_out(), "qwen_candidate_%lu_blocker_count: %u\n", index, fact->blocker_count);
    if (include_blockers) {
        for (i = 0; i < fact->blocker_count; ++i) {
            model_target_out_writef(model_target_out(), "qwen_candidate_%lu_blocker_%u: %s\n", index, i, fact->blockers[i]);
        }
    }
}

static int print_model_target_qwen_metal_missing(const char *release, const char *target)
{
    model_target_out_writef(model_target_out(), "model-target: qwen-metal\n");
    model_target_out_writef(model_target_out(), "status: qwen-metal-pressure-report-fail\n");
    model_target_out_writef(model_target_out(), "release: %s\n", release && release[0] ? release : "v0.1.0");
    model_target_out_writef(model_target_out(), "target_requested: %s\n", target && target[0] ? target : "none");
    model_target_out_writef(model_target_out(), "lane_id: qwen-metal\n");
    model_target_out_writef(model_target_out(), "target_family: qwen\n");
    model_target_out_writef(model_target_out(), "target_class: source-model-candidate\n");
    model_target_out_writef(model_target_out(), "runtime_shape: dense-or-dense-like-candidate-pending-source-config\n");
    model_target_out_writef(model_target_out(), "hardware_lane: apple-silicon-metal\n");
    model_target_out_writef(model_target_out(), "backend_lane: metal-planned\n");
    model_target_out_writef(model_target_out(), "full_runtime_candidate_status: candidate-planned\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    return 2;
}

static int print_model_target_qwen_metal_unsupported_release(const char *release)
{
    model_target_out_writef(model_target_out(), "model-target: qwen-metal\n");
    model_target_out_writef(model_target_out(), "status: unsupported-release\n");
    model_target_out_writef(model_target_out(), "release: %s\n", release && release[0] ? release : "unknown");
    model_target_out_writef(model_target_out(), "lane_id: qwen-metal\n");
    model_target_out_writef(model_target_out(), "target_family: qwen\n");
    model_target_out_writef(model_target_out(), "target_class: source-model-candidate\n");
    model_target_out_writef(model_target_out(), "runtime_shape: dense-or-dense-like-candidate-pending-source-config\n");
    model_target_out_writef(model_target_out(), "hardware_lane: apple-silicon-metal\n");
    model_target_out_writef(model_target_out(), "backend_lane: metal-planned\n");
    model_target_out_writef(model_target_out(), "full_runtime_candidate_status: missing\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    return 2;
}

static int print_model_target_qwen_metal_report(const char *release,
                                                const char *target_id,
                                                int include_candidates,
                                                int include_hardware,
                                                int include_backend,
                                                int include_source,
                                                int include_blockers,
                                                int include_next)
{
    const yvex_qwen_metal_candidate_fact *target_fact = NULL;
    unsigned long i;

    if (target_id) {
        target_fact = find_qwen_metal_candidate_fact(target_id);
        if (!target_fact) {
            return print_model_target_qwen_metal_missing(release, target_id);
        }
    }

    model_target_out_writef(model_target_out(), "model-target: qwen-metal\n");
    model_target_out_writef(model_target_out(), "status: qwen-metal-pressure-report\n");
    model_target_out_writef(model_target_out(), "release: %s\n", release);
    model_target_out_writef(model_target_out(), "lane_id: qwen-metal\n");
    model_target_out_writef(model_target_out(), "target_family: qwen\n");
    model_target_out_writef(model_target_out(), "target_class: source-model-candidate\n");
    model_target_out_writef(model_target_out(), "stage: report-only\n");
    model_target_out_writef(model_target_out(), "eligibility: pressure-target-only\n");
    model_target_out_writef(model_target_out(), "candidate_id: qwen3-8b\n");
    model_target_out_writef(model_target_out(), "candidate_stage: source-target-profiled\n");
    model_target_out_writef(model_target_out(), "candidate_eligibility: pressure-target-only\n");
    model_target_out_writef(model_target_out(), "source_target_status: profiled\n");
    model_target_out_writef(model_target_out(), "runtime_shape: dense-or-dense-like-candidate-pending-source-config\n");
    model_target_out_writef(model_target_out(), "hardware_lane: apple-silicon-metal\n");
    model_target_out_writef(model_target_out(), "backend_lane: metal-planned\n");
    model_target_out_writef(model_target_out(), "source_status: missing\n");
    model_target_out_writef(model_target_out(), "artifact_status: missing\n");
    model_target_out_writef(model_target_out(), "metal_backend_status: unsupported\n");
    model_target_out_writef(model_target_out(), "qwen_runtime_status: unsupported\n");
    model_target_out_writef(model_target_out(), "full_runtime_candidate_status: candidate-planned\n");
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: unsupported-full-model\n");
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");

    if (include_candidates || target_id) {
        if (target_fact) {
            model_target_out_writef(model_target_out(), "qwen_candidate_count: 1\n");
            print_qwen_metal_candidate(0, target_fact, include_blockers);
        } else {
            model_target_out_writef(model_target_out(), "qwen_candidate_count: %lu\n", qwen_metal_candidate_fact_count);
            for (i = 0; i < qwen_metal_candidate_fact_count; ++i) {
                print_qwen_metal_candidate(i, &qwen_metal_candidate_facts[i], include_blockers);
            }
        }
    }

    if (include_hardware) {
        model_target_out_writef(model_target_out(), "hardware_profile_status: planned\n");
        model_target_out_writef(model_target_out(), "machine_profile_required: true\n");
        model_target_out_writef(model_target_out(), "unified_memory_report_required: true\n");
        model_target_out_writef(model_target_out(), "metal_device_report_required: true\n");
    }
    if (include_backend) {
        model_target_out_writef(model_target_out(), "metal_feasibility_status: missing\n");
        model_target_out_writef(model_target_out(), "metal_allocation_status: unsupported\n");
        model_target_out_writef(model_target_out(), "metal_graph_primitive_status: unsupported\n");
        model_target_out_writef(model_target_out(), "cuda_lane_independent: true\n");
    }
    if (include_source) {
        model_target_out_writef(model_target_out(), "source_family: qwen\n");
        model_target_out_writef(model_target_out(), "source_target_status: profiled\n");
        model_target_out_writef(model_target_out(), "source_manifest_status: missing\n");
        model_target_out_writef(model_target_out(), "native_tensor_inventory_status: missing\n");
        model_target_out_writef(model_target_out(), "source_config_status: missing\n");
        model_target_out_writef(model_target_out(), "model_class_profile_status: command-visible\n");
        model_target_out_writef(model_target_out(), "model_class_role_mapping_status: not-implemented\n");
    }
    if (include_blockers) {
        for (i = 0; i < qwen_metal_blocker_count; ++i) {
            model_target_out_writef(model_target_out(), "blocker_%lu: %s\n", i, qwen_metal_blockers[i]);
        }
    }
    if (include_next) {
        model_target_out_writef(model_target_out(), "next_required_rows: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    }
    return 0;
}

static int print_model_target_qwen_metal_normal(const char *release,
                                                const char *target_id)
{
    const yvex_qwen_metal_candidate_fact *target_fact = NULL;

    if (target_id) {
        target_fact = find_qwen_metal_candidate_fact(target_id);
        if (!target_fact) {
            return print_model_target_qwen_metal_missing(release, target_id);
        }
    }

    model_target_out_writef(model_target_out(), "report: model-target qwen-metal\n");
    model_target_out_writef(model_target_out(), "status: pressure-target-only\n");
    model_target_out_writef(model_target_out(), "release: %s\n", release);
    model_target_out_writef(model_target_out(), "lane: qwen-metal / apple-silicon-metal\n");
    model_target_out_writef(model_target_out(), "target: qwen3-8b\n");
    model_target_out_writef(model_target_out(), "candidate: source-target-profiled pressure-target-only\n");
    model_target_out_writef(model_target_out(), "source_target: profiled\n");
    model_target_out_writef(model_target_out(), "source: missing\n");
    model_target_out_writef(model_target_out(), "backend: metal unsupported\n");
    model_target_out_writef(model_target_out(), "next: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    model_target_out_writef(model_target_out(), "boundary: report-only; generation unsupported; benchmark not measured\n");
    return 0;
}

static int print_model_target_decision_unsupported_release(const char *release)
{
    const char *value;

    value = release && release[0] ? release : "unknown";
    model_target_out_writef(model_target_out(), "target_decision: %s\n", value);
    model_target_out_writef(model_target_out(), "status: unsupported-release\n");
    model_target_out_writef(model_target_out(), "release: %s\n", value);
    model_target_out_writef(model_target_out(), "decision_state: deferred\n");
    model_target_out_writef(model_target_out(), "selected_target_id: none\n");
    model_target_out_writef(model_target_out(), "selected_target_class: none\n");
    model_target_out_writef(model_target_out(), "selected_family: none\n");
    model_target_out_writef(model_target_out(), "selected_model: none\n");
    model_target_out_writef(model_target_out(), "selected_source_class: none\n");
    model_target_out_writef(model_target_out(), "selected_artifact_class: none\n");
    model_target_out_writef(model_target_out(), "selected_backend_policy: none\n");
    model_target_out_writef(model_target_out(), "selected_reason: unsupported release target decision vocabulary\n");
    model_target_out_writef(model_target_out(), "full_runtime_candidate_status: unknown\n");
    model_target_out_writef(model_target_out(), "candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "eligible_candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "ineligible_candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "selected_runtime_slice_eligible: false\n");
    model_target_out_writef(model_target_out(), "source_only_eligible: false\n");
    model_target_out_writef(model_target_out(), "external_reference_eligible: false\n");
    model_target_out_writef(model_target_out(), "fixture_only_eligible: false\n");
    model_target_out_writef(model_target_out(), "deepseek_pressure_status: unknown\n");
    model_target_out_writef(model_target_out(), "glm_pressure_status: unknown\n");
    model_target_out_writef(model_target_out(), "qwen_metal_pressure_status: unknown\n");
    model_target_out_writef(model_target_out(), "dense_candidate_status: unknown\n");
    model_target_out_writef(model_target_out(), "moe_candidate_status: unknown\n");
    model_target_out_writef(model_target_out(), "selected_candidate_tensor_coverage: none\n");
    model_target_out_writef(model_target_out(), "selected_candidate_artifact_status: none\n");
    model_target_out_writef(model_target_out(), "selected_candidate_integrity_status: none\n");
    model_target_out_writef(model_target_out(), "selected_candidate_backend_status: none\n");
    model_target_out_writef(model_target_out(), "selected_candidate_graph_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_prefill_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_kv_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_decode_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_logits_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_sampling_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_generation_status: unsupported\n");
    print_target_decision_constant_tail();
    return 2;
}

static int print_model_target_decision_missing_candidate(const char *release,
                                                        const char *candidate)
{
    const char *release_value;
    const char *candidate_value;

    release_value = release && release[0] ? release : "v0.1.0";
    candidate_value = candidate && candidate[0] ? candidate : "none";
    model_target_out_writef(model_target_out(), "target_decision: %s\n", release_value);
    model_target_out_writef(model_target_out(), "status: missing-candidate\n");
    model_target_out_writef(model_target_out(), "release: %s\n", release_value);
    model_target_out_writef(model_target_out(), "candidate_requested: %s\n", candidate_value);
    model_target_out_writef(model_target_out(), "decision_state: blocked-no-candidate\n");
    model_target_out_writef(model_target_out(), "selected_target_id: none\n");
    model_target_out_writef(model_target_out(), "selected_target_class: none\n");
    model_target_out_writef(model_target_out(), "selected_family: none\n");
    model_target_out_writef(model_target_out(), "selected_model: none\n");
    model_target_out_writef(model_target_out(), "selected_source_class: none\n");
    model_target_out_writef(model_target_out(), "selected_artifact_class: none\n");
    model_target_out_writef(model_target_out(), "selected_backend_policy: none\n");
    model_target_out_writef(model_target_out(), "selected_reason: requested candidate is not registered\n");
    model_target_out_writef(model_target_out(), "full_runtime_candidate_status: missing\n");
    model_target_out_writef(model_target_out(), "candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "eligible_candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "ineligible_candidate_count: 0\n");
    model_target_out_writef(model_target_out(), "selected_runtime_slice_eligible: false\n");
    model_target_out_writef(model_target_out(), "source_only_eligible: false\n");
    model_target_out_writef(model_target_out(), "external_reference_eligible: false\n");
    model_target_out_writef(model_target_out(), "fixture_only_eligible: false\n");
    model_target_out_writef(model_target_out(), "deepseek_pressure_status: selected-slice-pressure-only\n");
    model_target_out_writef(model_target_out(), "glm_pressure_status: source-storage-pressure-only\n");
    model_target_out_writef(model_target_out(), "qwen_metal_pressure_status: planned-portability-pressure-only\n");
    model_target_out_writef(model_target_out(), "dense_candidate_status: missing\n");
    model_target_out_writef(model_target_out(), "moe_candidate_status: blocked-missing-tensor-map\n");
    model_target_out_writef(model_target_out(), "selected_candidate_tensor_coverage: none\n");
    model_target_out_writef(model_target_out(), "selected_candidate_artifact_status: none\n");
    model_target_out_writef(model_target_out(), "selected_candidate_integrity_status: none\n");
    model_target_out_writef(model_target_out(), "selected_candidate_backend_status: none\n");
    model_target_out_writef(model_target_out(), "selected_candidate_graph_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_prefill_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_kv_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_decode_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_logits_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_sampling_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_generation_status: unsupported\n");
    print_target_decision_constant_tail();
    return 2;
}

static int print_model_target_decision_report(const char *release,
                                              const yvex_model_target_record *candidate_filter)
{
    const yvex_model_target_record *selected = NULL;
    unsigned long candidate_count = 0;
    unsigned long eligible_count = 0;
    unsigned long ineligible_count = 0;
    unsigned long i;
    unsigned long out_index = 0;

    for (i = 0; i < model_target_count; ++i) {
        const yvex_model_target_record *record = &model_targets[i];
        int include_record = candidate_filter ? record == candidate_filter : 1;
        int eligible;

        if (!include_record) continue;
        eligible = target_decision_is_full_runtime_candidate(record);
        candidate_count++;
        if (eligible) {
            eligible_count++;
            if (!selected) selected = record;
        } else {
            ineligible_count++;
        }
    }

    model_target_out_writef(model_target_out(), "target_decision: %s\n", release);
    model_target_out_writef(model_target_out(), "status: %s\n", selected ? "target-decision-selected" : "target-decision-blocked");
    model_target_out_writef(model_target_out(), "release: %s\n", release);
    model_target_out_writef(model_target_out(), "decision_state: %s\n", selected ? "selected" : "blocked-no-candidate");
    model_target_out_writef(model_target_out(), "selected_target_id: %s\n", selected ? selected->target_id : "none");
    model_target_out_writef(model_target_out(), "selected_target_class: %s\n", selected ? target_decision_candidate_class(selected) : "none");
    model_target_out_writef(model_target_out(), "selected_family: %s\n", selected ? selected->family : "none");
    model_target_out_writef(model_target_out(), "selected_model: %s\n", selected ? selected->model : "none");
    model_target_out_writef(model_target_out(), "selected_source_class: %s\n", selected ? selected->source_artifact_class : "none");
    model_target_out_writef(model_target_out(), "selected_artifact_class: %s\n", selected ? selected->target_artifact_class : "none");
    model_target_out_writef(model_target_out(), "selected_backend_policy: %s\n", selected ? "cpu-cuda-capability-required" : "none");
    model_target_out_writef(model_target_out(), "selected_reason: %s\n", selected
           ? "registered full-runtime candidate selected for v0.1.0 planning"
           : "no current full-runtime-candidate target is eligible for v0.1.0");
    model_target_out_writef(model_target_out(), "full_runtime_candidate_status: %s\n", eligible_count ? "present" : "missing");
    model_target_out_writef(model_target_out(), "candidate_count: %lu\n", candidate_count);
    model_target_out_writef(model_target_out(), "eligible_candidate_count: %lu\n", eligible_count);
    model_target_out_writef(model_target_out(), "ineligible_candidate_count: %lu\n", ineligible_count);
    model_target_out_writef(model_target_out(), "selected_runtime_slice_eligible: false\n");
    model_target_out_writef(model_target_out(), "source_only_eligible: false\n");
    model_target_out_writef(model_target_out(), "external_reference_eligible: false\n");
    model_target_out_writef(model_target_out(), "fixture_only_eligible: false\n");
    model_target_out_writef(model_target_out(), "deepseek_pressure_status: selected-slice-pressure-only\n");
    model_target_out_writef(model_target_out(), "glm_pressure_status: source-storage-pressure-only\n");
    model_target_out_writef(model_target_out(), "qwen_metal_pressure_status: planned-portability-pressure-only\n");
    model_target_out_writef(model_target_out(), "dense_candidate_status: %s\n", selected ? "selected" : "missing");
    model_target_out_writef(model_target_out(), "moe_candidate_status: %s\n", selected ? "target-dependent" : "blocked-missing-tensor-map");
    model_target_out_writef(model_target_out(), "selected_candidate_tensor_coverage: %s\n", selected ? "requires-report" : "none");
    model_target_out_writef(model_target_out(), "selected_candidate_artifact_status: %s\n", selected ? "requires-integrity-gate" : "none");
    model_target_out_writef(model_target_out(), "selected_candidate_integrity_status: %s\n", selected ? "requires-integrity-gate" : "none");
    model_target_out_writef(model_target_out(), "selected_candidate_backend_status: %s\n", selected ? "requires-backend-gate" : "none");
    model_target_out_writef(model_target_out(), "selected_candidate_graph_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_prefill_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_kv_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_decode_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_logits_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_sampling_status: unsupported\n");
    model_target_out_writef(model_target_out(), "selected_candidate_generation_status: unsupported\n");
    print_target_decision_constant_tail();

    for (i = 0; i < model_target_count; ++i) {
        const yvex_model_target_record *record = &model_targets[i];
        if (candidate_filter && record != candidate_filter) continue;
        model_target_out_writef(model_target_out(), "\n");
        print_model_target_decision_candidate(out_index++, record);
    }
    return 0;
}

static int print_model_target_decision_normal(const char *release,
                                              const yvex_model_target_record *candidate_filter)
{
    const yvex_model_target_record *selected = NULL;
    unsigned long candidate_count = 0;
    unsigned long eligible_count = 0;
    unsigned long ineligible_count = 0;
    unsigned long i;

    for (i = 0; i < model_target_count; ++i) {
        const yvex_model_target_record *record = &model_targets[i];
        int include_record = candidate_filter ? record == candidate_filter : 1;
        int eligible;

        if (!include_record) continue;
        eligible = target_decision_is_full_runtime_candidate(record);
        candidate_count++;
        if (eligible) {
            eligible_count++;
            if (!selected) selected = record;
        } else {
            ineligible_count++;
        }
    }

    model_target_out_writef(model_target_out(), "report: target-decision\n");
    model_target_out_writef(model_target_out(), "status: %s\n", selected ? "target-decision-selected" : "target-decision-blocked");
    model_target_out_writef(model_target_out(), "release: %s\n", release);
    model_target_out_writef(model_target_out(), "selected: %s\n", selected ? selected->target_id : "none");
    model_target_out_writef(model_target_out(), "eligible: %lu / %lu candidates (%lu ineligible)\n",
           eligible_count, candidate_count, ineligible_count);
    model_target_out_writef(model_target_out(), "top_blocker: %s\n", selected ? "none" : "no eligible full-runtime candidate");
    model_target_out_writef(model_target_out(), "next: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    model_target_out_writef(model_target_out(), "boundary: report-only; generation unsupported; benchmark not measured\n");
    return 0;
}

static void print_model_target_usage(FILE *fp)
{
    model_target_out_writef(fp, "usage: " "yvex model-target classes\n");
    model_target_out_writef(fp, "       yvex model-target list [--audit | --output normal|table|audit]\n");
    model_target_out_writef(fp, "       yvex model-target candidate --release v0.1.0 [options]\n");
    model_target_out_writef(fp, "       yvex model-target dense-candidate --release v0.1.0 [options]\n");
    model_target_out_writef(fp, "       yvex model-target qwen-metal --release v0.1.0 [options]\n");
    model_target_out_writef(fp, "       yvex model-target decision --release v0.1.0 [options]\n");
    model_target_out_writef(fp, "       yvex model-target class-profile TARGET [--models-root DIR] [--source DIR] [--audit | --output normal|table|audit]\n");
    model_target_out_writef(fp, "       yvex model-target tensor-collection TARGET [--models-root DIR] [--source DIR] [--audit | --output normal|table|audit]\n");
    model_target_out_writef(fp, "       yvex model-target missing-roles TARGET [--models-root DIR] [--source DIR] [--audit | --output normal|table|audit|json]\n");
    model_target_out_writef(fp, "       yvex model-target tokenizer-map TARGET [--models-root DIR] [--source DIR] [--audit | --output normal|table|audit|json]\n");
    model_target_out_writef(fp, "       yvex model-target tensor-map TARGET [--role output-head|tokenizer|missing-roles | --gate v0.1.0] [--models-root DIR] [--source DIR] [--audit | --output normal|table|audit] [--check-output-contract normal|table|audit]\n");
    model_target_out_writef(fp, "       yvex model-target quant-policy TARGET [--models-root DIR] [--source DIR] [--role-support] [--audit | --output normal|table|audit] [--check-output-contract normal|table|audit]\n");
    model_target_out_writef(fp, "       yvex model-target quant-policy --gate v0.1.0 [--models-root DIR] [--audit | --output normal|table|audit]\n");
    model_target_out_writef(fp, "       yvex model-target inspect TARGET [--paths] [--models-root DIR] [--audit | --output normal|table|audit]\n");
}

static void model_target_legacy_help(FILE *fp)
{
    print_model_target_usage(fp);
    model_target_out_writef(fp, "\n--paths           show expected operator-local source, artifact, report, reference, and registry paths\n");
    model_target_out_writef(fp, "--models-root DIR override configured operator model root for this command only\n");
    model_target_out_writef(fp, "--audit | --output normal|table|audit\n");
    model_target_out_writef(fp, "\nDecision report:\n");
    model_target_out_writef(fp, "  yvex model-target decision --release v0.1.0 --include-candidates --include-blockers --include-next\n");
    model_target_out_writef(fp, "  This command records the v0.1.0 target decision. It does not download models, emit artifacts, materialize tensors, execute graph work, run prefill, decode, logits, sampling, generation, evaluation, or benchmarks.\n");
    model_target_out_writef(fp, "  Default output is compact. Use --audit or --output audit for full row-promotion fields.\n");
    model_target_out_writef(fp, "\nCandidate report:\n");
    model_target_out_writef(fp, "  yvex model-target candidate --release v0.1.0 --include-candidates --include-blockers --include-next\n");
    model_target_out_writef(fp, "  The candidate report evaluates full-runtime target eligibility for a release. It does not select a ready model, download weights, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    model_target_out_writef(fp, "  Default output is compact. Use --audit or --output audit for candidate lists, blockers, and next-row evidence.\n");
    model_target_out_writef(fp, "\nDense candidate report:\n");
    model_target_out_writef(fp, "  yvex model-target dense-candidate --release v0.1.0 --include-candidates --include-requirements --include-blockers --include-next\n");
    model_target_out_writef(fp, "  The dense-candidate report evaluates whether a dense model target can become the first v0.1.0 full-runtime candidate. It does not download weights, emit artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    model_target_out_writef(fp, "  Default output is compact. Use --audit or --output audit for requirements and blocker detail.\n");
    model_target_out_writef(fp, "\nQwen/Metal pressure report:\n");
    model_target_out_writef(fp, "  yvex model-target qwen-metal --release v0.1.0 --include-candidates --include-hardware --include-backend --include-source --include-blockers --include-next\n");
    model_target_out_writef(fp, "  The Qwen/Metal pressure report records a planned reduced-scale Apple Silicon / Metal lane for future full-runtime work. It does not download weights, implement Metal, emit Qwen artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    model_target_out_writef(fp, "  Default output is compact. Use --audit or --output audit for hardware, backend, source, and blocker detail.\n");
    model_target_out_writef(fp, "\nModel-class profile:\n");
    model_target_out_writef(fp, "  yvex model-target class-profile qwen3-8b --audit\n");
    model_target_out_writef(fp, "  yvex model-target class-profile gemma-4-12b-it --audit\n");
    model_target_out_writef(fp, "  The class-profile report reads safetensors headers only and counts lexical tensor-name patterns. It does not map tensor roles, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    model_target_out_writef(fp, "\nTensor collection inventory:\n");
    model_target_out_writef(fp, "  yvex model-target tensor-collection qwen3-8b --audit\n");
    model_target_out_writef(fp, "  yvex model-target tensor-collection gemma-4-12b-it --audit\n");
    model_target_out_writef(fp, "  The tensor collection inventory reads safetensors headers only and groups lexical tensor candidates for the selected source target. It does not map runtime roles, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    model_target_out_writef(fp, "\nTensor naming map:\n");
    model_target_out_writef(fp, "  yvex model-target tensor-map qwen3-8b --audit\n");
    model_target_out_writef(fp, "  yvex model-target tensor-map gemma-4-12b-it --audit\n");
    model_target_out_writef(fp, "  The tensor naming map reads safetensors headers only and assigns native source tensor names to canonical YVEX role labels. It does not complete runtime role coverage, build runtime descriptors, emit artifacts, materialize tensors, feed graph consumers, execute the model, generate, evaluate, benchmark, or mark a release ready.\n");
    model_target_out_writef(fp, "\nOutput-head tensor map:\n");
    model_target_out_writef(fp, "  yvex model-target tensor-map qwen3-8b --role output-head --audit\n");
    model_target_out_writef(fp, "  yvex model-target tensor-map gemma-4-12b-it --role output-head --audit\n");
    model_target_out_writef(fp, "  The output-head tensor map reads safetensors headers only and identifies output-head, final-norm, and embedding candidates. It does not compute logits, complete runtime descriptors, feed graph consumers, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    model_target_out_writef(fp, "\nTokenizer metadata map:\n");
    model_target_out_writef(fp, "  yvex model-target tensor-map qwen3-8b --role tokenizer --audit\n");
    model_target_out_writef(fp, "  yvex model-target tensor-map gemma-4-12b-it --role tokenizer --audit\n");
    model_target_out_writef(fp, "  yvex model-target tokenizer-map qwen3-6-35b-a3b --models-root ~/lab/models --audit\n");
    model_target_out_writef(fp, "  yvex model-target tokenizer-map gemma-4-31b-it --models-root ~/lab/models --output table\n");
    model_target_out_writef(fp, "  The tokenizer metadata map reads local sidecars only and reports tokenizer/config/special-token metadata candidates. It does not tokenize, detokenize, apply chat templates, stop on EOS, compute logits, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    model_target_out_writef(fp, "\nMissing-role blocker report:\n");
    model_target_out_writef(fp, "  yvex model-target missing-roles qwen3-8b --audit\n");
    model_target_out_writef(fp, "  yvex model-target missing-roles gemma-4-12b-it --audit\n");
    model_target_out_writef(fp, "  The missing-role report aggregates header-derived tensor naming, output-head, tokenizer metadata, and planned artifact facts into the blocker list that prevents full GGUF emission. It does not load tensor payloads, emit GGUF, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    model_target_out_writef(fp, "\nTensor mapping gate:\n");
    model_target_out_writef(fp, "  yvex model-target tensor-map qwen3-8b --gate v0.1.0 --audit\n");
    model_target_out_writef(fp, "  yvex model-target tensor-map gemma-4-12b-it --gate v0.1.0 --audit\n");
    model_target_out_writef(fp, "  The tensor mapping gate aggregates model-class, tensor-collection, tensor naming, output-head, tokenizer metadata, and missing-role reports. It can pass only into artifact/quantization planning; it does not emit artifacts, construct runtime descriptors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    model_target_out_writef(fp, "\nQtype policy report:\n");
    model_target_out_writef(fp, "  yvex model-target quant-policy qwen3-8b --audit\n");
    model_target_out_writef(fp, "  yvex model-target quant-policy gemma-4-12b-it --audit\n");
    model_target_out_writef(fp, "  yvex model-target quant-policy qwen3-6-35b-a3b --role-support --models-root ~/lab/models\n");
    model_target_out_writef(fp, "  yvex model-target quant-policy --gate v0.1.0 --models-root ~/lab/models --output table\n");
    model_target_out_writef(fp, "  The qtype policy report consumes source/header/mapping evidence and existing YVEX qtype policy rows for artifact planning. It does not load tensor payloads, quantize tensors, emit GGUF, materialize tensors, construct runtime descriptors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    model_target_out_writef(fp, "\nDefault output is compact. Use --audit for full diagnostic fields.\n");
    model_target_out_writef(fp, "Model targets are pressure objects, not capability claims.\n");
    model_target_out_writef(fp, "External GGUFs and external runners are reference evidence only.\n");
    model_target_out_writef(fp, "Model-target path reporting does not read model payloads, create artifacts, register aliases, or claim runtime support.\n");
}

static void print_model_target_classes(void)
{
    unsigned long i;

    model_target_out_writef(model_target_out(), "status: model-target-classes\n");
    for (i = 0; i < model_target_class_count; ++i) {
        const yvex_model_target_class_record *record = &model_target_classes[i];
        model_target_out_writef(model_target_out(), "class: %s\n", record->class_id);
        model_target_out_writef(model_target_out(), "capability_claim: %s\n", record->capability_claim);
        model_target_out_writef(model_target_out(), "runtime_execution: %s\n", record->runtime_execution);
        model_target_out_writef(model_target_out(), "generation: %s\n", record->generation);
        model_target_out_writef(model_target_out(), "description: %s\n", record->description);
        if (i + 1 < model_target_class_count) {
            model_target_out_writef(model_target_out(), "\n");
        }
    }
}

static int model_target_is_selected_slice(const yvex_model_target_record *record)
{
    return record && strcmp(record->target_class, "selected-runtime-slice") == 0;
}

static const char *model_target_source_artifact_status(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (model_target_is_selected_slice(record)) return "unknown";
    if (strcmp(record->source_artifact_class, "official-source-tensors-planned") == 0) {
        return "missing";
    }
    if (strcmp(record->source_artifact_class, "official-safetensors-huge") == 0) {
        return "planned";
    }
    if (strcmp(record->source_artifact_class, "external-GGUF-reference") == 0 ||
        strcmp(record->source_artifact_class, "external-runner-reference") == 0) {
        return "external-reference-only";
    }
    return "unknown";
}

static const char *model_target_source_artifact_format(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strcmp(record->source_artifact_class, "official-source-tensors-planned") == 0) {
        return "safetensors+config-tokenizer-sidecars";
    }
    if (strcmp(record->source_artifact_class, "official-safetensors") == 0 ||
        strcmp(record->source_artifact_class, "official-safetensors-huge") == 0) {
        return "safetensors";
    }
    if (strcmp(record->source_artifact_class, "external-GGUF-reference") == 0) {
        return "gguf";
    }
    if (strcmp(record->source_artifact_class, "external-runner-reference") == 0) {
        return "external-runner";
    }
    return "unknown";
}

static const char *model_target_source_artifact_origin(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strncmp(record->source_artifact_class, "official", 8) == 0) return "official";
    if (strncmp(record->source_artifact_class, "external", 8) == 0) return "external-reference";
    if (strncmp(record->source_artifact_class, "YVEX", 4) == 0) return "local";
    if (strcmp(record->source_artifact_class, "unknown-source-artifact") == 0) return "unknown";
    return "planned";
}

static const char *model_target_source_artifact_authority(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strncmp(record->source_artifact_class, "official", 8) == 0) return "upstream-official";
    if (strncmp(record->source_artifact_class, "external", 8) == 0) return "external-reference";
    if (strncmp(record->source_artifact_class, "YVEX", 4) == 0) return "YVEX";
    return "unknown";
}

static const char *model_target_source_sidecar_status(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strcmp(record->source_artifact_class, "official-source-tensors-planned") == 0) {
        return "missing";
    }
    if (strcmp(record->source_artifact_class, "official-safetensors") == 0) {
        return "unknown";
    }
    if (strcmp(record->source_artifact_class, "official-safetensors-huge") == 0) {
        return "planned";
    }
    return "unknown";
}

static const char *model_target_source_tensor_container(const yvex_model_target_record *record)
{
    const char *format = model_target_source_artifact_format(record);

    if (strcmp(format, "safetensors") == 0 ||
        strcmp(format, "safetensors+config-tokenizer-sidecars") == 0) {
        return "safetensors";
    }
    if (strcmp(format, "gguf") == 0) {
        return "gguf";
    }
    if (strcmp(format, "external-runner") == 0) {
        return "none";
    }
    return "unknown";
}

static const char *model_target_source_tensor_payload_status(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strcmp(model_target_source_artifact_status(record), "missing") == 0) {
        return "not-present";
    }
    if (model_target_is_selected_slice(record)) {
        return "not-read";
    }
    if (strcmp(record->source_artifact_class, "official-safetensors-huge") == 0) {
        return "not-read";
    }
    return "unsupported";
}

static const char *model_target_source_provenance_status(const yvex_model_target_record *record)
{
    const char *artifact_status;

    if (!record) return "unknown";
    artifact_status = model_target_source_artifact_status(record);
    if (strcmp(artifact_status, "missing") == 0) return "missing";
    if (strcmp(artifact_status, "planned") == 0) return "planned";
    if (strcmp(artifact_status, "external-reference-only") == 0) {
        return "external-reference-only";
    }
    return "unknown";
}

static const char *model_target_source_origin(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strncmp(record->source_artifact_class, "official", 8) == 0) {
        return "planned-official";
    }
    if (strncmp(record->source_artifact_class, "external", 8) == 0) {
        return "external-reference";
    }
    if (strncmp(record->source_artifact_class, "YVEX", 4) == 0) {
        return "YVEX-produced";
    }
    return "unknown";
}

static const char *model_target_source_authority(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strncmp(record->source_artifact_class, "official", 8) == 0) {
        return "upstream-official-planned";
    }
    if (strncmp(record->source_artifact_class, "external", 8) == 0) {
        return "external-reference-only";
    }
    if (strncmp(record->source_artifact_class, "YVEX", 4) == 0) {
        return "YVEX";
    }
    return "unknown";
}

static const char *model_target_source_revision_status(const yvex_model_target_record *record)
{
    (void)record;
    return "unknown";
}

static const char *model_target_source_identity_status(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strcmp(model_target_source_artifact_status(record), "missing") == 0) {
        return "not-present";
    }
    return "not-verified";
}

static const char *model_target_source_hash_status(const yvex_model_target_record *record)
{
    (void)record;
    return "not-computed";
}

static const char *model_target_source_verification_status(
    const yvex_model_target_record *record)
{
    (void)record;
    return "not-verified";
}

static const char *model_target_native_inventory_status(
    const yvex_model_target_record *record)
{
    const char *artifact_status;

    if (!record) return "unknown";
    artifact_status = model_target_source_artifact_status(record);
    if (strcmp(artifact_status, "missing") == 0) return "missing";
    if (strcmp(artifact_status, "planned") == 0) return "planned";
    if (model_target_is_selected_slice(record)) return "unknown";
    if (strcmp(model_target_source_tensor_container(record), "safetensors") == 0) {
        return "not-inventoried";
    }
    return "unknown";
}

static const char *model_target_source_tensor_metadata_status(
    const yvex_model_target_record *record)
{
    const char *artifact_status;

    if (!record) return "unknown";
    artifact_status = model_target_source_artifact_status(record);
    if (strcmp(artifact_status, "missing") == 0) return "missing";
    if (strcmp(artifact_status, "planned") == 0) return "planned";
    if (model_target_is_selected_slice(record)) return "unknown";
    if (strcmp(model_target_source_tensor_container(record), "safetensors") == 0) {
        return "not-inventoried";
    }
    return "unknown";
}

static const char *model_target_target_artifact_status(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strcmp(record->target_artifact_class, "YVEX-produced-selected-GGUF") == 0) {
        return "present";
    }
    if (strcmp(record->target_artifact_class, "future-YVEX-produced-GGUF") == 0) {
        return "planned";
    }
    if (strcmp(record->target_artifact_class, "none-source-only") == 0) {
        return "report-only";
    }
    if (strcmp(record->target_artifact_class, "external-reference-only") == 0) {
        return "external-reference-only";
    }
    return "unknown";
}

static const char *model_target_target_artifact_origin(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strcmp(record->target_artifact_class, "YVEX-produced-selected-GGUF") == 0) {
        return "YVEX";
    }
    if (strcmp(record->target_artifact_class, "future-YVEX-produced-GGUF") == 0) {
        return "planned";
    }
    if (strcmp(record->target_artifact_class, "external-reference-only") == 0) {
        return "external-reference";
    }
    if (strcmp(record->target_artifact_class, "none-source-only") == 0) {
        return "none";
    }
    return "unknown";
}

static const char *model_target_target_artifact_required(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    return strcmp(record->target_artifact_class, "none-source-only") == 0 ? "false" : "true";
}

static const char *model_target_external_reference_status(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    return strcmp(record->external_reference, "true") == 0 ? "reference-only" : "false";
}

static const char *model_target_yvex_produced_artifact_status(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strcmp(record->target_artifact_class, "YVEX-produced-selected-GGUF") == 0) {
        return "selected-only";
    }
    if (strcmp(record->target_artifact_class, "future-YVEX-produced-GGUF") == 0) {
        return "planned";
    }
    return "not-applicable";
}

static const char *model_target_runtime_display(const yvex_model_target_record *record)
{
    if (!record) return "unsupported";
    if (strcmp(record->target_id, "deepseek4-v4-flash-selected-embed") == 0) {
        return "selected-boundary-only";
    }
    if (strcmp(record->target_id, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0) {
        return "selected-segment-boundary-only";
    }
    return record->runtime_execution;
}

static int model_target_is_source_model_candidate(const yvex_model_target_record *record)
{
    return record && strcmp(record->target_class, "source-model-candidate") == 0;
}

static const char *model_target_runtime_shape(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strcmp(record->target_id, "qwen3-8b") == 0) {
        return "causal-decoder-candidate-pending-config";
    }
    if (strcmp(record->target_id, "gemma-4-12b-it") == 0) {
        return "dense-candidate-pending-source-config";
    }
    return "not-applicable";
}

static const char *model_target_backend_selection(const yvex_model_target_record *record)
{
    return model_target_is_source_model_candidate(record) ? "deferred" : "not-applicable";
}

static const char *model_target_backend_pressure(const yvex_model_target_record *record)
{
    if (!record) return "unknown";
    if (strcmp(record->target_id, "qwen3-8b") == 0) {
        return "metal-planned";
    }
    if (strcmp(record->target_id, "gemma-4-12b-it") == 0) {
        return "cpu-cuda-baseline-planned";
    }
    return "not-applicable";
}

/*
 * print_model_target_list()
 *
 * Purpose:
 *   render compact model-target list output for the model-target command.
 *
 * Inputs:
 *   reads the static model-target registry owned by this CLI quarantine file.
 *
 * Effects:
 *   prints normal operator output to model_target_out() and may include report-only audit
 *   hints; it does not mutate target state or create artifacts.
 *
 * Failure:
 *   no runtime failure path; output is deterministic over static records.
 *
 * Boundary:
 *   report rendering does not create capability, model support, generation
 *   support, eval evidence, benchmark evidence, or release readiness.
 */
static void print_model_target_list(void)
{
    unsigned long i;

    model_target_out_writef(model_target_out(), "status: model-target-list\n");
    for (i = 0; i < model_target_count; ++i) {
        const yvex_model_target_record *record = &model_targets[i];
        model_target_out_writef(model_target_out(), "target: %s\n", record->target_id);
        model_target_out_writef(model_target_out(), "family: %s\n", record->family);
        model_target_out_writef(model_target_out(), "target_class: %s\n", record->target_class);
        model_target_out_writef(model_target_out(), "source_artifact_class: %s\n", record->source_artifact_class);
        model_target_out_writef(model_target_out(), "source_artifact_status: %s\n",
               model_target_source_artifact_status(record));
        model_target_out_writef(model_target_out(), "target_artifact_class: %s\n", record->target_artifact_class);
        model_target_out_writef(model_target_out(), "target_artifact_status: %s\n",
               model_target_target_artifact_status(record));
        model_target_out_writef(model_target_out(), "source_artifact_origin: %s\n",
               model_target_source_artifact_origin(record));
        model_target_out_writef(model_target_out(), "target_artifact_origin: %s\n",
               model_target_target_artifact_origin(record));
        model_target_out_writef(model_target_out(), "source_provenance_status: %s\n",
               model_target_source_provenance_status(record));
        model_target_out_writef(model_target_out(), "source_origin: %s\n", model_target_source_origin(record));
        model_target_out_writef(model_target_out(), "source_authority: %s\n", model_target_source_authority(record));
        model_target_out_writef(model_target_out(), "source_revision_status: %s\n",
               model_target_source_revision_status(record));
        model_target_out_writef(model_target_out(), "source_identity_status: %s\n",
               model_target_source_identity_status(record));
        model_target_out_writef(model_target_out(), "source_hash_status: %s\n", model_target_source_hash_status(record));
        model_target_out_writef(model_target_out(), "source_verification_status: %s\n",
               model_target_source_verification_status(record));
        model_target_out_writef(model_target_out(), "native_inventory_status: %s\n",
               model_target_native_inventory_status(record));
        model_target_out_writef(model_target_out(), "native_tensor_count: 0\n");
        model_target_out_writef(model_target_out(), "native_safetensors_payload_loaded: false\n");
        model_target_out_writef(model_target_out(), "source_tensor_metadata_status: %s\n",
               model_target_source_tensor_metadata_status(record));
        model_target_out_writef(model_target_out(), "source_tensor_count: 0\n");
        model_target_out_writef(model_target_out(), "source_tensor_metadata_payload_loaded: false\n");
        model_target_out_writef(model_target_out(), "source_tensor_metadata_payload_bytes_read: 0\n");
        print_model_class_audit_hint(record);
        print_tensor_collection_audit_hint(record);
        print_tensor_map_audit_hint(record);
        print_output_head_map_audit_hint(record);
        print_tokenizer_map_audit_hint(record);
        print_missing_role_report_audit_hint(record);
        print_tensor_mapping_gate_audit_hint(record);
        model_target_out_writef(model_target_out(), "runtime_shape: %s\n", model_target_runtime_shape(record));
        model_target_out_writef(model_target_out(), "backend_selection: %s\n", model_target_backend_selection(record));
        model_target_out_writef(model_target_out(), "backend_pressure: %s\n", model_target_backend_pressure(record));
        model_target_out_writef(model_target_out(), "runtime_execution: %s\n", record->runtime_execution);
        model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
        model_target_out_writef(model_target_out(), "generation: %s\n", record->generation);
        model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
        model_target_out_writef(model_target_out(), "release_ready: false\n");
        if (i + 1 < model_target_count) {
            model_target_out_writef(model_target_out(), "\n");
        }
    }
}

static void print_model_target_list_table(void)
{
    unsigned long i;

    model_target_out_writef(model_target_out(), "MODEL TARGETS  count=%lu\n\n", model_target_count);
    model_target_out_writef(model_target_out(), "%-43s  %-8s  %-40s  %-11s  %s\n",
           "TARGET",
           "FAMILY",
           "CLASS",
           "RUNTIME",
           "GENERATION");
    for (i = 0; i < model_target_count; ++i) {
        const yvex_model_target_record *record = &model_targets[i];
        model_target_out_writef(model_target_out(), "%-43s  %-8s  %-40s  %-11s  %s\n",
               record->target_id,
               record->family,
               record->target_class,
               record->runtime_execution,
               record->generation);
    }
    model_target_out_writef(model_target_out(), "status: model-target-list\n");
}

static void print_model_target_record(const yvex_model_target_record *record)
{
    model_target_out_writef(model_target_out(), "status: model-target\n");
    model_target_out_writef(model_target_out(), "target_id: %s\n", record->target_id);
    model_target_out_writef(model_target_out(), "family: %s\n", record->family);
    model_target_out_writef(model_target_out(), "model: %s\n", record->model);
    model_target_out_writef(model_target_out(), "target_class: %s\n", record->target_class);
    model_target_out_writef(model_target_out(), "runtime_shape: %s\n", model_target_runtime_shape(record));
    model_target_out_writef(model_target_out(), "backend_selection: %s\n", model_target_backend_selection(record));
    model_target_out_writef(model_target_out(), "backend_pressure: %s\n", model_target_backend_pressure(record));
    model_target_out_writef(model_target_out(), "source_artifact_class: %s\n", record->source_artifact_class);
    model_target_out_writef(model_target_out(), "source_artifact_status: %s\n", model_target_source_artifact_status(record));
    model_target_out_writef(model_target_out(), "source_artifact_format: %s\n", model_target_source_artifact_format(record));
    model_target_out_writef(model_target_out(), "source_artifact_origin: %s\n", model_target_source_artifact_origin(record));
    model_target_out_writef(model_target_out(), "source_artifact_authority: %s\n",
           model_target_source_artifact_authority(record));
    model_target_out_writef(model_target_out(), "source_sidecar_status: %s\n", model_target_source_sidecar_status(record));
    model_target_out_writef(model_target_out(), "source_tensor_container: %s\n", model_target_source_tensor_container(record));
    model_target_out_writef(model_target_out(), "source_tensor_payload_status: %s\n",
           model_target_source_tensor_payload_status(record));
    model_target_out_writef(model_target_out(), "source_provenance_status: %s\n",
           model_target_source_provenance_status(record));
    model_target_out_writef(model_target_out(), "source_origin: %s\n", model_target_source_origin(record));
    model_target_out_writef(model_target_out(), "source_authority: %s\n", model_target_source_authority(record));
    model_target_out_writef(model_target_out(), "source_revision_status: %s\n",
           model_target_source_revision_status(record));
    model_target_out_writef(model_target_out(), "source_identity_status: %s\n",
           model_target_source_identity_status(record));
    model_target_out_writef(model_target_out(), "source_hash_status: %s\n", model_target_source_hash_status(record));
    model_target_out_writef(model_target_out(), "source_verification_status: %s\n",
           model_target_source_verification_status(record));
    model_target_out_writef(model_target_out(), "native_inventory_status: %s\n",
           model_target_native_inventory_status(record));
    model_target_out_writef(model_target_out(), "native_tensor_count: 0\n");
    model_target_out_writef(model_target_out(), "native_safetensors_payload_loaded: false\n");
    model_target_out_writef(model_target_out(), "source_tensor_metadata_status: %s\n",
           model_target_source_tensor_metadata_status(record));
    model_target_out_writef(model_target_out(), "source_tensor_count: 0\n");
    model_target_out_writef(model_target_out(), "source_tensor_metadata_payload_loaded: false\n");
    model_target_out_writef(model_target_out(), "source_tensor_metadata_payload_bytes_read: 0\n");
    print_model_class_audit_hint(record);
    print_tensor_collection_audit_hint(record);
    print_tensor_map_audit_hint(record);
    print_output_head_map_audit_hint(record);
    print_tokenizer_map_audit_hint(record);
    print_missing_role_report_audit_hint(record);
    print_tensor_mapping_gate_audit_hint(record);
    model_target_out_writef(model_target_out(), "target_artifact_class: %s\n", record->target_artifact_class);
    model_target_out_writef(model_target_out(), "target_artifact_status: %s\n", model_target_target_artifact_status(record));
    model_target_out_writef(model_target_out(), "target_artifact_origin: %s\n", model_target_target_artifact_origin(record));
    model_target_out_writef(model_target_out(), "target_artifact_required: %s\n",
           model_target_target_artifact_required(record));
    model_target_out_writef(model_target_out(), "external_reference_status: %s\n",
           model_target_external_reference_status(record));
    model_target_out_writef(model_target_out(), "yvex_produced_artifact_status: %s\n",
           model_target_yvex_produced_artifact_status(record));
    model_target_out_writef(model_target_out(), "pressure_purpose: %s\n", record->pressure_purpose);
    model_target_out_writef(model_target_out(), "tensor_set: %s\n", record->tensor_set);
    model_target_out_writef(model_target_out(), "local_path_class: %s\n", record->local_path_class);
    model_target_out_writef(model_target_out(), "source_footprint_class: %s\n", record->source_footprint_class);
    model_target_out_writef(model_target_out(), "runtime_boundary: %s\n", record->runtime_boundary);
    model_target_out_writef(model_target_out(), "runtime_execution: %s\n", record->runtime_execution);
    model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
    model_target_out_writef(model_target_out(), "generation: %s\n", record->generation);
    model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
    model_target_out_writef(model_target_out(), "release_ready: false\n");
    model_target_out_writef(model_target_out(), "external_reference: %s\n", record->external_reference);
}

static void print_model_target_record_normal(const yvex_model_target_record *record)
{
    if (model_target_is_source_model_candidate(record)) {
        model_target_out_writef(model_target_out(), "target: %s\n", record->target_id);
        model_target_out_writef(model_target_out(), "family: %s  class=%s\n", record->family, record->target_class);
        model_target_out_writef(model_target_out(), "source: %s  status=%s\n",
               record->source_artifact_class,
               model_target_source_artifact_status(record));
        model_target_out_writef(model_target_out(), "artifact: %s  status=%s\n",
               record->target_artifact_class,
               model_target_target_artifact_status(record));
        model_target_out_writef(model_target_out(), "backend_selection: %s\n", model_target_backend_selection(record));
        model_target_out_writef(model_target_out(), "backend_pressure: %s\n", model_target_backend_pressure(record));
        model_target_out_writef(model_target_out(), "runtime: %s\n", record->runtime_execution);
        model_target_out_writef(model_target_out(), "generation: %s\n", record->generation);
        model_target_out_writef(model_target_out(), "next: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
        model_target_out_writef(model_target_out(), "boundary: target/source profile only; no source download/runtime/generation\n");
        model_target_out_writef(model_target_out(), "status: model-target\n");
        return;
    }
    if (strcmp(record->target_id, "glm-5.2-official-safetensors") == 0) {
        model_target_out_writef(model_target_out(), "target: %s\n", record->target_id);
        model_target_out_writef(model_target_out(), "family: %s  class=%s\n", record->family, record->target_class);
        model_target_out_writef(model_target_out(), "source: %s  status=%s\n",
               record->source_artifact_class,
               model_target_source_artifact_status(record));
        model_target_out_writef(model_target_out(), "artifact: %s  status=%s\n",
               record->target_artifact_class,
               model_target_target_artifact_status(record));
        model_target_out_writef(model_target_out(), "runtime: %s\n", record->runtime_execution);
        model_target_out_writef(model_target_out(), "generation: %s\n", record->generation);
        model_target_out_writef(model_target_out(), "next: V010.SOURCE.8\n");
        model_target_out_writef(model_target_out(), "boundary: source/storage pressure only; no GLM runtime/generation\n");
        model_target_out_writef(model_target_out(), "status: model-target\n");
        return;
    }
    if (model_target_is_selected_slice(record)) {
        model_target_out_writef(model_target_out(), "target: %s\n", record->target_id);
        model_target_out_writef(model_target_out(), "family: %s  class=%s\n", record->family, record->target_class);
        model_target_out_writef(model_target_out(), "source: %s  status=%s\n",
               record->source_artifact_class,
               model_target_source_artifact_status(record));
        model_target_out_writef(model_target_out(), "artifact: %s  status=%s\n",
               record->target_artifact_class,
               model_target_target_artifact_status(record));
        model_target_out_writef(model_target_out(), "runtime: %s\n", model_target_runtime_display(record));
        model_target_out_writef(model_target_out(), "generation: %s\n", record->generation);
        model_target_out_writef(model_target_out(), "boundary: selected-slice only; no full-runtime generation\n");
        model_target_out_writef(model_target_out(), "status: model-target\n");
        return;
    }

    model_target_out_writef(model_target_out(), "target: %s\n", record->target_id);
    model_target_out_writef(model_target_out(), "family: %s class=%s\n", record->family, record->target_class);
    model_target_out_writef(model_target_out(), "source: %s  status=%s\n",
           record->source_artifact_class,
           model_target_source_artifact_status(record));
    model_target_out_writef(model_target_out(), "artifact: %s  status=%s\n",
           record->target_artifact_class,
           model_target_target_artifact_status(record));
    model_target_out_writef(model_target_out(), "runtime: %s generation=%s\n",
           record->runtime_execution,
           record->generation);
    model_target_out_writef(model_target_out(), "top_blocker: %s\n",
           strcmp(record->runtime_execution, "unsupported") == 0
               ? "full-runtime target facts incomplete"
               : "full-runtime execution unsupported");
    model_target_out_writef(model_target_out(), "boundary: target report only, no runtime execution\n");
    model_target_out_writef(model_target_out(), "status: model-target\n");
}

static void print_model_target_report_table(const char *report,
                                            const char *status,
                                            const char *selected,
                                            unsigned long eligible_count)
{
    model_target_out_writef(model_target_out(), "%-24s  %-8s  %-8s  %8s  %s\n",
           "REPORT",
           "STATUS",
           "SELECTED",
           "ELIGIBLE",
           "NEXT");
    model_target_out_writef(model_target_out(), "%-24s  %-8s  %-8s  %8lu  %s\n",
           report ? report : "report",
           status ? status : "blocked",
           selected ? selected : "none",
           eligible_count,
           YVEX_MODEL_CLASS_NEXT_ROW);
}

static int path_exists(const char *path)
{
    struct stat st;
    return path && path[0] != '\0' && stat(path, &st) == 0 ? 1 : 0;
}

static int format_model_target_artifact_path(char *out, size_t cap,
                                             const yvex_operator_paths *operator_paths,
                                             const char *family, const char *filename)
{
    int n;

    if (!out || cap == 0 || !operator_paths || !family || !filename) {
        model_target_out_writef(model_target_err(), "model-target: artifact path fields are required\n");
        return 2;
    }
    n = snprintf(out, cap, "%s/%s/%s", operator_paths->gguf_root, family, filename);
    if (n < 0 || (size_t)n >= cap) {
        model_target_out_writef(model_target_err(), "model-target: artifact path is too long\n");
        return 2;
    }
    return 0;
}

static int print_model_target_paths(const yvex_model_target_record *record,
                                    const char *models_root_override,
                                    int audit_output)
{
    yvex_paths paths;
    yvex_operator_paths operator_paths;
    yvex_error err;
    char source_path[YVEX_PATH_CAP];
    char report_dir[YVEX_PATH_CAP];
    char reference_dir[YVEX_PATH_CAP];
    char registry_dir[YVEX_PATH_CAP];
    char artifact_path[YVEX_PATH_CAP];
    const char *family_key;
    const char *registry_alias;
    const char *source_class;
    const char *target_class;
    const char *runtime_execution;
    int source_exists;
    int report_exists;
    int reference_exists;
    int registry_exists;
    int artifact_exists;
    int rc;

    memset(&paths, 0, sizeof(paths));
    yvex_error_clear(&err);

    rc = yvex_operator_paths_resolve(&paths, models_root_override, &operator_paths, &err);
    if (rc != YVEX_OK) {
        model_target_out_writef(model_target_err(), "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }

    if (strcmp(record->family, "DeepSeek") == 0) {
        family_key = "deepseek";
    } else if (strcmp(record->family, "GLM") == 0) {
        family_key = "glm";
    } else if (strcmp(record->family, "Qwen") == 0) {
        family_key = "qwen";
    } else if (strcmp(record->family, "Gemma") == 0) {
        family_key = "gemma";
    } else {
        model_target_out_writef(model_target_err(), "model-target: no path mapping for family: %s\n", record->family);
        return 2;
    }

    rc = yvex_operator_paths_resolve_target(&operator_paths, family_key, "source",
                                            source_path, sizeof(source_path), &source_exists, &err);
    if (rc != YVEX_OK) {
        model_target_out_writef(model_target_err(), "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }
    rc = yvex_operator_paths_resolve_target(&operator_paths, family_key, "reports",
                                            report_dir, sizeof(report_dir), &report_exists, &err);
    if (rc != YVEX_OK) {
        model_target_out_writef(model_target_err(), "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }
    rc = yvex_operator_paths_resolve_target(&operator_paths, family_key, "reference",
                                            reference_dir, sizeof(reference_dir), &reference_exists, &err);
    if (rc != YVEX_OK) {
        model_target_out_writef(model_target_err(), "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }
    rc = yvex_operator_paths_resolve_target(&operator_paths, family_key, "registry",
                                            registry_dir, sizeof(registry_dir), &registry_exists, &err);
    if (rc != YVEX_OK) {
        model_target_out_writef(model_target_err(), "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }

    source_class = record->source_artifact_class;
    if (strcmp(record->target_id, "deepseek4-v4-flash-selected-embed") == 0) {
        rc = format_model_target_artifact_path(
            artifact_path, sizeof(artifact_path), &operator_paths, "deepseek",
            "deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf");
        if (rc != 0) {
            return rc;
        }
        registry_alias = record->target_id;
        target_class = record->target_artifact_class;
        runtime_execution = "selected-boundary-only";
        artifact_exists = path_exists(artifact_path);
    } else if (strcmp(record->target_id, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0) {
        rc = format_model_target_artifact_path(
            artifact_path, sizeof(artifact_path), &operator_paths, "deepseek",
            "deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf");
        if (rc != 0) {
            return rc;
        }
        registry_alias = record->target_id;
        target_class = record->target_artifact_class;
        runtime_execution = "selected-segment-boundary-only";
        artifact_exists = path_exists(artifact_path);
    } else if (strcmp(record->target_id, "glm-5.2-official-safetensors") == 0) {
        snprintf(artifact_path, sizeof(artifact_path), "%s", "planned");
        registry_alias = "none";
        target_class = record->target_artifact_class;
        runtime_execution = "unsupported";
        artifact_exists = 0;
    } else if (model_target_is_source_model_candidate(record)) {
        rc = format_model_target_artifact_path(
            artifact_path, sizeof(artifact_path), &operator_paths, family_key,
            record->target_id);
        if (rc != 0) {
            return rc;
        }
        registry_alias = "none";
        source_class = record->source_artifact_class;
        target_class = record->target_artifact_class;
        runtime_execution = "unsupported";
        artifact_exists = path_exists(artifact_path);
    } else {
        model_target_out_writef(model_target_err(), "model-target: no path mapping for target: %s\n", record->target_id);
        return 2;
    }

    if (!audit_output) {
        if (model_target_is_source_model_candidate(record)) {
            model_target_out_writef(model_target_out(), "target: %s\n", record->target_id);
            model_target_out_writef(model_target_out(), "source: %s  %s\n", source_exists ? "present" : "missing", source_path);
            model_target_out_writef(model_target_out(), "source_class: %s\n", source_class);
            model_target_out_writef(model_target_out(), "artifact: planned  %s\n", artifact_path);
            model_target_out_writef(model_target_out(), "artifact_class: %s\n", target_class);
            model_target_out_writef(model_target_out(), "backend_selection: %s\n", model_target_backend_selection(record));
            model_target_out_writef(model_target_out(), "backend_pressure: %s\n", model_target_backend_pressure(record));
            model_target_out_writef(model_target_out(), "reports: %s\n", report_dir);
            model_target_out_writef(model_target_out(), "registry: %s\n", registry_dir);
            model_target_out_writef(model_target_out(), "boundary: path report only, no runtime execution\n");
            model_target_out_writef(model_target_out(), "status: model-target-paths\n");
            return 0;
        }
        model_target_out_writef(model_target_out(), "target: %s\n", record->target_id);
        model_target_out_writef(model_target_out(), "models_root: %s\n", operator_paths.models_root);
        model_target_out_writef(model_target_out(), "source: %s exists=%s\n", source_path, source_exists ? "true" : "false");
        model_target_out_writef(model_target_out(), "source_class: %s\n", source_class);
        model_target_out_writef(model_target_out(), "artifact: %s exists=%s\n", artifact_path, artifact_exists ? "true" : "false");
        model_target_out_writef(model_target_out(), "artifact_class: %s\n", target_class);
        model_target_out_writef(model_target_out(), "registry_alias: %s\n", registry_alias);
        model_target_out_writef(model_target_out(), "boundary: path report only, no payload read or runtime execution\n");
        model_target_out_writef(model_target_out(), "status: model-target-paths\n");
    } else {
        model_target_out_writef(model_target_out(), "models_root_source: %s\n", operator_paths.models_root_source);
        model_target_out_writef(model_target_out(), "models_root: %s\n", operator_paths.models_root);
        model_target_out_writef(model_target_out(), "source_path: %s\n", source_path);
        model_target_out_writef(model_target_out(), "source_exists: %s\n", source_exists ? "true" : "false");
        model_target_out_writef(model_target_out(), "artifact_path: %s\n", artifact_path);
        model_target_out_writef(model_target_out(), "artifact_exists: %s\n", artifact_exists ? "true" : "false");
        model_target_out_writef(model_target_out(), "report_dir: %s\n", report_dir);
        model_target_out_writef(model_target_out(), "report_dir_exists: %s\n", report_exists ? "true" : "false");
        model_target_out_writef(model_target_out(), "reference_dir: %s\n", reference_dir);
        model_target_out_writef(model_target_out(), "reference_dir_exists: %s\n", reference_exists ? "true" : "false");
        model_target_out_writef(model_target_out(), "registry_dir: %s\n", registry_dir);
        model_target_out_writef(model_target_out(), "registry_dir_exists: %s\n", registry_exists ? "true" : "false");
        model_target_out_writef(model_target_out(), "registry_alias: %s\n", registry_alias);
        model_target_out_writef(model_target_out(), "source_artifact_class: %s\n", source_class);
        model_target_out_writef(model_target_out(), "source_artifact_status: %s\n",
               source_exists ? "present" : model_target_source_artifact_status(record));
        model_target_out_writef(model_target_out(), "source_artifact_format: %s\n",
               model_target_source_artifact_format(record));
        model_target_out_writef(model_target_out(), "source_artifact_origin: %s\n",
               model_target_source_artifact_origin(record));
        model_target_out_writef(model_target_out(), "source_artifact_authority: %s\n",
               model_target_source_artifact_authority(record));
        model_target_out_writef(model_target_out(), "source_sidecar_status: %s\n",
               model_target_source_sidecar_status(record));
        model_target_out_writef(model_target_out(), "source_tensor_container: %s\n",
               model_target_source_tensor_container(record));
        model_target_out_writef(model_target_out(), "source_tensor_payload_status: %s\n",
               source_exists ? "present-not-loaded"
                             : model_target_source_tensor_payload_status(record));
        model_target_out_writef(model_target_out(), "target_artifact_class: %s\n", target_class);
        model_target_out_writef(model_target_out(), "target_artifact_status: %s\n",
               artifact_exists ? "present" : model_target_target_artifact_status(record));
        model_target_out_writef(model_target_out(), "target_artifact_origin: %s\n",
               model_target_target_artifact_origin(record));
        model_target_out_writef(model_target_out(), "target_artifact_required: %s\n",
               model_target_target_artifact_required(record));
        model_target_out_writef(model_target_out(), "backend_selection: %s\n", model_target_backend_selection(record));
        model_target_out_writef(model_target_out(), "backend_pressure: %s\n", model_target_backend_pressure(record));
        model_target_out_writef(model_target_out(), "external_reference_status: %s\n",
               model_target_external_reference_status(record));
        model_target_out_writef(model_target_out(), "yvex_produced_artifact_status: %s\n",
               model_target_yvex_produced_artifact_status(record));
        model_target_out_writef(model_target_out(), "runtime_execution: %s\n", runtime_execution);
        model_target_out_writef(model_target_out(), "runtime_claim: unsupported\n");
        model_target_out_writef(model_target_out(), "generation: unsupported\n");
        model_target_out_writef(model_target_out(), "benchmark_status: not-measured\n");
        model_target_out_writef(model_target_out(), "release_ready: false\n");
        model_target_out_writef(model_target_out(), "status: model-target-paths\n");
    }
    return 0;
}

/*
 * yvex_model_target_command()
 *
 * Purpose:
 *   parse and dispatch the model-target command grammar, usage/help, report
 *   rendering, output-mode selection, and transitional output-contract checks.
 *
 * Inputs:
 *   argc/argv are borrowed CLI arguments; command handlers borrow static target
 *   registry and local report sidecars as needed.
 *
 * Effects:
 *   prints normal/table/audit/report output, reads header/sidecar evidence for
 *   report builders, and may write report-only map sidecars for dynamic
 *   downloaded targets. It does not own domain model state or execute runtime
 *   graph/generation paths.
 *
 * Failure:
 *   returns parser exit code 2 for invalid grammar/options and command-specific
 *   failure codes for report builder failures.
 *
 * Boundary:
 *   model-target command grammar and CLI reports expose existing facts and do
 *   not create capability, quantization, artifact emission, runtime support,
 *   generation support, eval, benchmark, throughput, or release readiness.
 */
static int model_target_legacy_command(int argc, char **argv)
{
    const yvex_model_target_record *record;
    const char *models_root = NULL;
    yvex_model_target_output_mode output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
    int want_paths = 0;
    int i;

    if (argc <= 2) {
        print_model_target_usage(model_target_err());
        return 2;
    }
    if (strcmp(argv[2], "help") == 0 || strcmp(argv[2], "--help") == 0) {
        if (argc != 3) {
            print_model_target_usage(model_target_err());
            return 2;
        }
        model_target_legacy_help(model_target_out());
        return 0;
    }
    if (strcmp(argv[2], "classes") == 0) {
        if (argc != 3) {
            print_model_target_usage(model_target_err());
            return 2;
        }
        print_model_target_classes();
        return 0;
    }
    if (strcmp(argv[2], "list") == 0) {
        yvex_model_target_output_mode output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        for (i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target list: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    model_target_out_writef(model_target_err(), "model-target list: unsupported output mode: %s\n", argv[i]);
                    return 2;
                }
            } else {
                model_target_out_writef(model_target_err(), "model-target list: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_model_target_list();
        } else {
            print_model_target_list_table();
        }
        return 0;
    }
    if (strcmp(argv[2], "candidate") == 0) {
        const char *release = NULL;
        const char *target_id = NULL;
        int include_candidates = 0;
        int include_pressure_targets = 0;
        int include_blockers = 0;
        int include_next = 0;
        yvex_model_target_output_mode output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;

        for (i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--help") == 0) {
                if (argc != 4) {
                    print_model_target_candidate_usage(model_target_err());
                    return 2;
                }
                print_model_target_candidate_help(model_target_out());
                return 0;
            } else if (strcmp(argv[i], "--release") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target candidate: --release requires VERSION\n");
                    return 2;
                }
                release = argv[++i];
            } else if (strcmp(argv[i], "--target") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target candidate: --target requires TARGET\n");
                    return 2;
                }
                target_id = argv[++i];
            } else if (strcmp(argv[i], "--include-candidates") == 0) {
                include_candidates = 1;
            } else if (strcmp(argv[i], "--include-pressure-targets") == 0) {
                include_pressure_targets = 1;
            } else if (strcmp(argv[i], "--include-blockers") == 0) {
                include_blockers = 1;
            } else if (strcmp(argv[i], "--include-next") == 0) {
                include_next = 1;
            } else if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target candidate: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    model_target_out_writef(model_target_err(), "model-target candidate: unsupported output mode: %s\n", argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--json") == 0) {
                model_target_out_writef(model_target_err(), "model-target candidate: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
            } else {
                model_target_out_writef(model_target_err(), "model-target candidate: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        if (!release || release[0] == '\0') {
            model_target_out_writef(model_target_err(), "model-target candidate: --release is required\n");
            print_model_target_candidate_usage(model_target_err());
            return 2;
        }
        if (strcmp(release, "v0.1.0") != 0) {
            return print_model_target_candidate_unsupported_release(release);
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_model_target_report_table("full-runtime-candidate",
                                            "missing",
                                            "none",
                                            0);
            return 0;
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_NORMAL) {
            return print_model_target_candidate_normal(release, target_id);
        }
        return print_model_target_candidate_report(release,
                                                   target_id,
                                                   include_candidates,
                                                   include_pressure_targets,
                                                   include_blockers,
                                                   include_next);
    }
    if (strcmp(argv[2], "dense-candidate") == 0) {
        const char *release = NULL;
        const char *target_id = NULL;
        int include_candidates = 0;
        int include_requirements = 0;
        int include_blockers = 0;
        int include_next = 0;
        yvex_model_target_output_mode output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;

        for (i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--help") == 0) {
                if (argc != 4) {
                    print_model_target_dense_candidate_usage(model_target_err());
                    return 2;
                }
                print_model_target_dense_candidate_help(model_target_out());
                return 0;
            } else if (strcmp(argv[i], "--release") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target dense-candidate: --release requires VERSION\n");
                    return 2;
                }
                release = argv[++i];
            } else if (strcmp(argv[i], "--target") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target dense-candidate: --target requires TARGET\n");
                    return 2;
                }
                target_id = argv[++i];
            } else if (strcmp(argv[i], "--include-candidates") == 0) {
                include_candidates = 1;
            } else if (strcmp(argv[i], "--include-requirements") == 0) {
                include_requirements = 1;
            } else if (strcmp(argv[i], "--include-blockers") == 0) {
                include_blockers = 1;
            } else if (strcmp(argv[i], "--include-next") == 0) {
                include_next = 1;
            } else if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target dense-candidate: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    model_target_out_writef(model_target_err(), "model-target dense-candidate: unsupported output mode: %s\n", argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--json") == 0) {
                model_target_out_writef(model_target_err(), "model-target dense-candidate: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
            } else {
                model_target_out_writef(model_target_err(), "model-target dense-candidate: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        if (!release || release[0] == '\0') {
            model_target_out_writef(model_target_err(), "model-target dense-candidate: --release is required\n");
            print_model_target_dense_candidate_usage(model_target_err());
            return 2;
        }
        if (strcmp(release, "v0.1.0") != 0) {
            return print_model_target_dense_candidate_unsupported_release(release);
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_model_target_report_table("dense-candidate",
                                            "missing",
                                            "none",
                                            0);
            return 0;
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_NORMAL) {
            return print_model_target_dense_candidate_normal(release, target_id);
        }
        return print_model_target_dense_candidate_report(release,
                                                         target_id,
                                                         include_candidates,
                                                         include_requirements,
                                                         include_blockers,
                                                         include_next);
    }
    if (strcmp(argv[2], "qwen-metal") == 0) {
        const char *release = NULL;
        const char *target_id = NULL;
        int include_candidates = 0;
        int include_hardware = 0;
        int include_backend = 0;
        int include_source = 0;
        int include_blockers = 0;
        int include_next = 0;
        yvex_model_target_output_mode output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;

        for (i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--help") == 0) {
                if (argc != 4) {
                    print_model_target_qwen_metal_usage(model_target_err());
                    return 2;
                }
                print_model_target_qwen_metal_help(model_target_out());
                return 0;
            } else if (strcmp(argv[i], "--release") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target qwen-metal: --release requires VERSION\n");
                    return 2;
                }
                release = argv[++i];
            } else if (strcmp(argv[i], "--target") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target qwen-metal: --target requires TARGET\n");
                    return 2;
                }
                target_id = argv[++i];
            } else if (strcmp(argv[i], "--include-candidates") == 0) {
                include_candidates = 1;
            } else if (strcmp(argv[i], "--include-hardware") == 0) {
                include_hardware = 1;
            } else if (strcmp(argv[i], "--include-backend") == 0) {
                include_backend = 1;
            } else if (strcmp(argv[i], "--include-source") == 0) {
                include_source = 1;
            } else if (strcmp(argv[i], "--include-blockers") == 0) {
                include_blockers = 1;
            } else if (strcmp(argv[i], "--include-next") == 0) {
                include_next = 1;
            } else if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target qwen-metal: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    model_target_out_writef(model_target_err(), "model-target qwen-metal: unsupported output mode: %s\n", argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--json") == 0) {
                model_target_out_writef(model_target_err(), "model-target qwen-metal: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
            } else {
                model_target_out_writef(model_target_err(), "model-target qwen-metal: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        if (!release || release[0] == '\0') {
            model_target_out_writef(model_target_err(), "model-target qwen-metal: --release is required\n");
            print_model_target_qwen_metal_usage(model_target_err());
            return 2;
        }
        if (strcmp(release, "v0.1.0") != 0) {
            return print_model_target_qwen_metal_unsupported_release(release);
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_model_target_report_table("qwen-metal-pressure",
                                            "pressure",
                                            "none",
                                            0);
            return 0;
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_NORMAL) {
            return print_model_target_qwen_metal_normal(release, target_id);
        }
        return print_model_target_qwen_metal_report(release,
                                                    target_id,
                                                    include_candidates,
                                                    include_hardware,
                                                    include_backend,
                                                    include_source,
                                                    include_blockers,
                                                    include_next);
    }
    if (strcmp(argv[2], "decision") == 0) {
        const yvex_model_target_record *candidate_filter = NULL;
        const char *release = NULL;
        const char *candidate_id = NULL;
        yvex_model_target_output_mode output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;

        for (i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--help") == 0) {
                if (argc != 4) {
                    print_model_target_decision_usage(model_target_err());
                    return 2;
                }
                print_model_target_decision_help(model_target_out());
                return 0;
            } else if (strcmp(argv[i], "--release") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target decision: --release requires VERSION\n");
                    return 2;
                }
                release = argv[++i];
            } else if (strcmp(argv[i], "--candidate") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target decision: --candidate requires TARGET\n");
                    return 2;
                }
                candidate_id = argv[++i];
            } else if (strcmp(argv[i], "--include-candidates") == 0 ||
                       strcmp(argv[i], "--include-pressure-targets") == 0 ||
                       strcmp(argv[i], "--include-blockers") == 0 ||
                       strcmp(argv[i], "--include-critical-path") == 0 ||
                       strcmp(argv[i], "--include-next") == 0 ||
                       strcmp(argv[i], "--strict") == 0) {
                continue;
            } else if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target decision: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    model_target_out_writef(model_target_err(), "model-target decision: unsupported output mode: %s\n", argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--json") == 0) {
                model_target_out_writef(model_target_err(), "model-target decision: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
            } else {
                model_target_out_writef(model_target_err(), "model-target decision: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        if (!release || release[0] == '\0') {
            model_target_out_writef(model_target_err(), "model-target decision: --release is required\n");
            print_model_target_decision_usage(model_target_err());
            return 2;
        }
        if (strcmp(release, "v0.1.0") != 0) {
            return print_model_target_decision_unsupported_release(release);
        }
        if (candidate_id) {
            candidate_filter = find_model_target(candidate_id);
            if (!candidate_filter) {
                return print_model_target_decision_missing_candidate(release, candidate_id);
            }
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_model_target_report_table("target-decision",
                                            "blocked",
                                            "none",
                                            0);
            return 0;
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_NORMAL) {
            return print_model_target_decision_normal(release, candidate_filter);
        }
        return print_model_target_decision_report(release, candidate_filter);
    }
    if (strcmp(argv[2], "class-profile") == 0) {
        const char *target_id = NULL;
        const char *source = NULL;
        const yvex_model_class_profile_spec *spec;
        yvex_dynamic_source_target dynamic_target;
        yvex_model_class_profile profile;
        int rc;

        output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        if (argc < 4) {
            model_target_out_writef(model_target_err(), "model-target class-profile: requires TARGET\n");
            return 2;
        }
        target_id = argv[3];
        record = NULL;
        spec = NULL;
        memset(&dynamic_target, 0, sizeof(dynamic_target));
        for (i = 4; i < argc; ++i) {
            if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target class-profile: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    model_target_out_writef(model_target_err(), "model-target class-profile: unsupported output mode: %s\n",
                            argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target class-profile: --models-root requires DIR\n");
                    return 2;
                }
                models_root = argv[++i];
            } else if (strcmp(argv[i], "--source") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target class-profile: --source requires DIR\n");
                    return 2;
                }
                source = argv[++i];
            } else if (strcmp(argv[i], "--json") == 0) {
                model_target_out_writef(model_target_err(), "model-target class-profile: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
            } else {
                model_target_out_writef(model_target_err(), "model-target class-profile: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        record = find_model_target(target_id);
        spec = find_model_class_profile_spec(target_id);
        if ((!record || !spec) &&
            model_target_resolve_dynamic_source_target(target_id,
                                                       models_root,
                                                       &dynamic_target)) {
            record = &dynamic_target.record;
            spec = &dynamic_target.spec;
            if (!source) source = dynamic_target.source_path;
        }
        if (!record || !spec) {
            model_target_out_writef(model_target_err(), "model-target class-profile: unsupported target: %s\n",
                    target_id && target_id[0] ? target_id : "none");
            return 2;
        }
        rc = model_class_build_profile(record, spec, models_root, source, &profile);
        if (rc != 0) {
            return rc;
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_model_class_profile_table(&profile);
        } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_model_class_profile_audit(&profile);
        } else {
            print_model_class_profile_normal(&profile);
        }
        return 0;
    }
    if (strcmp(argv[2], "tensor-collection") == 0) {
        const char *target_id = NULL;
        const char *source = NULL;
        const yvex_model_class_profile_spec *spec;
        yvex_dynamic_source_target dynamic_target;
        yvex_tensor_collection_profile profile;
        int rc;

        output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        if (argc < 4) {
            model_target_out_writef(model_target_err(), "model-target tensor-collection: requires TARGET\n");
            return 2;
        }
        target_id = argv[3];
        record = NULL;
        spec = NULL;
        memset(&dynamic_target, 0, sizeof(dynamic_target));
        for (i = 4; i < argc; ++i) {
            if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target tensor-collection: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    model_target_out_writef(model_target_err(), "model-target tensor-collection: unsupported output mode: %s\n",
                            argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target tensor-collection: --models-root requires DIR\n");
                    return 2;
                }
                models_root = argv[++i];
            } else if (strcmp(argv[i], "--source") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target tensor-collection: --source requires DIR\n");
                    return 2;
                }
                source = argv[++i];
            } else if (strcmp(argv[i], "--json") == 0) {
                model_target_out_writef(model_target_err(), "model-target tensor-collection: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
            } else {
                model_target_out_writef(model_target_err(), "model-target tensor-collection: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        record = find_model_target(target_id);
        spec = find_model_class_profile_spec(target_id);
        if ((!record || !spec) &&
            model_target_resolve_dynamic_source_target(target_id,
                                                       models_root,
                                                       &dynamic_target)) {
            record = &dynamic_target.record;
            spec = &dynamic_target.spec;
            if (!source) source = dynamic_target.source_path;
        }
        if (!record || !spec) {
            model_target_out_writef(model_target_err(), "model-target tensor-collection: unsupported target: %s\n",
                    target_id && target_id[0] ? target_id : "none");
            return 2;
        }
        rc = build_tensor_collection_profile(record, spec, models_root, source, &profile);
        if (rc != 0) {
            return rc;
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_tensor_collection_table(&profile);
        } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_tensor_collection_audit(&profile);
        } else {
            print_tensor_collection_normal(&profile);
        }
        return 0;
    }
    if (strcmp(argv[2], "missing-roles") == 0) {
        const char *target_id = NULL;
        const char *source = NULL;
        const yvex_model_class_profile_spec *spec;
        yvex_dynamic_source_target dynamic_target;
        yvex_missing_role_report_profile profile;
        yvex_tensor_naming_profile naming_profile;
        yvex_output_head_map_profile output_head_profile;
        yvex_missing_roles_porcelain_context porcelain;
        yvex_paths paths;
        yvex_operator_paths operator_paths;
        yvex_error err;
        int output_json = 0;
        int rc;

        output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        if (argc < 4) {
            model_target_out_writef(model_target_err(), "model-target missing-roles: requires TARGET\n");
            return 2;
        }
        target_id = argv[3];
        record = NULL;
        spec = NULL;
        memset(&dynamic_target, 0, sizeof(dynamic_target));
        yvex_error_clear(&err);
        for (i = 4; i < argc; ++i) {
            if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
                output_json = 0;
            } else if (strcmp(argv[i], "--json") == 0) {
                output_json = 1;
            } else if (strcmp(argv[i], "--output") == 0) {
                const char *value;
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target missing-roles: --output requires normal|table|audit|json\n");
                    return 2;
                }
                value = argv[++i];
                if (strcmp(value, "json") == 0) {
                    output_json = 1;
                } else if (parse_model_target_output_mode(value, &output_mode)) {
                    output_json = 0;
                } else {
                    model_target_out_writef(model_target_err(), "model-target missing-roles: unsupported output mode: %s\n",
                            value);
                    return 2;
                }
            } else if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target missing-roles: --models-root requires DIR\n");
                    return 2;
                }
                models_root = argv[++i];
            } else if (strcmp(argv[i], "--source") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target missing-roles: --source requires DIR\n");
                    return 2;
                }
                source = argv[++i];
            } else {
                model_target_out_writef(model_target_err(), "model-target missing-roles: unknown option: %s\n",
                        argv[i]);
                return 2;
            }
        }

        record = find_model_target(target_id);
        spec = find_model_class_profile_spec(target_id);
        if ((!record || !spec) &&
            model_target_resolve_dynamic_source_target(target_id,
                                                       models_root,
                                                       &dynamic_target)) {
            record = &dynamic_target.record;
            spec = &dynamic_target.spec;
            if (!source) source = dynamic_target.source_path;
        }
        if (!record || !spec ||
            (strcmp(spec->family_key, "qwen") != 0 &&
             strcmp(spec->family_key, "gemma") != 0)) {
            model_target_out_writef(model_target_err(), "model-target missing-roles: unsupported target: %s\n",
                    target_id && target_id[0] ? target_id : "none");
            return 2;
        }
        rc = yvex_paths_default(&paths, &err);
        if (rc != YVEX_OK ||
            yvex_operator_paths_resolve(&paths, models_root, &operator_paths,
                                        &err) != YVEX_OK) {
            model_target_out_writef(model_target_err(), "model-target missing-roles: cannot resolve operator paths: %s\n",
                    err.message[0] ? err.message : "unknown");
            return 2;
        }
        rc = build_missing_role_report_profile(record, spec, models_root, source,
                                               &profile);
        if (rc != 0) {
            return rc;
        }
        if (dynamic_target.found) {
            rc = build_tensor_naming_profile(record, spec, models_root, source,
                                             &naming_profile);
            if (rc != 0) {
                return rc;
            }
            if (!write_tensor_map_sidecar(dynamic_target.tensor_map_path,
                                          &naming_profile)) {
                model_target_out_writef(model_target_err(),
                        "model-target missing-roles: cannot write tensor map sidecar: %s\n",
                        dynamic_target.tensor_map_path);
                return 3;
            }
            rc = build_output_head_map_profile(record, spec, models_root, source,
                                               &output_head_profile);
            if (rc != 0) {
                return rc;
            }
            if (!write_output_head_map_sidecar(dynamic_target.output_head_map_path,
                                               &output_head_profile)) {
                model_target_out_writef(model_target_err(),
                        "model-target missing-roles: cannot write output-head map sidecar: %s\n",
                        dynamic_target.output_head_map_path);
                return 3;
            }
        }
        build_missing_roles_porcelain_context(&operator_paths, &profile,
                                              &porcelain);
        if (output_json) {
            print_missing_roles_porcelain_json(&profile, &porcelain);
        } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_missing_roles_porcelain_table(&profile, &porcelain);
        } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_missing_roles_porcelain_audit(&profile, &porcelain);
        } else {
            print_missing_roles_porcelain_normal(&profile, &porcelain);
        }
        return 0;
    }
    if (strcmp(argv[2], "tokenizer-map") == 0) {
        const char *target_id = NULL;
        const char *source = NULL;
        const yvex_model_class_profile_spec *spec;
        yvex_dynamic_source_target dynamic_target;
        yvex_tokenizer_map_profile tokenizer_profile;
        int output_json = 0;
        int rc;

        output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        if (argc < 4) {
            model_target_out_writef(model_target_err(), "model-target tokenizer-map: requires TARGET\n");
            return 2;
        }
        target_id = argv[3];
        record = NULL;
        spec = NULL;
        memset(&dynamic_target, 0, sizeof(dynamic_target));
        for (i = 4; i < argc; ++i) {
            if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
                output_json = 0;
            } else if (strcmp(argv[i], "--json") == 0) {
                output_json = 1;
            } else if (strcmp(argv[i], "--output") == 0) {
                const char *value;
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(),
                            "model-target tokenizer-map: --output requires normal|table|audit|json\n");
                    return 2;
                }
                value = argv[++i];
                if (strcmp(value, "json") == 0) {
                    output_json = 1;
                } else if (parse_model_target_output_mode(value, &output_mode)) {
                    output_json = 0;
                } else {
                    model_target_out_writef(model_target_err(),
                            "model-target tokenizer-map: unsupported output mode: %s\n",
                            value);
                    return 2;
                }
            } else if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(),
                            "model-target tokenizer-map: --models-root requires DIR\n");
                    return 2;
                }
                models_root = argv[++i];
            } else if (strcmp(argv[i], "--source") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(),
                            "model-target tokenizer-map: --source requires DIR\n");
                    return 2;
                }
                source = argv[++i];
            } else {
                model_target_out_writef(model_target_err(), "model-target tokenizer-map: unknown option: %s\n",
                        argv[i]);
                return 2;
            }
        }
        record = find_model_target(target_id);
        spec = find_model_class_profile_spec(target_id);
        if ((!record || !spec) &&
            model_target_resolve_dynamic_source_target(target_id,
                                                       models_root,
                                                       &dynamic_target)) {
            record = &dynamic_target.record;
            spec = &dynamic_target.spec;
            if (!source) source = dynamic_target.source_path;
        }
        if (!record || !spec ||
            (strcmp(spec->family_key, "qwen") != 0 &&
             strcmp(spec->family_key, "gemma") != 0)) {
            model_target_out_writef(model_target_err(), "model-target tokenizer-map: unsupported target: %s\n",
                    target_id && target_id[0] ? target_id : "none");
            return 2;
        }
        rc = build_tokenizer_map_profile(record, spec, models_root, source,
                                         &tokenizer_profile);
        if (rc != 0) {
            return rc;
        }
        if (dynamic_target.found &&
            !write_tokenizer_map_sidecar(dynamic_target.tokenizer_map_path,
                                         &tokenizer_profile)) {
            model_target_out_writef(model_target_err(),
                    "model-target tokenizer-map: cannot write tokenizer map sidecar: %s\n",
                    dynamic_target.tokenizer_map_path);
            return 3;
        }
        if (output_json) {
            print_tokenizer_map_json(&tokenizer_profile);
        } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_tokenizer_map_table(&tokenizer_profile);
        } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_tokenizer_map_audit(&tokenizer_profile);
        } else {
            print_tokenizer_map_normal(&tokenizer_profile);
        }
        return 0;
    }
    if (strcmp(argv[2], "tensor-map") == 0) {
        const char *target_id = NULL;
        const char *source = NULL;
        const char *role = NULL;
        const char *gate = NULL;
        const yvex_model_class_profile_spec *spec;
        yvex_dynamic_source_target dynamic_target;
        yvex_tensor_naming_profile profile;
        yvex_output_head_map_profile output_head_profile;
        yvex_tokenizer_map_profile tokenizer_profile;
        yvex_missing_role_report_profile missing_role_profile;
        yvex_tensor_mapping_gate_profile gate_profile;
        yvex_model_target_output_mode check_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        const char *check_mode_text = NULL;
        int check_output_contract = 0;
        int rc;

        output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        if (argc < 4) {
            model_target_out_writef(model_target_err(), "model-target tensor-map: requires TARGET\n");
            return 2;
        }
        target_id = argv[3];
        record = NULL;
        spec = NULL;
        memset(&dynamic_target, 0, sizeof(dynamic_target));
        for (i = 4; i < argc; ++i) {
            if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target tensor-map: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    model_target_out_writef(model_target_err(), "model-target tensor-map: unsupported output mode: %s\n",
                            argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target tensor-map: --models-root requires DIR\n");
                    return 2;
                }
                models_root = argv[++i];
            } else if (strcmp(argv[i], "--source") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target tensor-map: --source requires DIR\n");
                    return 2;
                }
                source = argv[++i];
            } else if (strcmp(argv[i], "--role") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target tensor-map: --role requires output-head|tokenizer|missing-roles\n");
                    return 2;
                }
                role = argv[++i];
                if (strcmp(role, "output-head") != 0 &&
                    strcmp(role, "tokenizer") != 0 &&
                    strcmp(role, "missing-roles") != 0) {
                    model_target_out_writef(model_target_err(), "model-target tensor-map: unsupported role: %s\n",
                            role);
                    return 2;
                }
            } else if (strcmp(argv[i], "--gate") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target tensor-map: --gate requires v0.1.0\n");
                    return 2;
                }
                gate = argv[++i];
                if (strcmp(gate, "v0.1.0") != 0) {
                    model_target_out_writef(model_target_err(), "model-target tensor-map: unsupported release: %s\n",
                            gate);
                    return 2;
                }
            } else if (strcmp(argv[i], "--check-output-contract") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    return output_contract_print_refusal(
                        "model-target tensor-map",
                        target_id,
                        "none",
                        "parser-error");
                }
                check_mode_text = argv[++i];
                if (!parse_model_target_output_mode(check_mode_text, &check_mode)) {
                    return output_contract_print_refusal(
                        "model-target tensor-map",
                        target_id,
                        check_mode_text,
                        "unsupported-mode");
                }
                check_output_contract = 1;
            } else if (strcmp(argv[i], "--json") == 0) {
                model_target_out_writef(model_target_err(), "model-target tensor-map: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
            } else {
                model_target_out_writef(model_target_err(), "model-target tensor-map: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        if (role && gate) {
            model_target_out_writef(model_target_err(), "model-target tensor-map: --gate cannot be combined with --role\n");
            return 2;
        }
        record = find_model_target(target_id);
        spec = find_model_class_profile_spec(target_id);
        if ((!record || !spec) &&
            model_target_resolve_dynamic_source_target(target_id,
                                                       models_root,
                                                       &dynamic_target)) {
            record = &dynamic_target.record;
            spec = &dynamic_target.spec;
            if (!source) source = dynamic_target.source_path;
        }
        if (!record || !spec ||
            (strcmp(spec->family_key, "qwen") != 0 &&
             strcmp(spec->family_key, "gemma") != 0)) {
            if (check_output_contract) {
                return output_contract_print_refusal(
                    "model-target tensor-map",
                    target_id,
                    check_mode_text,
                    "unsupported-target");
            }
            model_target_out_writef(model_target_err(), "model-target tensor-map: unsupported target: %s\n",
                    target_id && target_id[0] ? target_id : "none");
            return 2;
        }
        if (gate) {
            rc = build_tensor_mapping_gate_profile(record, models_root, source,
                                                   &gate_profile);
            if (rc != 0) {
                return rc;
            }
            if (check_output_contract) {
                return output_contract_print_result(
                    YVEX_OUTPUT_CONTRACT_MAPPING_GATE,
                    record->target_id,
                    check_mode,
                    &gate_profile);
            }
            if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
                print_tensor_mapping_gate_table(&gate_profile);
            } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
                print_tensor_mapping_gate_audit(&gate_profile);
            } else {
                print_tensor_mapping_gate_normal(&gate_profile);
            }
            return 0;
        }
        if (role && strcmp(role, "output-head") == 0) {
            rc = build_output_head_map_profile(record, spec, models_root, source,
                                               &output_head_profile);
            if (rc != 0) {
                return rc;
            }
            if (!check_output_contract &&
                dynamic_target.found &&
                !write_output_head_map_sidecar(dynamic_target.output_head_map_path,
                                               &output_head_profile)) {
                model_target_out_writef(model_target_err(),
                        "model-target tensor-map: cannot write output-head map sidecar: %s\n",
                        dynamic_target.output_head_map_path);
                return 3;
            }
            if (check_output_contract) {
                return output_contract_print_result(
                    YVEX_OUTPUT_CONTRACT_OUTPUT_HEAD,
                    record->target_id,
                    check_mode,
                    &output_head_profile);
            }
            if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
                print_output_head_map_table(&output_head_profile);
            } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
                print_output_head_map_audit(&output_head_profile);
            } else {
                print_output_head_map_normal(&output_head_profile);
            }
            return 0;
        }
        if (role && strcmp(role, "tokenizer") == 0) {
            if (check_output_contract) {
                return output_contract_print_refusal(
                    "model-target tensor-map tokenizer",
                    target_id,
                    check_mode_text,
                    "unsupported-mode");
            }
            rc = build_tokenizer_map_profile(record, spec, models_root, source,
                                             &tokenizer_profile);
            if (rc != 0) {
                return rc;
            }
            if (dynamic_target.found &&
                !write_tokenizer_map_sidecar(dynamic_target.tokenizer_map_path,
                                             &tokenizer_profile)) {
                model_target_out_writef(model_target_err(),
                        "model-target tensor-map: cannot write tokenizer map sidecar: %s\n",
                        dynamic_target.tokenizer_map_path);
                return 3;
            }
            if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
                print_tokenizer_map_table(&tokenizer_profile);
            } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
                print_tokenizer_map_audit(&tokenizer_profile);
            } else {
                print_tokenizer_map_normal(&tokenizer_profile);
            }
            return 0;
        }
        if (role && strcmp(role, "missing-roles") == 0) {
            rc = build_missing_role_report_profile(record, spec, models_root, source,
                                                   &missing_role_profile);
            if (rc != 0) {
                return rc;
            }
            if (check_output_contract) {
                return output_contract_print_result(
                    YVEX_OUTPUT_CONTRACT_MISSING_ROLES,
                    record->target_id,
                    check_mode,
                    &missing_role_profile);
            }
            if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
                print_missing_role_report_table(&missing_role_profile);
            } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
                print_missing_role_report_audit(&missing_role_profile);
            } else {
                print_missing_role_report_normal(&missing_role_profile);
            }
            return 0;
        }
        rc = build_tensor_naming_profile(record, spec, models_root, source, &profile);
        if (rc != 0) {
            return rc;
        }
        if (!check_output_contract &&
            dynamic_target.found &&
            !write_tensor_map_sidecar(dynamic_target.tensor_map_path, &profile)) {
            model_target_out_writef(model_target_err(),
                    "model-target tensor-map: cannot write tensor map sidecar: %s\n",
                    dynamic_target.tensor_map_path);
            return 3;
        }
        if (!check_output_contract && dynamic_target.found) {
            rc = build_output_head_map_profile(record, spec, models_root, source,
                                               &output_head_profile);
            if (rc != 0) {
                return rc;
            }
            if (!write_output_head_map_sidecar(dynamic_target.output_head_map_path,
                                               &output_head_profile)) {
                model_target_out_writef(model_target_err(),
                        "model-target tensor-map: cannot write output-head map sidecar: %s\n",
                        dynamic_target.output_head_map_path);
                return 3;
            }
        }
        if (check_output_contract) {
            return output_contract_print_result(
                YVEX_OUTPUT_CONTRACT_TENSOR_MAP,
                record->target_id,
                check_mode,
                &profile);
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_tensor_naming_table(&profile);
        } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_tensor_naming_audit(&profile);
        } else {
            print_tensor_naming_normal(&profile);
        }
        return 0;
    }
    if (strcmp(argv[2], "quant-policy") == 0) {
        const char *target_id = NULL;
        const char *source = NULL;
        const char *gate = NULL;
        yvex_qtype_policy_profile profile;
        yvex_qtype_role_support_profile role_profile;
        yvex_qtype_role_support_gate_profile gate_profile;
        yvex_dynamic_source_target dynamic_target;
        const yvex_model_class_profile_spec *spec = NULL;
        yvex_model_target_output_mode check_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        const char *check_mode_text = NULL;
        int check_output_contract = 0;
        int role_support = 0;
        int start_arg;
        int rc;

        output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        if (argc >= 4 && argv[3][0] != '-') {
            target_id = argv[3];
            start_arg = 4;
        } else {
            start_arg = 3;
        }
        for (i = start_arg; i < argc; ++i) {
            if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--role-support") == 0 ||
                       strcmp(argv[i], "--roles") == 0) {
                role_support = 1;
            } else if (strcmp(argv[i], "--gate") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target quant-policy: --gate requires v0.1.0\n");
                    return 2;
                }
                gate = argv[++i];
                if (strcmp(gate, "v0.1.0") != 0) {
                    model_target_out_writef(model_target_err(), "model-target quant-policy: unsupported gate: %s\n",
                            gate);
                    return 2;
                }
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target quant-policy: --output requires normal|table|audit\n");
                    return 2;
                }
                ++i;
                if (strcmp(argv[i], "json") == 0) {
                    model_target_out_writef(model_target_err(), "model-target quant-policy: JSON output is unsupported; use --output normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[i], &output_mode)) {
                    model_target_out_writef(model_target_err(), "model-target quant-policy: unsupported output mode: %s\n",
                            argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target quant-policy: --models-root requires DIR\n");
                    return 2;
                }
                models_root = argv[++i];
            } else if (strcmp(argv[i], "--source") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    model_target_out_writef(model_target_err(), "model-target quant-policy: --source requires DIR\n");
                    return 2;
                }
                source = argv[++i];
            } else if (strcmp(argv[i], "--check-output-contract") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    return output_contract_print_refusal(
                        "model-target quant-policy",
                        target_id,
                        "none",
                        "parser-error");
                }
                check_mode_text = argv[++i];
                if (!parse_model_target_output_mode(check_mode_text, &check_mode)) {
                    return output_contract_print_refusal(
                        "model-target quant-policy",
                        target_id,
                        check_mode_text,
                        "unsupported-mode");
                }
                check_output_contract = 1;
            } else if (strcmp(argv[i], "--json") == 0) {
                model_target_out_writef(model_target_err(), "model-target quant-policy: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
            } else {
                model_target_out_writef(model_target_err(), "model-target quant-policy: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        if (gate) {
            if (target_id) {
                model_target_out_writef(model_target_err(), "model-target quant-policy: --gate v0.1.0 is an aggregate report and does not take TARGET\n");
                return 2;
            }
            if (check_output_contract) {
                model_target_out_writef(model_target_err(), "model-target quant-policy: --check-output-contract is unsupported for --gate\n");
                return 2;
            }
            rc = build_qtype_role_support_gate_profile(models_root, &gate_profile);
            if (rc != 0) return rc;
            if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
                print_qtype_role_support_gate_audit(&gate_profile);
            } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
                print_qtype_role_support_gate_table(&gate_profile);
                print_qtype_role_support_boundary();
            } else {
                print_qtype_role_support_gate_normal(&gate_profile);
            }
            return 0;
        }
        if (!target_id) {
            model_target_out_writef(model_target_err(), "model-target quant-policy: requires TARGET\n");
            return 2;
        }
        memset(&dynamic_target, 0, sizeof(dynamic_target));
        record = find_model_target(target_id);
        if (!record && role_support &&
            model_target_resolve_dynamic_source_target(target_id,
                                                       models_root,
                                                       &dynamic_target)) {
            record = &dynamic_target.record;
            spec = &dynamic_target.spec;
            if (!source) source = dynamic_target.source_path;
        }
        if (!record) {
            if (check_output_contract) {
                return output_contract_print_refusal(
                    "model-target quant-policy",
                    target_id,
                    check_mode_text,
                    "unsupported-target");
            }
            return print_qtype_policy_unsupported_target(target_id, output_mode);
        }
        if (role_support) {
            if (check_output_contract) {
                model_target_out_writef(model_target_err(), "model-target quant-policy: --check-output-contract is unsupported for --role-support\n");
                return 2;
            }
            rc = build_qtype_role_support_profile(record, spec, models_root,
                                                  source, &role_profile);
            if (rc != 0) return rc;
            if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
                print_qtype_role_support_table(&role_profile);
            } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
                print_qtype_role_support_audit(&role_profile);
            } else {
                print_qtype_role_support_normal(&role_profile);
            }
            return 0;
        }
        rc = build_qtype_policy_profile(record, models_root, source, &profile);
        if (rc != 0) {
            return rc;
        }
        if (check_output_contract) {
            return output_contract_print_result(
                YVEX_OUTPUT_CONTRACT_QTYPE_POLICY,
                record->target_id,
                check_mode,
                &profile);
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            print_qtype_policy_table(&profile);
        } else if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_qtype_policy_audit(&profile);
        } else {
            print_qtype_policy_normal(&profile);
        }
        return 0;
    }
    if (strcmp(argv[2], "inspect") == 0) {
        output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        if (argc < 4) {
            print_model_target_usage(model_target_err());
            return 2;
        }
        record = find_model_target(argv[3]);
        if (!record) {
            model_target_out_writef(model_target_err(), "model-target: unknown target: %s\n", argv[3]);
            return 2;
        }
        for (i = 4; i < argc; ++i) {
            if (strcmp(argv[i], "--paths") == 0) {
                want_paths = 1;
            } else if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target inspect: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    model_target_out_writef(model_target_err(), "model-target inspect: unsupported output mode: %s\n", argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc) {
                    model_target_out_writef(model_target_err(), "model-target: --models-root requires DIR\n");
                    return 2;
                }
                models_root = argv[++i];
            } else {
                model_target_out_writef(model_target_err(), "model-target: unknown inspect option: %s\n", argv[i]);
                return 2;
            }
        }
        if (models_root && !want_paths) {
            model_target_out_writef(model_target_err(), "model-target: --models-root requires --paths\n");
            return 2;
        }
        if (want_paths) {
            if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
                print_model_target_record(record);
            }
            return print_model_target_paths(record,
                                            models_root,
                                            output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT);
        }
        if (output_mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            print_model_target_record(record);
        } else {
            print_model_target_record_normal(record);
        }
        return 0;
    }

    model_target_out_writef(model_target_err(), "model-target: unknown subcommand: %s\n", argv[2]);
    model_target_out_writef(model_target_err(), "Try 'yvex help model-target' for usage.\n");
    return 2;
}

/*
 * yvex_model_target_report_close()
 *
 * Purpose:
 *   release buffers owned by a model-target report.
 *
 * Inputs:
 *   report is an owned-by-caller report object that may contain heap buffers.
 *
 * Effects:
 *   frees captured output buffers and clears the report by value.
 *
 * Failure:
 *   none.
 *
 * Boundary:
 *   report cleanup does not write output or change model-target facts.
 */
void yvex_model_target_report_close(yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    free(report->stdout_text);
    free(report->stderr_text);
    memset(report, 0, sizeof(*report));
}

static int model_target_report_capture_begin(FILE **out_fp,
                                             char **out_text,
                                             size_t *out_len,
                                             FILE **err_fp,
                                             char **err_text,
                                             size_t *err_len,
                                             yvex_error *err)
{
    *out_text = NULL;
    *err_text = NULL;
    *out_len = 0u;
    *err_len = 0u;
    *out_fp = open_memstream(out_text, out_len);
    if (!*out_fp) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_target_report",
                       "failed to allocate stdout capture");
        return YVEX_ERR_NOMEM;
    }
    *err_fp = open_memstream(err_text, err_len);
    if (!*err_fp) {
        fclose(*out_fp);
        free(*out_text);
        *out_fp = NULL;
        *out_text = NULL;
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_target_report",
                       "failed to allocate stderr capture");
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

static int model_target_report_capture_finish(FILE *out_fp,
                                              FILE *err_fp,
                                              char **out_text,
                                              char **err_text,
                                              yvex_error *err)
{
    int failed = 0;

    if (out_fp && fclose(out_fp) != 0) {
        failed = 1;
    }
    if (err_fp && fclose(err_fp) != 0) {
        failed = 1;
    }
    if (failed) {
        free(*out_text);
        free(*err_text);
        *out_text = NULL;
        *err_text = NULL;
        yvex_error_set(err, YVEX_ERR_IO, "model_target_report",
                       "failed to close report capture stream");
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int model_target_report_build_from_argv(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    FILE *out_fp = NULL;
    FILE *err_fp = NULL;
    char *out_text = NULL;
    char *err_text = NULL;
    size_t out_len = 0u;
    size_t err_len = 0u;
    int exit_code;
    int rc;

    rc = model_target_report_capture_begin(&out_fp, &out_text, &out_len,
                                           &err_fp, &err_text, &err_len, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    model_target_capture_out = out_fp;
    model_target_capture_err = err_fp;
    exit_code = model_target_legacy_command(request->argc, request->argv);
    model_target_capture_out = NULL;
    model_target_capture_err = NULL;
    rc = model_target_report_capture_finish(out_fp, err_fp, &out_text,
                                            &err_text, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    report->kind = request->kind;
    report->mode = request->mode;
    report->status = exit_code == 0 ? "complete" : "failed";
    report->stdout_text = out_text;
    report->stderr_text = err_text;
    report->stdout_len = out_len;
    report->stderr_len = err_len;
    report->exit_code = exit_code;
    yvex_error_clear(err);
    return YVEX_OK;
}

/*
 * yvex_model_target_report_build()
 *
 * Purpose:
 *   build a typed model-target report from parsed command arguments.
 *
 * Inputs:
 *   request borrows argc/argv and report receives owned captured output.
 *
 * Effects:
 *   runs existing model-target report construction against capture streams; it
 *   may inspect local headers/sidecars and write explicit sidecar files through
 *   existing report paths, but it does not write process stdout/stderr.
 *
 * Failure:
 *   returns invalid-arg, allocation, or capture IO errors through err.
 *
 * Boundary:
 *   report building preserves report-only model-target behavior and does not
 *   implement quantization, artifact emission, runtime execution, generation,
 *   eval, benchmark, or release readiness.
 */
int yvex_model_target_report_build(const yvex_model_target_request *request,
                                   yvex_model_target_report *report,
                                   yvex_error *err)
{
    if (!request || !report || !request->argv || request->argc <= 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_target_report",
                       "request, argc, argv, and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(report, 0, sizeof(*report));
    return model_target_report_build_from_argv(request, report, err);
}

/*
 * yvex_model_target_help_report_build()
 *
 * Purpose:
 *   build the model-target help report for callers that do not own argv.
 *
 * Inputs:
 *   report receives owned captured help text.
 *
 * Effects:
 *   renders legacy help into a typed report buffer; it does not write stdout or
 *   stderr directly.
 *
 * Failure:
 *   returns allocation or capture IO errors through err.
 *
 * Boundary:
 *   help output describes existing report surfaces only and creates no runtime
 *   or generation capability.
 */
int yvex_model_target_help_report_build(yvex_model_target_report *report,
                                        yvex_error *err)
{
    FILE *out_fp = NULL;
    FILE *err_fp = NULL;
    char *out_text = NULL;
    char *err_text = NULL;
    size_t out_len = 0u;
    size_t err_len = 0u;
    int rc;

    if (!report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_target_report",
                       "report is required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(report, 0, sizeof(*report));
    rc = model_target_report_capture_begin(&out_fp, &out_text, &out_len,
                                           &err_fp, &err_text, &err_len, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    model_target_capture_out = out_fp;
    model_target_capture_err = err_fp;
    model_target_legacy_help(model_target_out());
    model_target_capture_out = NULL;
    model_target_capture_err = NULL;
    rc = model_target_report_capture_finish(out_fp, err_fp, &out_text,
                                            &err_text, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    report->kind = YVEX_MODEL_TARGET_COMMAND_HELP;
    report->mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
    report->status = "complete";
    report->stdout_text = out_text;
    report->stderr_text = err_text;
    report->stdout_len = out_len;
    report->stderr_len = err_len;
    report->exit_code = 0;
    yvex_error_clear(err);
    return YVEX_OK;
}
