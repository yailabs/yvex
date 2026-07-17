/*
 * materialization_runtime.c - materialization and runtime descriptor tests.
 *
 * Owner: tests/unit.
 * Owns: focused fixture coverage for complete-admission materialization plans,
 *   bounded payload access, lifecycle refusal, and runtime descriptor
 *   projection.
 * Does not own: target-scale DeepSeek proof, graph execution, generation, or
 *   release claims.
 * Invariants: uses only tiny GGUF fixtures and synthetic complete-admission
 *   facts bound to the opened fixture snapshot.
 * Boundary: fixture materialization is not full runtime support.
 */
#include "src/artifact/yvex_artifact_materialize.h"
#include "src/model/yvex_runtime_descriptor.h"

#include <stdio.h>
#include <string.h>

#include <yvex/yvex.h>

#include "test.h"

static void fill_fixture_admission(const yvex_artifact *artifact,
                                   const yvex_tensor_table *tensors,
                                   yvex_complete_artifact_admission *admission)
{
    memset(admission, 0, sizeof(*admission));
    admission->artifact_class = YVEX_ARTIFACT_CLASS_COMPLETE_YVEX;
    admission->tensor_count = yvex_tensor_table_count(tensors);
    admission->file_bytes = yvex_artifact_size(artifact);
    admission->payload_bytes = 128ull;
    admission->materialization_input_ready = 1;
    admission->tokenizer_complete = 1;
    admission->complete = 1;
    admission->runtime_supported = 0;
    (void)snprintf(admission->artifact_path,
                   sizeof(admission->artifact_path), "%s",
                   yvex_artifact_path(artifact));
    (void)snprintf(admission->artifact_identity,
                   sizeof(admission->artifact_identity),
                   "1111111111111111111111111111111111111111111111111111111111111111");
    (void)snprintf(admission->profile_identity,
                   sizeof(admission->profile_identity),
                   "fixture-profile");
    (void)snprintf(admission->writer_plan_identity,
                   sizeof(admission->writer_plan_identity),
                   "fixture-writer-plan");
    (void)yvex_artifact_snapshot_get(artifact, &admission->file_snapshot, NULL);
}

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
    options.map = 0;
    yvex_error_clear(&err);
    rc = yvex_artifact_open(artifact, &options, &err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(gguf, *artifact, &err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(tensors, *gguf, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "fixture open failed: %s: %s\n",
                yvex_error_where(&err), yvex_error_message(&err));
        yvex_tensor_table_close(*tensors);
        yvex_gguf_close(*gguf);
        yvex_artifact_close(*artifact);
        return 1;
    }
    return 0;
}

static int test_materialization_fixture(void)
{
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_complete_artifact_admission admission;
    yvex_materialization_options options;
    yvex_materialization_plan *plan = NULL;
    yvex_materialization_session *session = NULL;
    yvex_materialization_failure failure;
    yvex_runtime_descriptor *descriptor = NULL;
    yvex_runtime_descriptor_failure descriptor_failure;
    const yvex_materialization_summary *summary;
    const yvex_runtime_descriptor_summary *descriptor_summary;
    const yvex_materialized_tensor_binding *binding;
    unsigned char bytes[16];
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(open_fixture(&artifact, &gguf, &tensors) == 0,
                     "open materialization fixture");
    fill_fixture_admission(artifact, tensors, &admission);
    yvex_materialization_options_default(&options);
    options.max_chunk_bytes = 16u;

    rc = yvex_materialization_plan_build(
        &plan, &admission, artifact, gguf, tensors, NULL, &options,
        &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "plan builds");
    summary = yvex_materialization_plan_summary(plan);
    YVEX_TEST_ASSERT(summary != NULL, "plan summary");
    YVEX_TEST_ASSERT(summary->status == YVEX_MATERIALIZATION_STATUS_PLANNED,
                     "plan status");
    YVEX_TEST_ASSERT(summary->tensor_count == 1ull, "plan tensor count");
    YVEX_TEST_ASSERT(summary->payload_bytes == 128ull, "plan payload bytes");
    YVEX_TEST_ASSERT(summary->file_backed_bytes_owned == 128ull,
                     "file backed bytes");
    YVEX_TEST_ASSERT(summary->execution_ready == 0, "execution not ready");
    YVEX_TEST_ASSERT(strlen(summary->plan_identity) == 64u,
                     "plan identity");

    binding = yvex_materialization_plan_find_name(plan, "token_embd.weight");
    YVEX_TEST_ASSERT(binding != NULL, "find binding");
    YVEX_TEST_ASSERT(binding->encoded_bytes == 128ull, "binding bytes");
    YVEX_TEST_ASSERT(binding->access_mode == YVEX_MATERIALIZATION_ACCESS_FILE_RANGE ||
                     binding->access_mode == YVEX_MATERIALIZATION_ACCESS_BACKEND_CANDIDATE_FILE_RANGE,
                     "binding access");

    rc = yvex_materialization_session_open(
        &session, plan, artifact, &options, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "session open");
    rc = yvex_materialization_session_commit(session, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "session commit");
    rc = yvex_materialization_session_read(
        session, binding, 0ull, bytes, sizeof(bytes), &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "session read");
    rc = yvex_materialization_session_walk_payload(
        session, NULL, NULL, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "session full walk");
    summary = yvex_materialization_session_summary(session);
    YVEX_TEST_ASSERT(summary->payload_bytes_accessed >= 144ull,
                     "payload access counted");
    YVEX_TEST_ASSERT(summary->peak_executor_owned_bytes == 16ull,
                     "bounded staging peak");

    rc = yvex_runtime_descriptor_build(
        &descriptor, &admission, session, &descriptor_failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "runtime descriptor builds");
    descriptor_summary = yvex_runtime_descriptor_summary_get(descriptor);
    YVEX_TEST_ASSERT(descriptor_summary != NULL, "descriptor summary");
    YVEX_TEST_ASSERT(descriptor_summary->status == YVEX_RUNTIME_DESCRIPTOR_STATUS_READY,
                     "descriptor status");
    YVEX_TEST_ASSERT(descriptor_summary->tensor_count == 1ull,
                     "descriptor tensor count");
    YVEX_TEST_ASSERT(descriptor_summary->graph_execution_ready == 0,
                     "graph not ready");
    YVEX_TEST_ASSERT(descriptor_summary->generation_ready == 0,
                     "generation not ready");
    YVEX_TEST_ASSERT(yvex_runtime_descriptor_find_name(
                         descriptor, "token_embd.weight") != NULL,
                     "descriptor find by name");

    yvex_runtime_descriptor_close(descriptor);
    yvex_materialization_session_close(session);
    yvex_materialization_plan_close(plan);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return 0;
}

static int test_materialization_refusals(void)
{
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_complete_artifact_admission admission;
    yvex_materialization_options options;
    yvex_materialization_plan *plan = NULL;
    yvex_materialization_session *session = NULL;
    yvex_materialization_failure failure;
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(open_fixture(&artifact, &gguf, &tensors) == 0,
                     "open refusal fixture");
    fill_fixture_admission(artifact, tensors, &admission);
    yvex_materialization_options_default(&options);
    admission.complete = 0;
    rc = yvex_materialization_plan_build(
        &plan, &admission, artifact, gguf, tensors, NULL, &options,
        &failure, &err);
    YVEX_TEST_ASSERT(rc != YVEX_OK, "incomplete admission refused");
    YVEX_TEST_ASSERT(failure.code == YVEX_MATERIALIZATION_FAILURE_ADMISSION,
                     "admission failure code");
    fill_fixture_admission(artifact, tensors, &admission);
    admission.file_snapshot.size += 1ull;
    rc = yvex_materialization_plan_build(
        &plan, &admission, artifact, gguf, tensors, NULL, &options,
        &failure, &err);
    YVEX_TEST_ASSERT(rc != YVEX_OK, "snapshot drift refused");
    YVEX_TEST_ASSERT(failure.code == YVEX_MATERIALIZATION_FAILURE_SNAPSHOT_DRIFT,
                     "snapshot failure code");

    fill_fixture_admission(artifact, tensors, &admission);
    options.max_chunk_bytes = 8u;
    options.cancel_after_first_chunk = 1;
    rc = yvex_materialization_plan_build(
        &plan, &admission, artifact, gguf, tensors, NULL, &options,
        &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "plan for cancellation");
    rc = yvex_materialization_session_open(
        &session, plan, artifact, &options, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "session for cancellation");
    rc = yvex_materialization_session_commit(session, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "commit for cancellation");
    rc = yvex_materialization_session_walk_payload(
        session, NULL, NULL, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_CANCELLED, "walk cancelled");
    YVEX_TEST_ASSERT(failure.code == YVEX_MATERIALIZATION_FAILURE_CANCELLED,
                     "cancel failure code");
    yvex_materialization_session_close(session);
    yvex_materialization_plan_close(plan);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return 0;
}

int yvex_test_materialization_runtime(void)
{
    if (test_materialization_fixture() != 0) return 1;
    if (test_materialization_refusals() != 0) return 1;
    return 0;
}
