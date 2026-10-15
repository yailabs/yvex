
#include <stdio.h>
#include <fenv.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/api.h>

#include "tests/test.h"

static int write_policy(const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    fprintf(fp,
            "{\n"
            "  \"schema\": \"yvex.quant_policy.v1\",\n"
            "  \"name\": \"test-policy\",\n"
            "  \"architecture\": \"deepseek4\",\n"
            "  \"rules\": [\n"
            "    {\"selector_kind\": \"pattern\", \"selector\": \"blk.*.ffn.experts.*\", \"qtype\": \"Q2_K\", \"requires_imatrix\": true}\n"
            "  ]\n"
            "}\n");
    return fclose(fp) == 0;
}

static int write_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    fputs(text, fp);
    return fclose(fp) == 0;
}

static int write_u32(FILE *fp, unsigned int value)
{
    unsigned char bytes[4] = {
        (unsigned char)value,
        (unsigned char)(value >> 8),
        (unsigned char)(value >> 16),
        (unsigned char)(value >> 24)
    };
    return fwrite(bytes, 1u, sizeof(bytes), fp) == sizeof(bytes);
}

static int write_f32(FILE *fp, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return write_u32(fp, bits);
}

static int write_data_entry(FILE *fp, const char *name, unsigned int values, float base)
{
    unsigned int index;
    size_t length = strlen(name);
    if (!write_u32(fp, (unsigned int)length) || fwrite(name, 1u, length, fp) != length ||
        !write_u32(fp, 3u) || !write_u32(fp, values))
        return 0;
    for (index = 0u; index < values; ++index)
        if (!write_f32(fp, base + (float)index * 0.25f))
            return 0;
    return 1;
}

static int write_imatrix_data(const char *path)
{
    const char *dataset = "tests/reference/calibration.txt";
    FILE *fp = fopen(path, "wb");
    int ok;
    if (!fp) return 0;
    ok = write_u32(fp, 2u) &&
         write_data_entry(fp, "blk.0.ffn_gate_exps.weight", 8u, 1.0f) &&
         write_data_entry(fp, "blk.0.ffn_down_exps.weight", 4u, 2.0f) &&
         write_u32(fp, 17u) && write_u32(fp, (unsigned int)strlen(dataset)) &&
         fwrite(dataset, 1u, strlen(dataset), fp) == strlen(dataset);
    return fclose(fp) == 0 && ok;
}

static int write_duplicate_imatrix_data(const char *path)
{
    const char *dataset = "tests/reference/calibration.txt";
    FILE *fp = fopen(path, "wb");
    int ok;
    if (!fp) return 0;
    ok = write_u32(fp, 2u) &&
         write_data_entry(fp, "blk.0.ffn_gate_exps.weight", 8u, 1.0f) &&
         write_data_entry(fp, "blk.0.ffn_gate_exps.weight", 8u, 2.0f) &&
         write_u32(fp, 17u) && write_u32(fp, (unsigned int)strlen(dataset)) &&
         fwrite(dataset, 1u, strlen(dataset), fp) == strlen(dataset);
    return fclose(fp) == 0 && ok;
}

static int test_data_snapshot(void)
{
    const char *path = "build/tests/imatrix/weights.dat";
    yvex_imatrix_data_options options;
    yvex_imatrix_data *data = NULL;
    yvex_imatrix_data_summary summary;
    yvex_imatrix_entry_summary entry;
    float values[3];
    FILE *fp;
    yvex_error err;

    YVEX_TEST_ASSERT(write_imatrix_data(path), "write canonical imatrix data");
    memset(&options, 0, sizeof(options));
    options.path = path;
    options.source_model_identity = "source-model-identity";
    options.calibration_dataset_identity = "calibration-dataset-identity";
    options.producer = "llama.cpp";
    options.producer_version = 1u;
    options.maximum_mapped_bytes = 1024u * 1024u;
    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(yvex_imatrix_data_open(&data, &options, &err) == YVEX_OK,
                     "open imatrix data");
    YVEX_TEST_ASSERT(yvex_imatrix_data_get_summary(data, &summary, &err) == YVEX_OK,
                     "imatrix data summary");
    YVEX_TEST_ASSERT(summary.complete && summary.snapshot_stable, "stable complete data");
    YVEX_TEST_ASSERT(summary.entry_count == 2u && summary.value_count == 12u,
                     "entry and value counts");
    YVEX_TEST_ASSERT(summary.calibration_chunk_count == 17u, "calibration chunks");
    YVEX_TEST_ASSERT_STREQ(summary.dataset_name, "tests/reference/calibration.txt",
                           "dataset trailer");
    YVEX_TEST_ASSERT(yvex_imatrix_data_find(data, "blk.0.ffn_gate_exps.weight", &entry,
                                            &err) == YVEX_OK,
                     "find imatrix entry");
    YVEX_TEST_ASSERT(entry.value_count == 8u, "entry extent");
    YVEX_TEST_ASSERT(yvex_imatrix_data_read(data, entry.ordinal, 2u, values, 3u, &err) ==
                         YVEX_OK,
                     "read imatrix interval");
    YVEX_TEST_ASSERT(values[0] == 1.5f && values[2] == 2.0f, "decoded little-endian values");
    fp = fopen(path, "ab");
    YVEX_TEST_ASSERT(fp && fputc(0, fp) != EOF && fclose(fp) == 0, "mutate admitted file");
    YVEX_TEST_ASSERT(yvex_imatrix_data_validate(data, &err) != YVEX_OK,
                     "snapshot drift refusal");
    yvex_imatrix_data_close(data);
    return 0;
}

static int test_duplicate_data_entry(void)
{
    const char *path = "build/tests/imatrix/duplicate.dat";
    yvex_imatrix_data_options options;
    yvex_imatrix_data *data = NULL;
    yvex_error err;

    YVEX_TEST_ASSERT(write_duplicate_imatrix_data(path), "write duplicate imatrix data");
    memset(&options, 0, sizeof(options));
    options.path = path;
    options.source_model_identity = "source-model-identity";
    options.calibration_dataset_identity = "calibration-dataset-identity";
    options.producer = "llama.cpp";
    options.producer_version = 1u;
    options.maximum_mapped_bytes = 1024u * 1024u;
    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(yvex_imatrix_data_open(&data, &options, &err) == YVEX_ERR_FORMAT,
                     "duplicate imatrix entry refuses");
    YVEX_TEST_ASSERT(data == NULL, "duplicate imatrix publishes no owner");
    return 0;
}

static int test_names(void)
{
    YVEX_TEST_ASSERT_STREQ(yvex_imatrix_status_name(YVEX_IMATRIX_STATUS_PRESENT), "present", "status name");
    YVEX_TEST_ASSERT_STREQ(yvex_imatrix_format_name(YVEX_IMATRIX_FORMAT_ROUTED_MOE_DAT), "routed_moe_dat", "format name");
    YVEX_TEST_ASSERT_STREQ(yvex_imatrix_coverage_kind_name(YVEX_IMATRIX_COVERAGE_ROUTED_MOE), "routed_moe", "coverage name");
    YVEX_TEST_ASSERT_STREQ(yvex_imatrix_issue_kind_name(YVEX_IMATRIX_ISSUE_FILE_MISSING), "file_missing", "issue name");
    YVEX_TEST_ASSERT(yvex_imatrix_status_from_name("present") == YVEX_IMATRIX_STATUS_PRESENT, "status parse");
    YVEX_TEST_ASSERT(yvex_imatrix_format_from_name("routed_moe_dat") == YVEX_IMATRIX_FORMAT_ROUTED_MOE_DAT, "format parse");
    return 0;
}

static int test_create_write_open_present(void)
{
    const char *dir = "build/tests/imatrix";
    const char *policy_path = "build/tests/imatrix/policy.json";
    const char *dat_path = "build/tests/imatrix/fake.dat";
    const char *manifest_path = "build/tests/imatrix/imatrix.json";
    yvex_imatrix_manifest_options options;
    yvex_imatrix_manifest *manifest = NULL;
    yvex_imatrix_manifest *opened = NULL;
    yvex_imatrix_summary summary;
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(system("rm -rf build/tests/imatrix && mkdir -p build/tests/imatrix") == 0,
                     "create imatrix dir");
    (void)dir;
    YVEX_TEST_ASSERT(write_policy(policy_path), "write policy");
    YVEX_TEST_ASSERT(write_file(dat_path, "fake-imatrix"), "write fake dat");

    memset(&options, 0, sizeof(options));
    options.name = "test-imatrix";
    options.architecture = "deepseek4";
    options.quant_policy_path = policy_path;
    options.imatrix_path = dat_path;
    options.calibration_dataset = "test-dataset";
    options.producer = "test";
    options.format = YVEX_IMATRIX_FORMAT_ROUTED_MOE_DAT;
    options.status = YVEX_IMATRIX_STATUS_PRESENT;

    yvex_error_clear(&err);
    rc = yvex_imatrix_manifest_create(&manifest, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "create manifest");
    YVEX_TEST_ASSERT(yvex_imatrix_manifest_validate(manifest, &err) == YVEX_OK, "validate manifest");
    YVEX_TEST_ASSERT(yvex_imatrix_manifest_get_summary(manifest, &summary, &err) == YVEX_OK, "summary");
    YVEX_TEST_ASSERT(summary.file_exists == 1, "file exists");
    YVEX_TEST_ASSERT(summary.requires_imatrix_rule_count == 1, "requires rule count");
    YVEX_TEST_ASSERT(summary.covered_rule_count == 1, "covered rule count");
    YVEX_TEST_ASSERT(summary.uncovered_rule_count == 0, "uncovered rule count");
    YVEX_TEST_ASSERT(yvex_imatrix_manifest_write_json(manifest_path, manifest, &err) == YVEX_OK, "write manifest");
    yvex_imatrix_manifest_close(manifest);

    rc = yvex_imatrix_manifest_open(&opened, manifest_path, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open manifest");
    YVEX_TEST_ASSERT(yvex_imatrix_manifest_get_summary(opened, &summary, &err) == YVEX_OK, "opened summary");
    YVEX_TEST_ASSERT_STREQ(summary.name, "test-imatrix", "opened name");
    YVEX_TEST_ASSERT(summary.file_exists == 1, "opened file exists");
    yvex_imatrix_manifest_close(opened);
    return 0;
}

static int test_missing_file(void)
{
    yvex_imatrix_manifest_options options;
    yvex_imatrix_manifest *manifest = NULL;
    yvex_imatrix_summary summary;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.name = "missing-imatrix";
    options.architecture = "deepseek4";
    options.imatrix_path = "build/tests/imatrix/missing.dat";
    options.format = YVEX_IMATRIX_FORMAT_ROUTED_MOE_DAT;
    options.status = YVEX_IMATRIX_STATUS_PRESENT;

    yvex_error_clear(&err);
    rc = yvex_imatrix_manifest_create(&manifest, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "create missing manifest");
    YVEX_TEST_ASSERT(yvex_imatrix_manifest_validate(manifest, &err) == YVEX_OK, "validate missing manifest");
    YVEX_TEST_ASSERT(yvex_imatrix_manifest_get_summary(manifest, &summary, &err) == YVEX_OK, "missing summary");
    YVEX_TEST_ASSERT(summary.file_exists == 0, "missing file exists false");
    YVEX_TEST_ASSERT(summary.status == YVEX_IMATRIX_STATUS_MISSING, "missing status");
    YVEX_TEST_ASSERT(summary.issue_count > 0, "missing issue");
    yvex_imatrix_manifest_close(manifest);
    return 0;
}

static int test_invalid_args(void)
{
    yvex_imatrix_manifest_options options;
    yvex_imatrix_manifest *manifest = NULL;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    yvex_error_clear(&err);
    rc = yvex_imatrix_manifest_create(&manifest, &options, &err);
    YVEX_TEST_ASSERT(rc != YVEX_OK, "invalid args fail");
    return 0;
}

static int test_accumulator_populations(void)
{
    const float rows[4][6] = {
        {1, -2, 3, -4, NAN, NAN}, {2, 0, -1, 4, NAN, NAN},
        {0.5f, 4, -3, 0, NAN, NAN}, {-1, -2, 0.25f, 8, NAN, NAN}};
    const unsigned long long indices[] = {2, 0, 2, 1, 3};
    const unsigned long long populations[] = {0, 0, 2, 2, 0};
    yvex_imatrix_accumulator *accumulator = NULL;
    yvex_imatrix_accumulator_options options = {
        .schema_version = YVEX_IMATRIX_ACCUMULATOR_OPTIONS_SCHEMA_V1,
        .feature_count = 4, .population_count = 3,
        .maximum_observations = 8, .maximum_owned_bytes = 4096};
    yvex_imatrix_observation_batch batch = {
        .schema_version = YVEX_IMATRIX_OBSERVATION_BATCH_SCHEMA_V1,
        .rows = &rows[0][0], .row_count = 4, .row_stride = 6, .sample_count = 5,
        .row_indices = indices, .population_ids = populations};
    yvex_imatrix_accumulator_summary summary;
    unsigned long long counts[3], observed = 999;
    float output[4] = {-77, -77, -77, -77};
    yvex_error err;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_create(&accumulator, &options, &err) == YVEX_OK &&
        yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_OK &&
        yvex_imatrix_accumulator_get_summary(accumulator, &summary, &err) == YVEX_OK &&
        yvex_imatrix_accumulator_counts(accumulator, counts, 3, &err) == YVEX_OK,
        "bounded routed-input sufficient statistics accumulate real selected samples");
    YVEX_TEST_ASSERT(summary.schema_version == YVEX_IMATRIX_ACCUMULATOR_SUMMARY_SCHEMA_V1 &&
        summary.observations == 5 && summary.batches == 1 && summary.observed_populations == 2 &&
        summary.value_count == 12 && !summary.complete && summary.owned_bytes <= 4096 &&
        counts[0] == 3 && counts[1] == 0 && counts[2] == 2,
        "coverage counts actual populations, not source rows or invented expert observations");
    for (unsigned long long population = 0; population < 3; population += 2) {
        YVEX_TEST_ASSERT(yvex_imatrix_accumulator_read(accumulator, population, 0,
            output, 4, &observed, &err) == YVEX_OK && observed == counts[population],
            "observed population has bounded F32 mean-square readout");
        for (unsigned feature = 0; feature < 4; ++feature) {
            long double sum = 0;
            for (unsigned sample = 0; sample < 5; ++sample)
                if (populations[sample] == population) {
                    long double value = rows[indices[sample]][feature];
                    sum += value * value;
                }
            float reference = (float)(sum / (long double)counts[population]);
            YVEX_TEST_ASSERT(memcmp(&reference, &output[feature], sizeof(float)) == 0,
                "independent long-double oracle matches finite exact-power fixture");
        }
    }
    for (unsigned i = 0; i < 4; ++i) output[i] = -77;
    observed = 999;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_read(accumulator, 1, 0,
        output, 4, &observed, &err) == YVEX_ERR_STATE && observed == 999 && output[0] == -77 && output[3] == -77,
        "missing population refuses without fabricated mean square or partial publication");
    const unsigned long long more_indices[] = {0, 1}, more_populations[] = {1, 1};
    batch.row_indices = more_indices; batch.population_ids = more_populations; batch.sample_count = 2;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_OK &&
        yvex_imatrix_accumulator_get_summary(accumulator, &summary, &err) == YVEX_OK &&
        summary.complete && summary.observations == 7 && summary.batches == 2,
        "later real observations close population coverage without filling missing data");
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_read(accumulator, 1, 1,
        output, 2, &observed, &err) == YVEX_OK && observed == 2 && output[0] == 2 && output[1] == 5,
        "windowed mean-square readout preserves exact feature geometry");
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_reset(accumulator, &err) == YVEX_OK &&
        yvex_imatrix_accumulator_get_summary(accumulator, &summary, &err) == YVEX_OK &&
        yvex_imatrix_accumulator_counts(accumulator, counts, 3, &err) == YVEX_OK &&
        !summary.complete && !summary.observations && !summary.batches && !summary.observed_populations &&
        !counts[0] && !counts[1] && !counts[2],
        "reset retires all observation counts and moments");
    yvex_imatrix_accumulator_close(accumulator);
    yvex_imatrix_accumulator_close(NULL);
    return 0;
}

typedef struct { unsigned calls, stop; } accumulator_cancellation;
static int accumulator_cancelled(void *context)
{
    accumulator_cancellation *state = context;
    return ++state->calls >= state->stop;
}

static int test_accumulator_refusal(void)
{
    yvex_imatrix_accumulator *accumulator = NULL;
    yvex_imatrix_accumulator_options options = {
        .schema_version = YVEX_IMATRIX_ACCUMULATOR_OPTIONS_SCHEMA_V1,
        .feature_count = 1, .population_count = 1,
        .maximum_observations = 8, .maximum_owned_bytes = 4096};
    yvex_imatrix_observation_batch batch = {
        .schema_version = YVEX_IMATRIX_OBSERVATION_BATCH_SCHEMA_V1,
        .row_count = 2, .row_stride = 1, .sample_count = 2};
    const float rows[] = {3, 4, NAN};
    unsigned long long counts[1], observed = 777;
    yvex_imatrix_accumulator_summary before, after;
    yvex_error err;
    float output = -77;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_create(&accumulator, &options, &err) == YVEX_OK,
        "dense statistics lifetime opens without attention or expert assumptions");
    batch.rows = rows;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_OK &&
        yvex_imatrix_accumulator_get_summary(accumulator, &before, &err) == YVEX_OK &&
        yvex_imatrix_accumulator_read(accumulator, 0, 0, &output, 1, &observed, &err) == YVEX_OK &&
        observed == 2 && output == 12.5f,
        "dense input has one real observation per row and literal mean square");
    batch.row_count = batch.sample_count = 3;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_ERR_FORMAT,
        "late nonfinite activation refuses the entire observation batch");
    const unsigned long long bad_indices[] = {0, 1, 3}, bad_populations[] = {0, 0, 1};
    batch.row_indices = bad_indices;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_ERR_BOUNDS,
        "late out-of-bounds source row refuses before any accumulation");
    batch.row_indices = NULL; batch.population_ids = bad_populations;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_ERR_BOUNDS,
        "late foreign population refuses before any accumulation");
    batch.population_ids = NULL; batch.row_count = batch.sample_count = 2;
    accumulator_cancellation cancellation = {.stop = 2};
    batch.cancel_requested = accumulator_cancelled; batch.cancel_context = &cancellation;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_ERR_CANCELLED,
        "cancellation during validation has no partial moment commit");
    cancellation.calls = 0; cancellation.stop = 3;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_ERR_CANCELLED,
        "cancellation immediately before commit leaves statistics unchanged");
    batch.cancel_requested = NULL;
    int original_rounding = fegetround();
    YVEX_TEST_ASSERT(original_rounding == FE_TONEAREST && fesetround(FE_DOWNWARD) == 0,
        "controlled rounding refusal fixture installs nondefault rounding");
    int rc = yvex_imatrix_accumulator_observe(accumulator, &batch, &err);
    int restored = fesetround(original_rounding);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_STATE && restored == 0,
        "unadmitted rounding refuses without modifying the caller's floating environment");
    batch.row_count = batch.sample_count = 7;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_ERR_BOUNDS,
        "total observation budget refuses before reading an oversized row view");
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_get_summary(accumulator, &after, &err) == YVEX_OK &&
        after.observations == before.observations && after.batches == before.batches &&
        after.observed_populations == before.observed_populations &&
        yvex_imatrix_accumulator_counts(accumulator, counts, 1, &err) == YVEX_OK && counts[0] == 2 &&
        yvex_imatrix_accumulator_read(accumulator, 0, 0, &output, 1, &observed, &err) == YVEX_OK && output == 12.5f,
        "all malformed, cancelled and refused observations preserve the previous complete statistics");
    output = -77; observed = 777;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_read(accumulator, 0, 1, &output, 1, &observed, &err) == YVEX_ERR_BOUNDS &&
        output == -77 && observed == 777,
        "out-of-range readout is failure atomic");
    yvex_imatrix_accumulator_close(accumulator);
    return 0;
}

static int test_accumulator_limits(void)
{
    yvex_imatrix_accumulator *accumulator = NULL;
    yvex_imatrix_accumulator_options options = {
        .schema_version = YVEX_IMATRIX_ACCUMULATOR_OPTIONS_SCHEMA_V1,
        .feature_count = 2, .population_count = 1,
        .maximum_observations = 8, .maximum_owned_bytes = 1};
    yvex_error err;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_create(&accumulator, &options, &err) == YVEX_ERR_NOMEM && !accumulator,
        "statistics construction refuses an insufficient exact owned-memory budget");
    options.maximum_owned_bytes = SIZE_MAX; options.feature_count = ULLONG_MAX; options.population_count = 2;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_create(&accumulator, &options, &err) == YVEX_ERR_BOUNDS && !accumulator,
        "overflowed statistics geometry refuses before allocating");
    options.feature_count = 2; options.population_count = 1; options.maximum_owned_bytes = 4096;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_create(&accumulator, &options, &err) == YVEX_OK,
        "bounded numeric-extrema statistics fixture opens");
    const float rows[] = {1, FLT_MAX};
    yvex_imatrix_observation_batch batch = {
        .schema_version = YVEX_IMATRIX_OBSERVATION_BATCH_SCHEMA_V1,
        .rows = rows, .row_count = 1, .row_stride = 2, .sample_count = 1};
    float output[2] = {-77, -77}; unsigned long long observed = 777;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_OK &&
        yvex_imatrix_accumulator_read(accumulator, 0, 0, output, 2, &observed, &err) == YVEX_ERR_FORMAT &&
        output[0] == -77 && output[1] == -77 && observed == 777,
        "finite F32 activation squares cannot silently overflow the encoded F32 readout");
    yvex_imatrix_accumulator_close(accumulator);
    return 0;
}

static int test_accumulator_ieee(void)
{
    yvex_imatrix_accumulator *accumulator = NULL;
    yvex_imatrix_accumulator_options options = {
        .schema_version = YVEX_IMATRIX_ACCUMULATOR_OPTIONS_SCHEMA_V1,
        .feature_count = 1, .population_count = 1,
        .maximum_observations = 8, .maximum_owned_bytes = 4096};
    yvex_imatrix_observation_batch batch = {
        .schema_version = YVEX_IMATRIX_OBSERVATION_BATCH_SCHEMA_V1,
        .row_stride = 1};
    const float step = 0x1p-12f;
    const float even_tie[] = {1 + step, 1 - step};
    const float odd_tie[] = {1 + step, 1 - step, 1 + step, 1 - step,
                            1 + step, 1 - step, 1 + 3 * step, 1 - 3 * step};
    unsigned long long observed;
    uint32_t bits;
    float output;
    yvex_error err;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_create(&accumulator, &options, &err) == YVEX_OK,
        "IEEE mean-square fixture opens");
    batch.rows = even_tie; batch.row_count = batch.sample_count = 2;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_OK &&
        yvex_imatrix_accumulator_read(accumulator, 0, 0, &output, 1, &observed, &err) == YVEX_OK,
        "exact even midpoint mean square is representable");
    memcpy(&bits, &output, sizeof(bits));
    YVEX_TEST_ASSERT(bits == 0x3f800000u && observed == 2,
        "F32 halfway publication rounds to the even lower significand");
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_reset(accumulator, &err) == YVEX_OK,
        "midpoint fixture resets");
    batch.rows = odd_tie; batch.row_count = batch.sample_count = 8;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_OK &&
        yvex_imatrix_accumulator_read(accumulator, 0, 0, &output, 1, &observed, &err) == YVEX_OK,
        "exact odd midpoint mean square is representable");
    memcpy(&bits, &output, sizeof(bits));
    YVEX_TEST_ASSERT(bits == 0x3f800002u && observed == 8,
        "F32 halfway publication rounds to the even upper significand");
    yvex_imatrix_accumulator_close(accumulator);
    options.feature_count = 2;
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_create(&accumulator, &options, &err) == YVEX_OK,
        "subnormal representation fixture opens");
    uint32_t input_bits[] = {57u << 23u, 1u};
    float tiny[2]; memcpy(tiny, input_bits, sizeof(tiny));
    batch.rows = tiny; batch.row_count = batch.sample_count = 1; batch.row_stride = 2;
    float published[2]; uint32_t published_bits[2];
    YVEX_TEST_ASSERT(yvex_imatrix_accumulator_observe(accumulator, &batch, &err) == YVEX_OK &&
        yvex_imatrix_accumulator_read(accumulator, 0, 0, published, 2, &observed, &err) == YVEX_OK,
        "normal and subnormal F32 inputs retain exact F64 squares");
    memcpy(published_bits, published, sizeof(published_bits));
    YVEX_TEST_ASSERT(published_bits[0] == 512u && published_bits[1] == 0u,
        "F32 readout retains representable subnormals and rounds only true underflow to zero");
    yvex_imatrix_accumulator_close(accumulator);
    return 0;
}

int yvex_test_imatrix(void)
{
    if (test_accumulator_populations() != 0) return 1;
    if (test_accumulator_refusal() != 0) return 1;
    if (test_accumulator_limits() != 0) return 1;
    if (test_accumulator_ieee() != 0) return 1;
    if (test_names() != 0) return 1;
    if (test_create_write_open_present() != 0) return 1;
    if (test_data_snapshot() != 0) return 1;
    if (test_duplicate_data_entry() != 0) return 1;
    if (test_missing_file() != 0) return 1;
    if (test_invalid_args() != 0) return 1;
    return 0;
}
