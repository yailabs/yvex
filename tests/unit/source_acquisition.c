#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <yvex/internal/source_acquisition.h>
#include <yvex/internal/core.h>

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

static int test_source_observation(void)
{
    char root[] = "/tmp/yvex-source-observation-XXXXXX";
    char source[YVEX_PATH_CAP], cache[YVEX_PATH_CAP], payload[YVEX_PATH_CAP];
    char partial[YVEX_PATH_CAP], event[YVEX_PATH_CAP], link[YVEX_PATH_CAP];
    yvex_source_acquisition_tree scan;
    yvex_source_acquisition_operation operation, before;
    yvex_error err;
    FILE *stream;
    YVEX_TEST_ASSERT(mkdtemp(root) != NULL, "isolated observation root");
    snprintf(source, sizeof(source), "%s/source", root);
    snprintf(cache, sizeof(cache), "%s/cache", root);
    snprintf(payload, sizeof(payload), "%s/source/model.safetensors", root);
    snprintf(partial, sizeof(partial), "%s/cache/model.incomplete", root);
    snprintf(event, sizeof(event), "%s/event.json", root);
    snprintf(link, sizeof(link), "%s/source/.cache", root);
    YVEX_TEST_ASSERT(mkdir(source, 0700) == 0 && mkdir(cache, 0700) == 0, "source/cache roots");
    stream = fopen(payload, "wb");
    YVEX_TEST_ASSERT(stream && fputs("1234", stream) >= 0 && fclose(stream) == 0, "bounded source file");
    stream = fopen(partial, "wb");
    YVEX_TEST_ASSERT(stream && fputs("provider-internal", stream) >= 0 && fclose(stream) == 0, "partial cache file");
    YVEX_TEST_ASSERT(symlink(cache, link) == 0, "cache alias");
    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(yvex_source_acquisition_scan(source, cache, &scan, &err) == YVEX_OK &&
        scan.bytes == 4ull && scan.files == 1ull && scan.shards == 1ull && scan.partials == 1ull,
        "cache writes are not committed canonical bytes or files");
    YVEX_TEST_ASSERT_STREQ(scan.largest_file, "model.safetensors", "relative largest file");
    memset(&operation, 0, sizeof(operation));
    operation.lifecycle = YVEX_SOURCE_ACQUISITION_DOWNLOADING;
    stream = fopen(event, "wb");
    YVEX_TEST_ASSERT(stream && fputs("{\"schema\":\"yvex.provider.acquisition.event.v1\","
        "\"sequence\":2,\"kind\":\"retry\",\"retry_count\":1,\"object\":\"model.safetensors\"}", stream) >= 0 &&
        fclose(stream) == 0, "provider retry fixture");
    YVEX_TEST_ASSERT(yvex_source_acquisition_observe(source, cache, event, &operation, 100ull, &err) == YVEX_OK,
        "observe selected tree and typed provider event");
    YVEX_TEST_ASSERT(operation.lifecycle == YVEX_SOURCE_ACQUISITION_RETRYING &&
        operation.progress.provider_event_sequence.value == 2ull && operation.progress.retry_count.value == 1ull,
        "retry remains a distinct lifecycle fact");
    YVEX_TEST_ASSERT(!operation.progress.inflight_selected_bytes.known &&
        !operation.progress.current_rate_bytes_per_second.known && !operation.progress.expected_bytes.known,
        "filesystem observations do not fabricate selected progress or throughput");
    before = operation;
    YVEX_TEST_ASSERT(yvex_source_acquisition_observe(link, NULL, event, &operation, 200ull, &err) != YVEX_OK &&
        !memcmp(&before, &operation, sizeof(operation)), "root symlink refusal is failure atomic");
    YVEX_TEST_ASSERT(unlink(link) == 0 && unlink(payload) == 0 && unlink(partial) == 0 && unlink(event) == 0 &&
        rmdir(source) == 0 && rmdir(cache) == 0 && rmdir(root) == 0, "remove exact test-owned fixture");
    return 0;
}

typedef struct {
    const char *path;
    const char *pending;
    const char *large;
    atomic_int finished;
    atomic_int failed;
} acquisition_atomic_reader_fixture;

static void *replace_acquisition_record(void *opaque)
{
    acquisition_atomic_reader_fixture *fixture = opaque;
    size_t iteration;
    for (iteration = 0u; iteration < 20000u; ++iteration) {
        const char *record = iteration % 2u ? fixture->large : "{}";
        size_t length = strlen(record);
        FILE *stream = fopen(fixture->pending, "wb");
        int failed;
        if (!stream) {
            atomic_store(&fixture->failed, 1);
            break;
        }
        failed = fwrite(record, 1u, length, stream) != length;
        if (fclose(stream) != 0) failed = 1;
        if (failed || rename(fixture->pending, fixture->path) != 0) {
            atomic_store(&fixture->failed, 1);
            break;
        }
    }
    atomic_store(&fixture->finished, 1);
    return NULL;
}

static int test_atomic_record_reader(void)
{
    char root[] = "/tmp/yvex-acquisition-read-XXXXXX";
    char path[YVEX_PATH_CAP], pending[YVEX_PATH_CAP], large[8192];
    acquisition_atomic_reader_fixture fixture;
    pthread_t writer;
    FILE *stream;
    size_t reads = 0u, torn = 0u;
    int joined, cleaned;
    YVEX_TEST_ASSERT(mkdtemp(root) != NULL, "atomic reader fixture root");
    snprintf(path, sizeof(path), "%s/operation.json", root);
    snprintf(pending, sizeof(pending), "%s/operation.pending", root);
    memset(large, ' ', sizeof(large) - 1u);
    large[0] = '{';
    large[sizeof(large) - 2u] = '}';
    large[sizeof(large) - 1u] = '\0';
    stream = fopen(path, "wb");
    YVEX_TEST_ASSERT(stream && fputs("{}", stream) >= 0 && fclose(stream) == 0,
                     "initial immutable reader record");
    {
        yvex_error err;
        size_t length = 123u;
        char *record;
        yvex_error_clear(&err);
        record = yvex_read_bounded_file(path, 1u, &length, &err);
        YVEX_TEST_ASSERT(!record && length == 0u && yvex_error_code(&err) == YVEX_ERR_BOUNDS,
                         "oversized opened metadata still refuses");
        YVEX_TEST_ASSERT(mkfifo(pending, 0600) == 0, "nonregular reader fixture");
        yvex_error_clear(&err);
        YVEX_TEST_ASSERT(!yvex_read_bounded_file(pending, sizeof(large), &length, &err) &&
                         length == 0u, "FIFO refuses without waiting for a writer");
        YVEX_TEST_ASSERT(unlink(pending) == 0, "remove exact FIFO fixture");
        YVEX_TEST_ASSERT(!yvex_read_bounded_file(root, sizeof(large), &length, &err) &&
                         length == 0u, "directory is not metadata");
        YVEX_TEST_ASSERT(!yvex_read_bounded_file(pending, sizeof(large), &length, &err) &&
                         length == 0u, "absent optional metadata remains absent");
    }
    fixture.path = path;
    fixture.pending = pending;
    fixture.large = large;
    atomic_init(&fixture.finished, 0);
    atomic_init(&fixture.failed, 0);
    YVEX_TEST_ASSERT(pthread_create(&writer, NULL, replace_acquisition_record,
                                    &fixture) == 0, "concurrent atomic publisher");
    do {
        yvex_error err;
        size_t length = 0u;
        char *record;
        yvex_error_clear(&err);
        record = yvex_read_bounded_file(path, sizeof(large), &length, &err);
        if (!record || !((length == 2u && !memcmp(record, "{}", 2u)) ||
                         (length == sizeof(large) - 1u &&
                          !memcmp(record, large, sizeof(large) - 1u)))) ++torn;
        free(record);
        ++reads;
    } while (!atomic_load(&fixture.finished) || reads < 20000u);
    joined = pthread_join(writer, NULL);
    (void)unlink(pending);
    cleaned = unlink(path) == 0 && rmdir(root) == 0;
    YVEX_TEST_ASSERT(joined == 0 && cleaned && !atomic_load(&fixture.failed),
                     "publisher retired and exact fixture cleaned");
    fprintf(stderr, "atomic metadata reader: %zu torn/%zu reads\n", torn, reads);
    YVEX_TEST_ASSERT(torn == 0u,
                     "bounded read must bind size and bytes to one opened inode");
    return 0;
}

int yvex_test_source_acquisition(void)
{
    if (test_operation_round_trip() != 0) return 1;
    if (test_reconciliation() != 0) return 1;
    if (test_source_observation() != 0) return 1;
    if (test_atomic_record_reader() != 0) return 1;
    return 0;
}
