/*
 * yvex_model.c - Dtypes, tensor metadata, descriptors, and weights.
 *
 * This file owns model-side structures built after GGUF parsing and before
 * runtime execution. It does not execute graph operations.
 */

#include <yvex/dtype.h>
#include <yvex/artifact_integrity.h>
#include <yvex/fs.h>
#include <yvex/model.h>
#include <yvex/model_registry.h>
#include <yvex/tensor.h>
#include <yvex/weights.h>

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Dtype registry */

static const yvex_dtype_info dtype_table[] = {
    {YVEX_DTYPE_UNKNOWN, "UNKNOWN",  UINT_MAX, 0,   0,   0, 0, 0},
    {YVEX_DTYPE_F32,     "F32",      0,        0,   0,   4, 0, 1},
    {YVEX_DTYPE_F16,     "F16",      1,        0,   0,   2, 0, 1},
    {YVEX_DTYPE_BF16,    "BF16",     30,       0,   0,   2, 0, 1},
    {YVEX_DTYPE_F64,     "F64",      28,       0,   0,   8, 0, 1},
    {YVEX_DTYPE_I8,      "I8",       24,       0,   0,   1, 0, 1},
    {YVEX_DTYPE_I16,     "I16",      25,       0,   0,   2, 0, 1},
    {YVEX_DTYPE_I32,     "I32",      26,       0,   0,   4, 0, 1},
    {YVEX_DTYPE_I64,     "I64",      27,       0,   0,   8, 0, 1},
    {YVEX_DTYPE_Q4_0,    "Q4_0",     2,        32,  18,  0, 1, 1},
    {YVEX_DTYPE_Q4_1,    "Q4_1",     3,        32,  20,  0, 1, 0},
    {YVEX_DTYPE_Q5_0,    "Q5_0",     6,        32,  22,  0, 1, 0},
    {YVEX_DTYPE_Q5_1,    "Q5_1",     7,        32,  24,  0, 1, 0},
    {YVEX_DTYPE_Q8_0,    "Q8_0",     8,        32,  34,  0, 1, 1},
    {YVEX_DTYPE_Q8_1,    "Q8_1",     9,        32,  40,  0, 1, 0},
    {YVEX_DTYPE_Q2_K,    "Q2_K",     10,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_Q3_K,    "Q3_K",     11,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_Q4_K,    "Q4_K",     12,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_Q5_K,    "Q5_K",     13,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_Q6_K,    "Q6_K",     14,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_Q8_K,    "Q8_K",     15,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ2_XXS, "IQ2_XXS",  16,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ2_XS,  "IQ2_XS",   17,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ3_XXS, "IQ3_XXS",  18,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ1_S,   "IQ1_S",    19,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ4_NL,  "IQ4_NL",   20,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ3_S,   "IQ3_S",    21,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ2_S,   "IQ2_S",    22,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ4_XS,  "IQ4_XS",   23,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ1_M,   "IQ1_M",    29,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_TQ1_0,   "TQ1_0",    34,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_TQ2_0,   "TQ2_0",    35,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_MXFP4,   "MXFP4",    39,       0,   0,   0, 1, 0},
};

static const unsigned long dtype_table_count = sizeof(dtype_table) / sizeof(dtype_table[0]);

const yvex_dtype_info *yvex_dtype_get_info(yvex_dtype dtype)
{
    unsigned long i;

    for (i = 0; i < dtype_table_count; ++i) {
        if (dtype_table[i].dtype == dtype) {
            return &dtype_table[i];
        }
    }

    return &dtype_table[0];
}

const yvex_dtype_info *yvex_dtype_from_ggml_type(unsigned int ggml_type)
{
    unsigned long i;

    for (i = 0; i < dtype_table_count; ++i) {
        if (dtype_table[i].ggml_type == ggml_type) {
            return &dtype_table[i];
        }
    }

    return &dtype_table[0];
}

const char *yvex_dtype_name(yvex_dtype dtype)
{
    return yvex_dtype_get_info(dtype)->name;
}

int yvex_dtype_storage_bytes(yvex_dtype dtype,
                             unsigned long long element_count,
                             unsigned long long *out,
                             yvex_error *err)
{
    const yvex_dtype_info *info;
    unsigned long long blocks;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_dtype_storage_bytes", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = 0;

    info = yvex_dtype_get_info(dtype);
    if (!info || info->dtype == YVEX_DTYPE_UNKNOWN || !info->is_supported_for_storage_accounting) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_dtype_storage_bytes",
                        "storage accounting unsupported for dtype %s", yvex_dtype_name(dtype));
        return YVEX_ERR_UNSUPPORTED;
    }

    if (info->scalar_bytes > 0) {
        if (element_count > ULLONG_MAX / info->scalar_bytes) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_dtype_storage_bytes",
                            "storage byte overflow for dtype %s", info->name);
            return YVEX_ERR_BOUNDS;
        }
        *out = element_count * info->scalar_bytes;
        yvex_error_clear(err);
        return YVEX_OK;
    }

    if (info->block_elems == 0 || info->block_bytes == 0) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_dtype_storage_bytes",
                        "block formula missing for dtype %s", info->name);
        return YVEX_ERR_UNSUPPORTED;
    }

    blocks = element_count / info->block_elems;
    if ((element_count % info->block_elems) != 0) {
        blocks += 1;
    }
    if (blocks > ULLONG_MAX / info->block_bytes) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_dtype_storage_bytes",
                        "block storage byte overflow for dtype %s", info->name);
        return YVEX_ERR_BOUNDS;
    }

    *out = blocks * info->block_bytes;
    yvex_error_clear(err);
    return YVEX_OK;
}



/* Model descriptors */

struct yvex_model_descriptor {
    yvex_arch arch;
    char *name;
    unsigned long long context_length;
    unsigned long long tensor_count;
    unsigned long long total_storage_bytes;
    unsigned long long unsupported_tensor_accounting_count;
    unsigned long long role_counts[YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN + 1];
};

static char *copy_bytes_string(const char *data, unsigned long long len)
{
    char *copy;

    if (!data) {
        return NULL;
    }
    if (len > (unsigned long long)(SIZE_MAX - 1)) {
        return NULL;
    }
    copy = (char *)malloc((size_t)len + 1u);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, data, (size_t)len);
    copy[len] = '\0';
    return copy;
}

static yvex_arch arch_from_name(const char *name)
{
    if (!name) {
        return YVEX_ARCH_UNKNOWN;
    }
    if (strcmp(name, "llama") == 0) return YVEX_ARCH_LLAMA;
    if (strcmp(name, "qwen") == 0) return YVEX_ARCH_QWEN;
    if (strcmp(name, "deepseek") == 0) return YVEX_ARCH_DEEPSEEK;
    if (strcmp(name, "gemma") == 0) return YVEX_ARCH_GEMMA;
    if (strcmp(name, "phi") == 0) return YVEX_ARCH_PHI;
    if (strcmp(name, "kimi") == 0) return YVEX_ARCH_KIMI;
    if (strcmp(name, "glm") == 0) return YVEX_ARCH_GLM;
    return YVEX_ARCH_UNKNOWN;
}

int yvex_model_descriptor_from_gguf(yvex_model_descriptor **out,
                                    const yvex_gguf *gguf,
                                    const yvex_tensor_table *tensors,
                                    yvex_error *err)
{
    yvex_model_descriptor *model;
    const yvex_gguf_value *value;
    const char *text;
    unsigned long long len;
    unsigned long long i;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_model_descriptor_from_gguf", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!gguf || !tensors) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_model_descriptor_from_gguf", "gguf and tensors are required");
        return YVEX_ERR_INVALID_ARG;
    }

    model = (yvex_model_descriptor *)calloc(1, sizeof(*model));
    if (!model) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_model_descriptor_from_gguf", "failed to allocate model descriptor");
        return YVEX_ERR_NOMEM;
    }

    value = yvex_gguf_metadata_find(gguf, "general.architecture");
    if (value && yvex_gguf_value_as_string(value, &text, &len) == YVEX_OK) {
        char *arch_text = copy_bytes_string(text, len);
        model->arch = arch_from_name(arch_text);
        free(arch_text);
    }

    value = yvex_gguf_metadata_find(gguf, "general.name");
    if (value && yvex_gguf_value_as_string(value, &text, &len) == YVEX_OK) {
        model->name = copy_bytes_string(text, len);
        if (!model->name) {
            yvex_model_descriptor_close(model);
            yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_model_descriptor_from_gguf", "failed to copy model name");
            return YVEX_ERR_NOMEM;
        }
    }

    value = yvex_gguf_metadata_find(gguf, "llama.context_length");
    if (value) {
        (void)yvex_gguf_value_as_u64(value, &model->context_length);
    }

    model->tensor_count = yvex_tensor_table_count(tensors);
    for (i = 0; i < model->tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, i);
        if (!tensor) {
            continue;
        }
        if (tensor->storage_bytes == 0) {
            model->unsupported_tensor_accounting_count += 1;
        } else {
            model->total_storage_bytes += tensor->storage_bytes;
        }
        if (tensor->role <= YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN) {
            model->role_counts[tensor->role] += 1;
        }
    }

    *out = model;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_model_descriptor_close(yvex_model_descriptor *model)
{
    if (!model) {
        return;
    }
    free(model->name);
    free(model);
}

yvex_arch yvex_model_arch(const yvex_model_descriptor *model)
{
    return model ? model->arch : YVEX_ARCH_UNKNOWN;
}

const char *yvex_arch_name(yvex_arch arch)
{
    switch (arch) {
    case YVEX_ARCH_UNKNOWN: return "unknown";
    case YVEX_ARCH_LLAMA: return "llama";
    case YVEX_ARCH_QWEN: return "qwen";
    case YVEX_ARCH_DEEPSEEK: return "deepseek";
    case YVEX_ARCH_GEMMA: return "gemma";
    case YVEX_ARCH_PHI: return "phi";
    case YVEX_ARCH_KIMI: return "kimi";
    case YVEX_ARCH_GLM: return "glm";
    }
    return "unknown";
}

const char *yvex_model_name(const yvex_model_descriptor *model)
{
    if (!model || !model->name) {
        return "";
    }
    return model->name;
}

unsigned long long yvex_model_context_length(const yvex_model_descriptor *model)
{
    return model ? model->context_length : 0;
}

unsigned long long yvex_model_tensor_count(const yvex_model_descriptor *model)
{
    return model ? model->tensor_count : 0;
}

unsigned long long yvex_model_total_storage_bytes(const yvex_model_descriptor *model)
{
    return model ? model->total_storage_bytes : 0;
}

unsigned long long yvex_model_unsupported_tensor_accounting_count(const yvex_model_descriptor *model)
{
    return model ? model->unsupported_tensor_accounting_count : 0;
}

unsigned long long yvex_model_role_count(const yvex_model_descriptor *model, yvex_tensor_role role)
{
    if (!model || role > YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN) {
        return 0;
    }
    return model->role_counts[role];
}



/* Tensor roles */

const char *yvex_tensor_role_name(yvex_tensor_role role)
{
    switch (role) {
    case YVEX_TENSOR_ROLE_UNKNOWN: return "unknown";
    case YVEX_TENSOR_ROLE_TOKEN_EMBEDDING: return "token_embedding";
    case YVEX_TENSOR_ROLE_OUTPUT_NORM: return "output_norm";
    case YVEX_TENSOR_ROLE_OUTPUT_HEAD: return "output_head";
    case YVEX_TENSOR_ROLE_ATTENTION_NORM: return "attention_norm";
    case YVEX_TENSOR_ROLE_ATTENTION_Q: return "attention_q";
    case YVEX_TENSOR_ROLE_ATTENTION_K: return "attention_k";
    case YVEX_TENSOR_ROLE_ATTENTION_V: return "attention_v";
    case YVEX_TENSOR_ROLE_ATTENTION_OUT: return "attention_out";
    case YVEX_TENSOR_ROLE_FFN_NORM: return "ffn_norm";
    case YVEX_TENSOR_ROLE_FFN_GATE: return "ffn_gate";
    case YVEX_TENSOR_ROLE_FFN_UP: return "ffn_up";
    case YVEX_TENSOR_ROLE_FFN_DOWN: return "ffn_down";
    case YVEX_TENSOR_ROLE_MOE_ROUTER: return "moe_router";
    case YVEX_TENSOR_ROLE_MOE_EXPERT_GATE: return "moe_expert_gate";
    case YVEX_TENSOR_ROLE_MOE_EXPERT_UP: return "moe_expert_up";
    case YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN: return "moe_expert_down";
    }
    return "unknown";
}

static int ends_with(const char *text, const char *suffix)
{
    size_t text_len;
    size_t suffix_len;

    if (!text || !suffix) {
        return 0;
    }

    text_len = strlen(text);
    suffix_len = strlen(suffix);
    if (suffix_len > text_len) {
        return 0;
    }

    return strcmp(text + text_len - suffix_len, suffix) == 0;
}

static int contains(const char *text, const char *needle)
{
    return text && needle && strstr(text, needle) != NULL;
}

yvex_tensor_role yvex_tensor_role_classify(const char *architecture,
                                           const char *tensor_name,
                                           unsigned int rank,
                                           const unsigned long long *dims,
                                           yvex_dtype dtype)
{
    (void)architecture;
    (void)rank;
    (void)dims;
    (void)dtype;

    if (!tensor_name) {
        return YVEX_TENSOR_ROLE_UNKNOWN;
    }

    if (strcmp(tensor_name, "token_embd.weight") == 0 ||
        strcmp(tensor_name, "model.embed_tokens.weight") == 0 ||
        strcmp(tensor_name, "tok_embeddings.weight") == 0) {
        return YVEX_TENSOR_ROLE_TOKEN_EMBEDDING;
    }
    if (strcmp(tensor_name, "output_norm.weight") == 0 ||
        strcmp(tensor_name, "model.norm.weight") == 0 ||
        strcmp(tensor_name, "norm.weight") == 0) {
        return YVEX_TENSOR_ROLE_OUTPUT_NORM;
    }
    if (strcmp(tensor_name, "output.weight") == 0 ||
        strcmp(tensor_name, "lm_head.weight") == 0) {
        return YVEX_TENSOR_ROLE_OUTPUT_HEAD;
    }
    if (ends_with(tensor_name, ".attn_norm.weight") ||
        ends_with(tensor_name, ".input_layernorm.weight")) {
        return YVEX_TENSOR_ROLE_ATTENTION_NORM;
    }
    if (ends_with(tensor_name, ".attn_q.weight") ||
        ends_with(tensor_name, ".self_attn.q_proj.weight")) {
        return YVEX_TENSOR_ROLE_ATTENTION_Q;
    }
    if (ends_with(tensor_name, ".attn_k.weight") ||
        ends_with(tensor_name, ".self_attn.k_proj.weight")) {
        return YVEX_TENSOR_ROLE_ATTENTION_K;
    }
    if (ends_with(tensor_name, ".attn_v.weight") ||
        ends_with(tensor_name, ".self_attn.v_proj.weight")) {
        return YVEX_TENSOR_ROLE_ATTENTION_V;
    }
    if (ends_with(tensor_name, ".attn_output.weight") ||
        ends_with(tensor_name, ".self_attn.o_proj.weight")) {
        return YVEX_TENSOR_ROLE_ATTENTION_OUT;
    }
    if (ends_with(tensor_name, ".ffn_norm.weight") ||
        ends_with(tensor_name, ".post_attention_layernorm.weight")) {
        return YVEX_TENSOR_ROLE_FFN_NORM;
    }
    if (ends_with(tensor_name, ".ffn_gate.weight") ||
        ends_with(tensor_name, ".mlp.gate_proj.weight")) {
        return YVEX_TENSOR_ROLE_FFN_GATE;
    }
    if (ends_with(tensor_name, ".ffn_up.weight") ||
        ends_with(tensor_name, ".mlp.up_proj.weight")) {
        return YVEX_TENSOR_ROLE_FFN_UP;
    }
    if (ends_with(tensor_name, ".ffn_down.weight") ||
        ends_with(tensor_name, ".mlp.down_proj.weight")) {
        return YVEX_TENSOR_ROLE_FFN_DOWN;
    }
    if (ends_with(tensor_name, ".ffn_gate_inp.weight") ||
        ends_with(tensor_name, ".mlp.gate.weight")) {
        return YVEX_TENSOR_ROLE_MOE_ROUTER;
    }
    if (contains(tensor_name, ".ffn.experts.") && ends_with(tensor_name, ".gate.weight")) {
        return YVEX_TENSOR_ROLE_MOE_EXPERT_GATE;
    }
    if (contains(tensor_name, ".ffn.experts.") && ends_with(tensor_name, ".up.weight")) {
        return YVEX_TENSOR_ROLE_MOE_EXPERT_UP;
    }
    if (contains(tensor_name, ".ffn.experts.") && ends_with(tensor_name, ".down.weight")) {
        return YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN;
    }

    return YVEX_TENSOR_ROLE_UNKNOWN;
}



/* Tensor table */

struct yvex_tensor_table {
    yvex_tensor_info *items;
    unsigned long long count;
};

static char *copy_string(const char *text)
{
    size_t len;
    char *copy;

    if (!text) {
        text = "";
    }
    len = strlen(text);
    copy = (char *)malloc(len + 1u);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len + 1u);
    return copy;
}

static int product_dims(const yvex_gguf_tensor_info *src, unsigned long long *out, yvex_error *err)
{
    unsigned int i;
    unsigned long long product = 1;

    for (i = 0; i < src->rank; ++i) {
        if (src->dims[i] == 0) {
            yvex_error_setf(err, YVEX_ERR_FORMAT, "yvex_tensor_table_from_gguf",
                            "tensor %s has zero dimension", src->name);
            return YVEX_ERR_FORMAT;
        }
        if (product > ULLONG_MAX / src->dims[i]) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_tensor_table_from_gguf",
                            "element count overflow for tensor %s", src->name);
            return YVEX_ERR_BOUNDS;
        }
        product *= src->dims[i];
    }

    *out = product;
    return YVEX_OK;
}

int yvex_tensor_table_from_gguf(yvex_tensor_table **out,
                                const yvex_gguf *gguf,
                                yvex_error *err)
{
    yvex_tensor_table *table;
    unsigned long long count;
    unsigned long long i;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_tensor_table_from_gguf", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!gguf) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_tensor_table_from_gguf", "gguf is required");
        return YVEX_ERR_INVALID_ARG;
    }

    count = yvex_gguf_tensor_count(gguf);
    table = (yvex_tensor_table *)calloc(1, sizeof(*table));
    if (!table) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_tensor_table_from_gguf", "failed to allocate tensor table");
        return YVEX_ERR_NOMEM;
    }
    table->count = count;

    if (count > 0) {
        if (count > (unsigned long long)(SIZE_MAX / sizeof(*table->items))) {
            yvex_tensor_table_close(table);
            yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_tensor_table_from_gguf", "tensor count too large");
            return YVEX_ERR_NOMEM;
        }
        table->items = (yvex_tensor_info *)calloc((size_t)count, sizeof(*table->items));
        if (!table->items) {
            yvex_tensor_table_close(table);
            yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_tensor_table_from_gguf", "failed to allocate tensor rows");
            return YVEX_ERR_NOMEM;
        }
    }

    for (i = 0; i < count; ++i) {
        const yvex_gguf_tensor_info *src = yvex_gguf_tensor_at(gguf, i);
        yvex_tensor_info *dst = &table->items[i];
        const yvex_dtype_info *dtype_info;
        int rc;

        dst->name = copy_string(src->name);
        if (!dst->name) {
            yvex_tensor_table_close(table);
            yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_tensor_table_from_gguf", "failed to copy tensor name");
            return YVEX_ERR_NOMEM;
        }
        dst->rank = src->rank;
        memcpy(dst->dims, src->dims, sizeof(dst->dims));
        dst->ggml_type = src->ggml_type;
        dst->relative_offset = src->relative_offset;
        dst->absolute_offset = src->absolute_offset;

        dtype_info = yvex_dtype_from_ggml_type(src->ggml_type);
        dst->dtype = dtype_info->dtype;

        rc = product_dims(src, &dst->element_count, err);
        if (rc != YVEX_OK) {
            yvex_tensor_table_close(table);
            return rc;
        }

        rc = yvex_dtype_storage_bytes(dst->dtype, dst->element_count, &dst->storage_bytes, err);
        if (rc == YVEX_ERR_UNSUPPORTED) {
            dst->storage_bytes = 0;
            yvex_error_clear(err);
        } else if (rc != YVEX_OK) {
            yvex_tensor_table_close(table);
            return rc;
        }

        dst->role = yvex_tensor_role_classify(NULL, dst->name, dst->rank, dst->dims, dst->dtype);
    }

    *out = table;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_tensor_table_close(yvex_tensor_table *table)
{
    unsigned long long i;

    if (!table) {
        return;
    }
    if (table->items) {
        for (i = 0; i < table->count; ++i) {
            free((char *)table->items[i].name);
            table->items[i].name = NULL;
        }
        free(table->items);
    }
    free(table);
}

unsigned long long yvex_tensor_table_count(const yvex_tensor_table *table)
{
    return table ? table->count : 0;
}

const yvex_tensor_info *yvex_tensor_table_at(const yvex_tensor_table *table,
                                             unsigned long long index)
{
    if (!table || index >= table->count) {
        return NULL;
    }
    return &table->items[index];
}

const yvex_tensor_info *yvex_tensor_table_find(const yvex_tensor_table *table,
                                               const char *name)
{
    unsigned long long i;

    if (!table || !name) {
        return NULL;
    }

    for (i = 0; i < table->count; ++i) {
        if (table->items[i].name && strcmp(table->items[i].name, name) == 0) {
            return &table->items[i];
        }
    }

    return NULL;
}



/* Materialized weights */

struct yvex_materialized_weight {
    char *name;
    yvex_dtype dtype;
    yvex_tensor_role role;
    unsigned long long bytes;
    yvex_weight_residency residency;
    yvex_device_tensor *device_tensor;
};

struct yvex_weight_table {
    yvex_backend *backend;
    char *backend_name;
    yvex_materialized_weight *items;
    unsigned long long count;
    yvex_materialize_summary summary;
};

static int yvex_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static char *yvex_weight_strdup(const char *text)
{
    size_t len;
    char *copy;

    if (!text) {
        text = "";
    }
    len = strlen(text);
    copy = (char *)malloc(len + 1u);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len + 1u);
    return copy;
}

static void yvex_materialized_weight_clear(yvex_weight_table *table,
                                           yvex_materialized_weight *weight)
{
    if (!weight) {
        return;
    }
    if (table && table->backend && weight->device_tensor) {
        yvex_backend_tensor_free(table->backend, weight->device_tensor);
    }
    weight->device_tensor = NULL;
    free(weight->name);
    weight->name = NULL;
}

void yvex_weight_table_close(yvex_weight_table *weights)
{
    unsigned long long i;

    if (!weights) {
        return;
    }
    for (i = 0; i < weights->count; ++i) {
        yvex_materialized_weight_clear(weights, &weights->items[i]);
    }
    free(weights->items);
    free(weights->backend_name);
    free(weights);
}

unsigned long long yvex_weight_table_count(const yvex_weight_table *weights)
{
    return weights ? weights->count : 0;
}

const yvex_materialized_weight *yvex_weight_table_at(const yvex_weight_table *weights,
                                                     unsigned long long index)
{
    if (!weights || index >= weights->count) {
        return NULL;
    }
    return &weights->items[index];
}

const yvex_materialized_weight *yvex_weight_table_find(const yvex_weight_table *weights,
                                                       const char *name)
{
    unsigned long long i;

    if (!weights || !name) {
        return NULL;
    }
    for (i = 0; i < weights->count; ++i) {
        if (weights->items[i].name && strcmp(weights->items[i].name, name) == 0) {
            return &weights->items[i];
        }
    }
    return NULL;
}

const char *yvex_weight_status_name(yvex_weight_status status)
{
    switch (status) {
    case YVEX_WEIGHT_STATUS_EMPTY: return "empty";
    case YVEX_WEIGHT_STATUS_MATERIALIZED: return "materialized";
    case YVEX_WEIGHT_STATUS_PARTIAL: return "partial";
    case YVEX_WEIGHT_STATUS_FAILED: return "failed";
    }
    return "unknown";
}

const char *yvex_weight_residency_name(yvex_weight_residency residency)
{
    switch (residency) {
    case YVEX_WEIGHT_RESIDENCY_HOST: return "host";
    case YVEX_WEIGHT_RESIDENCY_CPU_BACKEND: return "cpu_backend";
    case YVEX_WEIGHT_RESIDENCY_CUDA_BACKEND: return "cuda_backend";
    }
    return "unknown";
}

const char *yvex_weight_name(const yvex_materialized_weight *weight)
{
    return weight && weight->name ? weight->name : "";
}

yvex_dtype yvex_weight_dtype(const yvex_materialized_weight *weight)
{
    return weight ? weight->dtype : YVEX_DTYPE_UNKNOWN;
}

yvex_tensor_role yvex_weight_role(const yvex_materialized_weight *weight)
{
    return weight ? weight->role : YVEX_TENSOR_ROLE_UNKNOWN;
}

unsigned long long yvex_weight_bytes(const yvex_materialized_weight *weight)
{
    return weight ? weight->bytes : 0;
}

yvex_weight_residency yvex_weight_residency_of(const yvex_materialized_weight *weight)
{
    return weight ? weight->residency : YVEX_WEIGHT_RESIDENCY_HOST;
}

const yvex_device_tensor *yvex_weight_device_tensor(const yvex_materialized_weight *weight)
{
    return weight ? weight->device_tensor : NULL;
}



static yvex_weight_residency residency_from_backend(const yvex_backend *backend)
{
    if (yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CUDA) {
        return YVEX_WEIGHT_RESIDENCY_CUDA_BACKEND;
    }
    if (yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CPU) {
        return YVEX_WEIGHT_RESIDENCY_CPU_BACKEND;
    }
    return YVEX_WEIGHT_RESIDENCY_HOST;
}

static int copy_tensor_dims(yvex_backend_tensor_desc *desc, const yvex_tensor_info *tensor)
{
    unsigned int i;

    if (!desc || !tensor || tensor->rank > YVEX_TENSOR_MAX_DIMS) {
        return 0;
    }
    for (i = 0; i < tensor->rank; ++i) {
        desc->dims[i] = tensor->dims[i];
    }
    return 1;
}

static int materialize_one(yvex_weight_table *table,
                           const yvex_artifact *artifact,
                           const yvex_gguf *gguf,
                           const yvex_tensor_info *tensor,
                           yvex_error *err)
{
    yvex_backend_tensor_desc desc;
    yvex_device_tensor *device_tensor = NULL;
    yvex_materialized_weight *weight;
    yvex_tensor_range range;
    const unsigned char *data;
    int rc;

    if (!table || !artifact || !gguf || !tensor) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_weight_table_materialize",
                       "table, artifact, gguf, and tensor are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(&range, 0, sizeof(range));
    rc = yvex_tensor_range_validate(artifact, gguf, tensor, &range, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    memset(&desc, 0, sizeof(desc));
    desc.name = tensor->name;
    desc.dtype = tensor->dtype;
    desc.rank = tensor->rank;
    desc.bytes = range.tensor_bytes;
    if (!copy_tensor_dims(&desc, tensor)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_weight_table_materialize",
                       "invalid tensor rank");
        return YVEX_ERR_INVALID_ARG;
    }

    table->summary.materialization_phase = "allocation";
    rc = yvex_backend_tensor_alloc(table->backend, &desc, &device_tensor, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    table->summary.allocation_attempted = 1;
    table->summary.bytes_allocated += range.tensor_bytes;
    if (yvex_test_env_enabled("YVEX_TEST_FAIL_MATERIALIZE_AFTER_ALLOC")) {
        yvex_backend_tensor_free(table->backend, device_tensor);
        table->summary.materialization_gate = "fail";
        table->summary.materialization_phase = "allocation";
        table->summary.cleanup_attempted = 1;
        table->summary.cleanup_status = "pass";
        table->summary.status = YVEX_WEIGHT_STATUS_FAILED;
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_weight_table_materialize",
                       "test materialization failure after allocation");
        return YVEX_ERR_BACKEND;
    }

    data = yvex_artifact_data(artifact);
    if (!data) {
        yvex_backend_tensor_free(table->backend, device_tensor);
        table->summary.cleanup_attempted = 1;
        table->summary.cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_weight_table_materialize",
                       "artifact data is unavailable");
        return YVEX_ERR_INVALID_ARG;
    }

    table->summary.materialization_phase = "transfer";
    table->summary.transfer_attempted = 1;
    rc = yvex_backend_tensor_write(table->backend,
                                   device_tensor,
                                   data + range.tensor_absolute_offset,
                                   range.tensor_bytes,
                                   err);
    if (rc != YVEX_OK) {
        yvex_backend_tensor_free(table->backend, device_tensor);
        table->summary.cleanup_attempted = 1;
        table->summary.cleanup_status = "pass";
        return rc;
    }
    table->summary.bytes_transferred += range.tensor_bytes;
    if (yvex_test_env_enabled("YVEX_TEST_FAIL_MATERIALIZE_AFTER_TRANSFER")) {
        yvex_backend_tensor_free(table->backend, device_tensor);
        table->summary.materialization_gate = "fail";
        table->summary.materialization_phase = "transfer";
        table->summary.cleanup_attempted = 1;
        table->summary.cleanup_status = "pass";
        table->summary.status = YVEX_WEIGHT_STATUS_FAILED;
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_weight_table_materialize",
                       "test materialization write failure after transfer");
        return YVEX_ERR_BACKEND;
    }

    weight = &table->items[table->count];
    weight->name = yvex_weight_strdup(tensor->name);
    if (!weight->name) {
        yvex_backend_tensor_free(table->backend, device_tensor);
        table->summary.cleanup_attempted = 1;
        table->summary.cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_weight_table_materialize",
                       "failed to copy weight name");
        return YVEX_ERR_NOMEM;
    }
    weight->dtype = tensor->dtype;
    weight->role = tensor->role;
    weight->bytes = range.tensor_bytes;
    weight->residency = residency_from_backend(table->backend);
    weight->device_tensor = device_tensor;
    table->count += 1;
    return YVEX_OK;
}

int yvex_weight_table_materialize(yvex_weight_table **out,
                                  const yvex_artifact *artifact,
                                  const yvex_gguf *gguf,
                                  const yvex_tensor_table *tensors,
                                  yvex_backend *backend,
                                  const yvex_materialize_options *options,
                                  yvex_error *err)
{
    yvex_weight_table *table;
    yvex_backend_memory_stats stats;
    unsigned long long tensor_count;
    unsigned long long i;
    int require_all = 0;
    int allow_unsupported = 0;
    int rc = YVEX_OK;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_weight_table_materialize", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!artifact || !gguf || !tensors || !backend) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_weight_table_materialize",
                       "artifact, gguf, tensors and backend are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (options) {
        require_all = options->require_all_tensors;
        allow_unsupported = options->allow_unsupported_dtype;
    }

    tensor_count = yvex_tensor_table_count(tensors);
    table = (yvex_weight_table *)calloc(1, sizeof(*table));
    if (!table) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_weight_table_materialize",
                       "failed to allocate weight table");
        return YVEX_ERR_NOMEM;
    }
    table->backend = backend;
    table->backend_name = yvex_weight_strdup(options && options->backend_name
                                             ? options->backend_name
                                             : yvex_backend_kind_name(yvex_backend_kind_of(backend)));
    table->items = (yvex_materialized_weight *)calloc((size_t)(tensor_count ? tensor_count : 1),
                                                      sizeof(*table->items));
    if (!table->backend_name || !table->items) {
        yvex_weight_table_close(table);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_weight_table_materialize",
                       "failed to allocate materialized weight rows");
        return YVEX_ERR_NOMEM;
    }

    table->summary.backend_name = table->backend_name;
    table->summary.materialization_gate = "fail";
    table->summary.materialization_phase = "preflight";
    table->summary.shape_status = "unchecked";
    table->summary.range_status = "unchecked";
    table->summary.backend_status = "ready";
    table->summary.cleanup_status = "not-needed";
    table->summary.tensors_total = tensor_count;
    table->summary.execution_ready = 0;

    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, i);
        yvex_tensor_range range;

        if (!tensor) {
            if (require_all) {
                yvex_weight_table_close(table);
                yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_weight_table_materialize",
                               "missing tensor table row");
                return YVEX_ERR_INVALID_ARG;
            }
            continue;
        }
        if (tensor->storage_bytes == 0) {
            if (require_all && !allow_unsupported) {
                yvex_weight_table_close(table);
                yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_weight_table_materialize",
                                "tensor %s has unsupported storage accounting", tensor->name);
                return YVEX_ERR_UNSUPPORTED;
            }
            continue;
        }
        memset(&range, 0, sizeof(range));
        rc = yvex_tensor_range_validate(artifact, gguf, tensor, &range, err);
        if (rc != YVEX_OK) {
            yvex_weight_table_close(table);
            return rc;
        }
        table->summary.bytes_planned += range.tensor_bytes;
    }
    table->summary.shape_status = "pass";
    table->summary.range_status = "pass";

    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, i);
        if (!tensor) {
            table->summary.tensors_failed += 1;
            if (require_all) {
                rc = YVEX_ERR_INVALID_ARG;
                yvex_error_set(err, rc, "yvex_weight_table_materialize",
                               "missing tensor table row");
                break;
            }
            continue;
        }

        if (tensor->storage_bytes > 0) {
            table->summary.bytes_total += tensor->storage_bytes;
        } else {
            table->summary.tensors_failed += 1;
            if (require_all && !allow_unsupported) {
                rc = YVEX_ERR_UNSUPPORTED;
                yvex_error_setf(err, rc, "yvex_weight_table_materialize",
                                "tensor %s has unsupported storage accounting", tensor->name);
                break;
            }
            continue;
        }

        rc = materialize_one(table, artifact, gguf, tensor, err);
        if (rc != YVEX_OK) {
            table->summary.tensors_failed += 1;
            if (require_all || rc == YVEX_ERR_BOUNDS || rc == YVEX_ERR_FORMAT ||
                rc == YVEX_ERR_BACKEND || rc == YVEX_ERR_NOMEM) {
                break;
            }
            rc = YVEX_OK;
            continue;
        }
        table->summary.tensors_materialized += 1;
        table->summary.bytes_materialized += tensor->storage_bytes;
    }

    if (rc != YVEX_OK) {
        yvex_weight_table_close(table);
        return rc;
    }

    if (table->summary.tensors_materialized == tensor_count &&
        table->summary.tensors_failed == 0) {
        table->summary.status = tensor_count == 0
            ? YVEX_WEIGHT_STATUS_EMPTY
            : YVEX_WEIGHT_STATUS_MATERIALIZED;
    } else {
        table->summary.status = YVEX_WEIGHT_STATUS_PARTIAL;
    }
    table->summary.materialization_gate =
        table->summary.status == YVEX_WEIGHT_STATUS_MATERIALIZED ? "pass" : "fail";
    table->summary.materialization_phase = "complete";
    table->summary.cleanup_status = "not-needed";

    if (yvex_backend_get_memory_stats(backend, &stats, err) == YVEX_OK) {
        table->summary.backend_allocated_bytes = stats.allocated_bytes;
    }

    *out = table;
    yvex_error_clear(err);
    return YVEX_OK;
}



int yvex_weight_table_get_summary(const yvex_weight_table *weights,
                                  yvex_materialize_summary *out,
                                  yvex_error *err)
{
    yvex_backend_memory_stats stats;

    if (!weights || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_weight_table_get_summary",
                       "weights and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memcpy(out, &weights->summary, sizeof(*out));
    if (weights->backend &&
        yvex_backend_get_memory_stats(weights->backend, &stats, err) == YVEX_OK) {
        out->backend_allocated_bytes = stats.allocated_bytes;
    }
    return YVEX_OK;
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
        "YVEX-produced selected GGUF",
        "YVEX-produced selected GGUF",
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
        "YVEX-produced selected GGUF",
        "YVEX-produced selected GGUF",
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
        "official safetensors",
        "future YVEX-produced GGUF",
        "huge-source-tensor-intake-moe-storage-stream-planning",
        "none",
        "hf/glm/GLM-5.2",
        "282 safetensors,1.5T-class",
        "source evidence only",
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
        "qwen-metal-portability-pressure",
        "metal-reduced-full-runtime-pressure",
        "report-only",
        "planned-portability-only",
        "planned",
        "planned",
        "missing-tensor-map",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        "HARDWARE.PROFILE.MAC.0,COMPUTE.BACKEND.METAL.0,OWI.TARGETS.QWEN.0",
        {
            "planned-portability-only",
            "missing-source-inventory",
            "missing-tensor-map",
            "missing-full-artifact",
            "missing-integrity-gate",
            "missing-real-prefill",
            "missing-real-decode",
            "missing-real-logits",
            "missing-real-sampling",
        },
        9,
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
        "qwen-metal-portability-pressure",
        "Qwen",
        "metal-reduced-full-runtime-pressure",
        "report-only",
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
        "V010.TARGET.7,V010.SOURCE.9,OWI.TARGETS.QWEN.0,COMPUTE.BACKEND.METAL.0",
        {
            "planned-portability-only",
            "missing-dense-source",
            "missing-dense-artifact",
            "missing-source-manifest",
            "missing-native-inventory",
            "missing-tensor-map",
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
        22,
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
    "missing-qwen-source-target",
    "missing-qwen-source-manifest",
    "missing-qwen-native-inventory",
    "missing-qwen-source-config",
    "missing-qwen-model-class-profile",
    "missing-qwen-tensor-map",
    "missing-qwen-tokenizer-map",
    "missing-qwen-output-head-map",
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
        "metal-reduced-full-runtime-pressure",
        "report-only",
        "pressure-target-only",
        "missing",
        "missing",
        "missing",
        "unsupported",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        {
            "missing-qwen-source-target",
            "missing-qwen-source-manifest",
            "missing-qwen-native-inventory",
            "missing-qwen-model-class-profile",
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
        "metal-reduced-full-runtime-pressure",
        "report-only",
        "pressure-target-only",
        "missing",
        "missing",
        "missing",
        "unsupported",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        {
            "missing-qwen-source-target",
            "missing-qwen-source-manifest",
            "missing-qwen-native-inventory",
            "missing-qwen-model-class-profile",
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
        "qwen-metal-portability",
        "metal-reduced-full-runtime-pressure",
        "report-only",
        "pressure-target-only",
        "planned",
        "planned",
        "missing",
        "unsupported",
        "unsupported",
        "unsupported-full-model",
        "not-measured",
        {
            "missing-qwen-source-target",
            "missing-qwen-source-config",
            "missing-qwen-model-class-profile",
            "missing-qwen-tokenizer-map",
            "missing-qwen-output-head-map",
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
    fprintf(fp, "usage: yvex model-target decision --release v0.1.0 [options]\n");
    fprintf(fp, "       yvex model-target decision --help\n");
    fprintf(fp, "\noptions:\n");
    fprintf(fp, "  --candidate TARGET             report decision facts for one target\n");
    fprintf(fp, "  --include-candidates           include target candidate classifications\n");
    fprintf(fp, "  --include-pressure-targets     include pressure-lane status fields\n");
    fprintf(fp, "  --include-blockers             include blocker row fields\n");
    fprintf(fp, "  --include-critical-path        include release-critical track fields\n");
    fprintf(fp, "  --include-next                 include deterministic next row fields\n");
    fprintf(fp, "  --strict                       keep invalid usage fatal; honest blocked decisions still pass\n");
}

static void print_model_target_decision_help(FILE *fp)
{
    print_model_target_decision_usage(fp);
    fprintf(fp, "\nThis command records the v0.1.0 target decision. It does not download models, emit artifacts, materialize tensors, execute graph work, run prefill, decode, logits, sampling, generation, evaluation, or benchmarks.\n");
    fprintf(fp, "v0.1.0 requires an honest full-runtime-candidate target before runtime graph, prefill, KV, decode, logits, sampling, and generation rows can advance.\n");
    fprintf(fp, "Selected runtime slices, source-only pressure targets, external references, and fixture-only targets are ineligible for full-runtime closure.\n");
}

static void print_model_target_candidate_usage(FILE *fp)
{
    fprintf(fp, "usage: yvex model-target candidate --release v0.1.0 [options]\n");
    fprintf(fp, "       yvex model-target candidate --help\n");
    fprintf(fp, "\noptions:\n");
    fprintf(fp, "  --target TARGET                report one candidate target\n");
    fprintf(fp, "  --include-candidates           include candidate classification blocks\n");
    fprintf(fp, "  --include-pressure-targets     include pressure target count fields\n");
    fprintf(fp, "  --include-blockers             include stable blocker fields\n");
    fprintf(fp, "  --include-next                 include next required row fields\n");
}

static void print_model_target_candidate_help(FILE *fp)
{
    print_model_target_candidate_usage(fp);
    fprintf(fp, "\nThe candidate report evaluates full-runtime target eligibility for a release. It does not select a ready model, download weights, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
}

static void print_model_target_dense_candidate_usage(FILE *fp)
{
    fprintf(fp, "usage: yvex model-target dense-candidate --release v0.1.0 [options]\n");
    fprintf(fp, "       yvex model-target dense-candidate --help\n");
    fprintf(fp, "\noptions:\n");
    fprintf(fp, "  --target TARGET                report one dense or dense-adjacent target\n");
    fprintf(fp, "  --include-candidates           include dense candidate classification blocks\n");
    fprintf(fp, "  --include-requirements         include required dense runtime role groups\n");
    fprintf(fp, "  --include-blockers             include stable blocker fields\n");
    fprintf(fp, "  --include-next                 include next required row fields\n");
}

static void print_model_target_dense_candidate_help(FILE *fp)
{
    print_model_target_dense_candidate_usage(fp);
    fprintf(fp, "\nThe dense-candidate report evaluates whether a dense model target can become the first v0.1.0 full-runtime candidate. It does not download weights, emit artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
}

static void print_model_target_qwen_metal_usage(FILE *fp)
{
    fprintf(fp, "usage: yvex model-target qwen-metal --release v0.1.0 [options]\n");
    fprintf(fp, "       yvex model-target qwen-metal --help\n");
    fprintf(fp, "\noptions:\n");
    fprintf(fp, "  --target TARGET                report one planned Qwen/Metal candidate slot\n");
    fprintf(fp, "  --include-candidates           include planned candidate slot blocks\n");
    fprintf(fp, "  --include-hardware             include Apple Silicon / Metal hardware pressure fields\n");
    fprintf(fp, "  --include-backend              include Metal backend pressure fields\n");
    fprintf(fp, "  --include-source               include Qwen source/config pressure fields\n");
    fprintf(fp, "  --include-blockers             include stable blocker fields\n");
    fprintf(fp, "  --include-next                 include next required row fields\n");
}

static void print_model_target_qwen_metal_help(FILE *fp)
{
    print_model_target_qwen_metal_usage(fp);
    fprintf(fp, "\nThe Qwen/Metal pressure report records a planned reduced-scale Apple Silicon / Metal lane for future full-runtime work. It does not download weights, implement Metal, emit Qwen artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
}

static int target_decision_is_full_runtime_candidate(const yvex_model_target_record *record)
{
    if (!record || !record->target_class) return 0;
    if (strcmp(record->target_class, "full-runtime-model") == 0) return 1;
    if (strcmp(record->target_class, "full-runtime-candidate") == 0) return 1;
    return 0;
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
    printf("candidate.%lu.id: %s\n", index, record->target_id);
    printf("candidate.%lu.class: %s\n", index, target_decision_candidate_class(record));
    printf("candidate.%lu.family: %s\n", index, record->family);
    printf("candidate.%lu.model: %s\n", index, record->model);
    printf("candidate.%lu.source_class: %s\n", index, record->source_artifact_class);
    printf("candidate.%lu.artifact_class: %s\n", index, record->target_artifact_class);
    printf("candidate.%lu.status: %s\n", index, status);
    printf("candidate.%lu.eligible_for_v010: %s\n", index,
           strcmp(status, "eligible") == 0 ? "true" : "false");
    printf("candidate.%lu.reason: %s\n", index, target_decision_candidate_reason(record));
    printf("candidate.%lu.next: %s\n", index, target_decision_candidate_next(record));
}

static void print_target_decision_constant_tail(void)
{
    printf("release_critical_tracks: TRACK.TARGET,TRACK.SOURCE,TRACK.ARTIFACT,TRACK.INTEGRITY,TRACK.MODEL,TRACK.TENSOR,TRACK.RESIDENCY,TRACK.BACKEND,TRACK.GRAPH,TRACK.PREFILL,TRACK.KV,TRACK.DECODE,TRACK.LOGITS,TRACK.SAMPLING,TRACK.TOKENIZER,TRACK.GENERATION,TRACK.OPERATOR,TRACK.CI,TRACK.RELEASE\n");
    printf("blocked_tracks: TRACK.TARGET,TRACK.ARTIFACT,TRACK.MODEL,TRACK.TENSOR,TRACK.GRAPH,TRACK.PREFILL,TRACK.KV,TRACK.DECODE,TRACK.LOGITS,TRACK.SAMPLING,TRACK.GENERATION\n");
    printf("excluded_tracks: TRACK.SERVE,TRACK.EVAL,TRACK.BENCH,TRACK.SPEC\n");
    printf("post010_tracks: TRACK.SERVE,TRACK.BENCH,TRACK.SPEC,TRACK.POST010\n");
    printf("blocking_rows: V010.TARGET.2,V010.TARGET.3,V010.TARGET.4,V010.MAP.2,V010.ARTIFACT.EMIT.2,V010.FULLMODEL.6\n");
    printf("next_required_rows: V010.TARGET.2\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
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
    printf("candidate_%lu_id: %s\n", index, fact->id);
    printf("candidate_%lu_class: %s\n", index, fact->class_name);
    printf("candidate_%lu_stage: %s\n", index, fact->stage);
    printf("candidate_%lu_eligibility: %s\n", index, fact->eligibility);
    printf("candidate_%lu_artifact_status: %s\n", index, fact->artifact_status);
    printf("candidate_%lu_source_status: %s\n", index, fact->source_status);
    printf("candidate_%lu_tensor_coverage_status: %s\n", index, fact->tensor_coverage_status);
    printf("candidate_%lu_runtime_path_status: %s\n", index, fact->runtime_path_status);
    printf("candidate_%lu_generation_status: %s\n", index, fact->generation_status);
    printf("candidate_%lu_benchmark_status: %s\n", index, fact->benchmark_status);
    printf("candidate_%lu_blocker_count: %u\n", index, fact->blocker_count);
    if (include_blockers) {
        for (i = 0; i < fact->blocker_count; ++i) {
            printf("candidate_%lu_blocker_%u: %s\n", index, i, fact->blockers[i]);
        }
    }
    if (include_next) {
        printf("candidate_%lu_next_required_rows: %s\n", index, fact->next_required_rows);
    }
}

static void print_registered_candidate(unsigned long index,
                                       const yvex_model_registry_entry *entry,
                                       int include_blockers,
                                       int include_next)
{
    const char *alias = entry && entry->alias ? entry->alias : "unknown-registered-alias";

    printf("candidate_%lu_id: %s\n", index, alias);
    printf("candidate_%lu_class: registered-alias\n", index);
    printf("candidate_%lu_stage: report-only\n", index);
    printf("candidate_%lu_eligibility: candidate-incomplete\n", index);
    printf("candidate_%lu_artifact_status: registered-artifact-not-inspected\n", index);
    printf("candidate_%lu_source_status: unknown\n", index);
    printf("candidate_%lu_tensor_coverage_status: %s\n", index,
           registered_candidate_tensor_status(entry));
    printf("candidate_%lu_runtime_path_status: unsupported\n", index);
    printf("candidate_%lu_generation_status: unsupported-full-model\n", index);
    printf("candidate_%lu_benchmark_status: not-measured\n", index);
    printf("candidate_%lu_blocker_count: 8\n", index);
    if (include_blockers) {
        printf("candidate_%lu_blocker_0: missing-source-inventory\n", index);
        printf("candidate_%lu_blocker_1: missing-tensor-map\n", index);
        printf("candidate_%lu_blocker_2: missing-required-tensor-coverage\n", index);
        printf("candidate_%lu_blocker_3: missing-tokenizer-metadata\n", index);
        printf("candidate_%lu_blocker_4: missing-output-head\n", index);
        printf("candidate_%lu_blocker_5: missing-real-prefill\n", index);
        printf("candidate_%lu_blocker_6: missing-real-decode\n", index);
        printf("candidate_%lu_blocker_7: missing-real-logits\n", index);
    }
    if (include_next) {
        printf("candidate_%lu_next_required_rows: V010.TARGET.3,V010.MAP.*,V010.FULLMODEL.*\n", index);
    }
}

static int print_model_target_candidate_missing(const char *release, const char *target)
{
    printf("model-target: candidate\n");
    printf("status: full-runtime-candidate-report-fail\n");
    printf("release: %s\n", release && release[0] ? release : "v0.1.0");
    printf("target_requested: %s\n", target && target[0] ? target : "none");
    printf("decision_state: blocked-no-candidate\n");
    printf("selected_target_id: none\n");
    printf("full_runtime_candidate_status: missing\n");
    printf("candidate_count: 0\n");
    printf("eligible_candidate_count: 0\n");
    printf("pressure_target_count: 0\n");
    printf("fixture_target_count: 0\n");
    printf("global_blocker: no eligible full-runtime candidate\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
    return 2;
}

static int print_model_target_candidate_unsupported_release(const char *release)
{
    printf("model-target: candidate\n");
    printf("status: unsupported-release\n");
    printf("release: %s\n", release && release[0] ? release : "unknown");
    printf("decision_state: blocked-no-candidate\n");
    printf("selected_target_id: none\n");
    printf("full_runtime_candidate_status: missing\n");
    printf("candidate_count: 0\n");
    printf("eligible_candidate_count: 0\n");
    printf("pressure_target_count: 0\n");
    printf("fixture_target_count: 0\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
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

    printf("model-target: candidate\n");
    printf("status: full-runtime-candidate-report\n");
    printf("release: %s\n", release);
    printf("decision_state: blocked-no-candidate\n");
    printf("selected_target_id: none\n");
    printf("full_runtime_candidate_status: missing\n");
    printf("candidate_count: %lu\n", candidate_count);
    printf("eligible_candidate_count: 0\n");
    printf("pressure_target_count: %lu\n", pressure_count);
    printf("fixture_target_count: %lu\n", fixture_count);
    printf("registered_alias_count: %lu\n", target_id ? (target_entry ? 1ul : 0ul) : registry_count);
    if (include_pressure_targets) {
        printf("deepseek_pressure_status: selected-slice-pressure-only\n");
        printf("glm_pressure_status: source-storage-pressure-only\n");
        printf("qwen_metal_pressure_status: planned-portability-pressure-only\n");
    }
    printf("global_blocker: no eligible full-runtime candidate\n");
    if (include_next) {
        printf("next_required_rows: V010.TARGET.3\n");
    }
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");

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
        printf("dense_candidate_%lu_required_role_%lu: %s\n",
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
    printf("dense_candidate_%lu_id: %s\n", index, fact->id);
    printf("dense_candidate_%lu_family: %s\n", index, fact->family);
    printf("dense_candidate_%lu_class: %s\n", index, fact->class_name);
    printf("dense_candidate_%lu_stage: %s\n", index, fact->stage);
    printf("dense_candidate_%lu_eligibility: %s\n", index, fact->eligibility);
    printf("dense_candidate_%lu_source_status: %s\n", index, fact->source_status);
    printf("dense_candidate_%lu_artifact_status: %s\n", index, fact->artifact_status);
    printf("dense_candidate_%lu_tensor_map_status: %s\n", index, fact->tensor_map_status);
    printf("dense_candidate_%lu_tensor_coverage_status: %s\n", index, fact->tensor_coverage_status);
    printf("dense_candidate_%lu_tokenizer_status: %s\n", index, fact->tokenizer_status);
    printf("dense_candidate_%lu_output_head_status: %s\n", index, fact->output_head_status);
    printf("dense_candidate_%lu_runtime_path_status: %s\n", index, fact->runtime_path_status);
    printf("dense_candidate_%lu_generation_status: %s\n", index, fact->generation_status);
    printf("dense_candidate_%lu_benchmark_status: %s\n", index, fact->benchmark_status);
    printf("dense_candidate_%lu_blocker_count: %u\n", index, fact->blocker_count);
    if (include_requirements) {
        print_dense_candidate_requirements(index);
    }
    if (include_blockers) {
        for (i = 0; i < fact->blocker_count; ++i) {
            printf("dense_candidate_%lu_blocker_%u: %s\n", index, i, fact->blockers[i]);
        }
    }
    if (include_next) {
        printf("dense_candidate_%lu_next_required_rows: %s\n", index, fact->next_required_rows);
    }
}

static void print_registered_dense_candidate(unsigned long index,
                                             const yvex_model_registry_entry *entry,
                                             int include_requirements,
                                             int include_blockers,
                                             int include_next)
{
    const char *alias = entry && entry->alias ? entry->alias : "unknown-registered-alias";

    printf("dense_candidate_%lu_id: %s\n", index, alias);
    printf("dense_candidate_%lu_family: registered\n", index);
    printf("dense_candidate_%lu_class: registered-alias\n", index);
    printf("dense_candidate_%lu_stage: report-only\n", index);
    printf("dense_candidate_%lu_eligibility: candidate-incomplete\n", index);
    printf("dense_candidate_%lu_source_status: unknown\n", index);
    printf("dense_candidate_%lu_artifact_status: registered-artifact-not-inspected\n", index);
    printf("dense_candidate_%lu_tensor_map_status: missing-tensor-map\n", index);
    printf("dense_candidate_%lu_tensor_coverage_status: %s\n", index,
           registered_candidate_tensor_status(entry));
    printf("dense_candidate_%lu_tokenizer_status: unknown\n", index);
    printf("dense_candidate_%lu_output_head_status: unknown\n", index);
    printf("dense_candidate_%lu_runtime_path_status: unsupported\n", index);
    printf("dense_candidate_%lu_generation_status: unsupported-full-model\n", index);
    printf("dense_candidate_%lu_benchmark_status: not-measured\n", index);
    printf("dense_candidate_%lu_blocker_count: 12\n", index);
    if (include_requirements) {
        print_dense_candidate_requirements(index);
    }
    if (include_blockers) {
        printf("dense_candidate_%lu_blocker_0: missing-dense-source\n", index);
        printf("dense_candidate_%lu_blocker_1: missing-dense-artifact\n", index);
        printf("dense_candidate_%lu_blocker_2: missing-source-manifest\n", index);
        printf("dense_candidate_%lu_blocker_3: missing-native-inventory\n", index);
        printf("dense_candidate_%lu_blocker_4: missing-tensor-map\n", index);
        printf("dense_candidate_%lu_blocker_5: missing-required-tensor-coverage\n", index);
        printf("dense_candidate_%lu_blocker_6: missing-tokenizer-metadata\n", index);
        printf("dense_candidate_%lu_blocker_7: missing-output-head\n", index);
        printf("dense_candidate_%lu_blocker_8: missing-dense-mlp\n", index);
        printf("dense_candidate_%lu_blocker_9: missing-real-prefill\n", index);
        printf("dense_candidate_%lu_blocker_10: missing-real-decode\n", index);
        printf("dense_candidate_%lu_blocker_11: missing-real-logits\n", index);
    }
    if (include_next) {
        printf("dense_candidate_%lu_next_required_rows: V010.TARGET.7,V010.SOURCE.9,V010.MAP.*\n", index);
    }
}

static int print_model_target_dense_candidate_missing(const char *release, const char *target)
{
    printf("model-target: dense-candidate\n");
    printf("status: dense-candidate-report-fail\n");
    printf("release: %s\n", release && release[0] ? release : "v0.1.0");
    printf("target_requested: %s\n", target && target[0] ? target : "none");
    printf("decision_state: dense-candidate-missing\n");
    printf("selected_dense_candidate_id: none\n");
    printf("dense_candidate_status: missing\n");
    printf("dense_candidate_count: 0\n");
    printf("eligible_dense_candidate_count: 0\n");
    printf("dense_pressure_target_count: 0\n");
    printf("fixture_target_count: 0\n");
    printf("global_blocker: no eligible dense full-runtime candidate\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
    return 2;
}

static int print_model_target_dense_candidate_unsupported_release(const char *release)
{
    printf("model-target: dense-candidate\n");
    printf("status: unsupported-release\n");
    printf("release: %s\n", release && release[0] ? release : "unknown");
    printf("decision_state: dense-candidate-missing\n");
    printf("selected_dense_candidate_id: none\n");
    printf("dense_candidate_status: missing\n");
    printf("dense_candidate_count: 0\n");
    printf("eligible_dense_candidate_count: 0\n");
    printf("dense_pressure_target_count: 0\n");
    printf("fixture_target_count: 0\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
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

    printf("model-target: dense-candidate\n");
    printf("status: dense-candidate-report\n");
    printf("release: %s\n", release);
    printf("decision_state: %s\n", eligible_count ? "dense-candidate-found" : "dense-candidate-missing");
    printf("selected_dense_candidate_id: none\n");
    printf("dense_candidate_status: %s\n",
           target_id ? dense_candidate_status_for_target(target_fact, target_entry)
                     : (eligible_count ? "candidate-found" : "missing"));
    printf("dense_candidate_count: %lu\n", dense_candidate_count);
    printf("eligible_dense_candidate_count: %lu\n", eligible_count);
    printf("dense_pressure_target_count: %lu\n", dense_pressure_count);
    printf("fixture_target_count: %lu\n", fixture_count);
    printf("registered_alias_count: %lu\n", target_id ? (target_entry ? 1ul : 0ul) : registry_count);
    printf("global_blocker: no eligible dense full-runtime candidate\n");
    if (include_next) {
        printf("next_required_rows: V010.TARGET.7\n");
    }
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");

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
    printf("qwen_candidate_%lu_id: %s\n", index, fact->id);
    printf("qwen_candidate_%lu_class: %s\n", index, fact->class_name);
    printf("qwen_candidate_%lu_stage: %s\n", index, fact->stage);
    printf("qwen_candidate_%lu_eligibility: %s\n", index, fact->eligibility);
    printf("qwen_candidate_%lu_source_status: %s\n", index, fact->source_status);
    printf("qwen_candidate_%lu_artifact_status: %s\n", index, fact->artifact_status);
    printf("qwen_candidate_%lu_tensor_map_status: %s\n", index, fact->tensor_map_status);
    printf("qwen_candidate_%lu_backend_status: %s\n", index, fact->backend_status);
    printf("qwen_candidate_%lu_runtime_status: %s\n", index, fact->runtime_status);
    printf("qwen_candidate_%lu_generation_status: %s\n", index, fact->generation_status);
    printf("qwen_candidate_%lu_benchmark_status: %s\n", index, fact->benchmark_status);
    printf("qwen_candidate_%lu_blocker_count: %u\n", index, fact->blocker_count);
    if (include_blockers) {
        for (i = 0; i < fact->blocker_count; ++i) {
            printf("qwen_candidate_%lu_blocker_%u: %s\n", index, i, fact->blockers[i]);
        }
    }
}

static int print_model_target_qwen_metal_missing(const char *release, const char *target)
{
    printf("model-target: qwen-metal\n");
    printf("status: qwen-metal-pressure-report-fail\n");
    printf("release: %s\n", release && release[0] ? release : "v0.1.0");
    printf("target_requested: %s\n", target && target[0] ? target : "none");
    printf("lane_id: qwen-metal\n");
    printf("target_family: qwen\n");
    printf("target_class: metal-reduced-full-runtime-pressure\n");
    printf("runtime_shape: dense-or-dense-like-candidate-pending-source-config\n");
    printf("hardware_lane: apple-silicon-metal\n");
    printf("backend_lane: metal-planned\n");
    printf("full_runtime_candidate_status: candidate-planned\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
    return 2;
}

static int print_model_target_qwen_metal_unsupported_release(const char *release)
{
    printf("model-target: qwen-metal\n");
    printf("status: unsupported-release\n");
    printf("release: %s\n", release && release[0] ? release : "unknown");
    printf("lane_id: qwen-metal\n");
    printf("target_family: qwen\n");
    printf("target_class: metal-reduced-full-runtime-pressure\n");
    printf("runtime_shape: dense-or-dense-like-candidate-pending-source-config\n");
    printf("hardware_lane: apple-silicon-metal\n");
    printf("backend_lane: metal-planned\n");
    printf("full_runtime_candidate_status: missing\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
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

    printf("model-target: qwen-metal\n");
    printf("status: qwen-metal-pressure-report\n");
    printf("release: %s\n", release);
    printf("lane_id: qwen-metal\n");
    printf("target_family: qwen\n");
    printf("target_class: metal-reduced-full-runtime-pressure\n");
    printf("stage: report-only\n");
    printf("eligibility: pressure-target-only\n");
    printf("runtime_shape: dense-or-dense-like-candidate-pending-source-config\n");
    printf("hardware_lane: apple-silicon-metal\n");
    printf("backend_lane: metal-planned\n");
    printf("source_status: missing\n");
    printf("artifact_status: missing\n");
    printf("metal_backend_status: unsupported\n");
    printf("qwen_runtime_status: unsupported\n");
    printf("full_runtime_candidate_status: candidate-planned\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");

    if (include_candidates || target_id) {
        if (target_fact) {
            printf("qwen_candidate_count: 1\n");
            print_qwen_metal_candidate(0, target_fact, include_blockers);
        } else {
            printf("qwen_candidate_count: %lu\n", qwen_metal_candidate_fact_count);
            for (i = 0; i < qwen_metal_candidate_fact_count; ++i) {
                print_qwen_metal_candidate(i, &qwen_metal_candidate_facts[i], include_blockers);
            }
        }
    }

    if (include_hardware) {
        printf("hardware_profile_status: planned\n");
        printf("machine_profile_required: true\n");
        printf("unified_memory_report_required: true\n");
        printf("metal_device_report_required: true\n");
    }
    if (include_backend) {
        printf("metal_feasibility_status: missing\n");
        printf("metal_allocation_status: unsupported\n");
        printf("metal_graph_primitive_status: unsupported\n");
        printf("cuda_lane_independent: true\n");
    }
    if (include_source) {
        printf("source_family: qwen\n");
        printf("source_manifest_status: missing\n");
        printf("native_tensor_inventory_status: missing\n");
        printf("source_config_status: missing\n");
        printf("model_class_profile_status: missing\n");
    }
    if (include_blockers) {
        for (i = 0; i < qwen_metal_blocker_count; ++i) {
            printf("blocker_%lu: %s\n", i, qwen_metal_blockers[i]);
        }
    }
    if (include_next) {
        printf("next_required_rows: V010.SOURCE.9\n");
    }
    return 0;
}

static int print_model_target_decision_unsupported_release(const char *release)
{
    const char *value;

    value = release && release[0] ? release : "unknown";
    printf("target_decision: %s\n", value);
    printf("status: unsupported-release\n");
    printf("release: %s\n", value);
    printf("decision_state: deferred\n");
    printf("selected_target_id: none\n");
    printf("selected_target_class: none\n");
    printf("selected_family: none\n");
    printf("selected_model: none\n");
    printf("selected_source_class: none\n");
    printf("selected_artifact_class: none\n");
    printf("selected_backend_policy: none\n");
    printf("selected_reason: unsupported release target decision vocabulary\n");
    printf("full_runtime_candidate_status: unknown\n");
    printf("candidate_count: 0\n");
    printf("eligible_candidate_count: 0\n");
    printf("ineligible_candidate_count: 0\n");
    printf("selected_runtime_slice_eligible: false\n");
    printf("source_only_eligible: false\n");
    printf("external_reference_eligible: false\n");
    printf("fixture_only_eligible: false\n");
    printf("deepseek_pressure_status: unknown\n");
    printf("glm_pressure_status: unknown\n");
    printf("qwen_metal_pressure_status: unknown\n");
    printf("dense_candidate_status: unknown\n");
    printf("moe_candidate_status: unknown\n");
    printf("selected_candidate_tensor_coverage: none\n");
    printf("selected_candidate_artifact_status: none\n");
    printf("selected_candidate_integrity_status: none\n");
    printf("selected_candidate_backend_status: none\n");
    printf("selected_candidate_graph_status: unsupported\n");
    printf("selected_candidate_prefill_status: unsupported\n");
    printf("selected_candidate_kv_status: unsupported\n");
    printf("selected_candidate_decode_status: unsupported\n");
    printf("selected_candidate_logits_status: unsupported\n");
    printf("selected_candidate_sampling_status: unsupported\n");
    printf("selected_candidate_generation_status: unsupported\n");
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
    printf("target_decision: %s\n", release_value);
    printf("status: missing-candidate\n");
    printf("release: %s\n", release_value);
    printf("candidate_requested: %s\n", candidate_value);
    printf("decision_state: blocked-no-candidate\n");
    printf("selected_target_id: none\n");
    printf("selected_target_class: none\n");
    printf("selected_family: none\n");
    printf("selected_model: none\n");
    printf("selected_source_class: none\n");
    printf("selected_artifact_class: none\n");
    printf("selected_backend_policy: none\n");
    printf("selected_reason: requested candidate is not registered\n");
    printf("full_runtime_candidate_status: missing\n");
    printf("candidate_count: 0\n");
    printf("eligible_candidate_count: 0\n");
    printf("ineligible_candidate_count: 0\n");
    printf("selected_runtime_slice_eligible: false\n");
    printf("source_only_eligible: false\n");
    printf("external_reference_eligible: false\n");
    printf("fixture_only_eligible: false\n");
    printf("deepseek_pressure_status: selected-slice-pressure-only\n");
    printf("glm_pressure_status: source-storage-pressure-only\n");
    printf("qwen_metal_pressure_status: planned-portability-pressure-only\n");
    printf("dense_candidate_status: missing\n");
    printf("moe_candidate_status: blocked-missing-tensor-map\n");
    printf("selected_candidate_tensor_coverage: none\n");
    printf("selected_candidate_artifact_status: none\n");
    printf("selected_candidate_integrity_status: none\n");
    printf("selected_candidate_backend_status: none\n");
    printf("selected_candidate_graph_status: unsupported\n");
    printf("selected_candidate_prefill_status: unsupported\n");
    printf("selected_candidate_kv_status: unsupported\n");
    printf("selected_candidate_decode_status: unsupported\n");
    printf("selected_candidate_logits_status: unsupported\n");
    printf("selected_candidate_sampling_status: unsupported\n");
    printf("selected_candidate_generation_status: unsupported\n");
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

    printf("target_decision: %s\n", release);
    printf("status: %s\n", selected ? "target-decision-selected" : "target-decision-blocked");
    printf("release: %s\n", release);
    printf("decision_state: %s\n", selected ? "selected" : "blocked-no-candidate");
    printf("selected_target_id: %s\n", selected ? selected->target_id : "none");
    printf("selected_target_class: %s\n", selected ? target_decision_candidate_class(selected) : "none");
    printf("selected_family: %s\n", selected ? selected->family : "none");
    printf("selected_model: %s\n", selected ? selected->model : "none");
    printf("selected_source_class: %s\n", selected ? selected->source_artifact_class : "none");
    printf("selected_artifact_class: %s\n", selected ? selected->target_artifact_class : "none");
    printf("selected_backend_policy: %s\n", selected ? "cpu-cuda-capability-required" : "none");
    printf("selected_reason: %s\n", selected
           ? "registered full-runtime candidate selected for v0.1.0 planning"
           : "no current full-runtime-candidate target is eligible for v0.1.0");
    printf("full_runtime_candidate_status: %s\n", eligible_count ? "present" : "missing");
    printf("candidate_count: %lu\n", candidate_count);
    printf("eligible_candidate_count: %lu\n", eligible_count);
    printf("ineligible_candidate_count: %lu\n", ineligible_count);
    printf("selected_runtime_slice_eligible: false\n");
    printf("source_only_eligible: false\n");
    printf("external_reference_eligible: false\n");
    printf("fixture_only_eligible: false\n");
    printf("deepseek_pressure_status: selected-slice-pressure-only\n");
    printf("glm_pressure_status: source-storage-pressure-only\n");
    printf("qwen_metal_pressure_status: planned-portability-pressure-only\n");
    printf("dense_candidate_status: %s\n", selected ? "selected" : "missing");
    printf("moe_candidate_status: %s\n", selected ? "target-dependent" : "blocked-missing-tensor-map");
    printf("selected_candidate_tensor_coverage: %s\n", selected ? "requires-report" : "none");
    printf("selected_candidate_artifact_status: %s\n", selected ? "requires-integrity-gate" : "none");
    printf("selected_candidate_integrity_status: %s\n", selected ? "requires-integrity-gate" : "none");
    printf("selected_candidate_backend_status: %s\n", selected ? "requires-backend-gate" : "none");
    printf("selected_candidate_graph_status: unsupported\n");
    printf("selected_candidate_prefill_status: unsupported\n");
    printf("selected_candidate_kv_status: unsupported\n");
    printf("selected_candidate_decode_status: unsupported\n");
    printf("selected_candidate_logits_status: unsupported\n");
    printf("selected_candidate_sampling_status: unsupported\n");
    printf("selected_candidate_generation_status: unsupported\n");
    print_target_decision_constant_tail();

    for (i = 0; i < model_target_count; ++i) {
        const yvex_model_target_record *record = &model_targets[i];
        if (candidate_filter && record != candidate_filter) continue;
        printf("\n");
        print_model_target_decision_candidate(out_index++, record);
    }
    return 0;
}

static void print_model_target_usage(FILE *fp)
{
    fprintf(fp, "usage: yvex model-target classes\n");
    fprintf(fp, "       yvex model-target list\n");
    fprintf(fp, "       yvex model-target candidate --release v0.1.0 [options]\n");
    fprintf(fp, "       yvex model-target dense-candidate --release v0.1.0 [options]\n");
    fprintf(fp, "       yvex model-target qwen-metal --release v0.1.0 [options]\n");
    fprintf(fp, "       yvex model-target decision --release v0.1.0 [options]\n");
    fprintf(fp, "       yvex model-target inspect TARGET [--paths] [--models-root DIR]\n");
}

void yvex_model_target_help(FILE *fp)
{
    print_model_target_usage(fp);
    fprintf(fp, "\n--paths           show expected operator-local source, artifact, report, reference, and registry paths\n");
    fprintf(fp, "--models-root DIR override configured operator model root for this command only\n");
    fprintf(fp, "\nDecision report:\n");
    fprintf(fp, "  yvex model-target decision --release v0.1.0 --include-candidates --include-blockers --include-next\n");
    fprintf(fp, "  This command records the v0.1.0 target decision. It does not download models, emit artifacts, materialize tensors, execute graph work, run prefill, decode, logits, sampling, generation, evaluation, or benchmarks.\n");
    fprintf(fp, "\nCandidate report:\n");
    fprintf(fp, "  yvex model-target candidate --release v0.1.0 --include-candidates --include-blockers --include-next\n");
    fprintf(fp, "  The candidate report evaluates full-runtime target eligibility for a release. It does not select a ready model, download weights, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "\nDense candidate report:\n");
    fprintf(fp, "  yvex model-target dense-candidate --release v0.1.0 --include-candidates --include-requirements --include-blockers --include-next\n");
    fprintf(fp, "  The dense-candidate report evaluates whether a dense model target can become the first v0.1.0 full-runtime candidate. It does not download weights, emit artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "\nQwen/Metal pressure report:\n");
    fprintf(fp, "  yvex model-target qwen-metal --release v0.1.0 --include-candidates --include-hardware --include-backend --include-source --include-blockers --include-next\n");
    fprintf(fp, "  The Qwen/Metal pressure report records a planned reduced-scale Apple Silicon / Metal lane for future full-runtime work. It does not download weights, implement Metal, emit Qwen artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "\nModel targets are pressure objects, not capability claims.\n");
    fprintf(fp, "External GGUFs and external runners are reference evidence only.\n");
    fprintf(fp, "Model-target path reporting does not read model payloads, create artifacts, register aliases, or claim runtime support.\n");
}

static void print_model_target_classes(void)
{
    unsigned long i;

    printf("status: model-target-classes\n");
    for (i = 0; i < model_target_class_count; ++i) {
        const yvex_model_target_class_record *record = &model_target_classes[i];
        printf("class: %s\n", record->class_id);
        printf("capability_claim: %s\n", record->capability_claim);
        printf("runtime_execution: %s\n", record->runtime_execution);
        printf("generation: %s\n", record->generation);
        printf("description: %s\n", record->description);
        if (i + 1 < model_target_class_count) {
            printf("\n");
        }
    }
}

static void print_model_target_list(void)
{
    unsigned long i;

    printf("status: model-target-list\n");
    for (i = 0; i < model_target_count; ++i) {
        const yvex_model_target_record *record = &model_targets[i];
        printf("target: %s\n", record->target_id);
        printf("family: %s\n", record->family);
        printf("target_class: %s\n", record->target_class);
        printf("runtime_execution: %s\n", record->runtime_execution);
        printf("generation: %s\n", record->generation);
        if (i + 1 < model_target_count) {
            printf("\n");
        }
    }
}

static void print_model_target_record(const yvex_model_target_record *record)
{
    printf("status: model-target\n");
    printf("target_id: %s\n", record->target_id);
    printf("family: %s\n", record->family);
    printf("model: %s\n", record->model);
    printf("target_class: %s\n", record->target_class);
    printf("source_artifact_class: %s\n", record->source_artifact_class);
    printf("target_artifact_class: %s\n", record->target_artifact_class);
    printf("pressure_purpose: %s\n", record->pressure_purpose);
    printf("tensor_set: %s\n", record->tensor_set);
    printf("local_path_class: %s\n", record->local_path_class);
    printf("source_footprint_class: %s\n", record->source_footprint_class);
    printf("runtime_boundary: %s\n", record->runtime_boundary);
    printf("runtime_execution: %s\n", record->runtime_execution);
    printf("generation: %s\n", record->generation);
    printf("external_reference: %s\n", record->external_reference);
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
        fprintf(stderr, "model-target: artifact path fields are required\n");
        return 2;
    }
    n = snprintf(out, cap, "%s/%s/%s", operator_paths->gguf_root, family, filename);
    if (n < 0 || (size_t)n >= cap) {
        fprintf(stderr, "model-target: artifact path is too long\n");
        return 2;
    }
    return 0;
}

static int print_model_target_paths(const yvex_model_target_record *record,
                                    const char *models_root_override)
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
        fprintf(stderr, "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }

    if (strcmp(record->family, "DeepSeek") == 0) {
        family_key = "deepseek";
    } else if (strcmp(record->family, "GLM") == 0) {
        family_key = "glm";
    } else {
        fprintf(stderr, "model-target: no path mapping for family: %s\n", record->family);
        return 2;
    }

    rc = yvex_operator_paths_resolve_target(&operator_paths, family_key, "source",
                                            source_path, sizeof(source_path), &source_exists, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }
    rc = yvex_operator_paths_resolve_target(&operator_paths, family_key, "reports",
                                            report_dir, sizeof(report_dir), &report_exists, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }
    rc = yvex_operator_paths_resolve_target(&operator_paths, family_key, "reference",
                                            reference_dir, sizeof(reference_dir), &reference_exists, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }
    rc = yvex_operator_paths_resolve_target(&operator_paths, family_key, "registry",
                                            registry_dir, sizeof(registry_dir), &registry_exists, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }

    source_class = "official safetensors";
    if (strcmp(record->target_id, "deepseek4-v4-flash-selected-embed") == 0) {
        rc = format_model_target_artifact_path(
            artifact_path, sizeof(artifact_path), &operator_paths, "deepseek",
            "deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf");
        if (rc != 0) {
            return rc;
        }
        registry_alias = record->target_id;
        target_class = "YVEX-produced selected GGUF";
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
        target_class = "YVEX-produced selected GGUF";
        runtime_execution = "selected-segment-boundary-only";
        artifact_exists = path_exists(artifact_path);
    } else if (strcmp(record->target_id, "glm-5.2-official-safetensors") == 0) {
        snprintf(artifact_path, sizeof(artifact_path), "%s", "planned");
        registry_alias = "none";
        target_class = "future YVEX-produced GGUF";
        runtime_execution = "unsupported";
        artifact_exists = 0;
    } else {
        fprintf(stderr, "model-target: no path mapping for target: %s\n", record->target_id);
        return 2;
    }

    printf("models_root_source: %s\n", operator_paths.models_root_source);
    printf("models_root: %s\n", operator_paths.models_root);
    printf("source_path: %s\n", source_path);
    printf("source_exists: %s\n", source_exists ? "true" : "false");
    printf("artifact_path: %s\n", artifact_path);
    printf("artifact_exists: %s\n", artifact_exists ? "true" : "false");
    printf("report_dir: %s\n", report_dir);
    printf("report_dir_exists: %s\n", report_exists ? "true" : "false");
    printf("reference_dir: %s\n", reference_dir);
    printf("reference_dir_exists: %s\n", reference_exists ? "true" : "false");
    printf("registry_dir: %s\n", registry_dir);
    printf("registry_dir_exists: %s\n", registry_exists ? "true" : "false");
    printf("registry_alias: %s\n", registry_alias);
    printf("source_artifact_class: %s\n", source_class);
    printf("target_artifact_class: %s\n", target_class);
    printf("runtime_execution: %s\n", runtime_execution);
    printf("generation: unsupported\n");
    return 0;
}

int yvex_model_target_command(int argc, char **argv)
{
    const yvex_model_target_record *record;
    const char *models_root = NULL;
    int want_paths = 0;
    int i;

    if (argc <= 2) {
        print_model_target_usage(stderr);
        return 2;
    }
    if (strcmp(argv[2], "help") == 0) {
        if (argc != 3) {
            print_model_target_usage(stderr);
            return 2;
        }
        yvex_model_target_help(stdout);
        return 0;
    }
    if (strcmp(argv[2], "classes") == 0) {
        if (argc != 3) {
            print_model_target_usage(stderr);
            return 2;
        }
        print_model_target_classes();
        return 0;
    }
    if (strcmp(argv[2], "list") == 0) {
        if (argc != 3) {
            print_model_target_usage(stderr);
            return 2;
        }
        print_model_target_list();
        return 0;
    }
    if (strcmp(argv[2], "candidate") == 0) {
        const char *release = NULL;
        const char *target_id = NULL;
        int include_candidates = 0;
        int include_pressure_targets = 0;
        int include_blockers = 0;
        int include_next = 0;

        for (i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--help") == 0) {
                if (argc != 4) {
                    print_model_target_candidate_usage(stderr);
                    return 2;
                }
                print_model_target_candidate_help(stdout);
                return 0;
            } else if (strcmp(argv[i], "--release") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target candidate: --release requires VERSION\n");
                    return 2;
                }
                release = argv[++i];
            } else if (strcmp(argv[i], "--target") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target candidate: --target requires TARGET\n");
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
            } else {
                fprintf(stderr, "model-target candidate: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        if (!release || release[0] == '\0') {
            fprintf(stderr, "model-target candidate: --release is required\n");
            print_model_target_candidate_usage(stderr);
            return 2;
        }
        if (strcmp(release, "v0.1.0") != 0) {
            return print_model_target_candidate_unsupported_release(release);
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

        for (i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--help") == 0) {
                if (argc != 4) {
                    print_model_target_dense_candidate_usage(stderr);
                    return 2;
                }
                print_model_target_dense_candidate_help(stdout);
                return 0;
            } else if (strcmp(argv[i], "--release") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target dense-candidate: --release requires VERSION\n");
                    return 2;
                }
                release = argv[++i];
            } else if (strcmp(argv[i], "--target") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target dense-candidate: --target requires TARGET\n");
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
            } else {
                fprintf(stderr, "model-target dense-candidate: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        if (!release || release[0] == '\0') {
            fprintf(stderr, "model-target dense-candidate: --release is required\n");
            print_model_target_dense_candidate_usage(stderr);
            return 2;
        }
        if (strcmp(release, "v0.1.0") != 0) {
            return print_model_target_dense_candidate_unsupported_release(release);
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

        for (i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--help") == 0) {
                if (argc != 4) {
                    print_model_target_qwen_metal_usage(stderr);
                    return 2;
                }
                print_model_target_qwen_metal_help(stdout);
                return 0;
            } else if (strcmp(argv[i], "--release") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target qwen-metal: --release requires VERSION\n");
                    return 2;
                }
                release = argv[++i];
            } else if (strcmp(argv[i], "--target") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target qwen-metal: --target requires TARGET\n");
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
            } else {
                fprintf(stderr, "model-target qwen-metal: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        if (!release || release[0] == '\0') {
            fprintf(stderr, "model-target qwen-metal: --release is required\n");
            print_model_target_qwen_metal_usage(stderr);
            return 2;
        }
        if (strcmp(release, "v0.1.0") != 0) {
            return print_model_target_qwen_metal_unsupported_release(release);
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

        for (i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--help") == 0) {
                if (argc != 4) {
                    print_model_target_decision_usage(stderr);
                    return 2;
                }
                print_model_target_decision_help(stdout);
                return 0;
            } else if (strcmp(argv[i], "--release") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target decision: --release requires VERSION\n");
                    return 2;
                }
                release = argv[++i];
            } else if (strcmp(argv[i], "--candidate") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target decision: --candidate requires TARGET\n");
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
            } else {
                fprintf(stderr, "model-target decision: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        if (!release || release[0] == '\0') {
            fprintf(stderr, "model-target decision: --release is required\n");
            print_model_target_decision_usage(stderr);
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
        return print_model_target_decision_report(release, candidate_filter);
    }
    if (strcmp(argv[2], "inspect") == 0) {
        if (argc < 4) {
            print_model_target_usage(stderr);
            return 2;
        }
        record = find_model_target(argv[3]);
        if (!record) {
            fprintf(stderr, "model-target: unknown target: %s\n", argv[3]);
            return 2;
        }
        for (i = 4; i < argc; ++i) {
            if (strcmp(argv[i], "--paths") == 0) {
                want_paths = 1;
            } else if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target: --models-root requires DIR\n");
                    return 2;
                }
                models_root = argv[++i];
            } else {
                fprintf(stderr, "model-target: unknown inspect option: %s\n", argv[i]);
                return 2;
            }
        }
        if (models_root && !want_paths) {
            fprintf(stderr, "model-target: --models-root requires --paths\n");
            return 2;
        }
        print_model_target_record(record);
        if (want_paths) {
            return print_model_target_paths(record, models_root);
        }
        return 0;
    }

    fprintf(stderr, "model-target: unknown subcommand: %s\n", argv[2]);
    fprintf(stderr, "Try 'yvex help model-target' for usage.\n");
    return 2;
}
