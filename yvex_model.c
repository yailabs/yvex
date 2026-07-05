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
#include <yvex/native_weights.h>
#include <yvex/qtype_support.h>
#include <yvex/tensor.h>
#include <yvex/weights.h>

#include <ctype.h>
#include <errno.h>
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
    fprintf(fp, "  --audit | --output normal|table|audit\n");
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
    fprintf(fp, "  --audit | --output normal|table|audit\n");
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
    fprintf(fp, "  --audit | --output normal|table|audit\n");
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
    fprintf(fp, "  --audit | --output normal|table|audit\n");
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

typedef enum {
    YVEX_MODEL_TARGET_OUTPUT_NORMAL = 0,
    YVEX_MODEL_TARGET_OUTPUT_TABLE,
    YVEX_MODEL_TARGET_OUTPUT_AUDIT
} yvex_model_target_output_mode;

#define YVEX_MODEL_CLASS_NEXT_ROW "V010.MAP.8"
#define YVEX_TENSOR_COLLECTION_NEXT_ROW "V010.MAP.8"
#define YVEX_TENSOR_NAMING_NEXT_ROW "V010.MAP.8"
#define YVEX_OUTPUT_HEAD_MAP_NEXT_ROW "V010.MAP.8"
#define YVEX_TOKENIZER_MAP_NEXT_ROW "V010.MAP.8"
#define YVEX_MISSING_ROLE_REPORT_NEXT_ROW "V010.MAP.9"
#define YVEX_TENSOR_MAPPING_GATE_NEXT_ROW "V010.QUANT.0"
#define YVEX_QTYPE_POLICY_NEXT_ROW "V010.QUANT.1"
#define YVEX_QTYPE_POLICY_BACK_ROW "V010.MAP.9"
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
    unsigned long long moe_router_count;
    unsigned long long moe_expert_count;
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
    char tokenizer_class[128];
    char model_type[64];
    const char *vocab_size_status;
    char vocab_size[32];
    char config_vocab_size[32];
    char tokenizer_vocab_size[32];
    char output_head_vocab_dim_candidate[32];
    const char *output_head_vocab_relation_status;
    const char *bos_token_id_status;
    char bos_token_id[32];
    const char *eos_token_id_status;
    char eos_token_id[32];
    const char *pad_token_id_status;
    char pad_token_id[32];
    const char *unk_token_id_status;
    char unk_token_id[32];
    const char *sep_token_id_status;
    char sep_token_id[32];
    const char *additional_special_tokens_status;
    char additional_special_tokens_count[32];
    const char *chat_template_status;
    const char *chat_template_present;
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
    const char *tokenizer_metadata_status;
    const char *config_metadata_status;
    const char *generation_metadata_status;
    const char *special_tokens_status;
    unsigned long long source_role_observed_count;
    unsigned long long source_role_missing_count;
    unsigned long long source_role_ambiguous_count;
    unsigned long long metadata_observed_count;
    unsigned long long metadata_missing_count;
    unsigned long long metadata_ambiguous_count;
    unsigned long long missing_entry_count;
    yvex_missing_role_entry missing_entries[16];
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
    fputc('"', fp);
    while (*s) {
        unsigned char ch = (unsigned char)*s++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', fp);
            fputc((int)ch, fp);
        } else if (ch == '\n') {
            fputs("\\n", fp);
        } else if (ch == '\r') {
            fputs("\\r", fp);
        } else if (ch == '\t') {
            fputs("\\t", fp);
        } else {
            fputc((int)ch, fp);
        }
    }
    fputc('"', fp);
}

static void model_target_json_field(FILE *fp,
                                    const char *key,
                                    const char *value,
                                    int comma)
{
    fprintf(fp, "  \"%s\": ", key);
    model_target_json_write_escaped(fp, value);
    fprintf(fp, "%s\n", comma ? "," : "");
}

static void model_target_json_u64_field(FILE *fp,
                                        const char *key,
                                        unsigned long long value,
                                        int comma)
{
    fprintf(fp, "  \"%s\": %llu%s\n", key, value, comma ? "," : "");
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

static int write_tensor_map_sidecar(const char *path,
                                    const yvex_tensor_naming_profile *profile)
{
    char tmp[YVEX_PATH_CAP];
    FILE *fp;

    if (!path || !path[0] || !profile) return 1;
    if (!model_target_json_open_tmp(path, tmp, sizeof(tmp), &fp)) return 0;
    fprintf(fp, "{\n");
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
    model_target_json_field(fp, "tokenizer_map_status", "missing", 1);
    model_target_json_field(fp, "runtime_claim", "unsupported", 1);
    model_target_json_field(fp, "generation", "unsupported-full-model", 1);
    model_target_json_field(fp, "benchmark_status", "not-measured", 0);
    fprintf(fp, "}\n");
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
    fprintf(fp, "{\n");
    model_target_json_field(fp, "schema", "yvex.source.output_head_map.v1", 1);
    model_target_json_field(fp, "row", "MODELS.SOURCE.MAP.HANDOFF.0", 1);
    model_target_json_field(fp, "status", "present-report-only", 1);
    model_target_json_field(fp, "target_id", profile->record->target_id, 1);
    model_target_json_field(fp, "family", profile->spec->family_key, 1);
    model_target_json_field(fp, "map_kind", "output-head", 1);
    model_target_json_field(fp, "source_path", profile->source_path, 1);
    model_target_json_field(fp, "output_head_status",
                            profile->output_head.present ? "present" : "missing", 1);
    model_target_json_field(fp, "output_head_native_name",
                            profile->output_head.native_name, 1);
    model_target_json_field(fp, "final_norm_status",
                            profile->final_norm.present ? "present" : "missing", 1);
    model_target_json_field(fp, "embedding_status",
                            profile->embedding.present ? "present" : "missing", 1);
    model_target_json_field(fp, "shape_relation_status",
                            profile->shape_relation_status, 1);
    model_target_json_field(fp, "runtime_claim", "unsupported", 1);
    model_target_json_field(fp, "generation", "unsupported-full-model", 1);
    model_target_json_field(fp, "benchmark_status", "not-measured", 0);
    fprintf(fp, "}\n");
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
            fprintf(stderr, "model-target class-profile: source path is too long\n");
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
            fprintf(stderr, "yvex: %s: %s\n",
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
            fprintf(stderr, "yvex: %s: %s\n",
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
                fprintf(stderr, "model-target class-profile: source path is too long\n");
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
    printf("model-class: %s\n", profile->spec->family_key);
    printf("target: %s\n", profile->record->target_id);
    printf("status: %s\n", profile->status);
    printf("class: %s\n", profile->spec->class_name);
    printf("evidence: header-metadata-only\n");
    printf("patterns: tensors=%llu attn=%llu mlp=%llu norm=%llu head=%llu moe=%llu\n",
           profile->tensor_count,
           model_class_profile_attention_count(profile),
           model_class_profile_mlp_count(profile),
           profile->norm_pattern_count,
           profile->output_head_pattern_count,
           model_class_profile_moe_count(profile));
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    printf("boundary: model-class profile only; no tensor role mapping/runtime/generation\n");
}

static void print_model_class_profile_table(
    const yvex_model_class_profile *profile)
{
    printf("MODEL CLASS PROFILE\n\n");
    printf("%-6s  %-24s  %-16s  %7s  %4s  %3s  %4s  %4s  %3s  %s\n",
           "FAMILY", "TARGET", "STATUS", "TENSORS",
           "ATTN", "MLP", "NORM", "HEAD", "MOE", "NEXT");
    printf("%-6s  %-24s  %-16s  %7llu  %4llu  %3llu  %4llu  %4llu  %3llu  %s\n",
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
    printf("model_class_profile_status: %s\n", profile->status);
    printf("model_class_family: %s\n", profile->spec->family_key);
    printf("model_class_target_id: %s\n", profile->record->target_id);
    printf("model_class_name: %s\n", profile->spec->class_name);
    printf("model_class_runtime_shape: %s\n", profile->spec->runtime_shape);
    printf("model_class_evidence_basis: header-metadata-only\n");
    printf("model_class_config_status: %s\n",
           profile->config_present ? "present" : "missing");
    printf("model_class_tokenizer_status: %s\n",
           profile->tokenizer_present ? "present" : "missing");
    printf("model_class_source_metadata_status: %s\n",
           profile->source_metadata_status);
    printf("model_class_tensor_count: %llu\n", profile->tensor_count);
    printf("model_class_embedding_pattern_count: %llu\n",
           profile->embedding_pattern_count);
    printf("model_class_attention_q_pattern_count: %llu\n",
           profile->attention_q_pattern_count);
    printf("model_class_attention_k_pattern_count: %llu\n",
           profile->attention_k_pattern_count);
    printf("model_class_attention_v_pattern_count: %llu\n",
           profile->attention_v_pattern_count);
    printf("model_class_attention_o_pattern_count: %llu\n",
           profile->attention_o_pattern_count);
    printf("model_class_mlp_gate_pattern_count: %llu\n",
           profile->mlp_gate_pattern_count);
    printf("model_class_mlp_up_pattern_count: %llu\n",
           profile->mlp_up_pattern_count);
    printf("model_class_mlp_down_pattern_count: %llu\n",
           profile->mlp_down_pattern_count);
    printf("model_class_norm_pattern_count: %llu\n",
           profile->norm_pattern_count);
    printf("model_class_output_head_pattern_count: %llu\n",
           profile->output_head_pattern_count);
    printf("model_class_moe_router_pattern_count: %llu\n",
           profile->moe_router_pattern_count);
    printf("model_class_moe_expert_pattern_count: %llu\n",
           profile->moe_expert_pattern_count);
    printf("model_class_other_pattern_count: %llu\n",
           profile->other_pattern_count);
    printf("model_class_pattern_status: lexical-only\n");
    printf("model_class_role_mapping_status: not-implemented\n");
    printf("model_class_runtime_status: unsupported\n");
    printf("backend_selection: deferred\n");
    printf("backend_pressure: %s\n", profile->spec->backend_pressure);
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
    printf("source_path: %s\n", profile->source_path);
    printf("source_path_source: %s\n", profile->source_path_source);
    printf("source_exists: %s\n", profile->source_exists ? "true" : "false");
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next_required_rows: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    printf("boundary: model-class profile only; no tensor role mapping/runtime/generation\n");
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
    printf("model_class_profile_status: command-visible\n");
    printf("model_class_family: %s\n", spec->family_key);
    printf("model_class_target_id: %s\n", spec->target_id);
    printf("model_class_name: %s\n", spec->class_name);
    printf("model_class_runtime_shape: %s\n", spec->runtime_shape);
    printf("model_class_evidence_basis: header-metadata-only\n");
    printf("model_class_pattern_status: lexical-only\n");
    printf("model_class_role_mapping_status: not-implemented\n");
    printf("model_class_runtime_status: unsupported\n");
}

static int tensor_collection_layer_index(const char *name,
                                              unsigned long *layer_index)
{
    const char *patterns[] = {
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
    printf("tensor-collection: %s\n", profile->spec->family_key);
    printf("target: %s\n", profile->record->target_id);
    printf("status: %s\n", profile->status);
    printf("stage: header-collection-inventory\n");
    printf("evidence: header-metadata-only\n");
    printf("collections: embedding=%llu attention_qkvo=%llu mlp_gud=%llu norm=%llu head=%llu moe=%llu\n",
           profile->embedding_tensor_count,
           profile->attention_complete_qkvo_layer_count,
           profile->mlp_complete_gud_layer_count,
           profile->norm_tensor_count,
           profile->output_head_tensor_count,
           profile->moe_router_count + profile->moe_expert_count);
    if (profile->source_exists && profile->tensor_count > 0) {
        printf("layers_observed: %llu\n", profile->layer_count_observed);
    }
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next: %s\n", YVEX_TENSOR_COLLECTION_NEXT_ROW);
    printf("boundary: tensor collection inventory only; no role mapping/runtime/generation\n");
}

static void print_tensor_collection_table(
    const yvex_tensor_collection_profile *profile)
{
    printf("TENSOR COLLECTION INVENTORY\n\n");
    printf("%-6s  %-10s  %-19s  %5s  %9s  %7s  %4s  %4s  %3s  %6s  %s\n",
           "FAMILY", "TARGET", "STATUS", "EMBED", "ATTN_QKVO",
           "MLP_GUD", "NORM", "HEAD", "MOE", "LAYERS", "NEXT");
    printf("%-6s  %-10s  %-19s  %5llu  %9llu  %7llu  %4llu  %4llu  %3llu  %6llu  %s\n",
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
    printf("tensor_collection_status: %s\n", profile->status);
    printf("tensor_collection_family: %s\n", profile->spec->family_key);
    printf("tensor_collection_target_id: %s\n", profile->record->target_id);
    printf("tensor_collection_stage: header-collection-inventory\n");
    printf("tensor_collection_evidence_basis: header-metadata-only\n");
    printf("tensor_collection_source_status: %s\n",
           profile->source_exists ? "present" : "missing");
    printf("tensor_collection_source_path: %s\n", profile->source_path);
    printf("tensor_collection_manifest_status: not-checked\n");
    printf("tensor_collection_config_status: %s\n",
           profile->config_present ? "present" : "missing");
    printf("tensor_collection_tokenizer_status: %s\n",
           profile->tokenizer_present ? "present" : "missing");
    printf("tensor_collection_tensor_count: %llu\n", profile->tensor_count);
    printf("tensor_collection_layer_count_observed: %llu\n",
           profile->layer_count_observed);
    printf("tensor_collection_embedding_status: %s\n",
           tensor_collection_present_status(profile->embedding_tensor_count));
    printf("tensor_collection_embedding_tensor_count: %llu\n",
           profile->embedding_tensor_count);
    printf("tensor_collection_attention_status: %s\n",
           tensor_collection_attention_status(profile));
    printf("tensor_collection_attention_q_count: %llu\n", profile->attention_q_count);
    printf("tensor_collection_attention_k_count: %llu\n", profile->attention_k_count);
    printf("tensor_collection_attention_v_count: %llu\n", profile->attention_v_count);
    printf("tensor_collection_attention_o_count: %llu\n", profile->attention_o_count);
    printf("tensor_collection_attention_complete_qkvo_layer_count: %llu\n",
           profile->attention_complete_qkvo_layer_count);
    printf("tensor_collection_mlp_status: %s\n",
           tensor_collection_mlp_status(profile));
    printf("tensor_collection_mlp_gate_count: %llu\n", profile->mlp_gate_count);
    printf("tensor_collection_mlp_up_count: %llu\n", profile->mlp_up_count);
    printf("tensor_collection_mlp_down_count: %llu\n", profile->mlp_down_count);
    printf("tensor_collection_mlp_complete_gud_layer_count: %llu\n",
           profile->mlp_complete_gud_layer_count);
    printf("tensor_collection_norm_status: %s\n",
           tensor_collection_present_status(profile->norm_tensor_count));
    printf("tensor_collection_norm_tensor_count: %llu\n",
           profile->norm_tensor_count);
    printf("tensor_collection_output_head_status: %s\n",
           tensor_collection_present_status(profile->output_head_tensor_count));
    printf("tensor_collection_output_head_tensor_count: %llu\n",
           profile->output_head_tensor_count);
    printf("tensor_collection_moe_status: %s\n",
           (profile->moe_router_count + profile->moe_expert_count) > 0
               ? "observed"
               : "not-observed");
    printf("tensor_collection_moe_router_count: %llu\n", profile->moe_router_count);
    printf("tensor_collection_moe_expert_count: %llu\n", profile->moe_expert_count);
    printf("tensor_collection_tokenizer_collection_status: %s\n",
           profile->tokenizer_present ? "sidecar-observed" : "missing");
    printf("tensor_collection_kv_runtime_state_status: runtime-state-required-not-implemented\n");
    printf("tensor_collection_validation_status: lexical-and-header-only\n");
    printf("tensor_collection_role_mapping_status: not-implemented\n");
    printf("tensor_collection_runtime_descriptor_status: not-implemented\n");
    printf("tensor_collection_graph_consumer_status: not-implemented\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next_required_rows: %s\n", YVEX_TENSOR_COLLECTION_NEXT_ROW);
    printf("boundary: tensor collection inventory only; no role mapping/runtime/generation\n");
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
    printf("tensor_collection_status: command-visible\n");
    printf("tensor_collection_family: %s\n", spec->family_key);
    printf("tensor_collection_target_id: %s\n", spec->target_id);
    printf("tensor_collection_stage: header-collection-inventory\n");
    printf("tensor_collection_evidence_basis: header-metadata-only\n");
    printf("tensor_collection_validation_status: lexical-and-header-only\n");
    printf("tensor_collection_role_mapping_status: not-implemented\n");
    printf("tensor_collection_runtime_descriptor_status: not-implemented\n");
    printf("tensor_collection_graph_consumer_status: not-implemented\n");
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
         strcmp(name, "embed_tokens.weight") == 0 ||
         model_class_name_contains_ci(name, "token_embd"))) {
        tensor_naming_set_mapped(entry, "embedding",
                                 "model.embedding.token.weight");
        return;
    }
    if (!has_layer &&
        (strcmp(name, "model.norm.weight") == 0 ||
         strcmp(name, "norm.weight") == 0)) {
        tensor_naming_set_mapped(entry, "norm", "model.final_norm.weight");
        return;
    }
    if (!has_layer &&
        (strcmp(name, "lm_head.weight") == 0 ||
         strcmp(name, "output.weight") == 0 ||
         tensor_naming_ends_with(name, ".output.weight"))) {
        tensor_naming_set_mapped(entry, "output-head",
                                 "model.output_head.weight");
        return;
    }
    if (!has_layer) return;

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
    if (strstr(name, ".input_layernorm.weight")) {
        snprintf(canonical, sizeof(canonical),
                 "model.layers.%lu.attention.norm.weight", layer_index);
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
        if (strstr(entry->canonical_role, ".router.weight")) {
            profile->moe_router_count++;
        } else {
            profile->moe_expert_count++;
        }
    }
}

static int tensor_naming_required_groups_present(
    const yvex_tensor_naming_profile *profile)
{
    return profile &&
           profile->embedding_count > 0 &&
           profile->attention_q_count > 0 &&
           profile->attention_k_count > 0 &&
           profile->attention_v_count > 0 &&
           profile->attention_o_count > 0 &&
           profile->mlp_gate_count > 0 &&
           profile->mlp_up_count > 0 &&
           profile->mlp_down_count > 0 &&
           profile->norm_count > 0 &&
           profile->output_head_count > 0;
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
    return profile->moe_router_count + profile->moe_expert_count;
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
    printf("tensor-map: %s [%s]\n",
           profile->record->target_id,
           compact_status_bracket(profile->status));
    printf("family: %s  stage: header-naming-map  evidence: header-only\n",
           profile->spec->family_key);
    printf("roles: total=%llu embedding=%llu attention=%llu mlp=%llu norm=%llu head=%llu moe=%llu unknown=%llu\n",
           profile->mapped_total_count,
           profile->embedding_count,
           profile->attention_count,
           profile->mlp_count,
           profile->norm_count,
           profile->output_head_count,
           tensor_naming_moe_count(profile),
           profile->unmapped_unknown_count);
    if (profile->source_exists && profile->tensor_count > 0) {
        printf("layers: %llu\n", profile->layer_count_observed);
    }
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next: %s\n", YVEX_TENSOR_NAMING_NEXT_ROW);
    printf("boundary: report-only; use --audit for tensor entries\n");
}

static void print_tensor_naming_table(
    const yvex_tensor_naming_profile *profile)
{
    printf("TENSOR NAMING MAP\n\n");
    printf("%-6s  %-15s  %-19s  %5s  %5s  %4s  %3s  %4s  %4s  %3s  %7s  %6s  %s\n",
           "FAMILY", "TARGET", "STATUS", "TOTAL", "EMBED", "ATTN",
           "MLP", "NORM", "HEAD", "MOE", "UNKNOWN", "LAYERS", "NEXT");
    printf("%-6s  %-15s  %-19s  %5llu  %5llu  %4llu  %3llu  %4llu  %4llu  %3llu  %7llu  %6llu  %s\n",
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

    printf("tensor_map_status: %s\n", profile->status);
    printf("tensor_map_family: %s\n", profile->spec->family_key);
    printf("tensor_map_target_id: %s\n", profile->record->target_id);
    printf("tensor_map_stage: header-naming-map\n");
    printf("tensor_map_evidence_basis: header-metadata-only\n");
    printf("tensor_map_source_status: %s\n",
           profile->source_exists ? "present" : "missing");
    printf("tensor_map_source_path: %s\n", profile->source_path);
    printf("tensor_map_manifest_status: not-checked\n");
    printf("tensor_map_config_status: %s\n",
           profile->config_present ? "present" : "missing");
    printf("tensor_map_tokenizer_status: %s\n",
           profile->tokenizer_present ? "present" : "missing");
    printf("tensor_map_tensor_count: %llu\n", profile->tensor_count);
    printf("tensor_map_mapped_total_count: %llu\n", profile->mapped_total_count);
    printf("tensor_map_unmapped_unknown_count: %llu\n",
           profile->unmapped_unknown_count);
    printf("tensor_map_ambiguous_count: %llu\n", profile->ambiguous_count);
    printf("tensor_map_layer_count_observed: %llu\n",
           profile->layer_count_observed);
    printf("tensor_map_embedding_count: %llu\n", profile->embedding_count);
    printf("tensor_map_attention_count: %llu\n", profile->attention_count);
    printf("tensor_map_attention_q_count: %llu\n", profile->attention_q_count);
    printf("tensor_map_attention_k_count: %llu\n", profile->attention_k_count);
    printf("tensor_map_attention_v_count: %llu\n", profile->attention_v_count);
    printf("tensor_map_attention_o_count: %llu\n", profile->attention_o_count);
    printf("tensor_map_mlp_count: %llu\n", profile->mlp_count);
    printf("tensor_map_mlp_gate_count: %llu\n", profile->mlp_gate_count);
    printf("tensor_map_mlp_up_count: %llu\n", profile->mlp_up_count);
    printf("tensor_map_mlp_down_count: %llu\n", profile->mlp_down_count);
    printf("tensor_map_norm_count: %llu\n", profile->norm_count);
    printf("tensor_map_output_head_count: %llu\n", profile->output_head_count);
    printf("tensor_map_moe_router_count: %llu\n", profile->moe_router_count);
    printf("tensor_map_moe_expert_count: %llu\n", profile->moe_expert_count);
    printf("tensor_map_tokenizer_sidecar_status: %s\n",
           profile->tokenizer_present ? "sidecar-observed" : "missing");
    printf("tensor_map_config_sidecar_status: %s\n",
           profile->config_present ? "sidecar-observed" : "missing");
    printf("tensor_map_validation_status: lexical-and-header-only\n");
    printf("tensor_map_canonical_role_status: mapped-candidates\n");
    printf("tensor_map_runtime_role_coverage_status: not-complete\n");
    printf("tensor_map_artifact_contract_status: not-implemented\n");
    printf("tensor_map_runtime_descriptor_status: not-implemented\n");
    printf("tensor_map_graph_consumer_status: not-implemented\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next_required_rows: %s\n", YVEX_TENSOR_NAMING_NEXT_ROW);
    for (i = 0; i < profile->entry_count; ++i) {
        const yvex_tensor_naming_entry *entry = &profile->entries[i];
        printf("tensor_map.entry.%llu.native_name: %s\n", i, entry->native_name);
        printf("tensor_map.entry.%llu.canonical_role: %s\n", i, entry->canonical_role);
        printf("tensor_map.entry.%llu.family: %s\n", i, entry->family);
        printf("tensor_map.entry.%llu.target_id: %s\n", i, entry->target_id);
        printf("tensor_map.entry.%llu.collection: %s\n", i, entry->collection);
        printf("tensor_map.entry.%llu.layer_index: %s\n", i, entry->layer_index);
        printf("tensor_map.entry.%llu.expert_index: %s\n", i, entry->expert_index);
        printf("tensor_map.entry.%llu.dtype: %s\n", i, entry->dtype);
        printf("tensor_map.entry.%llu.rank: %s\n", i, entry->rank);
        printf("tensor_map.entry.%llu.shape: %s\n", i,
               entry->shape[0] ? entry->shape : "unknown");
        printf("tensor_map.entry.%llu.source_file: %s\n", i, entry->source_file);
        printf("tensor_map.entry.%llu.mapping_status: %s\n",
               i, entry->mapping_status);
        printf("tensor_map.entry.%llu.mapping: %s -> %s\n",
               i, entry->native_name, entry->canonical_role);
    }
    printf("boundary: %stensor naming map only; no runtime descriptor/graph/runtime/generation\n",
           tensor_naming_is_dense_family(profile) ? "dense " : "");
}

static void print_tensor_map_audit_hint(const yvex_model_target_record *record)
{
    const yvex_model_class_profile_spec *spec;

    if (!record) return;
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) return;
    printf("tensor_map_status: not-run\n");
    printf("tensor_map_family: %s\n", spec->family_key);
    printf("tensor_map_target_id: %s\n", spec->target_id);
    printf("tensor_map_stage: header-naming-map\n");
    printf("tensor_map_evidence_basis: header-metadata-only\n");
    printf("tensor_map_next: %s\n", YVEX_TENSOR_NAMING_NEXT_ROW);
}

static void print_output_head_map_audit_hint(const yvex_model_target_record *record)
{
    const yvex_model_class_profile_spec *spec;

    if (!record) return;
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) return;
    printf("output_head_map_status: not-run\n");
    printf("output_head_map_family: %s\n", spec->family_key);
    printf("output_head_map_target_id: %s\n", spec->target_id);
    printf("output_head_map_stage: header-output-head-map\n");
    printf("output_head_map_evidence_basis: header-metadata-only\n");
    printf("output_head_map_next: %s\n", YVEX_OUTPUT_HEAD_MAP_NEXT_ROW);
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
            strcmp(name, "output.weight") == 0 ||
            strcmp(name, "model.output.weight") == 0 ||
            strcmp(name, "output_head.weight") == 0 ||
            strcmp(name, "model.output_head.weight") == 0);
}

static int output_head_map_name_is_embedding(const char *name)
{
    return name &&
           (strcmp(name, "model.embed_tokens.weight") == 0 ||
            strcmp(name, "embed_tokens.weight") == 0 ||
            strcmp(name, "token_embd.weight") == 0 ||
            strcmp(name, "tok_embeddings.weight") == 0 ||
            strcmp(name, "embeddings.weight") == 0);
}

static int output_head_map_name_is_final_norm(const char *name)
{
    return name &&
           (strcmp(name, "model.norm.weight") == 0 ||
            strcmp(name, "norm.weight") == 0 ||
            strcmp(name, "final_norm.weight") == 0 ||
            strcmp(name, "model.final_norm.weight") == 0);
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
    if (profile->tensor_count == 0) {
        profile->status = "metadata-missing";
        profile->top_blocker = tensor_naming_header_blocker(spec);
    } else if (profile->output_head_candidate_count == 0) {
        profile->status = "output-head-missing";
        profile->top_blocker = "missing-output-head-tensor";
        profile->output_head_missing_status = "missing";
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
    printf("output-head-map: %s [%s]\n",
           profile->record->target_id,
           compact_status_bracket(profile->status));
    printf("family: %s  evidence: header-only\n", profile->spec->family_key);
    printf("head: %s  final_norm: %s  embedding: %s  tie: %s\n",
           output_head_map_normal_role(&profile->output_head),
           output_head_map_normal_role(&profile->final_norm),
           output_head_map_normal_role(&profile->embedding),
           profile->tie_policy_status);
    printf("shape: %s\n", profile->shape_relation_status);
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next: %s\n", YVEX_OUTPUT_HEAD_MAP_NEXT_ROW);
    printf("boundary: mapping only; no logits/runtime/generation\n");
}

static void print_output_head_map_table(
    const yvex_output_head_map_profile *profile)
{
    printf("OUTPUT HEAD TENSOR MAP\n\n");
    printf("%-6s  %-15s  %-26s  %-4s  %-10s  %-5s  %-34s  %-24s  %s\n",
           "FAMILY", "TARGET", "STATUS", "HEAD", "FINAL_NORM", "EMBED",
           "TIE_POLICY", "SHAPE_RELATION", "NEXT");
    printf("%-6s  %-15s  %-26s  %-4s  %-10s  %-5s  %-34s  %-24s  %s\n",
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
    printf("output_head.entry.%s.native_name: %s\n", name, entry->native_name);
    printf("output_head.entry.%s.canonical_role: %s\n",
           name, entry->canonical_role);
    printf("output_head.entry.%s.mapping_status: %s\n",
           name, entry->mapping_status);
    printf("output_head.entry.%s.dtype: %s\n", name, entry->dtype);
    printf("output_head.entry.%s.rank: %s\n", name, entry->rank);
    printf("output_head.entry.%s.shape: %s\n", name, entry->shape);
    printf("output_head.entry.%s.source_file: %s\n", name, entry->source_file);
}

static void print_output_head_map_audit(
    const yvex_output_head_map_profile *profile)
{
    printf("output_head_map_status: %s\n", profile->status);
    printf("output_head_map_family: %s\n", profile->spec->family_key);
    printf("output_head_map_target_id: %s\n", profile->record->target_id);
    printf("output_head_map_stage: header-output-head-map\n");
    printf("output_head_map_evidence_basis: header-metadata-only\n");
    printf("output_head_map_source_status: %s\n",
           profile->source_exists ? "present" : "missing");
    printf("output_head_map_source_path: %s\n", profile->source_path);
    printf("output_head_map_manifest_status: not-checked\n");
    printf("output_head_map_config_status: %s\n",
           profile->config_present ? "present" : "missing");
    printf("output_head_map_tokenizer_status: %s\n",
           profile->tokenizer_present ? "present" : "missing");
    printf("output_head_native_name: %s\n", profile->output_head.native_name);
    printf("output_head_canonical_role: %s\n",
           profile->output_head.canonical_role);
    printf("output_head_mapping_status: %s\n",
           profile->output_head.mapping_status);
    printf("output_head_candidate_count: %llu\n",
           profile->output_head_candidate_count);
    printf("output_head_ambiguous_count: %llu\n",
           profile->output_head_ambiguous_count);
    printf("output_head_missing_status: %s\n",
           profile->output_head_missing_status);
    printf("output_head_dtype: %s\n", profile->output_head.dtype);
    printf("output_head_rank: %s\n", profile->output_head.rank);
    printf("output_head_shape: %s\n", profile->output_head.shape);
    printf("output_head_vocab_dim_candidate: %s\n",
           profile->output_head.vocab_dim_candidate);
    printf("output_head_hidden_dim_candidate: %s\n",
           profile->output_head.hidden_dim_candidate);
    printf("embedding_native_name: %s\n", profile->embedding.native_name);
    printf("embedding_canonical_role: %s\n", profile->embedding.canonical_role);
    printf("embedding_dtype: %s\n", profile->embedding.dtype);
    printf("embedding_rank: %s\n", profile->embedding.rank);
    printf("embedding_shape: %s\n", profile->embedding.shape);
    printf("final_norm_native_name: %s\n", profile->final_norm.native_name);
    printf("final_norm_canonical_role: %s\n",
           profile->final_norm.canonical_role);
    printf("final_norm_dtype: %s\n", profile->final_norm.dtype);
    printf("final_norm_rank: %s\n", profile->final_norm.rank);
    printf("final_norm_shape: %s\n", profile->final_norm.shape);
    printf("tie_policy_status: %s\n", profile->tie_policy_status);
    printf("config_tie_word_embeddings_status: %s\n",
           profile->config_tie_word_embeddings_status);
    printf("shape_relation_status: %s\n", profile->shape_relation_status);
    printf("output_head_runtime_consumer_status: not-implemented\n");
    printf("output_head_logits_status: not-implemented\n");
    printf("output_head_artifact_contract_status: not-implemented\n");
    printf("output_head_runtime_descriptor_status: not-implemented\n");
    printf("output_head_graph_consumer_status: not-implemented\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next_required_rows: %s\n", YVEX_OUTPUT_HEAD_MAP_NEXT_ROW);
    print_output_head_entry_audit("output", &profile->output_head);
    print_output_head_entry_audit("embedding", &profile->embedding);
    print_output_head_entry_audit("final_norm", &profile->final_norm);
    printf("boundary: output-head tensor mapping only; no logits/runtime/generation\n");
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
    snprintf(profile->bos_token_id, sizeof(profile->bos_token_id), "unknown");
    snprintf(profile->eos_token_id, sizeof(profile->eos_token_id), "unknown");
    snprintf(profile->pad_token_id, sizeof(profile->pad_token_id), "unknown");
    snprintf(profile->unk_token_id, sizeof(profile->unk_token_id), "unknown");
    snprintf(profile->sep_token_id, sizeof(profile->sep_token_id), "unknown");
    profile->additional_special_tokens_status = "missing";
    snprintf(profile->additional_special_tokens_count,
             sizeof(profile->additional_special_tokens_count), "0");
    profile->chat_template_status = "unknown";
    profile->chat_template_present = "unknown";
}

static int build_tokenizer_map_profile(
    const yvex_model_target_record *record,
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
    spec = find_model_class_profile_spec(record->target_id);
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
        if (tokenizer_map_json_has_key(json, "chat_template")) {
            profile->chat_template_status = "present";
            profile->chat_template_present = "true";
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
    }

    tokenizer_map_probe_json_sidecar(&profile->vocab_json, json, sizeof(json));
    tokenizer_map_probe_plain_sidecar(&profile->merges_txt);
    tokenizer_map_probe_plain_sidecar(&profile->tokenizer_model);

    tokenizer_map_choose_vocab(profile);
    tokenizer_map_output_head_relation(profile, models_root_override,
                                       source_override);

    sidecar_count =
        tokenizer_map_sidecar_present(&profile->tokenizer_json) +
        tokenizer_map_sidecar_present(&profile->tokenizer_config) +
        tokenizer_map_sidecar_present(&profile->special_tokens_map) +
        tokenizer_map_sidecar_present(&profile->generation_config) +
        tokenizer_map_sidecar_present(&profile->config_json) +
        tokenizer_map_sidecar_present(&profile->vocab_json) +
        tokenizer_map_sidecar_present(&profile->merges_txt) +
        tokenizer_map_sidecar_present(&profile->tokenizer_model);
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
        profile->status = "tokenizer-metadata-profiled";
        profile->top_blocker = "missing-tokenizer-runtime";
    } else {
        profile->status = "tokenizer-metadata-incomplete";
        profile->top_blocker = "incomplete-tokenizer-metadata";
    }
    return 0;
}

static void print_tokenizer_map_normal(
    const yvex_tokenizer_map_profile *profile)
{
    printf("tokenizer-map: %s\n", profile->spec->family_key);
    printf("target: %s\n", profile->record->target_id);
    printf("status: %s\n", profile->status);
    printf("stage: metadata-tokenizer-map\n");
    printf("evidence: sidecar-metadata-only\n");
    printf("tokenizer: %s\n",
           tokenizer_map_normal_sidecar(&profile->tokenizer_json,
                                        "tokenizer.json present"));
    printf("config: %s\n",
           tokenizer_map_normal_sidecar(&profile->config_json,
                                        "config.json present"));
    printf("vocab_size: %s\n", profile->vocab_size);
    printf("special_tokens: bos=%s eos=%s pad=%s unk=%s\n",
           profile->bos_token_id,
           profile->eos_token_id,
           profile->pad_token_id,
           profile->unk_token_id);
    printf("chat_template: %s\n", profile->chat_template_present);
    printf("output_head_relation: %s\n",
           profile->output_head_vocab_relation_status);
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next: %s\n", YVEX_TOKENIZER_MAP_NEXT_ROW);
    printf("boundary: tokenizer metadata mapping only; no tokenization/runtime/generation\n");
}

static void print_tokenizer_map_table(
    const yvex_tokenizer_map_profile *profile)
{
    printf("TOKENIZER METADATA MAP\n\n");
    printf("%-6s  %-15s  %-27s  %-9s  %-6s  %-7s  %-7s  %-7s  %-13s  %-30s  %s\n",
           "FAMILY", "TARGET", "STATUS", "TOKENIZER", "CONFIG", "VOCAB",
           "EOS", "PAD", "CHAT_TEMPLATE", "HEAD_RELATION", "NEXT");
    printf("%-6s  %-15s  %-27s  %-9s  %-6s  %-7s  %-7s  %-7s  %-13s  %-30s  %s\n",
           profile->spec->family_key,
           profile->record->target_id,
           profile->status,
           tokenizer_map_yes_no(&profile->tokenizer_json),
           tokenizer_map_yes_no(&profile->config_json),
           profile->vocab_size,
           profile->eos_token_id,
           profile->pad_token_id,
           profile->chat_template_present,
           profile->output_head_vocab_relation_status,
           YVEX_TOKENIZER_MAP_NEXT_ROW);
}

static void print_tokenizer_map_audit(
    const yvex_tokenizer_map_profile *profile)
{
    printf("tokenizer_map_status: %s\n", profile->status);
    printf("tokenizer_map_family: %s\n", profile->spec->family_key);
    printf("tokenizer_map_target_id: %s\n", profile->record->target_id);
    printf("tokenizer_map_stage: metadata-tokenizer-map\n");
    printf("tokenizer_map_evidence_basis: sidecar-metadata-only\n");
    printf("tokenizer_map_source_status: %s\n",
           profile->source_exists ? "present" : "missing");
    printf("tokenizer_map_source_path: %s\n", profile->source_path);
    printf("tokenizer_json_status: %s\n", profile->tokenizer_json.status);
    printf("tokenizer_json_path: %s\n", profile->tokenizer_json.path);
    printf("tokenizer_config_status: %s\n", profile->tokenizer_config.status);
    printf("tokenizer_config_path: %s\n", profile->tokenizer_config.path);
    printf("special_tokens_map_status: %s\n",
           profile->special_tokens_map.status);
    printf("special_tokens_map_path: %s\n", profile->special_tokens_map.path);
    printf("generation_config_status: %s\n",
           profile->generation_config.status);
    printf("generation_config_path: %s\n", profile->generation_config.path);
    printf("config_json_status: %s\n", profile->config_json.status);
    printf("config_json_path: %s\n", profile->config_json.path);
    printf("vocab_json_status: %s\n", profile->vocab_json.status);
    printf("merges_txt_status: %s\n", profile->merges_txt.status);
    printf("tokenizer_model_status: %s\n", profile->tokenizer_model.status);
    printf("tokenizer_class: %s\n", profile->tokenizer_class);
    printf("model_type: %s\n", profile->model_type);
    printf("vocab_size_status: %s\n", profile->vocab_size_status);
    printf("vocab_size: %s\n", profile->vocab_size);
    printf("config_vocab_size: %s\n", profile->config_vocab_size);
    printf("tokenizer_vocab_size: %s\n", profile->tokenizer_vocab_size);
    printf("output_head_vocab_dim_candidate: %s\n",
           profile->output_head_vocab_dim_candidate);
    printf("output_head_vocab_relation_status: %s\n",
           profile->output_head_vocab_relation_status);
    printf("bos_token_id_status: %s\n", profile->bos_token_id_status);
    printf("bos_token_id: %s\n", profile->bos_token_id);
    printf("eos_token_id_status: %s\n", profile->eos_token_id_status);
    printf("eos_token_id: %s\n", profile->eos_token_id);
    printf("pad_token_id_status: %s\n", profile->pad_token_id_status);
    printf("pad_token_id: %s\n", profile->pad_token_id);
    printf("unk_token_id_status: %s\n", profile->unk_token_id_status);
    printf("unk_token_id: %s\n", profile->unk_token_id);
    printf("sep_token_id_status: %s\n", profile->sep_token_id_status);
    printf("sep_token_id: %s\n", profile->sep_token_id);
    printf("additional_special_tokens_status: %s\n",
           profile->additional_special_tokens_status);
    printf("additional_special_tokens_count: %s\n",
           profile->additional_special_tokens_count);
    printf("chat_template_status: %s\n", profile->chat_template_status);
    printf("chat_template_present: %s\n", profile->chat_template_present);
    printf("chat_template_runtime_status: not-implemented\n");
    printf("tokenizer_runtime_status: not-implemented\n");
    printf("tokenization_status: not-implemented\n");
    printf("detokenization_status: not-implemented\n");
    printf("eos_stop_policy_status: not-implemented\n");
    printf("stop_token_policy_status: not-implemented\n");
    printf("prompt_template_runtime_status: not-implemented\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next_required_rows: %s\n", YVEX_TOKENIZER_MAP_NEXT_ROW);
    printf("boundary: tokenizer metadata mapping only; no tokenization/runtime/generation\n");
}

static void print_tokenizer_map_audit_hint(const yvex_model_target_record *record)
{
    const yvex_model_class_profile_spec *spec;

    if (!record) return;
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) return;
    printf("tokenizer_map_status: not-run\n");
    printf("tokenizer_map_family: %s\n", spec->family_key);
    printf("tokenizer_map_target_id: %s\n", spec->target_id);
    printf("tokenizer_map_stage: metadata-tokenizer-map\n");
    printf("tokenizer_map_evidence_basis: sidecar-metadata-only\n");
    printf("tokenizer_runtime_status: not-implemented\n");
    printf("tokenizer_map_next: %s\n", YVEX_TOKENIZER_MAP_NEXT_ROW);
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
    profile->spec = find_model_class_profile_spec(record->target_id);
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
    rc = build_tokenizer_map_profile(record, models_root_override,
                                     source_override, &tokenizer_profile);
    if (rc != 0) return rc;

    snprintf(profile->source_path, sizeof(profile->source_path), "%s",
             naming_profile.source_path);
    snprintf(profile->source_path_source, sizeof(profile->source_path_source),
             "%s", naming_profile.source_path_source);
    profile->source_exists = naming_profile.source_exists;

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
            output_profile.output_head_candidate_count > 1
                ? "ambiguous"
                : missing_role_many_status(output_profile.output_head_candidate_count);
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
    missing_role_add_entry(profile, "output_head", profile->output_head_status,
                           strcmp(profile->output_head_status, "ambiguous") == 0
                               ? "source-role-ambiguous" : "source-role-missing");
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
    printf("%s: ", label);
    for (i = 0; i < profile->missing_entry_count; ++i) {
        const yvex_missing_role_entry *entry = &profile->missing_entries[i];
        int is_metadata = strstr(entry->blocker_class, "metadata") != NULL;
        if (metadata != is_metadata || strcmp(entry->status, "missing") != 0) {
            continue;
        }
        printf("%s%s", first ? "" : ",", entry->name);
        first = 0;
    }
    if (first) printf("none");
    printf("\n");
}

static void print_missing_role_report_normal(
    const yvex_missing_role_report_profile *profile)
{
    printf("missing-roles: %s [%s]\n",
           profile->record->target_id,
           compact_status_bracket(profile->status));
    printf("family: %s  evidence: header+sidecar-only\n",
           profile->spec->family_key);
    printf("source_roles: %llu/12 present, %llu missing, %llu ambiguous\n",
           profile->source_role_observed_count,
           profile->source_role_missing_count,
           profile->source_role_ambiguous_count);
    printf("metadata_roles: %llu/4 present, %llu missing, %llu ambiguous\n",
           profile->metadata_observed_count,
           profile->metadata_missing_count,
           profile->metadata_ambiguous_count);
    if (profile->source_role_missing_count > 0) {
        print_missing_role_list("missing_source", profile, 0);
    }
    if (profile->metadata_missing_count > 0) {
        print_missing_role_list("missing_metadata", profile, 1);
    }
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next: %s\n", YVEX_MISSING_ROLE_REPORT_NEXT_ROW);
    printf("boundary: report-only; use --audit for role details\n");
}

static void print_missing_role_report_table(
    const yvex_missing_role_report_profile *profile)
{
    printf("MISSING ROLE BLOCKER REPORT\n\n");
    printf("%-6s  %-15s  %-29s  %7s  %8s  %9s  %8s  %9s  %-30s  %s\n",
           "FAMILY", "TARGET", "STATUS", "OBS_SRC", "MISS_SRC",
           "AMBIG_SRC", "OBS_META", "MISS_META", "TOP_BLOCKER", "NEXT");
    printf("%-6s  %-15s  %-29s  %7llu  %8llu  %9llu  %8llu  %9llu  %-30s  %s\n",
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

    printf("missing_role_report_status: %s\n", profile->status);
    printf("missing_role_report_family: %s\n", profile->spec->family_key);
    printf("missing_role_report_target_id: %s\n", profile->record->target_id);
    printf("missing_role_report_stage: missing-role-blocker-report\n");
    printf("missing_role_report_evidence_basis: header-and-sidecar-metadata-only\n");
    printf("missing_role_report_source_status: %s\n",
           profile->source_exists ? "present" : "missing");
    printf("missing_role_report_source_path: %s\n", profile->source_path);
    printf("missing_role_source_role_required_count: 12\n");
    printf("missing_role_source_role_observed_count: %llu\n",
           profile->source_role_observed_count);
    printf("missing_role_source_role_missing_count: %llu\n",
           profile->source_role_missing_count);
    printf("missing_role_source_role_ambiguous_count: %llu\n",
           profile->source_role_ambiguous_count);
    printf("missing_role_metadata_required_count: 4\n");
    printf("missing_role_metadata_observed_count: %llu\n",
           profile->metadata_observed_count);
    printf("missing_role_metadata_missing_count: %llu\n",
           profile->metadata_missing_count);
    printf("missing_role_metadata_ambiguous_count: %llu\n",
           profile->metadata_ambiguous_count);
    printf("missing_role_embedding_status: %s\n", profile->embedding_status);
    printf("missing_role_attention_norm_status: %s\n",
           profile->attention_norm_status);
    printf("missing_role_attention_q_status: %s\n",
           profile->attention_q_status);
    printf("missing_role_attention_k_status: %s\n",
           profile->attention_k_status);
    printf("missing_role_attention_v_status: %s\n",
           profile->attention_v_status);
    printf("missing_role_attention_o_status: %s\n",
           profile->attention_o_status);
    printf("missing_role_mlp_norm_status: %s\n", profile->mlp_norm_status);
    printf("missing_role_mlp_gate_status: %s\n", profile->mlp_gate_status);
    printf("missing_role_mlp_up_status: %s\n", profile->mlp_up_status);
    printf("missing_role_mlp_down_status: %s\n", profile->mlp_down_status);
    printf("missing_role_final_norm_status: %s\n", profile->final_norm_status);
    printf("missing_role_output_head_status: %s\n",
           profile->output_head_status);
    printf("missing_role_tokenizer_metadata_status: %s\n",
           profile->tokenizer_metadata_status);
    printf("missing_role_config_metadata_status: %s\n",
           profile->config_metadata_status);
    printf("missing_role_generation_metadata_status: %s\n",
           profile->generation_metadata_status);
    printf("missing_role_special_tokens_status: %s\n",
           profile->special_tokens_status);
    printf("missing_role_artifact_contract_status: missing\n");
    printf("missing_role_runtime_descriptor_status: missing\n");
    printf("missing_role_graph_consumer_status: missing\n");
    printf("missing_role_backend_residency_status: missing\n");
    printf("missing_role_attention_runtime_status: missing\n");
    printf("missing_role_kv_runtime_state_status: missing\n");
    printf("missing_role_prefill_runtime_status: missing\n");
    printf("missing_role_decode_runtime_status: missing\n");
    printf("missing_role_logits_runtime_status: missing\n");
    printf("missing_role_tokenizer_runtime_status: missing\n");
    printf("missing_role_sampling_runtime_status: missing\n");
    printf("missing_role_generation_runtime_status: missing\n");
    printf("missing_role_eval_benchmark_status: missing\n");
    printf("missing_role_top_blocker: %s\n", profile->top_blocker);
    printf("missing_role_next_required_row: %s\n",
           YVEX_MISSING_ROLE_REPORT_NEXT_ROW);
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
    for (i = 0; i < profile->missing_entry_count; ++i) {
        const yvex_missing_role_entry *entry = &profile->missing_entries[i];
        printf("missing_role.entry.%llu.role: %s\n", i, entry->name);
        printf("missing_role.entry.%llu.blocker_class: %s\n",
               i, entry->blocker_class);
        printf("missing_role.entry.%llu.status: %s\n", i, entry->status);
    }
    printf("boundary: missing-role report only; no artifact/runtime descriptor/graph/runtime/generation\n");
}

static void print_missing_role_report_audit_hint(
    const yvex_model_target_record *record)
{
    const yvex_model_class_profile_spec *spec;

    if (!record) return;
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) return;
    printf("missing_role_report_status: not-run\n");
    printf("missing_role_report_family: %s\n", spec->family_key);
    printf("missing_role_report_target_id: %s\n", spec->target_id);
    printf("missing_role_report_stage: missing-role-blocker-report\n");
    printf("missing_role_top_blocker: not-run\n");
    printf("missing_role_next_required_row: %s\n",
           YVEX_MISSING_ROLE_REPORT_NEXT_ROW);
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
           strcmp(profile->status, "tokenizer-metadata-profiled") == 0;
}

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
    rc = build_tokenizer_map_profile(record, models_root_override,
                                     source_override, &profile->tokenizer);
    if (rc != 0) return rc;
    rc = build_missing_role_report_profile(record, models_root_override,
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
    printf("boundary: V010.MAP.9 is a report-only tensor mapping gate. It does not load tensor payloads, emit artifacts, complete quantization/artifact contract, construct runtime descriptors, attach backend residency, feed graph consumers, execute prefill/decode/logits/tokenizer/sampling/generation, evaluate, benchmark, claim throughput, or mark v0.1.0 release-ready.\n");
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

    printf("tensor-mapping-gate: %s [%s]\n",
           profile->record->target_id,
           compact_status_bracket(profile->status));
    printf("gate: v0.1.0  family: %s\n", profile->spec->family_key);
    printf("roles: source %llu/12, metadata %llu/4, missing %llu, ambiguous %llu\n",
           profile->missing_role.source_role_observed_count,
           profile->missing_role.metadata_observed_count,
           missing_count,
           ambiguous_count);
    if (strcmp(profile->missing_roles, "none") != 0) {
        printf("missing: %s\n", profile->missing_roles);
    }
    if (strcmp(profile->ambiguous_roles, "none") != 0) {
        printf("ambiguous: %s\n", profile->ambiguous_roles);
    }
    printf("result: %s\n", profile->gate_result);
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next: %s\n", profile->next_required_row);
    printf("boundary: report-only; no artifact/runtime/generation\n");
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

    printf("TENSOR MAPPING GATE\n\n");
    printf("%-15s  %-6s  %-8s  %-12s  %-10s  %7s  %5s  %-34s  %-30s  %s\n",
           "TARGET", "FAMILY", "GATE", "SOURCE_ROLES", "META_ROLES",
           "MISSING", "AMBIG", "TOP_BLOCKER", "STATUS", "NEXT");
    printf("%-15s  %-6s  %-8s  %2llu/12         %2llu/4       %7llu  %5llu  %-34s  %-30s  %s\n",
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
    printf("tensor_mapping_gate_status: %s\n", profile->status);
    printf("tensor_mapping_gate: v0.1.0-tensor-mapping\n");
    printf("tensor_mapping_gate_result: %s\n", profile->gate_result);
    printf("tensor_mapping_gate_target_id: %s\n", profile->record->target_id);
    printf("tensor_mapping_gate_family: %s\n", profile->spec->family_key);
    printf("tensor_mapping_gate_model_class: %s\n", profile->spec->class_name);
    printf("tensor_mapping_gate_source_class: %s\n",
           profile->record->source_artifact_class);
    printf("tensor_mapping_gate_source_status: %s\n",
           tensor_mapping_gate_source_ready(profile) ? "present" : "missing");
    printf("tensor_mapping_gate_source_path: %s\n",
           profile->missing_role.source_path);
    printf("tensor_mapping_gate_source_path_source: %s\n",
           profile->missing_role.source_path_source);
    printf("tensor_mapping_gate_source_report_path: not-written\n");
    printf("tensor_mapping_gate_tensor_map_sidecar_path: not-written\n");
    printf("tensor_mapping_gate_output_head_map_sidecar_path: not-written\n");
    printf("tensor_mapping_gate_tokenizer_metadata_source_path: %s\n",
           profile->tokenizer.source_path);
    printf("tensor_mapping_gate_missing_role_report_sidecar_path: not-written\n");
    printf("map_source.0: source-tensor-metadata-inventory\n");
    printf("map_source.1: model-class-profile\n");
    printf("map_source.2: tensor-collection-inventory\n");
    printf("map_source.3: tensor-naming-map\n");
    printf("map_source.4: output-head-tensor-map\n");
    printf("map_source.5: tokenizer-metadata-map\n");
    printf("map_source.6: missing-role-blocker-report\n");
    printf("model_class_profile_status: %s\n", profile->model_class.status);
    printf("tensor_collection_status: %s\n", profile->tensor_collection.status);
    printf("tensor_naming_map_status: %s\n", profile->tensor_naming.status);
    printf("output_head_map_status: %s\n", profile->output_head.status);
    printf("tokenizer_metadata_map_status: %s\n", profile->tokenizer.status);
    printf("missing_role_report_status: %s\n", profile->missing_role.status);
    printf("expected_source_role_count: 12\n");
    printf("observed_source_role_count: %llu\n",
           profile->missing_role.source_role_observed_count);
    printf("missing_source_role_count: %llu\n",
           profile->missing_role.source_role_missing_count);
    printf("ambiguous_source_role_count: %llu\n",
           profile->missing_role.source_role_ambiguous_count);
    printf("expected_metadata_role_count: 4\n");
    printf("observed_metadata_role_count: %llu\n",
           profile->missing_role.metadata_observed_count);
    printf("missing_metadata_role_count: %llu\n",
           profile->missing_role.metadata_missing_count);
    printf("ambiguous_metadata_role_count: %llu\n",
           profile->missing_role.metadata_ambiguous_count);
    printf("missing_source_roles: %s\n", profile->missing_source_roles);
    printf("missing_metadata_roles: %s\n", profile->missing_metadata_roles);
    printf("missing_roles: %s\n", profile->missing_roles);
    printf("ambiguous_roles: %s\n", profile->ambiguous_roles);
    printf("downstream_blockers: artifact_contract=missing qtype_policy=missing runtime_descriptor=missing graph_consumer=missing backend_residency=missing logits_runtime=missing tokenizer_runtime=missing generation_runtime=missing eval_benchmark=missing\n");
    printf("artifact_contract_status: missing\n");
    printf("qtype_policy_status: missing\n");
    printf("runtime_descriptor_status: missing\n");
    printf("graph_consumer_status: missing\n");
    printf("backend_residency_status: missing\n");
    printf("logits_runtime_status: missing\n");
    printf("tokenizer_runtime_status: missing\n");
    printf("generation_runtime_status: missing\n");
    printf("eval_benchmark_status: missing\n");
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next_required_rows: %s\n", profile->next_required_row);
    printf("payload_bytes_read: false\n");
    printf("artifact_emitted: false\n");
    printf("runtime_descriptor_constructed: false\n");
    printf("graph_consumer_fed: false\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
    print_tensor_mapping_gate_boundary();
}

static void print_tensor_mapping_gate_audit_hint(
    const yvex_model_target_record *record)
{
    const yvex_model_class_profile_spec *spec;

    if (!record) return;
    spec = find_model_class_profile_spec(record->target_id);
    if (!spec) return;
    printf("tensor_mapping_gate_status: not-run\n");
    printf("tensor_mapping_gate: v0.1.0-tensor-mapping\n");
    printf("tensor_mapping_gate_family: %s\n", spec->family_key);
    printf("tensor_mapping_gate_target_id: %s\n", spec->target_id);
    printf("tensor_mapping_gate_next_required_row: %s\n",
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
    printf("boundary: report-only; no quantization/artifact/runtime\n");
}

static void print_qtype_policy_normal(
    const yvex_qtype_policy_profile *profile)
{
    printf("qtype-policy: %s [%s]\n",
           profile->target_id,
           compact_status_bracket(profile->status));
    printf("family: %s  mapping_gate: %s\n",
           profile->family,
           profile->mapping_gate_status);
    printf("source_dtype: %s\n", profile->dtype_profile);
    printf("policy: %s\n", profile->qtype_policy);
    if (strcmp(profile->preferred_qtype, "none") != 0) {
        printf("preferred: %s\n", profile->preferred_qtype);
    }
    if (strcmp(profile->candidate_qtypes, "none") != 0) {
        printf("candidates: %s\n", profile->candidate_qtypes);
    }
    if (strcmp(profile->refused_qtypes, "none") != 0) {
        printf("refused: %s\n", profile->refused_qtypes);
    }
    printf("top_blocker: %s\n", profile->top_blocker);
    printf("next: %s\n", profile->next_required_row);
    print_qtype_policy_boundary();
}

static void print_qtype_policy_table(
    const yvex_qtype_policy_profile *profile)
{
    printf("QTYPE POLICY\n\n");
    printf("%-16s  %-6s  %-28s  %-32s  %-9s  %-14s  %-25s  %-30s  %s\n",
           "TARGET", "FAMILY", "SOURCE_DTYPE", "POLICY", "PREFERRED",
           "CANDIDATES", "REFUSED", "STATUS", "NEXT");
    printf("%-16s  %-6s  %-28s  %-32s  %-9s  %-14s  %-25s  %-30s  %s\n",
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
    printf("report: qtype-policy\n");
    printf("status: %s\n", profile->status);
    printf("target_id: %s\n", profile->target_id);
    printf("family: %s\n", profile->family);
    printf("target_class: %s\n",
           profile->record ? profile->record->target_class : "unknown");
    printf("model_class: %s\n", profile->model_class);
    printf("source_path: %s\n", profile->source_path);
    printf("source_class: %s\n", profile->source_class);
    printf("target_artifact_class: %s\n", profile->target_artifact_class);
    printf("source_metadata_status: %s\n",
           profile->gate.model_class.source_metadata_status
               ? profile->gate.model_class.source_metadata_status
               : "not-run");
    printf("source_dtype_profile_status: %s\n",
           profile->source_dtype_profile_status);
    printf("source_dtype_counts: %s\n", profile->dtype_counts);
    printf("source_tensor_count: %llu\n", profile->source_tensor_count);
    printf("source_declared_data_bytes: %llu\n",
           profile->source_declared_data_bytes);
    printf("mapping_gate_status: %s\n", profile->mapping_gate_status);
    printf("mapping_gate_report_path: not-written\n");
    printf("tensor_map_status: %s\n",
           profile->gate.tensor_naming.status
               ? profile->gate.tensor_naming.status
               : "not-run");
    printf("output_head_map_status: %s\n",
           profile->gate.output_head.status
               ? profile->gate.output_head.status
               : "not-run");
    printf("tokenizer_metadata_map_status: %s\n",
           profile->gate.tokenizer.status
               ? profile->gate.tokenizer.status
               : "not-run");
    printf("missing_role_report_status: %s\n",
           profile->gate.missing_role.status
               ? profile->gate.missing_role.status
               : "not-run");
    printf("qtype_policy_basis: %s\n", profile->policy_basis);
    printf("qtype_policy_status: %s\n", profile->qtype_policy_status);
    printf("preferred_qtype: %s\n", profile->preferred_qtype);
    printf("candidate_qtypes: %s\n", profile->candidate_qtypes);
    printf("refused_qtypes: %s\n", profile->refused_qtypes);
    printf("refusal_reasons: %s\n", profile->refusal_reasons);
    printf("per_role_qtype_status: %s\n", profile->per_role_qtype_status);
    printf("compute_support_status: %s\n", profile->compute_support_status);
    printf("calibration_status: %s\n", profile->calibration_status);
    printf("imatrix_status: %s\n", profile->imatrix_status);
    printf("artifact_emit_status: %s\n", profile->artifact_emit_status);
    printf("artifact_identity_status: missing\n");
    printf("materialization_status: unsupported\n");
    printf("runtime_descriptor_status: missing\n");
    printf("graph_consumer_status: missing\n");
    printf("backend_residency_status: missing\n");
    printf("downstream_blockers: per_role_qtype=deferred compute_refusal_matrix=deferred calibration_imatrix=deferred artifact_emit=missing artifact_identity=missing runtime_descriptor=missing graph_consumer=missing backend_residency=missing generation_runtime=missing eval_benchmark=missing\n");
    printf("next_required_rows: %s\n", profile->next_required_row);
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
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

    printf("report: model-target candidate\n");
    printf("status: blocked-no-candidate\n");
    printf("release: %s\n", release);
    printf("selected: none\n");
    printf("candidates: 0 eligible / %lu known (%lu pressure, %lu fixture)\n",
           candidate_count, pressure_count, fixture_count);
    printf("top_blocker: no eligible full-runtime candidate\n");
    printf("next: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    printf("boundary: report-only; generation unsupported; benchmark not measured\n");
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
        printf("dense_candidate_%lu_next_required_rows: V010.TARGET.7,V010.MAP.*\n", index);
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

    printf("report: model-target dense-candidate\n");
    printf("status: %s\n", eligible_count ? "dense-candidate-found" : "dense-candidate-missing");
    printf("release: %s\n", release);
    printf("selected: none\n");
    printf("candidates: %lu eligible / %lu known (%lu dense pressure)\n",
           eligible_count, dense_candidate_count, dense_pressure_count);
    printf("top_blocker: no selected dense full-runtime candidate\n");
    printf("next: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    printf("boundary: report-only; generation unsupported; benchmark not measured\n");
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
    printf("qwen_candidate_%lu_source_target_status: %s\n", index, fact->source_target_status);
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
    printf("target_class: source-model-candidate\n");
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
    printf("target_class: source-model-candidate\n");
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
    printf("target_class: source-model-candidate\n");
    printf("stage: report-only\n");
    printf("eligibility: pressure-target-only\n");
    printf("candidate_id: qwen3-8b\n");
    printf("candidate_stage: source-target-profiled\n");
    printf("candidate_eligibility: pressure-target-only\n");
    printf("source_target_status: profiled\n");
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
        printf("source_target_status: profiled\n");
        printf("source_manifest_status: missing\n");
        printf("native_tensor_inventory_status: missing\n");
        printf("source_config_status: missing\n");
        printf("model_class_profile_status: command-visible\n");
        printf("model_class_role_mapping_status: not-implemented\n");
    }
    if (include_blockers) {
        for (i = 0; i < qwen_metal_blocker_count; ++i) {
            printf("blocker_%lu: %s\n", i, qwen_metal_blockers[i]);
        }
    }
    if (include_next) {
        printf("next_required_rows: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
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

    printf("report: model-target qwen-metal\n");
    printf("status: pressure-target-only\n");
    printf("release: %s\n", release);
    printf("lane: qwen-metal / apple-silicon-metal\n");
    printf("target: qwen3-8b\n");
    printf("candidate: source-target-profiled pressure-target-only\n");
    printf("source_target: profiled\n");
    printf("source: missing\n");
    printf("backend: metal unsupported\n");
    printf("next: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    printf("boundary: report-only; generation unsupported; benchmark not measured\n");
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

    printf("report: target-decision\n");
    printf("status: %s\n", selected ? "target-decision-selected" : "target-decision-blocked");
    printf("release: %s\n", release);
    printf("selected: %s\n", selected ? selected->target_id : "none");
    printf("eligible: %lu / %lu candidates (%lu ineligible)\n",
           eligible_count, candidate_count, ineligible_count);
    printf("top_blocker: %s\n", selected ? "none" : "no eligible full-runtime candidate");
    printf("next: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
    printf("boundary: report-only; generation unsupported; benchmark not measured\n");
    return 0;
}

static void print_model_target_usage(FILE *fp)
{
    fprintf(fp, "usage: yvex model-target classes\n");
    fprintf(fp, "       yvex model-target list [--audit | --output normal|table|audit]\n");
    fprintf(fp, "       yvex model-target candidate --release v0.1.0 [options]\n");
    fprintf(fp, "       yvex model-target dense-candidate --release v0.1.0 [options]\n");
    fprintf(fp, "       yvex model-target qwen-metal --release v0.1.0 [options]\n");
    fprintf(fp, "       yvex model-target decision --release v0.1.0 [options]\n");
    fprintf(fp, "       yvex model-target class-profile TARGET [--models-root DIR] [--source DIR] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "       yvex model-target tensor-collection TARGET [--models-root DIR] [--source DIR] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "       yvex model-target tensor-map TARGET [--role output-head|tokenizer|missing-roles | --gate v0.1.0] [--models-root DIR] [--source DIR] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "       yvex model-target quant-policy TARGET [--models-root DIR] [--source DIR] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "       yvex model-target inspect TARGET [--paths] [--models-root DIR] [--audit | --output normal|table|audit]\n");
}

void yvex_model_target_help(FILE *fp)
{
    print_model_target_usage(fp);
    fprintf(fp, "\n--paths           show expected operator-local source, artifact, report, reference, and registry paths\n");
    fprintf(fp, "--models-root DIR override configured operator model root for this command only\n");
    fprintf(fp, "--audit | --output normal|table|audit\n");
    fprintf(fp, "\nDecision report:\n");
    fprintf(fp, "  yvex model-target decision --release v0.1.0 --include-candidates --include-blockers --include-next\n");
    fprintf(fp, "  This command records the v0.1.0 target decision. It does not download models, emit artifacts, materialize tensors, execute graph work, run prefill, decode, logits, sampling, generation, evaluation, or benchmarks.\n");
    fprintf(fp, "  Default output is compact. Use --audit or --output audit for full row-promotion fields.\n");
    fprintf(fp, "\nCandidate report:\n");
    fprintf(fp, "  yvex model-target candidate --release v0.1.0 --include-candidates --include-blockers --include-next\n");
    fprintf(fp, "  The candidate report evaluates full-runtime target eligibility for a release. It does not select a ready model, download weights, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "  Default output is compact. Use --audit or --output audit for candidate lists, blockers, and next-row evidence.\n");
    fprintf(fp, "\nDense candidate report:\n");
    fprintf(fp, "  yvex model-target dense-candidate --release v0.1.0 --include-candidates --include-requirements --include-blockers --include-next\n");
    fprintf(fp, "  The dense-candidate report evaluates whether a dense model target can become the first v0.1.0 full-runtime candidate. It does not download weights, emit artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "  Default output is compact. Use --audit or --output audit for requirements and blocker detail.\n");
    fprintf(fp, "\nQwen/Metal pressure report:\n");
    fprintf(fp, "  yvex model-target qwen-metal --release v0.1.0 --include-candidates --include-hardware --include-backend --include-source --include-blockers --include-next\n");
    fprintf(fp, "  The Qwen/Metal pressure report records a planned reduced-scale Apple Silicon / Metal lane for future full-runtime work. It does not download weights, implement Metal, emit Qwen artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "  Default output is compact. Use --audit or --output audit for hardware, backend, source, and blocker detail.\n");
    fprintf(fp, "\nModel-class profile:\n");
    fprintf(fp, "  yvex model-target class-profile qwen3-8b --audit\n");
    fprintf(fp, "  yvex model-target class-profile gemma-4-12b-it --audit\n");
    fprintf(fp, "  The class-profile report reads safetensors headers only and counts lexical tensor-name patterns. It does not map tensor roles, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "\nTensor collection inventory:\n");
    fprintf(fp, "  yvex model-target tensor-collection qwen3-8b --audit\n");
    fprintf(fp, "  yvex model-target tensor-collection gemma-4-12b-it --audit\n");
    fprintf(fp, "  The tensor collection inventory reads safetensors headers only and groups lexical tensor candidates for the selected source target. It does not map runtime roles, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "\nTensor naming map:\n");
    fprintf(fp, "  yvex model-target tensor-map qwen3-8b --audit\n");
    fprintf(fp, "  yvex model-target tensor-map gemma-4-12b-it --audit\n");
    fprintf(fp, "  The tensor naming map reads safetensors headers only and assigns native source tensor names to canonical YVEX role labels. It does not complete runtime role coverage, build runtime descriptors, emit artifacts, materialize tensors, feed graph consumers, execute the model, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "\nOutput-head tensor map:\n");
    fprintf(fp, "  yvex model-target tensor-map qwen3-8b --role output-head --audit\n");
    fprintf(fp, "  yvex model-target tensor-map gemma-4-12b-it --role output-head --audit\n");
    fprintf(fp, "  The output-head tensor map reads safetensors headers only and identifies output-head, final-norm, and embedding candidates. It does not compute logits, complete runtime descriptors, feed graph consumers, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "\nTokenizer metadata map:\n");
    fprintf(fp, "  yvex model-target tensor-map qwen3-8b --role tokenizer --audit\n");
    fprintf(fp, "  yvex model-target tensor-map gemma-4-12b-it --role tokenizer --audit\n");
    fprintf(fp, "  yvex model-target tensor-map qwen3-8b --role missing-roles --audit\n");
    fprintf(fp, "  yvex model-target tensor-map gemma-4-12b-it --role missing-roles --audit\n");
    fprintf(fp, "  The tokenizer metadata map reads local sidecars only and reports tokenizer/config/special-token metadata candidates. It does not tokenize, detokenize, apply chat templates, stop on EOS, compute logits, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "\nTensor mapping gate:\n");
    fprintf(fp, "  yvex model-target tensor-map qwen3-8b --gate v0.1.0 --audit\n");
    fprintf(fp, "  yvex model-target tensor-map gemma-4-12b-it --gate v0.1.0 --audit\n");
    fprintf(fp, "  The tensor mapping gate aggregates model-class, tensor-collection, tensor naming, output-head, tokenizer metadata, and missing-role reports. It can pass only into artifact/quantization planning; it does not emit artifacts, construct runtime descriptors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "\nQtype policy report:\n");
    fprintf(fp, "  yvex model-target quant-policy qwen3-8b --audit\n");
    fprintf(fp, "  yvex model-target quant-policy gemma-4-12b-it --audit\n");
    fprintf(fp, "  The qtype policy report consumes source/header/mapping evidence and existing YVEX qtype policy rows for artifact planning. It does not load tensor payloads, quantize tensors, emit GGUF, materialize tensors, construct runtime descriptors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
    fprintf(fp, "\nDefault output is compact. Use --audit for full diagnostic fields.\n");
    fprintf(fp, "Model targets are pressure objects, not capability claims.\n");
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

static void print_model_target_list(void)
{
    unsigned long i;

    printf("status: model-target-list\n");
    for (i = 0; i < model_target_count; ++i) {
        const yvex_model_target_record *record = &model_targets[i];
        printf("target: %s\n", record->target_id);
        printf("family: %s\n", record->family);
        printf("target_class: %s\n", record->target_class);
        printf("source_artifact_class: %s\n", record->source_artifact_class);
        printf("source_artifact_status: %s\n",
               model_target_source_artifact_status(record));
        printf("target_artifact_class: %s\n", record->target_artifact_class);
        printf("target_artifact_status: %s\n",
               model_target_target_artifact_status(record));
        printf("source_artifact_origin: %s\n",
               model_target_source_artifact_origin(record));
        printf("target_artifact_origin: %s\n",
               model_target_target_artifact_origin(record));
        printf("source_provenance_status: %s\n",
               model_target_source_provenance_status(record));
        printf("source_origin: %s\n", model_target_source_origin(record));
        printf("source_authority: %s\n", model_target_source_authority(record));
        printf("source_revision_status: %s\n",
               model_target_source_revision_status(record));
        printf("source_identity_status: %s\n",
               model_target_source_identity_status(record));
        printf("source_hash_status: %s\n", model_target_source_hash_status(record));
        printf("source_verification_status: %s\n",
               model_target_source_verification_status(record));
        printf("native_inventory_status: %s\n",
               model_target_native_inventory_status(record));
        printf("native_tensor_count: 0\n");
        printf("native_safetensors_payload_loaded: false\n");
        printf("source_tensor_metadata_status: %s\n",
               model_target_source_tensor_metadata_status(record));
        printf("source_tensor_count: 0\n");
        printf("source_tensor_metadata_payload_loaded: false\n");
        printf("source_tensor_metadata_payload_bytes_read: 0\n");
        print_model_class_audit_hint(record);
        print_tensor_collection_audit_hint(record);
        print_tensor_map_audit_hint(record);
        print_output_head_map_audit_hint(record);
        print_tokenizer_map_audit_hint(record);
        print_missing_role_report_audit_hint(record);
        print_tensor_mapping_gate_audit_hint(record);
        printf("runtime_shape: %s\n", model_target_runtime_shape(record));
        printf("backend_selection: %s\n", model_target_backend_selection(record));
        printf("backend_pressure: %s\n", model_target_backend_pressure(record));
        printf("runtime_execution: %s\n", record->runtime_execution);
        printf("runtime_claim: unsupported\n");
        printf("generation: %s\n", record->generation);
        printf("benchmark_status: not-measured\n");
        printf("release_ready: false\n");
        if (i + 1 < model_target_count) {
            printf("\n");
        }
    }
}

static void print_model_target_list_table(void)
{
    unsigned long i;

    printf("MODEL TARGETS  count=%lu\n\n", model_target_count);
    printf("%-43s  %-8s  %-40s  %-11s  %s\n",
           "TARGET",
           "FAMILY",
           "CLASS",
           "RUNTIME",
           "GENERATION");
    for (i = 0; i < model_target_count; ++i) {
        const yvex_model_target_record *record = &model_targets[i];
        printf("%-43s  %-8s  %-40s  %-11s  %s\n",
               record->target_id,
               record->family,
               record->target_class,
               record->runtime_execution,
               record->generation);
    }
    printf("status: model-target-list\n");
}

static void print_model_target_record(const yvex_model_target_record *record)
{
    printf("status: model-target\n");
    printf("target_id: %s\n", record->target_id);
    printf("family: %s\n", record->family);
    printf("model: %s\n", record->model);
    printf("target_class: %s\n", record->target_class);
    printf("runtime_shape: %s\n", model_target_runtime_shape(record));
    printf("backend_selection: %s\n", model_target_backend_selection(record));
    printf("backend_pressure: %s\n", model_target_backend_pressure(record));
    printf("source_artifact_class: %s\n", record->source_artifact_class);
    printf("source_artifact_status: %s\n", model_target_source_artifact_status(record));
    printf("source_artifact_format: %s\n", model_target_source_artifact_format(record));
    printf("source_artifact_origin: %s\n", model_target_source_artifact_origin(record));
    printf("source_artifact_authority: %s\n",
           model_target_source_artifact_authority(record));
    printf("source_sidecar_status: %s\n", model_target_source_sidecar_status(record));
    printf("source_tensor_container: %s\n", model_target_source_tensor_container(record));
    printf("source_tensor_payload_status: %s\n",
           model_target_source_tensor_payload_status(record));
    printf("source_provenance_status: %s\n",
           model_target_source_provenance_status(record));
    printf("source_origin: %s\n", model_target_source_origin(record));
    printf("source_authority: %s\n", model_target_source_authority(record));
    printf("source_revision_status: %s\n",
           model_target_source_revision_status(record));
    printf("source_identity_status: %s\n",
           model_target_source_identity_status(record));
    printf("source_hash_status: %s\n", model_target_source_hash_status(record));
    printf("source_verification_status: %s\n",
           model_target_source_verification_status(record));
    printf("native_inventory_status: %s\n",
           model_target_native_inventory_status(record));
    printf("native_tensor_count: 0\n");
    printf("native_safetensors_payload_loaded: false\n");
    printf("source_tensor_metadata_status: %s\n",
           model_target_source_tensor_metadata_status(record));
    printf("source_tensor_count: 0\n");
    printf("source_tensor_metadata_payload_loaded: false\n");
    printf("source_tensor_metadata_payload_bytes_read: 0\n");
    print_model_class_audit_hint(record);
    print_tensor_collection_audit_hint(record);
    print_tensor_map_audit_hint(record);
    print_output_head_map_audit_hint(record);
    print_tokenizer_map_audit_hint(record);
    print_missing_role_report_audit_hint(record);
    print_tensor_mapping_gate_audit_hint(record);
    printf("target_artifact_class: %s\n", record->target_artifact_class);
    printf("target_artifact_status: %s\n", model_target_target_artifact_status(record));
    printf("target_artifact_origin: %s\n", model_target_target_artifact_origin(record));
    printf("target_artifact_required: %s\n",
           model_target_target_artifact_required(record));
    printf("external_reference_status: %s\n",
           model_target_external_reference_status(record));
    printf("yvex_produced_artifact_status: %s\n",
           model_target_yvex_produced_artifact_status(record));
    printf("pressure_purpose: %s\n", record->pressure_purpose);
    printf("tensor_set: %s\n", record->tensor_set);
    printf("local_path_class: %s\n", record->local_path_class);
    printf("source_footprint_class: %s\n", record->source_footprint_class);
    printf("runtime_boundary: %s\n", record->runtime_boundary);
    printf("runtime_execution: %s\n", record->runtime_execution);
    printf("runtime_claim: unsupported\n");
    printf("generation: %s\n", record->generation);
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
    printf("external_reference: %s\n", record->external_reference);
}

static void print_model_target_record_normal(const yvex_model_target_record *record)
{
    if (model_target_is_source_model_candidate(record)) {
        printf("target: %s\n", record->target_id);
        printf("family: %s  class=%s\n", record->family, record->target_class);
        printf("source: %s  status=%s\n",
               record->source_artifact_class,
               model_target_source_artifact_status(record));
        printf("artifact: %s  status=%s\n",
               record->target_artifact_class,
               model_target_target_artifact_status(record));
        printf("backend_selection: %s\n", model_target_backend_selection(record));
        printf("backend_pressure: %s\n", model_target_backend_pressure(record));
        printf("runtime: %s\n", record->runtime_execution);
        printf("generation: %s\n", record->generation);
        printf("next: %s\n", YVEX_MODEL_CLASS_NEXT_ROW);
        printf("boundary: target/source profile only; no source download/runtime/generation\n");
        printf("status: model-target\n");
        return;
    }
    if (strcmp(record->target_id, "glm-5.2-official-safetensors") == 0) {
        printf("target: %s\n", record->target_id);
        printf("family: %s  class=%s\n", record->family, record->target_class);
        printf("source: %s  status=%s\n",
               record->source_artifact_class,
               model_target_source_artifact_status(record));
        printf("artifact: %s  status=%s\n",
               record->target_artifact_class,
               model_target_target_artifact_status(record));
        printf("runtime: %s\n", record->runtime_execution);
        printf("generation: %s\n", record->generation);
        printf("next: V010.SOURCE.8\n");
        printf("boundary: source/storage pressure only; no GLM runtime/generation\n");
        printf("status: model-target\n");
        return;
    }
    if (model_target_is_selected_slice(record)) {
        printf("target: %s\n", record->target_id);
        printf("family: %s  class=%s\n", record->family, record->target_class);
        printf("source: %s  status=%s\n",
               record->source_artifact_class,
               model_target_source_artifact_status(record));
        printf("artifact: %s  status=%s\n",
               record->target_artifact_class,
               model_target_target_artifact_status(record));
        printf("runtime: %s\n", model_target_runtime_display(record));
        printf("generation: %s\n", record->generation);
        printf("boundary: selected-slice only; no full-runtime generation\n");
        printf("status: model-target\n");
        return;
    }

    printf("target: %s\n", record->target_id);
    printf("family: %s class=%s\n", record->family, record->target_class);
    printf("source: %s  status=%s\n",
           record->source_artifact_class,
           model_target_source_artifact_status(record));
    printf("artifact: %s  status=%s\n",
           record->target_artifact_class,
           model_target_target_artifact_status(record));
    printf("runtime: %s generation=%s\n",
           record->runtime_execution,
           record->generation);
    printf("top_blocker: %s\n",
           strcmp(record->runtime_execution, "unsupported") == 0
               ? "full-runtime target facts incomplete"
               : "full-runtime execution unsupported");
    printf("boundary: target report only, no runtime execution\n");
    printf("status: model-target\n");
}

static void print_model_target_report_table(const char *report,
                                            const char *status,
                                            const char *selected,
                                            unsigned long eligible_count)
{
    printf("%-24s  %-8s  %-8s  %8s  %s\n",
           "REPORT",
           "STATUS",
           "SELECTED",
           "ELIGIBLE",
           "NEXT");
    printf("%-24s  %-8s  %-8s  %8lu  %s\n",
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
        fprintf(stderr, "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
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
        fprintf(stderr, "model-target: no path mapping for target: %s\n", record->target_id);
        return 2;
    }

    if (!audit_output) {
        if (model_target_is_source_model_candidate(record)) {
            printf("target: %s\n", record->target_id);
            printf("source: %s  %s\n", source_exists ? "present" : "missing", source_path);
            printf("source_class: %s\n", source_class);
            printf("artifact: planned  %s\n", artifact_path);
            printf("artifact_class: %s\n", target_class);
            printf("backend_selection: %s\n", model_target_backend_selection(record));
            printf("backend_pressure: %s\n", model_target_backend_pressure(record));
            printf("reports: %s\n", report_dir);
            printf("registry: %s\n", registry_dir);
            printf("boundary: path report only, no runtime execution\n");
            printf("status: model-target-paths\n");
            return 0;
        }
        printf("target: %s\n", record->target_id);
        printf("models_root: %s\n", operator_paths.models_root);
        printf("source: %s exists=%s\n", source_path, source_exists ? "true" : "false");
        printf("source_class: %s\n", source_class);
        printf("artifact: %s exists=%s\n", artifact_path, artifact_exists ? "true" : "false");
        printf("artifact_class: %s\n", target_class);
        printf("registry_alias: %s\n", registry_alias);
        printf("boundary: path report only, no payload read or runtime execution\n");
        printf("status: model-target-paths\n");
    } else {
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
        printf("source_artifact_status: %s\n",
               source_exists ? "present" : model_target_source_artifact_status(record));
        printf("source_artifact_format: %s\n",
               model_target_source_artifact_format(record));
        printf("source_artifact_origin: %s\n",
               model_target_source_artifact_origin(record));
        printf("source_artifact_authority: %s\n",
               model_target_source_artifact_authority(record));
        printf("source_sidecar_status: %s\n",
               model_target_source_sidecar_status(record));
        printf("source_tensor_container: %s\n",
               model_target_source_tensor_container(record));
        printf("source_tensor_payload_status: %s\n",
               source_exists ? "present-not-loaded"
                             : model_target_source_tensor_payload_status(record));
        printf("target_artifact_class: %s\n", target_class);
        printf("target_artifact_status: %s\n",
               artifact_exists ? "present" : model_target_target_artifact_status(record));
        printf("target_artifact_origin: %s\n",
               model_target_target_artifact_origin(record));
        printf("target_artifact_required: %s\n",
               model_target_target_artifact_required(record));
        printf("backend_selection: %s\n", model_target_backend_selection(record));
        printf("backend_pressure: %s\n", model_target_backend_pressure(record));
        printf("external_reference_status: %s\n",
               model_target_external_reference_status(record));
        printf("yvex_produced_artifact_status: %s\n",
               model_target_yvex_produced_artifact_status(record));
        printf("runtime_execution: %s\n", runtime_execution);
        printf("runtime_claim: unsupported\n");
        printf("generation: unsupported\n");
        printf("benchmark_status: not-measured\n");
        printf("release_ready: false\n");
        printf("status: model-target-paths\n");
    }
    return 0;
}

int yvex_model_target_command(int argc, char **argv)
{
    const yvex_model_target_record *record;
    const char *models_root = NULL;
    yvex_model_target_output_mode output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
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
        yvex_model_target_output_mode output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        for (i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target list: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    fprintf(stderr, "model-target list: unsupported output mode: %s\n", argv[i]);
                    return 2;
                }
            } else {
                fprintf(stderr, "model-target list: unknown option: %s\n", argv[i]);
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
            } else if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target candidate: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    fprintf(stderr, "model-target candidate: unsupported output mode: %s\n", argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--json") == 0) {
                fprintf(stderr, "model-target candidate: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
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
            } else if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target dense-candidate: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    fprintf(stderr, "model-target dense-candidate: unsupported output mode: %s\n", argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--json") == 0) {
                fprintf(stderr, "model-target dense-candidate: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
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
            } else if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target qwen-metal: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    fprintf(stderr, "model-target qwen-metal: unsupported output mode: %s\n", argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--json") == 0) {
                fprintf(stderr, "model-target qwen-metal: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
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
            } else if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target decision: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    fprintf(stderr, "model-target decision: unsupported output mode: %s\n", argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--json") == 0) {
                fprintf(stderr, "model-target decision: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
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
            fprintf(stderr, "model-target class-profile: requires TARGET\n");
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
                    fprintf(stderr, "model-target class-profile: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    fprintf(stderr, "model-target class-profile: unsupported output mode: %s\n",
                            argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    fprintf(stderr, "model-target class-profile: --models-root requires DIR\n");
                    return 2;
                }
                models_root = argv[++i];
            } else if (strcmp(argv[i], "--source") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    fprintf(stderr, "model-target class-profile: --source requires DIR\n");
                    return 2;
                }
                source = argv[++i];
            } else if (strcmp(argv[i], "--json") == 0) {
                fprintf(stderr, "model-target class-profile: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
            } else {
                fprintf(stderr, "model-target class-profile: unknown option: %s\n", argv[i]);
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
            fprintf(stderr, "model-target class-profile: unsupported target: %s\n",
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
            fprintf(stderr, "model-target tensor-collection: requires TARGET\n");
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
                    fprintf(stderr, "model-target tensor-collection: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    fprintf(stderr, "model-target tensor-collection: unsupported output mode: %s\n",
                            argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    fprintf(stderr, "model-target tensor-collection: --models-root requires DIR\n");
                    return 2;
                }
                models_root = argv[++i];
            } else if (strcmp(argv[i], "--source") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    fprintf(stderr, "model-target tensor-collection: --source requires DIR\n");
                    return 2;
                }
                source = argv[++i];
            } else if (strcmp(argv[i], "--json") == 0) {
                fprintf(stderr, "model-target tensor-collection: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
            } else {
                fprintf(stderr, "model-target tensor-collection: unknown option: %s\n", argv[i]);
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
            fprintf(stderr, "model-target tensor-collection: unsupported target: %s\n",
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
        int rc;

        output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        if (argc < 4) {
            fprintf(stderr, "model-target tensor-map: requires TARGET\n");
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
                    fprintf(stderr, "model-target tensor-map: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    fprintf(stderr, "model-target tensor-map: unsupported output mode: %s\n",
                            argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    fprintf(stderr, "model-target tensor-map: --models-root requires DIR\n");
                    return 2;
                }
                models_root = argv[++i];
            } else if (strcmp(argv[i], "--source") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    fprintf(stderr, "model-target tensor-map: --source requires DIR\n");
                    return 2;
                }
                source = argv[++i];
            } else if (strcmp(argv[i], "--role") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    fprintf(stderr, "model-target tensor-map: --role requires output-head|tokenizer|missing-roles\n");
                    return 2;
                }
                role = argv[++i];
                if (strcmp(role, "output-head") != 0 &&
                    strcmp(role, "tokenizer") != 0 &&
                    strcmp(role, "missing-roles") != 0) {
                    fprintf(stderr, "model-target tensor-map: unsupported role: %s\n",
                            role);
                    return 2;
                }
            } else if (strcmp(argv[i], "--gate") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    fprintf(stderr, "model-target tensor-map: --gate requires v0.1.0\n");
                    return 2;
                }
                gate = argv[++i];
                if (strcmp(gate, "v0.1.0") != 0) {
                    fprintf(stderr, "model-target tensor-map: unsupported release: %s\n",
                            gate);
                    return 2;
                }
            } else if (strcmp(argv[i], "--json") == 0) {
                fprintf(stderr, "model-target tensor-map: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
            } else {
                fprintf(stderr, "model-target tensor-map: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        if (role && gate) {
            fprintf(stderr, "model-target tensor-map: --gate cannot be combined with --role\n");
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
            fprintf(stderr, "model-target tensor-map: unsupported target: %s\n",
                    target_id && target_id[0] ? target_id : "none");
            return 2;
        }
        if (gate) {
            rc = build_tensor_mapping_gate_profile(record, models_root, source,
                                                   &gate_profile);
            if (rc != 0) {
                return rc;
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
            if (dynamic_target.found &&
                !write_output_head_map_sidecar(dynamic_target.output_head_map_path,
                                               &output_head_profile)) {
                fprintf(stderr,
                        "model-target tensor-map: cannot write output-head map sidecar: %s\n",
                        dynamic_target.output_head_map_path);
                return 3;
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
            rc = build_tokenizer_map_profile(record, models_root, source,
                                             &tokenizer_profile);
            if (rc != 0) {
                return rc;
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
            rc = build_missing_role_report_profile(record, models_root, source,
                                                   &missing_role_profile);
            if (rc != 0) {
                return rc;
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
        if (dynamic_target.found &&
            !write_tensor_map_sidecar(dynamic_target.tensor_map_path, &profile)) {
            fprintf(stderr,
                    "model-target tensor-map: cannot write tensor map sidecar: %s\n",
                    dynamic_target.tensor_map_path);
            return 3;
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
        yvex_qtype_policy_profile profile;
        int rc;

        output_mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        if (argc < 4) {
            fprintf(stderr, "model-target quant-policy: requires TARGET\n");
            return 2;
        }
        target_id = argv[3];
        for (i = 4; i < argc; ++i) {
            if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target quant-policy: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    fprintf(stderr, "model-target quant-policy: unsupported output mode: %s\n",
                            argv[i]);
                    return 2;
                }
            } else if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    fprintf(stderr, "model-target quant-policy: --models-root requires DIR\n");
                    return 2;
                }
                models_root = argv[++i];
            } else if (strcmp(argv[i], "--source") == 0) {
                if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                    fprintf(stderr, "model-target quant-policy: --source requires DIR\n");
                    return 2;
                }
                source = argv[++i];
            } else if (strcmp(argv[i], "--json") == 0) {
                fprintf(stderr, "model-target quant-policy: JSON output is unsupported; use --output normal|table|audit\n");
                return 2;
            } else {
                fprintf(stderr, "model-target quant-policy: unknown option: %s\n", argv[i]);
                return 2;
            }
        }
        record = find_model_target(target_id);
        if (!record) {
            return print_qtype_policy_unsupported_target(target_id, output_mode);
        }
        rc = build_qtype_policy_profile(record, models_root, source, &profile);
        if (rc != 0) {
            return rc;
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
            } else if (strcmp(argv[i], "--audit") == 0) {
                output_mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            } else if (strcmp(argv[i], "--output") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "model-target inspect: --output requires normal|table|audit\n");
                    return 2;
                }
                if (!parse_model_target_output_mode(argv[++i], &output_mode)) {
                    fprintf(stderr, "model-target inspect: unsupported output mode: %s\n", argv[i]);
                    return 2;
                }
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

    fprintf(stderr, "model-target: unknown subcommand: %s\n", argv[2]);
    fprintf(stderr, "Try 'yvex help model-target' for usage.\n");
    return 2;
}
