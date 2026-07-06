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
#include <unistd.h>

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
