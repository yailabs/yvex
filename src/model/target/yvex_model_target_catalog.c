/*
 * yvex_model_target_catalog.c - model-target catalog facts.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   static model-target and target-class catalog facts.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, sidecar writing, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   catalog facts remain report-only pressure objects and do not claim runtime
 *   or generation support.
 *
 * Boundary:
 *   target catalog entries are not capability claims.
 */
#include "yvex_model_target_catalog.h"

#include <string.h>

static const yvex_model_target_class_record catalog_model_target_classes[] = {
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

static const yvex_model_target_record catalog_model_targets[] = {
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

const yvex_model_target_record *yvex_model_target_find(const char *target_id)
{
    unsigned long i;

    if (!target_id) return NULL;
    for (i = 0; i < yvex_model_target_count(); ++i) {
        if (strcmp(catalog_model_targets[i].target_id, target_id) == 0) {
            return &catalog_model_targets[i];
        }
    }
    return NULL;
}

const yvex_model_target_class_record *yvex_model_target_class_find(const char *class_id)
{
    unsigned long i;

    if (!class_id) return NULL;
    for (i = 0; i < yvex_model_target_class_count(); ++i) {
        if (strcmp(catalog_model_target_classes[i].class_id, class_id) == 0) {
            return &catalog_model_target_classes[i];
        }
    }
    return NULL;
}

unsigned long yvex_model_target_count(void)
{
    return sizeof(catalog_model_targets) / sizeof(catalog_model_targets[0]);
}

const yvex_model_target_record *yvex_model_target_at(unsigned long index)
{
    return index < yvex_model_target_count() ? &catalog_model_targets[index] : NULL;
}

unsigned long yvex_model_target_class_count(void)
{
    return sizeof(catalog_model_target_classes) / sizeof(catalog_model_target_classes[0]);
}

const yvex_model_target_class_record *yvex_model_target_class_at(unsigned long index)
{
    return index < yvex_model_target_class_count()
               ? &catalog_model_target_classes[index]
               : NULL;
}
