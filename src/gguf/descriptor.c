/* Owner: gguf.descriptor
 * Owns: GGUF descriptor, tensor-info, name, layout, range, and artifact-name facts.
 * Does not own: container parsing, payload I/O, writer publication, or runtime admission.
 * Invariants: every projection is checked, deterministic, and independent of mutable runtime state.
 * Boundary: structural descriptor acceptance does not imply artifact or execution support.
 * Purpose: centralize the small pure projections that define one GGUF tensor descriptor.
 * Inputs: admitted parser facts, typed tensor roles, shapes, qtypes, and naming components.
 * Effects: mutates only caller-provided result objects and reason pointers; performs no I/O.
 * Failure: invalid or incomplete facts fail closed without publishing a successful descriptor. */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <yvex/internal/gguf.h>

/* Purpose: project admitted section facts into one fail-closed descriptor result.
 * Inputs: immutable container, metadata, tensor-info, qtype, and range facts.
 * Effects: initializes and then conditionally accepts the caller-owned descriptor.
 * Failure: missing or refused inputs leave a typed non-success status and stable reason.
 * Boundary: projection cannot promote complete-artifact or runtime support. */
void yvex_gguf_descriptor_abi_from_sections(const yvex_gguf_container_abi *container,
                                            const yvex_gguf_metadata_abi *metadata,
                                            const yvex_gguf_tensor_info_abi *tensor_info,
                                            const yvex_gguf_qtype_abi *qtype,
                                            const yvex_gguf_range_fact *range,
                                            yvex_gguf_descriptor_abi *descriptor) {
    if (!descriptor)
        return;
    descriptor->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    descriptor->reason = "GGUF descriptor not evaluated";

    if (!container || !metadata || !tensor_info || !qtype || !range) {
        descriptor->status = YVEX_GGUF_ABI_SECTION_NOT_PRESENT;
        descriptor->reason = "GGUF descriptor requires all ABI sections";
        return;
    }

    if (container->status != YVEX_GGUF_ABI_SECTION_OK) {
        descriptor->status = container->status;
        descriptor->reason = "GGUF descriptor blocked by container ABI";
        return;
    }
    if (metadata->status != YVEX_GGUF_ABI_SECTION_OK) {
        descriptor->status = metadata->status;
        descriptor->reason = "GGUF descriptor blocked by metadata ABI";
        return;
    }
    if (tensor_info->status != YVEX_GGUF_ABI_SECTION_OK) {
        descriptor->status = tensor_info->status;
        descriptor->reason = "GGUF descriptor blocked by tensor_info ABI";
        return;
    }
    if (qtype->status != YVEX_GGUF_ABI_SECTION_OK) {
        descriptor->status = qtype->status;
        descriptor->reason = "GGUF descriptor blocked by qtype byte geometry ABI";
        return;
    }
    if (range->status != YVEX_GGUF_ABI_SECTION_OK) {
        descriptor->status = range->status;
        descriptor->reason = "GGUF descriptor blocked by range ABI";
        return;
    }

    descriptor->status = YVEX_GGUF_ABI_SECTION_OK;
    descriptor->reason =
        "GGUF structural descriptor accepted; complete artifact integrity remains blocked";
}

/* Purpose: initialize a tensor-info result to its fail-closed state.
 * Inputs: optional caller-owned result.
 * Effects: clears all counters and installs a stable not-evaluated reason.
 * Failure: a null result is ignored.
 * Boundary: initialization performs no parsing or allocation. */
void yvex_gguf_tensor_info_abi_init(yvex_gguf_tensor_info_abi *abi) {
    if (!abi)
        return;
    abi->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    abi->tensor_count = 0ull;
    abi->max_rank = 0u;
    abi->rank_one_tensor_count = 0ull;
    abi->named_tensor_count = 0ull;
    abi->qtype_known_tensor_count = 0ull;
    abi->qtype_refused_tensor_count = 0ull;
    abi->reason = "GGUF tensor_info ABI not evaluated";
}

/* Purpose: validate one tensor rank against the pinned GGUF rank bound.
 * Inputs: rank and optional reason sink.
 * Effects: writes a borrowed static reason when requested.
 * Failure: returns zero for zero or excessive rank.
 * Boundary: rank admission does not validate dimensions or payload geometry. */
static int tensor_info_rank_supported(unsigned int rank, const char **reason) {
    if (rank > 0u && rank <= 4u) {
        if (reason)
            *reason = "tensor_info rank accepted by GGUF ABI boundary";
        return 1;
    }
    if (reason)
        *reason = "unsupported GGUF tensor_info rank";
    return 0;
}

/* Purpose: summarize all parser-owned tensor directory entries into ABI facts.
 * Inputs: immutable parsed GGUF view, caller-owned summary, and optional reason sink.
 * Effects: scans tensor metadata and updates only the supplied summary.
 * Failure: malformed ranks, names, shapes, or qtypes produce typed refusal counters.
 * Boundary: the scan reads directory facts only and never reads tensor payload bytes. */
int yvex_gguf_tensor_info_abi_from_gguf(const yvex_gguf *gguf, yvex_gguf_tensor_info_abi *abi,
                                        const char **reason) {
    unsigned long long i;
    unsigned long long count;

    yvex_gguf_tensor_info_abi_init(abi);
    if (!gguf || !abi) {
        if (reason)
            *reason = "GGUF tensor_info requires a parsed GGUF view";
        return 0;
    }

    count = yvex_gguf_tensor_count(gguf);
    abi->tensor_count = count;

    for (i = 0ull; i < count; ++i) {
        const yvex_gguf_tensor_info *tensor = yvex_gguf_tensor_at(gguf, i);
        unsigned int d;

        if (!tensor || !tensor->name || tensor->name[0] == '\0') {
            abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
            abi->reason = "tensor_info entry is missing a tensor name";
            if (reason)
                *reason = abi->reason;
            return 0;
        }
        abi->named_tensor_count += 1ull;

        if (!tensor_info_rank_supported(tensor->rank, reason)) {
            abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
            abi->reason = reason ? *reason : "tensor_info rank unsupported";
            return 0;
        }
        if (tensor->rank > abi->max_rank) {
            abi->max_rank = tensor->rank;
        }

        for (d = 0u; d < tensor->rank; ++d) {
            if (tensor->dims[d] == 0ull) {
                abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
                abi->reason = "tensor_info dimension is zero";
                if (reason)
                    *reason = abi->reason;
                return 0;
            }
        }
        if (tensor->rank == 1u) {
            abi->rank_one_tensor_count += 1ull;
        }

        if (yvex_gguf_qtype_supported_for_storage(tensor->ggml_type, NULL)) {
            abi->qtype_known_tensor_count += 1ull;
        } else {
            abi->qtype_refused_tensor_count += 1ull;
        }
    }

    abi->status = YVEX_GGUF_ABI_SECTION_OK;
    abi->reason = "GGUF tensor_info ABI accepted";
    if (reason)
        *reason = abi->reason;
    return 1;
}

/* Purpose: map one admitted role to its pinned GGUF name or layer-local suffix.
 * Inputs: typed tensor role and caller-owned layer-scope flag.
 * Effects: returns borrowed text and records whether the layer prefix is required.
 * Failure: returns null for roles without a standard name.
 * Boundary: this is format naming, not logical tensor identity. */
typedef struct {
    const char *name;
    int layer_scoped;
} gguf_role_name;

static const gguf_role_name gguf_role_names[YVEX_TENSOR_ROLE_COUNT] = {
    [YVEX_TENSOR_ROLE_TOKEN_EMBEDDING] = {"token_embd.weight", 0},
    [YVEX_TENSOR_ROLE_OUTPUT_NORM] = {"output_norm.weight", 0},
    [YVEX_TENSOR_ROLE_OUTPUT_HEAD] = {"output.weight", 0},
    [YVEX_TENSOR_ROLE_HC_HEAD_FUNCTION] = {"output_hc_fn.weight", 0},
    [YVEX_TENSOR_ROLE_HC_HEAD_BASE] = {"output_hc_base.weight", 0},
    [YVEX_TENSOR_ROLE_HC_HEAD_SCALE] = {"output_hc_scale.weight", 0},
    [YVEX_TENSOR_ROLE_ATTENTION_NORM] = {"attn_norm.weight", 1},
    [YVEX_TENSOR_ROLE_FFN_NORM] = {"ffn_norm.weight", 1},
    [YVEX_TENSOR_ROLE_ATTENTION_SINKS] = {"attn_sinks.weight", 1},
    [YVEX_TENSOR_ROLE_ATTENTION_Q_A] = {"attn_q_a.weight", 1},
    [YVEX_TENSOR_ROLE_ATTENTION_Q_B] = {"attn_q_b.weight", 1},
    [YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM] = {"attn_q_a_norm.weight", 1},
    [YVEX_TENSOR_ROLE_ATTENTION_KV] = {"attn_kv.weight", 1},
    [YVEX_TENSOR_ROLE_ATTENTION_KV_NORM] = {"attn_kv_a_norm.weight", 1},
    [YVEX_TENSOR_ROLE_ATTENTION_OUT_A] = {"attn_output_a.weight", 1},
    [YVEX_TENSOR_ROLE_ATTENTION_OUT_B] = {"attn_output_b.weight", 1},
    [YVEX_TENSOR_ROLE_HC_ATTENTION_FUNCTION] = {"hc_attn_fn.weight", 1},
    [YVEX_TENSOR_ROLE_HC_ATTENTION_BASE] = {"hc_attn_base.weight", 1},
    [YVEX_TENSOR_ROLE_HC_ATTENTION_SCALE] = {"hc_attn_scale.weight", 1},
    [YVEX_TENSOR_ROLE_HC_FFN_FUNCTION] = {"hc_ffn_fn.weight", 1},
    [YVEX_TENSOR_ROLE_HC_FFN_BASE] = {"hc_ffn_base.weight", 1},
    [YVEX_TENSOR_ROLE_HC_FFN_SCALE] = {"hc_ffn_scale.weight", 1},
    [YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV] = {"attn_compressor_kv.weight", 1},
    [YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE] = {"attn_compressor_gate.weight", 1},
    [YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE] = {"attn_compressor_ape.weight", 1},
    [YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM] = {"attn_compressor_norm.weight", 1},
    [YVEX_TENSOR_ROLE_INDEXER_PROJECTION] = {"indexer.proj.weight", 1},
    [YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B] = {"indexer.attn_q_b.weight", 1},
    [YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV] = {"indexer_compressor_kv.weight", 1},
    [YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE] = {"indexer_compressor_gate.weight", 1},
    [YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE] = {"indexer_compressor_ape.weight", 1},
    [YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM] = {"indexer_compressor_norm.weight", 1},
    [YVEX_TENSOR_ROLE_MOE_ROUTER] = {"ffn_gate_inp.weight", 1},
    [YVEX_TENSOR_ROLE_MOE_ROUTER_BIAS] = {"exp_probs_b.bias", 1},
    [YVEX_TENSOR_ROLE_MOE_ROUTER_TABLE] = {"ffn_gate_tid2eid.weight", 1},
    [YVEX_TENSOR_ROLE_MOE_EXPERT_GATE] = {"ffn_gate_exps.weight", 1},
    [YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN] = {"ffn_down_exps.weight", 1},
    [YVEX_TENSOR_ROLE_MOE_EXPERT_UP] = {"ffn_up_exps.weight", 1},
    [YVEX_TENSOR_ROLE_MOE_SHARED_EXPERT_GATE] = {"ffn_gate_shexp.weight", 1},
    [YVEX_TENSOR_ROLE_MOE_SHARED_EXPERT_DOWN] = {"ffn_down_shexp.weight", 1},
    [YVEX_TENSOR_ROLE_MOE_SHARED_EXPERT_UP] = {"ffn_up_shexp.weight", 1},
};

/* Purpose: project one typed role to its canonical GGUF base name and scope. */
static const char *gguf_standard_role_name(yvex_tensor_role role, int *layer_scoped) {
    if (role <= YVEX_TENSOR_ROLE_UNKNOWN || role >= YVEX_TENSOR_ROLE_COUNT)
        return NULL;
    *layer_scoped = gguf_role_names[role].layer_scoped;
    return gguf_role_names[role].name;
}

/* Purpose: resolve one typed role and scope into a deterministic GGUF tensor name.
 * Inputs: role, scope indexes, output buffer, and optional provenance sink.
 * Effects: writes one bounded name and provenance value.
 * Failure: unsupported roles or insufficient output capacity return zero.
 * Boundary: emitted names remain format facts and do not define logical model identity. */
int yvex_gguf_name_map_resolve(yvex_tensor_role role, int mtp_extension,
                               unsigned long long layer_index, unsigned long long predictor_index,
                               char *out, size_t out_cap, yvex_gguf_name_provenance *provenance,
                               const char **reason) {
    const char *name;
    int layer_scoped;
    int written;

    if (!out || out_cap == 0u || !provenance || role <= YVEX_TENSOR_ROLE_UNKNOWN ||
        role >= YVEX_TENSOR_ROLE_COUNT) {
        if (reason)
            *reason = "invalid typed role or output buffer";
        return 0;
    }
    if (mtp_extension) {
        written = snprintf(out, out_cap, "yvex.mtp.v1.%llu.%s.weight", predictor_index,
                           yvex_tensor_role_name(role));
        *provenance = YVEX_GGUF_NAME_YVEX_EXTENSION;
    } else {
        name = gguf_standard_role_name(role, &layer_scoped);
        if (!name) {
            if (reason)
                *reason = "role has no pinned DeepSeek-V4 GGUF name";
            return 0;
        }
        if (layer_scoped) {
            written = snprintf(out, out_cap, "blk.%llu.%s", layer_index, name);
        } else {
            written = snprintf(out, out_cap, "%s", name);
        }
        *provenance = YVEX_GGUF_NAME_PINNED_STANDARD;
    }
    if (written < 0 || (size_t)written >= out_cap) {
        if (reason)
            *reason = "emitted GGUF name exceeds mapping bounds";
        return 0;
    }
    if (reason)
        *reason = "admitted canonical logical name";
    return 1;
}

/* Purpose: validate one emitted tensor shape and qtype against GGUF row geometry.
 * Inputs: typed role, qtype, rank, dimensions, and optional reason sink.
 * Effects: writes a borrowed static reason when requested.
 * Failure: invalid rank, dimensions, qtype, or block divisibility returns zero.
 * Boundary: validation plans geometry but neither quantizes nor emits bytes. */
int yvex_gguf_layout_map_shape_supported(yvex_tensor_role role, unsigned int qtype,
                                         unsigned int rank, const unsigned long long *dims,
                                         const char **reason) {
    unsigned int i;

    if (role <= YVEX_TENSOR_ROLE_UNKNOWN || role >= YVEX_TENSOR_ROLE_COUNT || !dims || rank == 0u ||
        rank > YVEX_TENSOR_MAX_DIMS) {
        if (reason)
            *reason = "invalid role, rank, or dimensions";
        return 0;
    }
    for (i = 0u; i < rank; ++i) {
        if (!dims[i]) {
            if (reason)
                *reason = "logical GGML dimension is zero";
            return 0;
        }
    }
    if (qtype != YVEX_GGUF_NO_FORCED_QTYPE) {
        yvex_gguf_qtype_storage_result storage;
        if (yvex_gguf_qtype_tensor_storage(qtype, dims, rank, &storage) !=
            YVEX_GGUF_QTYPE_STORAGE_OK) {
            if (reason)
                *reason = storage.reason;
            return 0;
        }
    }
    if (reason)
        *reason = "admitted logical GGML shape";
    return 1;
}

/* Purpose: initialize one range fact to a fail-closed not-evaluated state.
 * Inputs: optional caller-owned fact.
 * Effects: clears geometry and installs a stable reason.
 * Failure: a null fact is ignored.
 * Boundary: initialization neither validates nor reads a file. */
void yvex_gguf_range_fact_init(yvex_gguf_range_fact *fact) {
    if (!fact)
        return;
    fact->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    fact->checked_tensor_count = 0ull;
    fact->tensor_data_offset = 0ull;
    fact->file_size = 0ull;
    fact->total_expected_storage_bytes = 0ull;
    fact->first_expected_storage_bytes = 0ull;
    fact->first_actual_available_bytes = 0ull;
    fact->qtype_checked_tensor_count = 0ull;
    fact->alignment = 0u;
    fact->reason = "GGUF range map not evaluated";
}

/* Purpose: project an accepted canonical layout into the legacy range summary.
 * Inputs: immutable layout, caller-owned fact, and optional reason sink.
 * Effects: initializes then copies checked layout counters into the fact.
 * Failure: null or refused layouts leave a non-success fact.
 * Boundary: projection does not reconstruct offsets or payload geometry. */
int yvex_gguf_range_fact_from_layout(const yvex_gguf_layout_result *layout,
                                     yvex_gguf_range_fact *fact, const char **reason) {
    yvex_gguf_range_fact_init(fact);
    if (!layout || !fact) {
        if (reason)
            *reason = "GGUF range projection requires a layout result";
        return 0;
    }
    fact->tensor_data_offset = layout->tensor_data_offset;
    fact->file_size = layout->actual_file_size;
    fact->alignment = layout->alignment;
    fact->checked_tensor_count = layout->tensors_validated;
    fact->qtype_checked_tensor_count = layout->tensors_validated;
    fact->total_expected_storage_bytes = layout->raw_tensor_bytes;
    if (!layout->accepted || layout->code != YVEX_GGUF_LAYOUT_OK) {
        fact->status = YVEX_GGUF_ABI_SECTION_REFUSED;
        fact->reason = layout->reason;
        if (reason)
            *reason = fact->reason;
        return 0;
    }
    fact->status = YVEX_GGUF_ABI_SECTION_OK;
    fact->reason = "GGUF ranges project the canonical global layout result";
    if (reason)
        *reason = fact->reason;
    return 1;
}

/* Purpose: recognize null and empty artifact-name components. */
static int is_empty(const char *s) {
    return !s || s[0] == '\0';
}

/* Purpose: reject whitespace that would make an artifact filename ambiguous. */
static int contains_space(const char *s) {
    while (s && *s) {
        if (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
            return 1;
        ++s;
    }
    return 0;
}

/* Purpose: detect reserved proof-like words in artifact-name components. */
static int has_ambiguous_word(const char *s) {
    return s && (strstr(s, "test") || strstr(s, "final") || strstr(s, "new") ||
                 strstr(s, "fixed") || strstr(s, "latest"));
}

/* Purpose: validate one semantic artifact-name component.
 * Inputs: component text, diagnostic label, and typed error sink.
 * Effects: records a typed refusal for invalid input.
 * Failure: empty, whitespace-bearing, or proof-like components return zero.
 * Boundary: component validation does not inspect the filesystem. */
static int validate_part(const char *part, const char *name, yvex_error *err) {
    if (is_empty(part)) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_name_suggest", "%s is required",
                        name);
        return YVEX_ERR_INVALID_ARG;
    }
    if (contains_space(part)) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_name_suggest",
                        "%s must not contain spaces", name);
        return YVEX_ERR_INVALID_ARG;
    }
    if (has_ambiguous_word(part)) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_name_suggest",
                        "%s contains ambiguous naming vocabulary", name);
        return YVEX_ERR_INVALID_ARG;
    }
    return YVEX_OK;
}

/* Purpose: construct one deterministic artifact filename from admitted semantic components.
 * Inputs: bounded output plus repository, revision, model, precision, hardware, and version text.
 * Effects: writes exactly one filename when all components and capacity are valid.
 * Failure: invalid components, formatting failure, or truncation return typed refusal.
 * Boundary: naming does not create, admit, or publish an artifact. */
int yvex_artifact_name_suggest(char *out, size_t out_size, const char *family, const char *model,
                               const char *scope, const char *artifact_class, const char *qprofile,
                               const char *calibration, const char *producer, const char *schema,
                               yvex_error *err) {
    int n;
    int rc;

    if (!out || out_size == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_name_suggest",
                       "output buffer is required");
        return YVEX_ERR_INVALID_ARG;
    }
    out[0] = '\0';
    rc = validate_part(family, "family", err);
    if (rc != YVEX_OK)
        return rc;
    rc = validate_part(model, "model", err);
    if (rc != YVEX_OK)
        return rc;
    rc = validate_part(scope, "scope", err);
    if (rc != YVEX_OK)
        return rc;
    rc = validate_part(artifact_class, "artifact_class", err);
    if (rc != YVEX_OK)
        return rc;
    rc = validate_part(qprofile, "qprofile", err);
    if (rc != YVEX_OK)
        return rc;
    rc = validate_part(calibration, "calibration", err);
    if (rc != YVEX_OK)
        return rc;
    rc = validate_part(producer, "producer", err);
    if (rc != YVEX_OK)
        return rc;
    rc = validate_part(schema, "schema", err);
    if (rc != YVEX_OK)
        return rc;
    if (strcmp(producer, "yvex") != 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_name_suggest",
                       "producer must be yvex");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(schema, "v1") != 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_name_suggest",
                       "schema must be v1");
        return YVEX_ERR_INVALID_ARG;
    }

    n = snprintf(out, out_size, "%s-%s-%s-%s-%s-%s-%s-%s.gguf", family, model, scope,
                 artifact_class, qprofile, calibration, producer, schema);
    if (n < 0 || (size_t)n >= out_size) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_artifact_name_suggest",
                       "artifact filename buffer too small");
        out[0] = '\0';
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}
