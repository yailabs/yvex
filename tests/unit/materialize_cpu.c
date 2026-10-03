
#include <stdlib.h>
#include <string.h>

#include <yvex/api.h>
#include <yvex/internal/materialization.h>

#include "tests/test.h"

static int open_fixture(yvex_artifact **artifact,
                        yvex_gguf **gguf,
                        yvex_tensor_table **tensors)
{
    yvex_artifact_options options;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.path = "tests/fixtures/gguf/valid-tokenizer-simple.gguf";
    options.readonly = 1;
    options.map = 1;

    rc = yvex_artifact_open(artifact, &options, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "artifact open failed: %s: %s\n",
                yvex_error_where(&err), yvex_error_message(&err));
        return 1;
    }
    rc = yvex_gguf_open(gguf, *artifact, &err);
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(tensors, *gguf, &err);
    }
    if (rc != YVEX_OK) {
        fprintf(stderr, "fixture parse failed: %s: %s\n",
                yvex_error_where(&err), yvex_error_message(&err));
        yvex_tensor_table_close(*tensors);
        yvex_gguf_close(*gguf);
        yvex_artifact_close(*artifact);
        return 1;
    }
    return 0;
}

static int test_cpu_materialization(void)
{
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_materialize_options options;
    yvex_materialize_summary summary;
    yvex_backend_memory_stats stats;
    const yvex_tensor_info *tensor;
    const yvex_materialized_weight *weight;
    unsigned char *readback;
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(open_fixture(&artifact, &gguf, &tensors) == 0, "open fixture");
    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu");

    memset(&options, 0, sizeof(options));
    options.backend_name = "cpu";
    rc = yvex_weight_table_materialize(&weights, artifact, gguf, tensors, backend, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "materialize fixture");
    rc = yvex_weight_table_get_summary(weights, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "summary");
    YVEX_TEST_ASSERT(summary.status == YVEX_WEIGHT_STATUS_MATERIALIZED, "status materialized");
    YVEX_TEST_ASSERT(summary.tensors_total == 1, "total tensors");
    YVEX_TEST_ASSERT(summary.tensors_materialized == 1, "materialized tensors");
    YVEX_TEST_ASSERT(summary.tensors_failed == 0, "failed tensors");
    YVEX_TEST_ASSERT(summary.bytes_total == 128, "bytes total");
    YVEX_TEST_ASSERT(summary.bytes_materialized == 128, "bytes materialized");
    YVEX_TEST_ASSERT(summary.backend_allocated_bytes == 128, "backend allocated");
    YVEX_TEST_ASSERT(summary.execution_ready == 0, "execution not ready");

    tensor = yvex_tensor_table_at(tensors, 0);
    weight = yvex_weight_table_find(weights, "token_embd.weight");
    YVEX_TEST_ASSERT(tensor != NULL, "source tensor");
    YVEX_TEST_ASSERT(weight != NULL, "find materialized weight");
    YVEX_TEST_ASSERT_STREQ(yvex_weight_name(weight), "token_embd.weight", "weight name");
    YVEX_TEST_ASSERT(yvex_weight_dtype(weight) == YVEX_DTYPE_F32, "weight dtype");
    YVEX_TEST_ASSERT(yvex_weight_role(weight) == YVEX_TENSOR_ROLE_TOKEN_EMBEDDING, "weight role");
    YVEX_TEST_ASSERT(yvex_weight_bytes(weight) == 128, "weight bytes");
    YVEX_TEST_ASSERT(yvex_weight_residency_of(weight) == YVEX_WEIGHT_RESIDENCY_CPU_BACKEND,
                     "cpu residency");

    readback = (unsigned char *)malloc((size_t)yvex_weight_bytes(weight));
    YVEX_TEST_ASSERT(readback != NULL, "readback alloc");
    rc = yvex_backend_tensor_read(backend,
                                  yvex_weight_device_tensor(weight),
                                  readback,
                                  yvex_weight_bytes(weight),
                                  &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read materialized backend tensor");
    YVEX_TEST_ASSERT(memcmp(readback,
                            yvex_artifact_data(artifact) + tensor->absolute_offset,
                            (size_t)tensor->storage_bytes) == 0,
                     "backend bytes match artifact tensor bytes");
    free(readback);

    yvex_weight_table_close(weights);
    weights = NULL;
    rc = yvex_backend_get_memory_stats(backend, &stats, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "post-close stats");
    YVEX_TEST_ASSERT(stats.allocated_bytes == 0, "weight close frees backend allocation");

    yvex_backend_close(backend);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return 0;
}

static int test_failure_report(void)
{
    const char *hooks[] = {"YVEX_TEST_FAIL_MATERIALIZE_AFTER_ALLOC",
                           "YVEX_TEST_FAIL_MATERIALIZE_AFTER_TRANSFER"};
    const char *phases[] = {"allocation", "transfer"};
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_materialize_options options;
    yvex_materialize_summary summary;
    yvex_backend_memory_stats stats;
    yvex_error err;
    yvex_device_tensor *independent = NULL;
    yvex_backend_tensor_desc descriptor;
    unsigned int i;
    YVEX_TEST_ASSERT(open_fixture(&artifact, &gguf, &tensors) == 0, "open fixture");
    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu");
    memset(&options, 0, sizeof(options));
    options.backend_name = "diagnostic-cpu";
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.name = "independent-owner";
    descriptor.dtype = YVEX_DTYPE_F32;
    descriptor.rank = 1;
    descriptor.dims[0] = 4;
    descriptor.bytes = 16;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &descriptor, &independent, &err) ==
                     YVEX_OK, "retain an independent allocation");
    for (i = 0; i < 2; ++i) {
        YVEX_TEST_ASSERT(setenv(hooks[i], "1", 1) == 0, "set failure hook");
        YVEX_TEST_ASSERT(yvex_weight_table_materialize_report(&weights, artifact, gguf, tensors,
                        backend, &options, &summary, &err) == YVEX_ERR_BACKEND,
                        "native failure remains fail closed");
        YVEX_TEST_ASSERT(unsetenv(hooks[i]) == 0, "retire failure hook");
        YVEX_TEST_ASSERT(weights == NULL, "failed table is not published");
        YVEX_TEST_ASSERT_STREQ(summary.backend_name, "diagnostic-cpu", "surviving backend name");
        YVEX_TEST_ASSERT_STREQ(summary.materialization_phase, phases[i], "actual failure phase");
        YVEX_TEST_ASSERT(summary.bytes_planned == 128, "actual planned bytes survive");
        YVEX_TEST_ASSERT(summary.bytes_allocated == 128, "actual allocated bytes survive");
        YVEX_TEST_ASSERT(summary.bytes_transferred == (i ? 128 : 0), "actual transferred bytes");
        YVEX_TEST_ASSERT(summary.cleanup_attempted, "retirement observed");
        YVEX_TEST_ASSERT_STREQ(summary.cleanup_status, "pass", "accounted cleanup verified");
        YVEX_TEST_ASSERT(summary.status == YVEX_WEIGHT_STATUS_FAILED, "failure status preserved");
        YVEX_TEST_ASSERT(summary.execution_ready == 0, "no execution promotion");
        YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &stats, &err) == YVEX_OK &&
                         stats.allocated_bytes == 16, "cleanup preserves independent ownership");
    }
    YVEX_TEST_ASSERT(yvex_weight_table_materialize_report(&weights, artifact, gguf, tensors,
                    backend, &options, &summary, &err) == YVEX_OK, "subsequent recovery");
    YVEX_TEST_ASSERT_STREQ(summary.materialization_gate, "pass", "recovery gate");
    yvex_weight_table_close(weights);
    weights = NULL;
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &stats, &err) == YVEX_OK &&
                     stats.allocated_bytes == 16, "recovery retires only its own resources");
    YVEX_TEST_ASSERT(yvex_weight_table_materialize_report(&weights, artifact, gguf, tensors,
                    backend, &options, NULL, &err) == YVEX_ERR_INVALID_ARG,
                    "required diagnostic extent refused");
    YVEX_TEST_ASSERT(weights == NULL, "invalid request does not publish");
    yvex_backend_tensor_free(backend, independent);
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &stats, &err) == YVEX_OK &&
                     stats.allocated_bytes == 0, "all explicitly owned resources retired");
    yvex_backend_close(backend);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return 0;
}

int yvex_test_materialize_cpu(void)
{
    if (test_cpu_materialization() != 0) return 1;
    if (test_failure_report() != 0) return 1;
    return 0;
}
