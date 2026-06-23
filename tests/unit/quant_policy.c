/*
 * YVEX - Quant policy tests
 *
 * File: tests/test_quant_policy.c
 * Layer: test
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <yvex/yvex.h>

#include "test.h"

static int write_policy(const char *path, const char *qtype)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    fprintf(fp,
            "{\n"
            "  \"schema\": \"yvex.quant_policy.v1\",\n"
            "  \"name\": \"test-policy\",\n"
            "  \"architecture\": \"deepseek4\",\n"
            "  \"rules\": [\n"
            "    {\"selector_kind\": \"role\", \"selector\": \"token_embedding\", \"qtype\": \"%s\", \"requires_imatrix\": false},\n"
            "    {\"selector_kind\": \"pattern\", \"selector\": \"blk.*.ffn.experts.*\", \"qtype\": \"Q2_K\", \"requires_imatrix\": true}\n"
            "  ]\n"
            "}\n",
            qtype);
    return fclose(fp) == 0;
}

static int test_names(void)
{
    YVEX_TEST_ASSERT_STREQ(yvex_quant_qtype_name(YVEX_QUANT_QTYPE_Q8_0), "Q8_0", "qtype name");
    YVEX_TEST_ASSERT_STREQ(yvex_quant_selector_kind_name(YVEX_QUANT_SELECTOR_ROLE), "role", "selector name");
    YVEX_TEST_ASSERT_STREQ(yvex_quant_policy_status_name(YVEX_QUANT_POLICY_STATUS_PARTIAL), "quant-policy-partial", "status name");
    YVEX_TEST_ASSERT_STREQ(yvex_quant_policy_issue_kind_name(YVEX_QUANT_POLICY_ISSUE_UNKNOWN_QTYPE), "unknown_qtype", "issue name");
    return 0;
}

static int test_open_validate_write(void)
{
    const char *dir = "build/tests/quant-policy";
    const char *path = "build/tests/quant-policy/policy.json";
    const char *out = "build/tests/quant-policy/written.json";
    yvex_quant_policy *policy = NULL;
    yvex_quant_policy_summary summary;
    const yvex_quant_policy_rule *rule;
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(system("rm -rf build/tests/quant-policy && mkdir -p build/tests/quant-policy") == 0,
                     "create policy dir");
    (void)dir;
    YVEX_TEST_ASSERT(write_policy(path, "Q8_0"), "write policy");
    yvex_error_clear(&err);
    rc = yvex_quant_policy_open(&policy, path, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open policy");
    YVEX_TEST_ASSERT(yvex_quant_policy_get_summary(policy, &summary, &err) == YVEX_OK, "summary");
    YVEX_TEST_ASSERT(summary.rule_count == 2, "rule count");
    YVEX_TEST_ASSERT(summary.requires_imatrix_count == 1, "requires imatrix count");
    YVEX_TEST_ASSERT(summary.storage_supported_count == 1, "Q8_0 storage supported only");
    YVEX_TEST_ASSERT(summary.compute_supported_count == 0, "quant compute unsupported");
    rule = yvex_quant_policy_rule_at(policy, 0);
    YVEX_TEST_ASSERT(rule && rule->role == YVEX_TENSOR_ROLE_TOKEN_EMBEDDING, "role parsed");
    YVEX_TEST_ASSERT(rule->qtype == YVEX_QUANT_QTYPE_Q8_0, "qtype parsed");
    YVEX_TEST_ASSERT(yvex_quant_policy_write_json(out, policy, &err) == YVEX_OK, "write policy JSON");
    yvex_quant_policy_close(policy);
    return 0;
}

static int test_reject_unknown_qtype(void)
{
    const char *path = "build/tests/quant-policy/bad.json";
    yvex_quant_policy *policy = NULL;
    yvex_quant_policy_summary summary;
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(write_policy(path, "BAD_QTYPE"), "write bad policy");
    yvex_error_clear(&err);
    rc = yvex_quant_policy_open(&policy, path, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "unknown qtype opens as invalid policy");
    YVEX_TEST_ASSERT(yvex_quant_policy_get_summary(policy, &summary, &err) == YVEX_OK, "bad summary");
    YVEX_TEST_ASSERT(summary.status == YVEX_QUANT_POLICY_STATUS_INVALID, "unknown qtype invalid");
    yvex_quant_policy_close(policy);
    return 0;
}

static int test_derive_fixture(void)
{
    yvex_quant_policy *policy = NULL;
    yvex_quant_policy_summary summary;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_quant_policy_create_from_template(&policy,
                                                "tests/fixtures/gguf/valid-tokenizer-simple.gguf",
                                                "llama",
                                                &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "derive fixture policy");
    YVEX_TEST_ASSERT(yvex_quant_policy_get_summary(policy, &summary, &err) == YVEX_OK, "derive summary");
    YVEX_TEST_ASSERT(summary.rule_count >= 1, "derived rules");
    yvex_quant_policy_close(policy);
    return 0;
}

int yvex_test_quant_policy(void)
{
    if (test_names() != 0) return 1;
    if (test_open_validate_write() != 0) return 1;
    if (test_reject_unknown_qtype() != 0) return 1;
    if (test_derive_fixture() != 0) return 1;
    return 0;
}
