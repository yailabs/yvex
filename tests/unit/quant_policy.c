
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <yvex/api.h>
#include <yvex/internal/families/deepseek_v4.h>
#include <yvex/internal/quant_numeric.h>

#include "tests/test.h"

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
    YVEX_TEST_ASSERT_STREQ(yvex_quant_qtype_name(YVEX_QUANT_QTYPE_MXFP4), "MXFP4",
                           "MXFP4 policy name");
    YVEX_TEST_ASSERT_STREQ(yvex_quant_selector_kind_name(YVEX_QUANT_SELECTOR_ROLE), "role", "selector name");
    YVEX_TEST_ASSERT_STREQ(yvex_quant_policy_status_name(YVEX_QUANT_POLICY_STATUS_PARTIAL), "quant-policy-partial", "status name");
    YVEX_TEST_ASSERT_STREQ(yvex_quant_policy_issue_kind_name(YVEX_QUANT_POLICY_ISSUE_UNKNOWN_QTYPE), "unknown_qtype", "issue name");
    return 0;
}

static int test_mxfp4_policy(void)
{
    const char *path = "build/tests/quant-policy/mxfp4.json";
    const char *out = "build/tests/quant-policy/mxfp4-written.json";
    yvex_quant_policy *policy = NULL;
    yvex_quant_policy *opened = NULL;
    yvex_quant_policy_summary summary;
    const yvex_quant_policy_rule *rule;
    yvex_error err;

    YVEX_TEST_ASSERT(write_policy(path, "MXFP4"), "write MXFP4 policy");
    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(yvex_quant_policy_open(&policy, path, &err) == YVEX_OK,
                     "open MXFP4 policy");
    YVEX_TEST_ASSERT(yvex_quant_policy_get_summary(policy, &summary, &err) == YVEX_OK &&
                         summary.status == YVEX_QUANT_POLICY_STATUS_VALID &&
                         summary.storage_supported_count == 2u &&
                         summary.compute_supported_count == 2u,
                     "MXFP4 policy consumes canonical storage and compute capability");
    rule = yvex_quant_policy_rule_at(policy, 0u);
    YVEX_TEST_ASSERT(rule && rule->qtype == YVEX_QUANT_QTYPE_MXFP4,
                     "MXFP4 policy action parsed");
    YVEX_TEST_ASSERT(yvex_quant_policy_write_json(out, policy, &err) == YVEX_OK,
                     "write MXFP4 policy");
    YVEX_TEST_ASSERT(yvex_quant_policy_open(&opened, out, &err) == YVEX_OK,
                     "reopen MXFP4 policy");
    rule = yvex_quant_policy_rule_at(opened, 0u);
    YVEX_TEST_ASSERT(rule && rule->qtype == YVEX_QUANT_QTYPE_MXFP4,
                     "MXFP4 policy roundtrip");
    yvex_quant_policy_close(opened);
    yvex_quant_policy_close(policy);
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
    YVEX_TEST_ASSERT(summary.storage_supported_count == 2,
                     "Q8_0 and Q2_K have storage geometry");
    YVEX_TEST_ASSERT(summary.compute_supported_count == 2,
                     "quant policy projects canonical CPU compute truth");
    rule = yvex_quant_policy_rule_at(policy, 0);
    YVEX_TEST_ASSERT(rule && rule->role == YVEX_TENSOR_ROLE_TOKEN_EMBEDDING, "role parsed");
    YVEX_TEST_ASSERT(rule->qtype == YVEX_QUANT_QTYPE_Q8_0, "qtype parsed");
    YVEX_TEST_ASSERT(yvex_quant_policy_write_json(out, policy, &err) == YVEX_OK, "write policy JSON");
    yvex_quant_policy *copy = NULL;
    yvex_quant_policy_summary copied;
    YVEX_TEST_ASSERT(yvex_quant_policy_clone(&copy, policy, &err) == YVEX_OK &&
        yvex_quant_policy_get_summary(copy, &copied, &err) == YVEX_OK &&
        copied.schema_version == 1u && !strcmp(copied.policy_identity, summary.policy_identity),
        "cloning a legacy explicit policy preserves its schema and identity");
    yvex_quant_policy_close(copy);
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

static int write_policy_v2(const char *path, unsigned int priority)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    fprintf(fp,
            "{\n"
            "  \"schema\": \"yvex.quant_policy.v2\",\n"
            "  \"name\": \"conjunctive-policy\",\n"
            "  \"architecture\": \"deepseek4-v4-flash-dspark\",\n"
            "  \"rules\": [\n"
            "    {\"match\": {\"role\": \"moe_expert_gate\", "
            "\"scope\": \"main_layer\", \"layer_first\": 2, \"layer_last\": 41, "
            "\"operation\": \"expert_aggregate\", \"physical_class\": \"quantizable\"}, "
            "\"action\": {\"qtype\": \"IQ2_XXS\", \"calibration\": \"required\", "
            "\"requires_cpu_compute\": true, \"requires_cuda_compute\": true}, "
            "\"priority\": %u, \"label\": \"calibrated routed gate\"},\n"
            "    {\"match\": {\"default\": true}, "
            "\"action\": {\"qtype\": \"Q8_0\", \"calibration\": \"none\", "
            "\"requires_cpu_compute\": true, \"requires_cuda_compute\": true}, "
            "\"priority\": 1, \"label\": \"default\"}\n"
            "  ]\n"
            "}\n", priority);
    return fclose(fp) == 0;
}

static int test_policy_v2_and_presets(void)
{
    static const char *const preset_identities[] = {
        "84a934800f49b2cb7cf031eb18c8c1660ef8250da751e0ea34f65852d7c5333f",
        "adb3f1549440ad39adb2cc08826e659e91ac95c289b9ce75a0efc3dbf89d60a3",
        "5942d4c1bc6ae5d140c72387ff93da14e5ed9a77a6ded3f41ac3f7e9596a0f24",
        "d66294b5e016806f2e283e8b0160e25385302fa6c9db4ac24a49e94140fa0ea9",
        "fe9a0e69e792e64e1dead0f5dd23f732019f79279e2ff2389693f2e5ea7ca581",
        "5f457af00c3c47af62f4ee6ddcdcd15292d569857c6515612c79f6847b75dd5c",
        "d3e8c279e1f5256b61e039e0cb0d462551d0790b73c9bead86789a4d6ec99338",
        "12080587185f1286503749fe157e05599aa8ea8d438e1608d76dcfe1092b283d",
    };
    const char *path = "build/tests/quant-policy/v2.json";
    const char *roundtrip = "build/tests/quant-policy/v2-roundtrip.json";
    const char *mutated = "build/tests/quant-policy/v2-mutated.json";
    yvex_quant_policy *policy = NULL;
    yvex_quant_policy *opened = NULL;
    yvex_quant_policy_summary summary;
    yvex_quant_policy_summary roundtrip_summary;
    const yvex_quant_policy_rule *rule;
    unsigned long long preset;
    const unsigned long long preset_count =
        sizeof(preset_identities) / sizeof(preset_identities[0]);
    char original_identity[YVEX_QUANT_POLICY_IDENTITY_CAP];
    yvex_error err;

    YVEX_TEST_ASSERT(write_policy_v2(path, 200u), "write policy v2");
    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(yvex_quant_policy_open(&policy, path, &err) == YVEX_OK,
                     "open policy v2");
    YVEX_TEST_ASSERT(yvex_quant_policy_validate(policy, NULL, &err) == YVEX_OK,
                     "validate policy v2");
    YVEX_TEST_ASSERT(yvex_quant_policy_get_summary(policy, &summary, &err) == YVEX_OK,
                     "policy v2 summary");
    YVEX_TEST_ASSERT(summary.schema_version == YVEX_QUANT_POLICY_SCHEMA_VERSION,
                     "policy v2 schema");
    YVEX_TEST_ASSERT(summary.status == YVEX_QUANT_POLICY_STATUS_VALID,
                     "policy v2 status");
    YVEX_TEST_ASSERT(summary.rule_count == 2u && summary.requires_imatrix_count == 1u,
                     "policy v2 counts");
    rule = yvex_quant_policy_rule_at(policy, 0u);
    YVEX_TEST_ASSERT(rule && rule->priority == 200u, "priority parsed");
    YVEX_TEST_ASSERT((rule->match_mask & YVEX_QUANT_MATCH_ROLE) &&
                         (rule->match_mask & YVEX_QUANT_MATCH_SCOPE) &&
                         (rule->match_mask & YVEX_QUANT_MATCH_LAYER_RANGE) &&
                         (rule->match_mask & YVEX_QUANT_MATCH_OPERATION) &&
                         (rule->match_mask & YVEX_QUANT_MATCH_PHYSICAL_CLASS),
                     "conjunctive matcher parsed");
    YVEX_TEST_ASSERT(rule->qtype == YVEX_QUANT_QTYPE_IQ2_XXS &&
                         rule->requires_imatrix && rule->requires_cpu_compute &&
                         rule->requires_cuda_compute,
                     "policy v2 action parsed");
    memcpy(original_identity, summary.policy_identity, sizeof(original_identity));
    YVEX_TEST_ASSERT(yvex_quant_policy_identity_validate(policy, &err) == YVEX_OK,
                     "policy identity validates");
    YVEX_TEST_ASSERT(yvex_quant_policy_write_json(roundtrip, policy, &err) == YVEX_OK,
                     "write policy v2");
    YVEX_TEST_ASSERT(yvex_quant_policy_open(&opened, roundtrip, &err) == YVEX_OK,
                     "reopen policy v2");
    YVEX_TEST_ASSERT(yvex_quant_policy_get_summary(opened, &roundtrip_summary, &err) == YVEX_OK,
                     "roundtrip policy summary");
    YVEX_TEST_ASSERT_STREQ(roundtrip_summary.policy_identity, original_identity,
                           "policy v2 roundtrip identity");
    yvex_quant_policy_close(opened);
    opened = NULL;
    YVEX_TEST_ASSERT(write_policy_v2(mutated, 201u), "write priority mutation");
    YVEX_TEST_ASSERT(yvex_quant_policy_open(&opened, mutated, &err) == YVEX_OK,
                     "open priority mutation");
    YVEX_TEST_ASSERT(yvex_quant_policy_get_summary(opened, &roundtrip_summary, &err) == YVEX_OK,
                     "mutation summary");
    YVEX_TEST_ASSERT(strcmp(roundtrip_summary.policy_identity, original_identity) != 0,
                     "priority changes policy identity");
    yvex_quant_policy_close(opened);
    yvex_quant_policy_close(policy);

    YVEX_TEST_ASSERT(yvex_quant_policy_preset_count() == preset_count,
                     "preset catalog count");
    for (preset = 0u; preset < preset_count; ++preset) {
        const char *name = yvex_quant_policy_preset_name(preset);
        policy = NULL;
        YVEX_TEST_ASSERT(name && yvex_quant_policy_preset_open(&policy, name, &err) == YVEX_OK,
                         "open built-in preset");
        YVEX_TEST_ASSERT(yvex_quant_policy_get_summary(policy, &summary, &err) == YVEX_OK &&
                             summary.status == YVEX_QUANT_POLICY_STATUS_VALID &&
                             yvex_quant_policy_identity_validate(policy, &err) == YVEX_OK,
                         "built-in preset is sealed");
        YVEX_TEST_ASSERT_STREQ(summary.policy_identity, preset_identities[preset],
                               "family-owned preset preserves admitted identity");
        yvex_quant_policy *copy = NULL;
        yvex_quant_policy_summary copied = {0};
        YVEX_TEST_ASSERT(yvex_quant_policy_clone(&copy, policy, &err) == YVEX_OK &&
            yvex_quant_policy_get_summary(copy, &copied, &err) == YVEX_OK &&
            !strcmp(copied.policy_identity, summary.policy_identity),
            "native policy borrowing produces an independently owned identity-exact clone");
        yvex_quant_policy_close(policy);
        YVEX_TEST_ASSERT(yvex_quant_policy_identity_validate(copy, &err) == YVEX_OK,
            "clone survives its producer lifetime");
        yvex_quant_policy_close(copy);
    }
    return 0;
}

static int test_dspark_bootstrap_policy(void)
{
    static const yvex_tensor_role exact_roles[] = {
        YVEX_TENSOR_ROLE_DRAFT_FEATURE_NORM,
        YVEX_TENSOR_ROLE_DRAFT_OUTPUT_NORM,
        YVEX_TENSOR_ROLE_DRAFT_MARKOV_EMBEDDING,
        YVEX_TENSOR_ROLE_DRAFT_MARKOV_OUTPUT,
        YVEX_TENSOR_ROLE_DRAFT_CONFIDENCE,
    };
    yvex_quant_policy *policy = NULL;
    yvex_quant_policy_summary summary;
    yvex_error err;
    unsigned long long index, role_index;
    int have_draft_default = 0;

    YVEX_TEST_ASSERT(strlen(YVEX_DEEPSEEK_QUANT_IMATRIX_SOURCE_IDENTITY) == 64u,
                     "DSpark bootstrap calibration has an explicit source identity");
    YVEX_TEST_ASSERT(YVEX_DEEPSEEK_QUANT_IMATRIX_DATASET_IDENTITY[0] != '\0',
                     "DSpark bootstrap calibration has an explicit dataset identity");

    YVEX_TEST_ASSERT(
        yvex_quant_policy_preset_open(
            &policy, "deepseek-v4-flash-dspark-bootstrap-q2-v1", &err) ==
                YVEX_OK &&
            yvex_quant_policy_get_summary(policy, &summary, &err) == YVEX_OK,
        "open the DSpark bootstrap policy");
    for (index = 0ull; index < summary.rule_count; ++index) {
        const yvex_quant_policy_rule *rule =
            yvex_quant_policy_rule_at(policy, index);
        if (rule &&
            (rule->match_mask & (YVEX_QUANT_MATCH_SCOPE |
                                 YVEX_QUANT_MATCH_PHYSICAL_CLASS)) ==
                (YVEX_QUANT_MATCH_SCOPE | YVEX_QUANT_MATCH_PHYSICAL_CLASS) &&
            rule->scope == YVEX_TENSOR_SCOPE_DRAFT &&
            rule->physical_class == YVEX_QUANT_POLICY_PHYSICAL_QUANTIZABLE &&
            rule->qtype == YVEX_QUANT_QTYPE_Q8_0)
            have_draft_default = 1;
    }
    YVEX_TEST_ASSERT(have_draft_default,
                     "DSpark approximable tensors have an explicit conservative policy");
    for (role_index = 0ull;
         role_index < sizeof(exact_roles) / sizeof(exact_roles[0]); ++role_index) {
        int found = 0;
        for (index = 0ull; index < summary.rule_count; ++index) {
            const yvex_quant_policy_rule *rule =
                yvex_quant_policy_rule_at(policy, index);
            if (rule && rule->role == exact_roles[role_index] &&
                rule->scope == YVEX_TENSOR_SCOPE_DRAFT &&
                rule->qtype == YVEX_QUANT_QTYPE_BF16)
                found = 1;
        }
        YVEX_TEST_ASSERT(found,
                         "DSpark control tensors remain exact in the bootstrap policy");
    }
    yvex_quant_policy_close(policy);
    return 0;
}

static int test_explicit_iq2_encoder(void)
{
    const char *path = "build/tests/quant-policy/encoder.json";
    const char *written = "build/tests/quant-policy/encoder-written.json";
    const char *encoders[] = {"legacy-grid-v1", "weighted-grid-v2"};
    const char *invalid[] = {
        "\"schema\":\"yvex.quant_policy.v3\"",
        "\"schema\":\"yvex.quant_policy.v2\",\"iq2_encoder\":\"weighted-grid-v2\"",
        "\"schema\":\"yvex.quant_policy.v3\",\"iq2_encoder\":\"unknown\"",
        "\"schema\":\"yvex.quant_policy.v3\",\"iq2_encoder\":null",
        "\"unexpected\":0,\"schema\":\"yvex.quant_policy.v3\",\"iq2_encoder\":\"weighted-grid-v2\"",
        "\"schema\":\"yvex.quant_policy.v3\",\"iq2_encoder\":\"weighted-grid-v2\",\"iq2_encoder\":\"legacy-grid-v1\""
    };
    yvex_error error = {0};
    char identities[2][YVEX_QUANT_POLICY_IDENTITY_CAP];
    unsigned int i;
    YVEX_TEST_ASSERT(!yvex_quant_policy_iq2_encoder_version(NULL), "absent policy is not a producer");
    for (i = 0u; i < 2u; ++i) {
        yvex_quant_policy *policy = NULL, *copy = NULL, *readback = NULL;
        yvex_quant_policy_summary summary = {0}, observed = {0};
        FILE *file = fopen(path, "wb");
        YVEX_TEST_ASSERT(file, "open explicit producer fixture");
        fprintf(file, "{\"schema\":\"yvex.quant_policy.v3\",\"iq2_encoder\":\"%s\","
                "\"name\":\"same-name\",\"architecture\":\"same-architecture\","
                "\"rules\":[{\"match\":{\"default\":true},\"action\":{\"qtype\":\"IQ2_XXS\",\"calibration\":\"required\"}}]}", encoders[i]);
        YVEX_TEST_ASSERT(!fclose(file) && yvex_quant_policy_open(&policy, path, &error) == YVEX_OK,
                         "explicit producer policy opens");
        YVEX_TEST_ASSERT(yvex_quant_policy_validate(policy, NULL, &error) == YVEX_OK &&
            yvex_quant_policy_get_summary(policy, &summary, &error) == YVEX_OK &&
            summary.status == YVEX_QUANT_POLICY_STATUS_VALID && summary.schema_version == 3u &&
            yvex_quant_policy_iq2_encoder_version(policy) == i + 1u,
            "policy owns the explicit producer, not a runtime decoder class");
        memcpy(identities[i], summary.policy_identity, sizeof(identities[i]));
        YVEX_TEST_ASSERT(yvex_quant_policy_clone(&copy, policy, &error) == YVEX_OK &&
            yvex_quant_policy_iq2_encoder_version(copy) == i + 1u &&
            yvex_quant_policy_write_json(written, copy, &error) == YVEX_OK &&
            yvex_quant_policy_open(&readback, written, &error) == YVEX_OK &&
            yvex_quant_policy_get_summary(readback, &observed, &error) == YVEX_OK &&
            !strcmp(summary.policy_identity, observed.policy_identity) &&
            yvex_quant_policy_iq2_encoder_version(readback) == i + 1u,
            "clone and canonical publication preserve producer identity");
        {
            yvex_quant_policy *reordered = NULL;
            FILE *permuted = fopen(path, "wb");
            YVEX_TEST_ASSERT(permuted, "open member-order control");
            fprintf(permuted, "{\"rules\":[{\"match\":{\"default\":true},"
                    "\"action\":{\"qtype\":\"IQ2_XXS\",\"calibration\":\"required\"}}],"
                    "\"name\":\"same-name\",\"iq2_encoder\":\"%s\","
                    "\"architecture\":\"same-architecture\",\"schema\":\"yvex.quant_policy.v3\"}", encoders[i]);
            YVEX_TEST_ASSERT(!fclose(permuted) && yvex_quant_policy_open(&reordered, path, &error) == YVEX_OK &&
                yvex_quant_policy_get_summary(reordered, &observed, &error) == YVEX_OK &&
                !strcmp(summary.policy_identity, observed.policy_identity),
                "JSON member order does not change the explicit producer or rule identity");
            yvex_quant_policy_close(reordered);
            permuted = fopen(path, "ab");
            YVEX_TEST_ASSERT(permuted && fputs(" null", permuted) >= 0 && !fclose(permuted) &&
                yvex_quant_policy_open(&reordered, path, &error) != YVEX_OK && !reordered,
                "explicit producer policy rejects trailing JSON values");
        }
        yvex_quant_policy_close(readback); yvex_quant_policy_close(copy); yvex_quant_policy_close(policy);
    }
    YVEX_TEST_ASSERT(strcmp(identities[0], identities[1]), "different producers cannot alias a physical policy");
    for (i = 0u; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        yvex_quant_policy *policy = NULL;
        FILE *file = fopen(path, "wb");
        YVEX_TEST_ASSERT(file, "open refused producer fixture");
        fprintf(file, "{%s,\"name\":\"refused\",\"architecture\":\"generic\",\"rules\":[]}", invalid[i]);
        YVEX_TEST_ASSERT(!fclose(file) && yvex_quant_policy_open(&policy, path, &error) != YVEX_OK &&
                         !policy, "unknown, missing, duplicate or old-schema producer refuses without publication");
    }
    {
        yvex_quant_policy *policy = NULL;
        FILE *file = fopen(path, "wb");
        YVEX_TEST_ASSERT(file && fputs("{\"schema\":\"yvex.quant_policy.v3\","
            "\"iq2_encoder\":\"weighted-grid-v2\",\"rules\":[]", file) >= 0 && !fclose(file) &&
            yvex_quant_policy_open(&policy, path, &error) != YVEX_OK && !policy,
            "unterminated explicit producer policy refuses");
    }
    return 0;
}

static int test_reconstruction_derivation(void)
{
    yvex_quant_policy_rule rule = {0};
    yvex_quant_policy_definition definition = {"derived", "generic", "test", &rule, 1ull};
    yvex_quant_policy *legacy = NULL, *weighted = NULL, *same = NULL, *readback = NULL;
    yvex_quant_policy_summary a = {0}, b = {0}, observed = {0};
    yvex_error error = {0};
    const char *path = "build/tests/quant-policy/derived-encoder.json";
    rule.schema_version = YVEX_QUANT_POLICY_SCHEMA_VERSION;
    rule.match_mask = YVEX_QUANT_MATCH_DEFAULT;
    rule.qtype = YVEX_QUANT_QTYPE_IQ2_XXS;
    rule.requires_imatrix = 1;
    YVEX_TEST_ASSERT(yvex_quant_policy_create_definition(&legacy, &definition, &error) == YVEX_OK &&
        yvex_quant_policy_get_summary(legacy, &a, &error) == YVEX_OK,
        "native definition owns the legacy policy");
    YVEX_TEST_ASSERT(yvex_quant_policy_with_iq2_encoder(&same, legacy, 1u, &error) == YVEX_OK &&
        yvex_quant_policy_get_summary(same, &observed, &error) == YVEX_OK &&
        !strcmp(a.policy_identity, observed.policy_identity), "no-op derivation preserves old identity");
    yvex_quant_policy_close(same); same = NULL;
    YVEX_TEST_ASSERT(yvex_quant_policy_with_iq2_encoder(&weighted, legacy, 2u, &error) == YVEX_OK &&
        yvex_quant_policy_get_summary(weighted, &b, &error) == YVEX_OK &&
        b.schema_version == 3u && strcmp(a.policy_identity, b.policy_identity) &&
        yvex_quant_policy_iq2_encoder_version(legacy) == 1u &&
        yvex_quant_policy_iq2_encoder_version(weighted) == 2u,
        "producer alternative changes immutable identity, not the original policy");
    YVEX_TEST_ASSERT(yvex_quant_policy_rule_count(weighted) == 1ull &&
        yvex_quant_policy_rule_at(weighted, 0ull)->qtype == rule.qtype &&
        yvex_quant_policy_rule_at(weighted, 0ull)->requires_imatrix,
        "reconstruction preserves Q and calibration requirements");
    YVEX_TEST_ASSERT(yvex_quant_policy_write_json(path, weighted, &error) == YVEX_OK &&
        yvex_quant_policy_open(&readback, path, &error) == YVEX_OK &&
        yvex_quant_policy_get_summary(readback, &observed, &error) == YVEX_OK &&
        !strcmp(b.policy_identity, observed.policy_identity),
        "derived producer publishes through the canonical reader and writer");
    for (unsigned int version = 0u; version <= 4u; version += 3u)
        YVEX_TEST_ASSERT(yvex_quant_policy_with_iq2_encoder(&same, legacy, version, &error) ==
            YVEX_ERR_INVALID_ARG && !same, "unknown producers refuse atomically");
    yvex_quant_policy_close(readback); yvex_quant_policy_close(weighted);
    yvex_quant_policy_close(legacy); legacy = NULL;
    rule.qtype = YVEX_QUANT_QTYPE_Q2_K; rule.requires_imatrix = 0;
    YVEX_TEST_ASSERT(yvex_quant_policy_create_definition(&legacy, &definition, &error) == YVEX_OK &&
        yvex_quant_policy_with_iq2_encoder(&same, legacy, 2u, &error) == YVEX_ERR_UNSUPPORTED && !same,
        "an absent codec does not acquire an invented reconstruction technique");
    yvex_quant_policy_close(legacy);
    return 0;
}

int yvex_test_quant_policy(void)
{
    if (test_names() != 0) return 1;
    if (test_open_validate_write() != 0) return 1;
    if (test_mxfp4_policy() != 0) return 1;
    if (test_reject_unknown_qtype() != 0) return 1;
    if (test_derive_fixture() != 0) return 1;
    if (test_policy_v2_and_presets() != 0) return 1;
    if (test_explicit_iq2_encoder() != 0) return 1;
    if (test_reconstruction_derivation() != 0) return 1;
    return test_dspark_bootstrap_policy();
}
