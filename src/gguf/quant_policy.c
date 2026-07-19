/* Owner: gguf.quant_policy
 * Owns: role-based quantization policy, template derivation, validation, and policy JSON IO.
 * Does not own: calibration data, job orchestration, numeric codecs, execution, or artifacts.
 * Invariants: every rule owns its selector and derives support from canonical qtype capabilities.
 * Boundary: policy selects admissible representations; it neither converts bytes nor writes GGUF.
 * Purpose: own one immutable-at-consumption policy document and its template projection.
 * Inputs: typed policy requests, admitted template descriptors, or bounded shared-parser JSON.
 * Effects: allocates policy state and performs explicit policy file IO and template reads.
 * Failure: typed errors publish no partial policy and cleanup closes every borrowed owner. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/artifact.h>
#include <yvex/gguf.h>
#include <yvex/internal/core.h>
#include <yvex/internal/io.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/model.h>
#include <yvex/qtype.h>
#include <yvex/quant.h>

struct yvex_quant_policy {
    char *name;
    char *architecture;
    char *source_kind;
    char *template_path;
    yvex_quant_policy_rule *rules;
    unsigned long long rule_count;
    unsigned long long rule_cap;
    yvex_quant_policy_summary summary;
};

typedef struct {
    int value;
    const char *name;
} policy_name;

typedef struct {
    yvex_quant_qtype qtype;
    yvex_dtype dtype;
} qtype_dtype;

static const policy_name selector_names[] = {
    {YVEX_QUANT_SELECTOR_UNKNOWN, "unknown"},
    {YVEX_QUANT_SELECTOR_ROLE, "role"},
    {YVEX_QUANT_SELECTOR_TENSOR_NAME, "tensor_name"},
    {YVEX_QUANT_SELECTOR_TENSOR_PATTERN, "pattern"},
    {YVEX_QUANT_SELECTOR_LAYER_RANGE, "layer_range"},
    {YVEX_QUANT_SELECTOR_EXPERT_GROUP, "expert_group"},
    {YVEX_QUANT_SELECTOR_DEFAULT, "default"},
};
static const policy_name selector_aliases[] = {
    {YVEX_QUANT_SELECTOR_TENSOR_NAME, "name"},
    {YVEX_QUANT_SELECTOR_TENSOR_PATTERN, "tensor_pattern"},
};
static const char *const policy_status_names[] = {
    "quant-policy-unknown", "quant-policy-valid", "quant-policy-partial", "quant-policy-invalid",
};
static const char *const policy_issue_names[] = {
    "none", "unknown_qtype", "unsupported_storage_qtype", "unsupported_compute_qtype",
    "unknown_role", "unmatched_selector", "template_qtype_mismatch", "requires_imatrix", "format",
};
static const qtype_dtype qtype_dtypes[] = {
    {YVEX_QUANT_QTYPE_F32, YVEX_DTYPE_F32},
    {YVEX_QUANT_QTYPE_F16, YVEX_DTYPE_F16},
    {YVEX_QUANT_QTYPE_BF16, YVEX_DTYPE_BF16},
    {YVEX_QUANT_QTYPE_Q8_0, YVEX_DTYPE_Q8_0},
    {YVEX_QUANT_QTYPE_Q4_0, YVEX_DTYPE_Q4_0},
    {YVEX_QUANT_QTYPE_Q4_K, YVEX_DTYPE_Q4_K},
    {YVEX_QUANT_QTYPE_Q5_K, YVEX_DTYPE_Q5_K},
    {YVEX_QUANT_QTYPE_Q6_K, YVEX_DTYPE_Q6_K},
    {YVEX_QUANT_QTYPE_Q2_K, YVEX_DTYPE_Q2_K},
    {YVEX_QUANT_QTYPE_IQ2_XXS, YVEX_DTYPE_IQ2_XXS},
    {YVEX_QUANT_QTYPE_IQ2_XS, YVEX_DTYPE_IQ2_XS},
    {YVEX_QUANT_QTYPE_IQ3_XXS, YVEX_DTYPE_IQ3_XXS},
    {YVEX_QUANT_QTYPE_IQ4_NL, YVEX_DTYPE_IQ4_NL},
};

/* Purpose: resolve one exact policy name-table value without accepting prefixes. */
static int policy_name_value(const policy_name *rows, size_t count, const char *name,
                             int fallback) {
    size_t index;
    if (name)
        for (index = 0u; index < count; ++index)
            if (strcmp(rows[index].name, name) == 0)
                return rows[index].value;
    return fallback;
}

static int policy_add_rule(yvex_quant_policy *policy, yvex_quant_selector_kind selector_kind,
                           const char *selector, yvex_tensor_role role, yvex_quant_qtype qtype,
                           int requires_imatrix, yvex_error *err);

static int policy_parse_json(yvex_quant_policy **out, const char *path, yvex_error *err);

int yvex_quant_policy_validate(yvex_quant_policy *policy, const char *template_path,
                               yvex_error *err);

static int policy_write_json_file(const char *out_path, const yvex_quant_policy *policy,
                                  yvex_error *err);

static yvex_quant_qtype qtype_from_name(const char *name);
static yvex_quant_selector_kind selector_from_name(const char *name);
static yvex_tensor_role role_from_name(const char *name);
static yvex_dtype qtype_to_dtype(yvex_quant_qtype qtype);
static int qtype_storage_supported(yvex_quant_qtype qtype);
static int qtype_compute_supported(yvex_quant_qtype qtype);

typedef struct {
    yvex_json cursor;
    const char *path;
    const char *context;
    yvex_error *err;
} policy_json;

/* Purpose: advance the policy-document cursor past insignificant JSON whitespace. */
static void policy_json_space(policy_json *json) {
    yvex_json_space(&json->cursor);
}

/* Purpose: report one policy-document JSON refusal without publishing policy state. */
static int policy_json_fail(policy_json *json, const char *message) {
    yvex_error_setf(json->err, YVEX_ERR_FORMAT, json->context, "%s in %s", message, json->path);
    return YVEX_ERR_FORMAT;
}

/* Purpose: consume one required structural byte from the policy document. */
static int policy_json_expect(policy_json *json, char expected) {
    yvex_json_space(&json->cursor);
    if (json->cursor.cursor >= json->cursor.end || *json->cursor.cursor != expected) {
        return policy_json_fail(json, "unexpected JSON token");
    }
    json->cursor.cursor++;
    return YVEX_OK;
}

/* Purpose: allocate one bounded policy string from the shared JSON cursor. */
static char *policy_json_string(policy_json *json) {
    char *value = yvex_json_string_dup(&json->cursor, 16u * 1024u * 1024u);

    if (!value) {
        policy_json_fail(json, "expected bounded JSON string");
    }
    return value;
}

/* Purpose: skip one unknown policy value through the shared bounded JSON grammar. */
static int policy_json_skip(policy_json *json) {
    return yvex_json_skip_value(&json->cursor) ? YVEX_OK
                                               : policy_json_fail(json, "malformed JSON value");
}

/* Purpose: parse one exact policy boolean without coercing another JSON value. */
static int policy_json_bool(policy_json *json, int *out) {
    return yvex_json_bool(&json->cursor, out) ? YVEX_OK
                                              : policy_json_fail(json, "expected boolean");
}

/* Purpose: read one regular policy document below the canonical metadata cap.
 * Inputs: path, output buffer and length slots, diagnostic context, and error sink.
 * Effects: allocates one bounded file buffer owned by the caller.
 * Failure: typed I/O or size refusal leaves the output null.
 * Boundary: the helper reads policy metadata, never model payload. */
static int policy_json_read(const char *path, char **out, size_t *length, const char *context,
                            yvex_error *err) {
    *out = yvex_read_bounded_file(path, 16u * 1024u * 1024u, length, err);
    if (*out)
        return YVEX_OK;
    if (yvex_error_code(err) == YVEX_OK) {
        yvex_error_setf(err, YVEX_ERR_IO, context, "cannot read JSON document: %s", path);
    }
    return yvex_error_code(err);
}

/* Purpose: project one policy qtype onto the canonical dtype spelling.
 * Inputs: policy qtype enum.
 * Effects: returns borrowed immutable text.
 * Failure: unsupported values map to UNKNOWN.
 * Boundary: naming does not claim codec or compute support. */
const char *yvex_quant_qtype_name(yvex_quant_qtype qtype) {
    yvex_dtype dtype;

    if (qtype == YVEX_QUANT_QTYPE_OTHER)
        return "OTHER";
    dtype = qtype_to_dtype(qtype);
    return dtype == YVEX_DTYPE_UNKNOWN ? "UNKNOWN" : yvex_dtype_name(dtype);
}

/* Purpose: parse one exact canonical qtype spelling without aliases. */
static yvex_quant_qtype qtype_from_name(const char *name) {
    yvex_quant_qtype qtype;

    if (!name)
        return YVEX_QUANT_QTYPE_UNKNOWN;
    if (strcmp(name, "OTHER") == 0)
        return YVEX_QUANT_QTYPE_OTHER;
    for (qtype = YVEX_QUANT_QTYPE_F32; qtype < YVEX_QUANT_QTYPE_OTHER;
         qtype = (yvex_quant_qtype)(qtype + 1)) {
        if (strcmp(name, yvex_quant_qtype_name(qtype)) == 0)
            return qtype;
    }
    return YVEX_QUANT_QTYPE_UNKNOWN;
}

/* Purpose: render one typed policy selector kind.
 * Inputs: selector-kind enum.
 * Effects: returns borrowed immutable text.
 * Failure: unsupported values map to unknown.
 * Boundary: rendering does not match a tensor. */
const char *yvex_quant_selector_kind_name(yvex_quant_selector_kind kind) {
    return kind >= YVEX_QUANT_SELECTOR_UNKNOWN && kind <= YVEX_QUANT_SELECTOR_DEFAULT
               ? selector_names[kind].name
               : selector_names[YVEX_QUANT_SELECTOR_UNKNOWN].name;
}

/* Purpose: parse one admitted policy selector spelling, including legacy document aliases.
 * Inputs: optional immutable selector text.
 * Effects: none.
 * Failure: null or unknown names yield the typed unknown selector.
 * Boundary: parsing does not validate selector arguments or choose a qtype. */
static yvex_quant_selector_kind selector_from_name(const char *name) {
    int value = policy_name_value(selector_names, sizeof(selector_names) / sizeof(selector_names[0]),
                                  name, YVEX_QUANT_SELECTOR_UNKNOWN);
    return (yvex_quant_selector_kind)(value != YVEX_QUANT_SELECTOR_UNKNOWN
                                          ? value
                                          : policy_name_value(selector_aliases,
                                                              sizeof(selector_aliases) /
                                                                  sizeof(selector_aliases[0]),
                                                              name, YVEX_QUANT_SELECTOR_UNKNOWN));
}

/* Purpose: render one quantization-policy validation state.
 * Inputs: policy-status enum.
 * Effects: returns borrowed immutable text.
 * Failure: unsupported values map to quant-policy-unknown.
 * Boundary: status rendering cannot promote a policy. */
const char *yvex_quant_policy_status_name(yvex_quant_policy_status status) {
    return status >= YVEX_QUANT_POLICY_STATUS_UNKNOWN && status <= YVEX_QUANT_POLICY_STATUS_INVALID
               ? policy_status_names[status]
               : policy_status_names[YVEX_QUANT_POLICY_STATUS_UNKNOWN];
}

/* Purpose: render one typed quantization-policy issue.
 * Inputs: issue enum.
 * Effects: returns borrowed immutable text.
 * Failure: unsupported values map to format refusal.
 * Boundary: issue rendering does not classify a new failure. */
const char *yvex_quant_policy_issue_kind_name(yvex_quant_policy_issue_kind issue) {
    return issue >= YVEX_QUANT_POLICY_ISSUE_NONE && issue <= YVEX_QUANT_POLICY_ISSUE_FORMAT
               ? policy_issue_names[issue]
               : policy_issue_names[YVEX_QUANT_POLICY_ISSUE_FORMAT];
}

/* Purpose: map one policy qtype to the canonical numeric dtype identity.
 * Inputs: policy qtype enum.
 * Effects: returns a value without mutation.
 * Failure: unknown and OTHER map to the unknown dtype.
 * Boundary: mapping identity does not select a physical codec. */
static yvex_dtype qtype_to_dtype(yvex_quant_qtype qtype) {
    size_t index;
    for (index = 0u; index < sizeof(qtype_dtypes) / sizeof(qtype_dtypes[0]); ++index)
        if (qtype_dtypes[index].qtype == qtype)
            return qtype_dtypes[index].dtype;
    return YVEX_DTYPE_UNKNOWN;
}

/* Purpose: query storage admission from the canonical numeric capability registry. */
static int qtype_storage_supported(yvex_quant_qtype qtype) {
    const yvex_quant_numeric_capability *capability =
        yvex_quant_numeric_capability_by_name(yvex_quant_qtype_name(qtype));
    return capability && capability->storage_admitted;
}

/* Purpose: query dedicated CPU compute admission from the canonical numeric registry. */
static int qtype_compute_supported(yvex_quant_qtype qtype) {
    const yvex_quant_numeric_capability *capability =
        yvex_quant_numeric_capability_by_name(yvex_quant_qtype_name(qtype));
    return capability && capability->dedicated_cpu_compute_available;
}

/* Purpose: resolve one exact canonical tensor-role spelling. */
static yvex_tensor_role role_from_name(const char *name) {
    unsigned int i;

    if (!name)
        return YVEX_TENSOR_ROLE_UNKNOWN;
    for (i = 0; i <= (unsigned int)YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN; ++i) {
        yvex_tensor_role role = (yvex_tensor_role)i;
        if (strcmp(name, yvex_tensor_role_name(role)) == 0)
            return role;
    }
    return YVEX_TENSOR_ROLE_UNKNOWN;
}

/* Purpose: recompute policy status and counters from the complete owned rule set.
 * Inputs: mutable policy under construction or validation.
 * Effects: replaces the embedded summary with borrowed rule-derived facts.
 * Failure: invalid rules are represented as issue counters and typed status.
 * Boundary: summary derivation does not execute or calibrate quantization. */
static void qp_refresh_summary(yvex_quant_policy *policy) {
    unsigned long long i;

    memset(&policy->summary, 0, sizeof(policy->summary));
    policy->summary.name = policy->name;
    policy->summary.architecture = policy->architecture;
    policy->summary.rule_count = policy->rule_count;
    policy->summary.status =
        policy->rule_count > 0 ? YVEX_QUANT_POLICY_STATUS_VALID : YVEX_QUANT_POLICY_STATUS_INVALID;
    for (i = 0; i < policy->rule_count; ++i) {
        yvex_quant_policy_rule *rule = &policy->rules[i];
        if (rule->requires_imatrix)
            policy->summary.requires_imatrix_count++;
        if (rule->storage_supported)
            policy->summary.storage_supported_count++;
        if (rule->compute_supported)
            policy->summary.compute_supported_count++;
        if (rule->qtype == YVEX_QUANT_QTYPE_UNKNOWN ||
            rule->selector_kind == YVEX_QUANT_SELECTOR_UNKNOWN ||
            (rule->selector_kind == YVEX_QUANT_SELECTOR_ROLE &&
             rule->role == YVEX_TENSOR_ROLE_UNKNOWN)) {
            policy->summary.issue_count++;
            policy->summary.status = YVEX_QUANT_POLICY_STATUS_INVALID;
        } else if (!rule->storage_supported || !rule->compute_supported || rule->requires_imatrix) {
            policy->summary.issue_count++;
            if (policy->summary.status == YVEX_QUANT_POLICY_STATUS_VALID) {
                policy->summary.status = YVEX_QUANT_POLICY_STATUS_PARTIAL;
            }
        }
    }
}

/* Purpose: append one owned physical-encoding rule to a mutable policy.
 * Inputs: policy, selector facts, qtype, calibration requirement, and error sink.
 * Effects: may grow rule storage, copies the selector, and refreshes summary state.
 * Failure: invalid input or allocation failure leaves the rule count unchanged.
 * Boundary: rule admission records policy; it does not encode tensor bytes. */
static int policy_add_rule(yvex_quant_policy *policy, yvex_quant_selector_kind selector_kind,
                           const char *selector, yvex_tensor_role role, yvex_quant_qtype qtype,
                           int requires_imatrix, yvex_error *err) {
    yvex_quant_policy_rule *next;
    yvex_quant_policy_rule *rule;

    if (!policy || !selector) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_add",
                       "policy and selector are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (policy->rule_count == policy->rule_cap) {
        unsigned long long cap = policy->rule_cap == 0 ? 8u : policy->rule_cap * 2u;
        next = (yvex_quant_policy_rule *)realloc(policy->rules,
                                                 (size_t)cap * sizeof(policy->rules[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_add", "rule allocation failed");
            return YVEX_ERR_NOMEM;
        }
        policy->rules = next;
        policy->rule_cap = cap;
    }
    rule = &policy->rules[policy->rule_count];
    memset(rule, 0, sizeof(*rule));
    rule->selector_kind = selector_kind;
    rule->selector = yvex_core_strdup(selector);
    if (!rule->selector) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_add", "selector allocation failed");
        return YVEX_ERR_NOMEM;
    }
    rule->role = role;
    rule->qtype = qtype;
    rule->requires_imatrix = requires_imatrix ? 1 : 0;
    rule->storage_supported = qtype_storage_supported(qtype);
    rule->compute_supported = qtype_compute_supported(qtype);
    policy->rule_count++;
    qp_refresh_summary(policy);
    return YVEX_OK;
}

/* Purpose: parse and validate one bounded quantization-policy document.
 * Inputs: output slot, source path, and typed error sink.
 * Effects: allocates an owned policy only after successful parsing.
 * Failure: malformed, unavailable, or invalid documents leave no accepted output.
 * Boundary: opening policy reads no model payload and performs no quantization. */
int yvex_quant_policy_open(yvex_quant_policy **out, const char *path, yvex_error *err) {
    int rc = policy_parse_json(out, path, err);
    if (rc == YVEX_OK) {
        rc = yvex_quant_policy_validate(*out, NULL, err);
    }
    return rc;
}

/* Purpose: release an owned quantization policy and every nested rule string.
 * Inputs: nullable policy owner.
 * Effects: frees strings, rule storage, and the policy object.
 * Failure: cannot report failure; null is a no-op.
 * Boundary: borrowed summary and rule views expire at close. */
void yvex_quant_policy_close(yvex_quant_policy *policy) {
    unsigned long long i;

    if (!policy)
        return;
    free(policy->name);
    free(policy->architecture);
    free(policy->source_kind);
    free(policy->template_path);
    for (i = 0; i < policy->rule_count; ++i) {
        free((char *)policy->rules[i].selector);
    }
    free(policy->rules);
    free(policy);
}

/* Purpose: serialize one policy through the canonical deterministic JSON writer.
 * Inputs: destination path, immutable policy, and typed error sink.
 * Effects: writes or replaces the requested policy document.
 * Failure: typed argument or I/O errors never report successful publication.
 * Boundary: policy serialization neither quantizes nor emits an artifact. */
int yvex_quant_policy_write_json(const char *out_path, const yvex_quant_policy *policy,
                                 yvex_error *err) {
    return policy_write_json_file(out_path, policy, err);
}

/* Purpose: project the current immutable policy summary into caller storage.
 * Inputs: policy, output summary, and typed error sink.
 * Effects: copies a summary whose strings remain borrowed from the policy.
 * Failure: null inputs return typed invalid-argument refusal.
 * Boundary: summary projection does not establish numeric support. */
int yvex_quant_policy_get_summary(const yvex_quant_policy *policy, yvex_quant_policy_summary *out,
                                  yvex_error *err) {
    if (!policy || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_summary",
                       "policy and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = policy->summary;
    return YVEX_OK;
}

/* Purpose: return the number of immutable rules in one policy.
 * Inputs: nullable policy.
 * Effects: returns a scalar without mutation.
 * Failure: null policy yields zero.
 * Boundary: count access exposes no mutable rule storage. */
unsigned long long yvex_quant_policy_rule_count(const yvex_quant_policy *policy) {
    return policy ? policy->rule_count : 0;
}

/* Purpose: borrow one indexed immutable policy rule.
 * Inputs: policy and zero-based rule index.
 * Effects: returns a view valid until policy close.
 * Failure: null policy or out-of-range index yields null.
 * Boundary: the returned rule cannot mutate the policy. */
const yvex_quant_policy_rule *yvex_quant_policy_rule_at(const yvex_quant_policy *policy,
                                                        unsigned long long index) {
    if (!policy || index >= policy->rule_count)
        return NULL;
    return &policy->rules[index];
}

/* Purpose: project one parsed template dtype into the corresponding policy qtype class.
 * Inputs: canonical tensor dtype.
 * Effects: none.
 * Failure: unsupported dtypes yield the typed unknown qtype.
 * Boundary: projection records source policy and never performs numeric conversion. */
static yvex_quant_qtype template_qtype_from_dtype(yvex_dtype dtype) {
    size_t index;
    for (index = 0u; index < sizeof(qtype_dtypes) / sizeof(qtype_dtypes[0]); ++index)
        if (qtype_dtypes[index].dtype == dtype)
            return qtype_dtypes[index].qtype;
    return YVEX_QUANT_QTYPE_OTHER;
}

/* Purpose: detect whether a role/qtype rule already exists in the current policy. */
static int qp_has_role_qtype(const yvex_quant_policy *policy, yvex_tensor_role role,
                             yvex_quant_qtype qtype) {
    unsigned long long i;

    for (i = 0; i < policy->rule_count; ++i) {
        const yvex_quant_policy_rule *rule = &policy->rules[i];
        if (rule->selector_kind == YVEX_QUANT_SELECTOR_ROLE && rule->role == role &&
            rule->qtype == qtype) {
            return 1;
        }
    }
    return 0;
}

/* Purpose: derive one role-keyed policy from an admitted GGUF template descriptor set.
 * Inputs: output slot, template path, architecture spelling, and typed error sink.
 * Effects: opens bounded artifact views and allocates one independently owned policy.
 * Failure: open, parse, allocation, or validation failure releases every partial owner.
 * Boundary: derivation reads template metadata only and performs no source quantization. */
int yvex_quant_policy_create_from_template(yvex_quant_policy **out, const char *template_path,
                                           const char *architecture, yvex_error *err) {
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_quant_policy *policy = NULL;
    unsigned long long i;
    int rc;

    if (!out || !template_path || !architecture) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_derive",
                       "out, template_path, and architecture are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    policy = (yvex_quant_policy *)calloc(1, sizeof(*policy));
    if (!policy) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_derive", "policy allocation failed");
        return YVEX_ERR_NOMEM;
    }
    policy->name = yvex_core_strdup("template-derived-policy");
    policy->architecture = yvex_core_strdup(architecture);
    policy->source_kind = yvex_core_strdup("template-derived");
    policy->template_path = yvex_core_strdup(template_path);
    if (!policy->name || !policy->architecture || !policy->source_kind || !policy->template_path) {
        rc = YVEX_ERR_NOMEM;
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_derive",
                       "policy string allocation failed");
        goto done;
    }

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = template_path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;
    rc = yvex_artifact_open(&artifact, &artifact_options, err);
    if (rc == YVEX_OK)
        rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK)
        rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc != YVEX_OK)
        goto done;

    for (i = 0; i < yvex_tensor_table_count(tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, i);
        yvex_quant_qtype qtype;

        if (!tensor)
            continue;
        qtype = template_qtype_from_dtype(tensor->dtype);
        if (tensor->role != YVEX_TENSOR_ROLE_UNKNOWN) {
            if (qp_has_role_qtype(policy, tensor->role, qtype))
                continue;
            rc = policy_add_rule(policy, YVEX_QUANT_SELECTOR_ROLE,
                                 yvex_tensor_role_name(tensor->role), tensor->role, qtype, 0, err);
        } else {
            rc = policy_add_rule(policy, YVEX_QUANT_SELECTOR_TENSOR_NAME, tensor->name,
                                 YVEX_TENSOR_ROLE_UNKNOWN, qtype, 0, err);
        }
        if (rc != YVEX_OK)
            goto done;
    }
    rc = yvex_quant_policy_validate(policy, NULL, err);
    if (rc != YVEX_OK)
        goto done;
    *out = policy;
    policy = NULL;
    rc = YVEX_OK;

done:
    yvex_quant_policy_close(policy);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return rc;
}

/* Purpose: parse the policy source-kind and template-path object.
 * Inputs: bounded cursor and policy under construction.
 * Effects: replaces owned source strings and skips unknown fields structurally.
 * Failure: malformed syntax or allocation failure returns typed refusal.
 * Boundary: path parsing does not open the referenced template. */
static int qj_parse_source(policy_json *j, yvex_quant_policy *policy) {
    int rc = policy_json_expect(j, '{');
    if (rc != YVEX_OK)
        return rc;
    while (j->cursor.cursor < j->cursor.end) {
        char *key;
        policy_json_space(j);
        if (j->cursor.cursor < j->cursor.end && *j->cursor.cursor == '}') {
            j->cursor.cursor++;
            return YVEX_OK;
        }
        key = policy_json_string(j);
        if (!key)
            return yvex_error_code(j->err);
        rc = policy_json_expect(j, ':');
        if (rc != YVEX_OK) {
            free(key);
            return rc;
        }
        if (strcmp(key, "kind") == 0) {
            free(policy->source_kind);
            policy->source_kind = policy_json_string(j);
        } else if (strcmp(key, "template_path") == 0) {
            free(policy->template_path);
            policy->template_path = policy_json_string(j);
        } else {
            rc = policy_json_skip(j);
        }
        free(key);
        if (rc != YVEX_OK)
            return rc;
        policy_json_space(j);
        if (j->cursor.cursor < j->cursor.end && *j->cursor.cursor == ',')
            j->cursor.cursor++;
    }
    return policy_json_fail(j, "unterminated source object");
}

/* Purpose: parse and append one complete typed policy rule.
 * Inputs: bounded cursor and policy under construction.
 * Effects: allocates temporary tokens and one owned rule on success.
 * Failure: missing fields, malformed values, or allocation failure append no partial rule.
 * Boundary: parsing a qtype choice does not claim its numeric implementation. */
static int qj_parse_rule(policy_json *j, yvex_quant_policy *policy) {
    char *selector_kind = NULL;
    char *selector = NULL;
    char *qtype = NULL;
    int requires_imatrix = 0;
    yvex_quant_selector_kind kind;
    yvex_quant_qtype qt;
    yvex_tensor_role role = YVEX_TENSOR_ROLE_UNKNOWN;
    int rc = policy_json_expect(j, '{');

    if (rc != YVEX_OK)
        return rc;
    while (j->cursor.cursor < j->cursor.end) {
        char *key;
        policy_json_space(j);
        if (j->cursor.cursor < j->cursor.end && *j->cursor.cursor == '}') {
            j->cursor.cursor++;
            break;
        }
        key = policy_json_string(j);
        if (!key) {
            rc = yvex_error_code(j->err);
            goto done;
        }
        rc = policy_json_expect(j, ':');
        if (rc != YVEX_OK) {
            free(key);
            goto done;
        }
        if (strcmp(key, "selector_kind") == 0) {
            free(selector_kind);
            selector_kind = policy_json_string(j);
            if (!selector_kind)
                rc = yvex_error_code(j->err);
        } else if (strcmp(key, "selector") == 0) {
            free(selector);
            selector = policy_json_string(j);
            if (!selector)
                rc = yvex_error_code(j->err);
        } else if (strcmp(key, "qtype") == 0) {
            free(qtype);
            qtype = policy_json_string(j);
            if (!qtype)
                rc = yvex_error_code(j->err);
        } else if (strcmp(key, "requires_imatrix") == 0) {
            rc = policy_json_bool(j, &requires_imatrix);
        } else {
            rc = policy_json_skip(j);
        }
        free(key);
        if (rc != YVEX_OK)
            goto done;
        policy_json_space(j);
        if (j->cursor.cursor < j->cursor.end && *j->cursor.cursor == ',')
            j->cursor.cursor++;
    }
    if (!selector_kind || !selector || !qtype) {
        rc = policy_json_fail(j, "policy rule missing selector_kind, selector, or qtype");
        goto done;
    }
    kind = selector_from_name(selector_kind);
    qt = qtype_from_name(qtype);
    if (kind == YVEX_QUANT_SELECTOR_ROLE)
        role = role_from_name(selector);
    rc = policy_add_rule(policy, kind, selector, role, qt, requires_imatrix, j->err);

done:
    free(selector_kind);
    free(selector);
    free(qtype);
    return rc;
}

/* Purpose: parse the ordered array of physical-encoding rules.
 * Inputs: bounded cursor and policy under construction.
 * Effects: appends complete rules in document order.
 * Failure: malformed delimiters or rows stop with typed format refusal.
 * Boundary: document order is policy evidence, not execution scheduling. */
static int qj_parse_rules(policy_json *j, yvex_quant_policy *policy) {
    int rc = policy_json_expect(j, '[');
    if (rc != YVEX_OK)
        return rc;
    policy_json_space(j);
    if (j->cursor.cursor < j->cursor.end && *j->cursor.cursor == ']') {
        j->cursor.cursor++;
        return YVEX_OK;
    }
    while (j->cursor.cursor < j->cursor.end) {
        rc = qj_parse_rule(j, policy);
        if (rc != YVEX_OK)
            return rc;
        policy_json_space(j);
        if (j->cursor.cursor < j->cursor.end && *j->cursor.cursor == ',') {
            j->cursor.cursor++;
            continue;
        }
        if (j->cursor.cursor < j->cursor.end && *j->cursor.cursor == ']') {
            j->cursor.cursor++;
            return YVEX_OK;
        }
        return policy_json_fail(j, "malformed rules array");
    }
    return policy_json_fail(j, "unterminated rules array");
}

/* Purpose: parse one complete bounded JSON document into an owned policy.
 * Inputs: output slot, source path, and typed error sink.
 * Effects: reads metadata bytes, allocates policy state, and stores complete rules.
 * Failure: schema, grammar, I/O, or allocation failure releases all partial state.
 * Boundary: policy parsing performs zero source payload reads. */
static int policy_parse_json(yvex_quant_policy **out, const char *path, yvex_error *err) {
    yvex_quant_policy *policy;
    policy_json j;
    char *buf = NULL;
    size_t len = 0u;
    int rc;

    if (!out || !path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_json", "out and path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    rc = policy_json_read(path, &buf, &len, "quant_policy_json", err);
    if (rc != YVEX_OK)
        return rc;
    policy = (yvex_quant_policy *)calloc(1, sizeof(*policy));
    if (!policy) {
        free(buf);
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_json", "policy allocation failed");
        return YVEX_ERR_NOMEM;
    }
    yvex_json_init(&j.cursor, buf, len);
    j.path = path;
    j.context = "quant_policy_json";
    j.err = err;
    rc = policy_json_expect(&j, '{');
    if (rc != YVEX_OK)
        goto fail;
    while (j.cursor.cursor < j.cursor.end) {
        char *key;
        policy_json_space(&j);
        if (j.cursor.cursor < j.cursor.end && *j.cursor.cursor == '}') {
            j.cursor.cursor++;
            break;
        }
        key = policy_json_string(&j);
        if (!key) {
            rc = yvex_error_code(err);
            goto fail;
        }
        rc = policy_json_expect(&j, ':');
        if (rc != YVEX_OK) {
            free(key);
            goto fail;
        }
        if (strcmp(key, "schema") == 0) {
            char *schema = policy_json_string(&j);
            if (!schema) {
                free(key);
                rc = yvex_error_code(err);
                goto fail;
            }
            if (strcmp(schema, "yvex.quant_policy.v1") != 0) {
                free(schema);
                free(key);
                rc = policy_json_fail(&j, "unsupported quant policy schema");
                goto fail;
            }
            free(schema);
        } else if (strcmp(key, "name") == 0) {
            free(policy->name);
            policy->name = policy_json_string(&j);
            if (!policy->name)
                rc = yvex_error_code(err);
        } else if (strcmp(key, "architecture") == 0) {
            free(policy->architecture);
            policy->architecture = policy_json_string(&j);
            if (!policy->architecture)
                rc = yvex_error_code(err);
        } else if (strcmp(key, "source") == 0) {
            rc = qj_parse_source(&j, policy);
        } else if (strcmp(key, "rules") == 0) {
            rc = qj_parse_rules(&j, policy);
        } else {
            rc = policy_json_skip(&j);
        }
        free(key);
        if (rc != YVEX_OK)
            goto fail;
        policy_json_space(&j);
        if (j.cursor.cursor < j.cursor.end && *j.cursor.cursor == ',')
            j.cursor.cursor++;
    }
    if (!policy->name)
        policy->name = yvex_core_strdup("unnamed-policy");
    if (!policy->architecture)
        policy->architecture = yvex_core_strdup("unknown");
    if (!policy->name || !policy->architecture) {
        rc = YVEX_ERR_NOMEM;
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_json", "policy string allocation failed");
        goto fail;
    }
    *out = policy;
    free(buf);
    return YVEX_OK;

fail:
    yvex_quant_policy_close(policy);
    free(buf);
    return rc;
}

/* Purpose: serialize one complete quantization policy deterministically.
 * Inputs: destination path, immutable policy, and typed error sink.
 * Effects: creates or replaces the requested JSON file and closes its stream.
 * Failure: invalid arguments, open failure, or close failure return typed errors.
 * Boundary: the writer publishes policy metadata, never encoded tensor bytes. */
static int policy_write_json_file(const char *out_path, const yvex_quant_policy *policy,
                                  yvex_error *err) {
    FILE *fp;
    unsigned long long i;

    if (!out_path || !policy) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_json",
                       "out_path and policy are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fp = fopen(out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_policy_json", "cannot open output policy: %s",
                        out_path);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.quant_policy.v1\",\n");
    fprintf(fp, "  \"name\": ");
    yvex_file_json_write_string(fp, policy->name);
    fprintf(fp, ",\n  \"architecture\": ");
    yvex_file_json_write_string(fp, policy->architecture);
    fprintf(fp, ",\n");
    if (policy->source_kind || policy->template_path) {
        fprintf(fp, "  \"source\": {\n");
        fprintf(fp, "    \"kind\": ");
        yvex_file_json_write_string(fp,
                                 policy->source_kind ? policy->source_kind : "template-derived");
        fprintf(fp, ",\n    \"template_path\": ");
        yvex_file_json_write_string(fp, policy->template_path);
        fprintf(fp, "\n  },\n");
    }
    fprintf(fp, "  \"rules\": [\n");
    for (i = 0; i < policy->rule_count; ++i) {
        const yvex_quant_policy_rule *rule = &policy->rules[i];
        fprintf(fp, "    {\"selector_kind\": ");
        yvex_file_json_write_string(fp, yvex_quant_selector_kind_name(rule->selector_kind));
        fprintf(fp, ", \"selector\": ");
        yvex_file_json_write_string(fp, rule->selector);
        fprintf(fp, ", \"qtype\": ");
        yvex_file_json_write_string(fp, yvex_quant_qtype_name(rule->qtype));
        fprintf(fp, ", \"requires_imatrix\": %s}%s\n", rule->requires_imatrix ? "true" : "false",
                i + 1u == policy->rule_count ? "" : ",");
    }
    fprintf(fp, "  ]\n}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_policy_json", "failed closing output policy: %s",
                        out_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

/* Purpose: project a template dtype into the qtype expected during policy validation.
 * Inputs: canonical dtype.
 * Effects: returns a value without mutation.
 * Failure: unsupported dtypes map to OTHER for explicit mismatch handling.
 * Boundary: projection does not admit a codec. */
static yvex_quant_qtype validate_qtype_from_dtype(yvex_dtype dtype) {
    return template_qtype_from_dtype(dtype);
}

/* Purpose: derive final policy support counters after optional template validation.
 * Inputs: mutable policy, extra issue count, and fatality flag.
 * Effects: refreshes rule capability facts and the embedded summary.
 * Failure: unsupported rules remain explicit partial or invalid state.
 * Boundary: registry projection does not execute numeric kernels. */
static void qp_set_summary(yvex_quant_policy *policy, unsigned long long extra_issues, int fatal) {
    unsigned long long i;

    memset(&policy->summary, 0, sizeof(policy->summary));
    policy->summary.name = policy->name;
    policy->summary.architecture = policy->architecture;
    policy->summary.rule_count = policy->rule_count;
    policy->summary.status =
        policy->rule_count > 0 ? YVEX_QUANT_POLICY_STATUS_VALID : YVEX_QUANT_POLICY_STATUS_INVALID;
    policy->summary.issue_count = extra_issues;
    if (extra_issues > 0) {
        policy->summary.status =
            fatal ? YVEX_QUANT_POLICY_STATUS_INVALID : YVEX_QUANT_POLICY_STATUS_PARTIAL;
    }

    for (i = 0; i < policy->rule_count; ++i) {
        yvex_quant_policy_rule *rule = &policy->rules[i];
        rule->storage_supported = qtype_storage_supported(rule->qtype);
        rule->compute_supported = qtype_compute_supported(rule->qtype);
        if (rule->requires_imatrix) {
            policy->summary.requires_imatrix_count++;
            policy->summary.issue_count++;
            if (policy->summary.status == YVEX_QUANT_POLICY_STATUS_VALID) {
                policy->summary.status = YVEX_QUANT_POLICY_STATUS_PARTIAL;
            }
        }
        if (rule->storage_supported)
            policy->summary.storage_supported_count++;
        else {
            policy->summary.issue_count++;
            if (policy->summary.status == YVEX_QUANT_POLICY_STATUS_VALID) {
                policy->summary.status = YVEX_QUANT_POLICY_STATUS_PARTIAL;
            }
        }
        if (rule->compute_supported)
            policy->summary.compute_supported_count++;
        else {
            policy->summary.issue_count++;
            if (policy->summary.status == YVEX_QUANT_POLICY_STATUS_VALID) {
                policy->summary.status = YVEX_QUANT_POLICY_STATUS_PARTIAL;
            }
        }
        if (rule->qtype == YVEX_QUANT_QTYPE_UNKNOWN ||
            rule->selector_kind == YVEX_QUANT_SELECTOR_UNKNOWN ||
            (rule->selector_kind == YVEX_QUANT_SELECTOR_ROLE &&
             rule->role == YVEX_TENSOR_ROLE_UNKNOWN)) {
            policy->summary.issue_count++;
            policy->summary.status = YVEX_QUANT_POLICY_STATUS_INVALID;
        }
    }
}

/* Purpose: match a policy selector containing at most one wildcard against one tensor name.
 * Inputs: optional pattern and canonical tensor name.
 * Effects: none.
 * Failure: null inputs or incompatible prefix/suffix return false.
 * Boundary: lexical matching cannot infer tensor role or transformation semantics. */
static int qp_match_pattern(const char *pattern, const char *name) {
    const char *star;
    size_t prefix_len;
    size_t suffix_len;
    size_t name_len;

    if (!pattern || !name)
        return 0;
    if (strcmp(pattern, "*") == 0)
        return 1;
    star = strchr(pattern, '*');
    if (!star)
        return strcmp(pattern, name) == 0;
    prefix_len = (size_t)(star - pattern);
    suffix_len = strlen(star + 1);
    name_len = strlen(name);
    if (name_len < prefix_len + suffix_len)
        return 0;
    if (strncmp(pattern, name, prefix_len) != 0)
        return 0;
    if (suffix_len > 0 && strcmp(name + name_len - suffix_len, star + 1) != 0)
        return 0;
    return 1;
}

/* Purpose: compare every policy rule against an admitted GGUF template tensor table.
 * Inputs: policy, template path, issue counter, and typed error sink.
 * Effects: opens bounded artifact/parser views and increments semantic mismatch counts.
 * Failure: artifact or parser failure closes all resources and returns typed refusal.
 * Boundary: template validation reads metadata only and performs no tensor execution. */
static int qp_validate_template(yvex_quant_policy *policy, const char *template_path,
                                unsigned long long *issues, yvex_error *err) {
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    unsigned long long i;
    int rc;

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = template_path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;
    rc = yvex_artifact_open(&artifact, &artifact_options, err);
    if (rc == YVEX_OK)
        rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK)
        rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc != YVEX_OK)
        goto done;

    for (i = 0; i < policy->rule_count; ++i) {
        const yvex_quant_policy_rule *rule = &policy->rules[i];
        unsigned long long j;
        int matched = 0;

        for (j = 0; j < yvex_tensor_table_count(tensors); ++j) {
            const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, j);
            int applies = 0;

            if (!tensor)
                continue;
            if (rule->selector_kind == YVEX_QUANT_SELECTOR_DEFAULT)
                applies = 1;
            else if (rule->selector_kind == YVEX_QUANT_SELECTOR_ROLE && tensor->role == rule->role)
                applies = 1;
            else if (rule->selector_kind == YVEX_QUANT_SELECTOR_TENSOR_NAME &&
                     strcmp(rule->selector, tensor->name) == 0)
                applies = 1;
            else if (rule->selector_kind == YVEX_QUANT_SELECTOR_TENSOR_PATTERN &&
                     qp_match_pattern(rule->selector, tensor->name))
                applies = 1;
            if (!applies)
                continue;
            matched = 1;
            if (validate_qtype_from_dtype(tensor->dtype) != rule->qtype) {
                (*issues)++;
            }
        }
        if (!matched)
            (*issues)++;
    }

done:
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return rc;
}

/* Purpose: validate policy identity, rules, capabilities, and optional template equivalence.
 * Inputs: mutable owned policy, optional template path, and typed error sink.
 * Effects: may allocate default identity strings and replaces the embedded summary.
 * Failure: invalid input, allocation, or template admission failure returns typed refusal.
 * Boundary: successful validation admits a plan document, not quantized output. */
int yvex_quant_policy_validate(yvex_quant_policy *policy, const char *template_path,
                               yvex_error *err) {
    unsigned long long template_issues = 0;
    int rc = YVEX_OK;

    if (!policy) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_validate", "policy is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!policy->name)
        policy->name = yvex_core_strdup("unnamed-policy");
    if (!policy->architecture)
        policy->architecture = yvex_core_strdup("unknown");
    if (!policy->name || !policy->architecture) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_validate",
                       "policy string allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (template_path) {
        rc = qp_validate_template(policy, template_path, &template_issues, err);
        if (rc != YVEX_OK)
            return rc;
    }
    qp_set_summary(policy, template_issues, policy->rule_count == 0);
    return YVEX_OK;
}
