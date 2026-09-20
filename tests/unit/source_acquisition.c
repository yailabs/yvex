#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <yvex/internal/source_acquisition.h>

#include "tests/test.h"

static const char selection_identity[] =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

static int test_operation_round_trip(void)
{
    char root[] = "/tmp/yvex-source-acquisition-XXXXXX", path[YVEX_PATH_CAP];
    yvex_source_acquisition_create_options options;
    yvex_source_acquisition_operation created, readback;
    yvex_error err;
    FILE *stream;

    YVEX_TEST_ASSERT(mkdtemp(root) != NULL, "temporary acquisition directory");
    YVEX_TEST_ASSERT(snprintf(path, sizeof(path), "%s/operation.json", root) <
                         (int)sizeof(path), "operation path");
    memset(&options, 0, sizeof(options));
    options.provider = "huggingface";
    options.repository = "org/model";
    options.revision = "0123456789abcdef0123456789abcdef01234567";
    options.selection_identity = selection_identity;
    options.source_path = "/models/source/hf/org/model/revision";
    options.generation = 4ull;
    options.now_unix = 100ull;
    options.stall_window_seconds = 10ull;
    options.expected_bytes = (yvex_source_acquisition_u64){1000ull, 1};
    options.selected_files = (yvex_source_acquisition_u64){5ull, 1};
    options.selected_shards = (yvex_source_acquisition_u64){3ull, 1};
    options.selected_sidecars = (yvex_source_acquisition_u64){2ull, 1};
    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(yvex_source_acquisition_operation_create(
                         &options, &created, &err) == YVEX_OK,
                     "create durable acquisition operation");
    YVEX_TEST_ASSERT(yvex_source_acquisition_process_capture(
                         getpid(), getpgrp(), &created.supervisor, &err) == YVEX_OK,
                     "capture authenticated supervisor identity");
    created.lifecycle = YVEX_SOURCE_ACQUISITION_DOWNLOADING;
    created.progress.committed_bytes = (yvex_source_acquisition_u64){400ull, 1};
    created.progress.provider_activity_bytes =
        (yvex_source_acquisition_u64){700ull, 1};
    created.progress.inflight_selected_bytes.known = 0;
    created.progress.completed_files = (yvex_source_acquisition_u64){2ull, 1};
    created.progress.incomplete_files = (yvex_source_acquisition_u64){3ull, 1};
    created.progress.last_progress_unix =
        (yvex_source_acquisition_u64){95ull, 1};
    created.progress.retry_count = (yvex_source_acquisition_u64){2ull, 1};
    YVEX_TEST_ASSERT(yvex_source_acquisition_operation_publish(
                         path, &created, &err) == YVEX_OK,
                     "publish operation atomically");
    YVEX_TEST_ASSERT(yvex_source_acquisition_operation_read(
                         path, &readback, &err) == YVEX_OK,
                     "reopen operation");
    YVEX_TEST_ASSERT_STREQ(readback.operation_id, created.operation_id,
                           "operation identity preserved");
    YVEX_TEST_ASSERT(readback.generation == 4ull, "generation preserved");
    YVEX_TEST_ASSERT(readback.progress.committed_bytes.known &&
                         readback.progress.committed_bytes.value == 400ull,
                     "committed progress preserved");
    YVEX_TEST_ASSERT(readback.progress.provider_activity_bytes.value == 700ull,
                     "provider activity remains distinct");
    YVEX_TEST_ASSERT(!readback.progress.inflight_selected_bytes.known,
                     "unknown selected in-flight bytes preserved");
    YVEX_TEST_ASSERT(readback.progress.retry_count.known &&
                         readback.progress.retry_count.value == 2ull,
                     "known retry count preserved");
    YVEX_TEST_ASSERT(yvex_source_acquisition_process_matches(&readback.supervisor),
                     "captured process identity matches");

    stream = fopen(path, "wb");
    YVEX_TEST_ASSERT(stream != NULL, "open malformed record");
    fputs("{\"schema\":\"yvex.source.acquisition.operation.v2\"}\n", stream);
    YVEX_TEST_ASSERT(fclose(stream) == 0, "close malformed record");
    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(yvex_source_acquisition_operation_read(
                         path, &readback, &err) == YVEX_ERR_FORMAT,
                     "future or malformed operation fails closed");
    YVEX_TEST_ASSERT(unlink(path) == 0 && rmdir(root) == 0, "remove fixture");
    return 0;
}

static int test_reconciliation(void)
{
    yvex_source_acquisition_operation operation;
    yvex_error err;

    memset(&operation, 0, sizeof(operation));
    operation.schema_version = YVEX_SOURCE_ACQUISITION_OPERATION_SCHEMA;
    operation.lifecycle = YVEX_SOURCE_ACQUISITION_DOWNLOADING;
    operation.progress.last_progress_unix =
        (yvex_source_acquisition_u64){100ull, 1};
    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(yvex_source_acquisition_reconcile(
                         &operation, 109ull, 10ull, &err) == YVEX_OK,
                     "reconcile slow but fresh progress");
    YVEX_TEST_ASSERT(operation.health == YVEX_SOURCE_ACQUISITION_HEALTH_HEALTHY,
                     "fresh progress is healthy");
    YVEX_TEST_ASSERT(yvex_source_acquisition_reconcile(
                         &operation, 110ull, 10ull, &err) == YVEX_OK,
                     "reconcile expired progress window");
    YVEX_TEST_ASSERT(operation.health == YVEX_SOURCE_ACQUISITION_HEALTH_STALLED,
                     "expired progress becomes stalled");

    operation.lifecycle = YVEX_SOURCE_ACQUISITION_RETRYING;
    operation.health = YVEX_SOURCE_ACQUISITION_HEALTH_UNKNOWN;
    YVEX_TEST_ASSERT(yvex_source_acquisition_reconcile(
                         &operation, 111ull, 10ull, &err) == YVEX_OK &&
                         operation.health == YVEX_SOURCE_ACQUISITION_HEALTH_DEGRADED,
                     "provider retry is distinct degraded lifecycle");
    operation.lifecycle = YVEX_SOURCE_ACQUISITION_DOWNLOADING;
    operation.progress.last_progress_unix.known = 0;
    YVEX_TEST_ASSERT(yvex_source_acquisition_reconcile(
                         &operation, 200ull, 10ull, &err) == YVEX_OK &&
                         operation.health == YVEX_SOURCE_ACQUISITION_HEALTH_UNKNOWN,
                     "unobservable progress remains unknown rather than stalled");

    YVEX_TEST_ASSERT(yvex_source_acquisition_process_capture(
                         getpid(), getpgrp(), &operation.supervisor, &err) == YVEX_OK,
                     "capture process for PID reuse negative");
    operation.supervisor.start_ticks++;
    operation.progress.last_progress_unix =
        (yvex_source_acquisition_u64){100ull, 1};
    operation.lifecycle = YVEX_SOURCE_ACQUISITION_DOWNLOADING;
    YVEX_TEST_ASSERT(!yvex_source_acquisition_process_matches(&operation.supervisor),
                     "same PID with different start time is rejected");
    YVEX_TEST_ASSERT(yvex_source_acquisition_reconcile(
                         &operation, 120ull, 10ull, &err) == YVEX_OK,
                     "reconcile stale supervisor identity");
    YVEX_TEST_ASSERT(operation.lifecycle == YVEX_SOURCE_ACQUISITION_STOPPED,
                     "lost supervisor becomes interrupted stopped state");
    YVEX_TEST_ASSERT_STREQ(operation.result, "interrupted-resumable",
                           "lost supervisor remains resumable");
    return 0;
}

int yvex_test_source_acquisition(void)
{
    if (test_operation_round_trip() != 0) return 1;
    if (test_reconciliation() != 0) return 1;
    return 0;
}
