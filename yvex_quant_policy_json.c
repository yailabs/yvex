/*
 * YVEX - Quant policy JSON parser/writer
 *
 * File: yvex_quant_policy_json.c
 * Layer: tool-plane implementation
 */
#include "yvex_quant_policy_internal.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *p;
    const char *end;
    const char *path;
    yvex_error *err;
} qp_json;

static void qj_skip_ws(qp_json *j)
{
    while (j->p < j->end && isspace((unsigned char)*j->p)) j->p++;
}

static int qj_fail(qp_json *j, const char *msg)
{
    yvex_error_setf(j->err, YVEX_ERR_FORMAT, "quant_policy_json", "%s in %s", msg, j->path);
    return YVEX_ERR_FORMAT;
}

static int qj_expect(qp_json *j, char ch)
{
    qj_skip_ws(j);
    if (j->p >= j->end || *j->p != ch) return qj_fail(j, "unexpected JSON token");
    j->p++;
    return YVEX_OK;
}

static char *qj_string(qp_json *j)
{
    char *out;
    size_t cap;
    size_t n = 0;

    qj_skip_ws(j);
    if (j->p >= j->end || *j->p != '"') {
        qj_fail(j, "expected JSON string");
        return NULL;
    }
    j->p++;
    cap = (size_t)(j->end - j->p) + 1u;
    out = (char *)malloc(cap);
    if (!out) {
        yvex_error_set(j->err, YVEX_ERR_NOMEM, "quant_policy_json", "string allocation failed");
        return NULL;
    }
    while (j->p < j->end) {
        char ch = *j->p++;
        if (ch == '"') {
            out[n] = '\0';
            return out;
        }
        if (ch == '\\') {
            if (j->p >= j->end) {
                free(out);
                qj_fail(j, "unterminated escape");
                return NULL;
            }
            ch = *j->p++;
            if (ch == '"' || ch == '\\' || ch == '/') out[n++] = ch;
            else if (ch == 'n') out[n++] = '\n';
            else if (ch == 'r') out[n++] = '\r';
            else if (ch == 't') out[n++] = '\t';
            else {
                free(out);
                qj_fail(j, "unsupported string escape");
                return NULL;
            }
        } else {
            out[n++] = ch;
        }
    }
    free(out);
    qj_fail(j, "unterminated string");
    return NULL;
}

static int qj_skip_value(qp_json *j);

static int qj_skip_literal(qp_json *j, const char *lit)
{
    size_t n = strlen(lit);
    if ((size_t)(j->end - j->p) < n || strncmp(j->p, lit, n) != 0) {
        return qj_fail(j, "unexpected literal");
    }
    j->p += n;
    return YVEX_OK;
}

static int qj_skip_object(qp_json *j)
{
    int rc = qj_expect(j, '{');
    if (rc != YVEX_OK) return rc;
    qj_skip_ws(j);
    if (j->p < j->end && *j->p == '}') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        char *key = qj_string(j);
        if (!key) return yvex_error_code(j->err);
        free(key);
        rc = qj_expect(j, ':');
        if (rc != YVEX_OK) return rc;
        rc = qj_skip_value(j);
        if (rc != YVEX_OK) return rc;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            return YVEX_OK;
        }
        return qj_fail(j, "malformed object");
    }
    return qj_fail(j, "unterminated object");
}

static int qj_skip_array(qp_json *j)
{
    int rc = qj_expect(j, '[');
    if (rc != YVEX_OK) return rc;
    qj_skip_ws(j);
    if (j->p < j->end && *j->p == ']') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        rc = qj_skip_value(j);
        if (rc != YVEX_OK) return rc;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == ']') {
            j->p++;
            return YVEX_OK;
        }
        return qj_fail(j, "malformed array");
    }
    return qj_fail(j, "unterminated array");
}

static int qj_skip_value(qp_json *j)
{
    char *s;
    qj_skip_ws(j);
    if (j->p >= j->end) return qj_fail(j, "expected value");
    if (*j->p == '{') return qj_skip_object(j);
    if (*j->p == '[') return qj_skip_array(j);
    if (*j->p == '"') {
        s = qj_string(j);
        if (!s) return yvex_error_code(j->err);
        free(s);
        return YVEX_OK;
    }
    if (*j->p == 't') return qj_skip_literal(j, "true");
    if (*j->p == 'f') return qj_skip_literal(j, "false");
    if (*j->p == 'n') return qj_skip_literal(j, "null");
    while (j->p < j->end && (isdigit((unsigned char)*j->p) || *j->p == '-')) j->p++;
    return YVEX_OK;
}

static int qj_bool(qp_json *j, int *out)
{
    qj_skip_ws(j);
    if (j->p < j->end && *j->p == 't') {
        int rc = qj_skip_literal(j, "true");
        *out = 1;
        return rc;
    }
    if (j->p < j->end && *j->p == 'f') {
        int rc = qj_skip_literal(j, "false");
        *out = 0;
        return rc;
    }
    return qj_fail(j, "expected boolean");
}

static int qj_parse_source(qp_json *j, yvex_quant_policy *policy)
{
    int rc = qj_expect(j, '{');
    if (rc != YVEX_OK) return rc;
    while (j->p < j->end) {
        char *key;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            return YVEX_OK;
        }
        key = qj_string(j);
        if (!key) return yvex_error_code(j->err);
        rc = qj_expect(j, ':');
        if (rc != YVEX_OK) {
            free(key);
            return rc;
        }
        if (strcmp(key, "kind") == 0) {
            free(policy->source_kind);
            policy->source_kind = qj_string(j);
        } else if (strcmp(key, "template_path") == 0) {
            free(policy->template_path);
            policy->template_path = qj_string(j);
        } else {
            rc = qj_skip_value(j);
        }
        free(key);
        if (rc != YVEX_OK) return rc;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') j->p++;
    }
    return qj_fail(j, "unterminated source object");
}

static int qj_parse_rule(qp_json *j, yvex_quant_policy *policy)
{
    char *selector_kind = NULL;
    char *selector = NULL;
    char *qtype = NULL;
    int requires_imatrix = 0;
    yvex_quant_selector_kind kind;
    yvex_quant_qtype qt;
    yvex_tensor_role role = YVEX_TENSOR_ROLE_UNKNOWN;
    int rc = qj_expect(j, '{');

    if (rc != YVEX_OK) return rc;
    while (j->p < j->end) {
        char *key;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            break;
        }
        key = qj_string(j);
        if (!key) {
            rc = yvex_error_code(j->err);
            goto done;
        }
        rc = qj_expect(j, ':');
        if (rc != YVEX_OK) {
            free(key);
            goto done;
        }
        if (strcmp(key, "selector_kind") == 0) {
            free(selector_kind);
            selector_kind = qj_string(j);
            if (!selector_kind) rc = yvex_error_code(j->err);
        } else if (strcmp(key, "selector") == 0) {
            free(selector);
            selector = qj_string(j);
            if (!selector) rc = yvex_error_code(j->err);
        } else if (strcmp(key, "qtype") == 0) {
            free(qtype);
            qtype = qj_string(j);
            if (!qtype) rc = yvex_error_code(j->err);
        } else if (strcmp(key, "requires_imatrix") == 0) {
            rc = qj_bool(j, &requires_imatrix);
        } else {
            rc = qj_skip_value(j);
        }
        free(key);
        if (rc != YVEX_OK) goto done;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') j->p++;
    }
    if (!selector_kind || !selector || !qtype) {
        rc = qj_fail(j, "policy rule missing selector_kind, selector, or qtype");
        goto done;
    }
    kind = yvex_quant_selector_kind_from_name(selector_kind);
    qt = yvex_quant_qtype_from_name(qtype);
    if (kind == YVEX_QUANT_SELECTOR_ROLE) role = yvex_quant_role_from_name(selector);
    rc = yvex_quant_policy_add_rule(policy, kind, selector, role, qt, requires_imatrix, j->err);

done:
    free(selector_kind);
    free(selector);
    free(qtype);
    return rc;
}

static int qj_parse_rules(qp_json *j, yvex_quant_policy *policy)
{
    int rc = qj_expect(j, '[');
    if (rc != YVEX_OK) return rc;
    qj_skip_ws(j);
    if (j->p < j->end && *j->p == ']') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        rc = qj_parse_rule(j, policy);
        if (rc != YVEX_OK) return rc;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == ']') {
            j->p++;
            return YVEX_OK;
        }
        return qj_fail(j, "malformed rules array");
    }
    return qj_fail(j, "unterminated rules array");
}

static int qj_read_file(const char *path, char **out, unsigned long long *len, yvex_error *err)
{
    FILE *fp;
    long size;
    char *buf;

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_policy_json", "cannot open policy: %s", path);
        return YVEX_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "quant_policy_json", "cannot size policy: %s", path);
        return YVEX_ERR_IO;
    }
    buf = (char *)malloc((size_t)size + 1u);
    if (!buf) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_json", "policy read allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "quant_policy_json", "cannot read policy: %s", path);
        return YVEX_ERR_IO;
    }
    fclose(fp);
    buf[size] = '\0';
    *out = buf;
    *len = (unsigned long long)size;
    return YVEX_OK;
}

int yvex_quant_policy_parse_json(yvex_quant_policy **out, const char *path, yvex_error *err)
{
    yvex_quant_policy *policy;
    qp_json j;
    char *buf = NULL;
    unsigned long long len = 0;
    int rc;

    if (!out || !path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_json", "out and path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    rc = qj_read_file(path, &buf, &len, err);
    if (rc != YVEX_OK) return rc;
    policy = (yvex_quant_policy *)calloc(1, sizeof(*policy));
    if (!policy) {
        free(buf);
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_json", "policy allocation failed");
        return YVEX_ERR_NOMEM;
    }
    j.p = buf;
    j.end = buf + len;
    j.path = path;
    j.err = err;
    rc = qj_expect(&j, '{');
    if (rc != YVEX_OK) goto fail;
    while (j.p < j.end) {
        char *key;
        qj_skip_ws(&j);
        if (j.p < j.end && *j.p == '}') {
            j.p++;
            break;
        }
        key = qj_string(&j);
        if (!key) {
            rc = yvex_error_code(err);
            goto fail;
        }
        rc = qj_expect(&j, ':');
        if (rc != YVEX_OK) {
            free(key);
            goto fail;
        }
        if (strcmp(key, "schema") == 0) {
            char *schema = qj_string(&j);
            if (!schema) {
                free(key);
                rc = yvex_error_code(err);
                goto fail;
            }
            if (strcmp(schema, "yvex.quant_policy.v1") != 0) {
                free(schema);
                free(key);
                rc = qj_fail(&j, "unsupported quant policy schema");
                goto fail;
            }
            free(schema);
        } else if (strcmp(key, "name") == 0) {
            free(policy->name);
            policy->name = qj_string(&j);
            if (!policy->name) rc = yvex_error_code(err);
        } else if (strcmp(key, "architecture") == 0) {
            free(policy->architecture);
            policy->architecture = qj_string(&j);
            if (!policy->architecture) rc = yvex_error_code(err);
        } else if (strcmp(key, "source") == 0) {
            rc = qj_parse_source(&j, policy);
        } else if (strcmp(key, "rules") == 0) {
            rc = qj_parse_rules(&j, policy);
        } else {
            rc = qj_skip_value(&j);
        }
        free(key);
        if (rc != YVEX_OK) goto fail;
        qj_skip_ws(&j);
        if (j.p < j.end && *j.p == ',') j.p++;
    }
    if (!policy->name) policy->name = yvex_quant_policy_strdup("unnamed-policy");
    if (!policy->architecture) policy->architecture = yvex_quant_policy_strdup("unknown");
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

static void qj_write_string(FILE *fp, const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");

    fputc('"', fp);
    while (*p) {
        unsigned char ch = *p++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', fp);
            fputc((int)ch, fp);
        } else if (ch == '\n') {
            fputs("\\n", fp);
        } else if (ch == '\r') {
            fputs("\\r", fp);
        } else if (ch == '\t') {
            fputs("\\t", fp);
        } else if (ch < 32) {
            fprintf(fp, "\\u%04x", (unsigned int)ch);
        } else {
            fputc((int)ch, fp);
        }
    }
    fputc('"', fp);
}

int yvex_quant_policy_write_json_file(const char *out_path,
                                      const yvex_quant_policy *policy,
                                      yvex_error *err)
{
    FILE *fp;
    unsigned long long i;

    if (!out_path || !policy) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_json", "out_path and policy are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fp = fopen(out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_policy_json", "cannot open output policy: %s", out_path);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.quant_policy.v1\",\n");
    fprintf(fp, "  \"name\": ");
    qj_write_string(fp, policy->name);
    fprintf(fp, ",\n  \"architecture\": ");
    qj_write_string(fp, policy->architecture);
    fprintf(fp, ",\n");
    if (policy->source_kind || policy->template_path) {
        fprintf(fp, "  \"source\": {\n");
        fprintf(fp, "    \"kind\": ");
        qj_write_string(fp, policy->source_kind ? policy->source_kind : "template-derived");
        fprintf(fp, ",\n    \"template_path\": ");
        qj_write_string(fp, policy->template_path);
        fprintf(fp, "\n  },\n");
    }
    fprintf(fp, "  \"rules\": [\n");
    for (i = 0; i < policy->rule_count; ++i) {
        const yvex_quant_policy_rule *rule = &policy->rules[i];
        fprintf(fp, "    {\"selector_kind\": ");
        qj_write_string(fp, yvex_quant_selector_kind_name(rule->selector_kind));
        fprintf(fp, ", \"selector\": ");
        qj_write_string(fp, rule->selector);
        fprintf(fp, ", \"qtype\": ");
        qj_write_string(fp, yvex_quant_qtype_name(rule->qtype));
        fprintf(fp, ", \"requires_imatrix\": %s}%s\n",
                rule->requires_imatrix ? "true" : "false",
                i + 1u == policy->rule_count ? "" : ",");
    }
    fprintf(fp, "  ]\n}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_policy_json", "failed closing output policy: %s", out_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}
