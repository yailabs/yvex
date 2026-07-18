/*
 * YVEX - KV cache tests
 *
 * File: tests/test_kv.c
 * Layer: test
 *
 * Purpose:
 *   Proves that the engine/session layer KV cache object reports unavailable
 *   state for descriptor-only construction and owns a minimal bounded F32
 *   append/read store when created from an explicit KV shape.
 *
 * Covers:
 *   - yvex_kv_cache_create
 *   - yvex_kv_cache_get_summary
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_kv
 *
 * Expected:
 *   - exits 0 on success
 */
#include <limits.h>
#include <string.h>

#include <yvex/api.h>

#include "tests/test.h"

static void fill_values(float *values, unsigned long long count, float base)
{
    unsigned long long i;

    for (i = 0; i < count; ++i) {
        values[i] = base + (float)i;
    }
}

static int test_kv_unavailable_for_fixture(void)
{
    yvex_engine *engine = NULL;
    yvex_kv_cache *kv = NULL;
    yvex_kv_summary summary;
    yvex_error err;
    int rc;

    rc = yvex_engine_open_path(&engine, "tests/fixtures/gguf/valid-tokenizer-simple.gguf", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open engine");
    rc = yvex_kv_cache_create(&kv, yvex_engine_model(engine), 4096, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "create kv");
    YVEX_TEST_ASSERT(yvex_kv_status_of(kv) == YVEX_KV_STATUS_UNAVAILABLE, "kv unavailable");
    rc = yvex_kv_cache_get_summary(kv, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "kv summary");
    YVEX_TEST_ASSERT(summary.context_length == 4096, "kv context");
    YVEX_TEST_ASSERT(summary.bytes == 0, "kv bytes zero");
    YVEX_TEST_ASSERT_STREQ(summary.owner, "session", "kv owner");
    YVEX_TEST_ASSERT_STREQ(summary.dtype, "none", "kv unavailable dtype");
    YVEX_TEST_ASSERT(summary.session_owned == 1, "kv session owned");
    YVEX_TEST_ASSERT(summary.decode_ready == 0, "kv decode false");
    YVEX_TEST_ASSERT(summary.logits_ready == 0, "kv logits false");
    YVEX_TEST_ASSERT(summary.generation_ready == 0, "kv generation false");

    yvex_kv_cache_close(kv);
    yvex_engine_close(engine);
    return 0;
}

static int test_kv_invalid_context(void)
{
    yvex_engine *engine = NULL;
    yvex_kv_cache *kv = NULL;
    yvex_error err;
    int rc;

    rc = yvex_engine_open_path(&engine, "tests/fixtures/gguf/valid-tokenizer-simple.gguf", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open engine invalid context");
    rc = yvex_kv_cache_create(&kv, yvex_engine_model(engine), 0, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG, "zero context rejected");
    YVEX_TEST_ASSERT(kv == NULL, "failed kv null");
    yvex_engine_close(engine);
    return 0;
}

static int test_kv_shape_append_read_clear(void)
{
    yvex_kv_shape shape;
    yvex_kv_cache *kv = NULL;
    yvex_kv_summary summary;
    yvex_error err;
    float values[16];
    float readback[16];
    unsigned long long position = 99ull;
    int rc;

    memset(&shape, 0, sizeof(shape));
    memset(values, 0, sizeof(values));
    memset(readback, 0, sizeof(readback));
    shape.layer_count = 1;
    shape.kv_head_count = 2;
    shape.head_dim = 4;
    shape.capacity = 2;

    rc = yvex_kv_cache_create_shape(&kv, &shape, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "create shaped kv");
    YVEX_TEST_ASSERT(yvex_kv_status_of(kv) == YVEX_KV_STATUS_ALLOCATED, "kv allocated");
    YVEX_TEST_ASSERT(yvex_kv_cache_position_value_count(kv) == 16, "kv value count");

    fill_values(values, 16, 10.0f);
    rc = yvex_kv_cache_append_position_f32(kv, values, 16, &position, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "append kv position zero");
    YVEX_TEST_ASSERT(position == 0, "first appended position");
    fill_values(values, 16, 100.0f);
    rc = yvex_kv_cache_append_position_f32(kv, values, 16, &position, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "append kv position one");
    YVEX_TEST_ASSERT(position == 1, "second appended position");

    rc = yvex_kv_cache_read_position_f32(kv, 1, readback, 16, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read kv position one");
    YVEX_TEST_ASSERT(readback[0] == 100.0f, "readback first value");
    YVEX_TEST_ASSERT(readback[15] == 115.0f, "readback last value");

    rc = yvex_kv_cache_get_summary(kv, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocated kv summary");
    YVEX_TEST_ASSERT_STREQ(yvex_kv_status_name(summary.status), "allocated", "allocated status");
    YVEX_TEST_ASSERT_STREQ(summary.owner, "session", "allocated kv owner");
    YVEX_TEST_ASSERT_STREQ(summary.dtype, "F32", "allocated kv dtype");
    YVEX_TEST_ASSERT(summary.context_length == 2, "kv capacity");
    YVEX_TEST_ASSERT(summary.layer_count == 1, "kv layers");
    YVEX_TEST_ASSERT(summary.kv_head_count == 2, "kv heads");
    YVEX_TEST_ASSERT(summary.head_dim == 4, "kv head dim");
    YVEX_TEST_ASSERT(summary.values_per_position == 16, "kv values per position");
    YVEX_TEST_ASSERT(summary.bytes_per_position == 64, "kv bytes per position");
    YVEX_TEST_ASSERT(summary.bytes == 128, "kv bytes planned");
    YVEX_TEST_ASSERT(summary.allocated_bytes == 128, "kv bytes allocated");
    YVEX_TEST_ASSERT(summary.written_positions == 2, "kv written positions");
    YVEX_TEST_ASSERT(summary.append_count == 2, "kv append count");
    YVEX_TEST_ASSERT(summary.read_count == 1, "kv read count");
    YVEX_TEST_ASSERT(summary.last_read_position == 1, "kv last read");
    YVEX_TEST_ASSERT_STREQ(summary.overflow_status, "not-overflowed", "kv overflow status");
    YVEX_TEST_ASSERT_STREQ(summary.cleanup_status, "not-needed", "kv cleanup status");
    YVEX_TEST_ASSERT(summary.session_owned == 1, "allocated kv session owned");
    YVEX_TEST_ASSERT(summary.decode_ready == 0, "allocated kv decode false");
    YVEX_TEST_ASSERT(summary.logits_ready == 0, "allocated kv logits false");
    YVEX_TEST_ASSERT(summary.generation_ready == 0, "allocated kv generation false");

    rc = yvex_kv_cache_clear(kv, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "clear kv");
    rc = yvex_kv_cache_get_summary(kv, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "cleared kv summary");
    YVEX_TEST_ASSERT(summary.written_positions == 0, "clear written positions");
    YVEX_TEST_ASSERT(summary.append_count == 0, "clear append count");
    YVEX_TEST_ASSERT(summary.read_count == 0, "clear read count");
    YVEX_TEST_ASSERT_STREQ(summary.cleanup_status, "pass", "clear cleanup status");

    yvex_kv_cache_close(kv);
    return 0;
}

static int test_kv_shape_failures(void)
{
    yvex_kv_shape shape;
    yvex_kv_cache *kv = NULL;
    yvex_error err;
    float values[16];
    float readback[16];
    unsigned long long position = 0ull;
    int rc;

    memset(&shape, 0, sizeof(shape));
    rc = yvex_kv_cache_create_shape(&kv, &shape, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG, "zero shape rejected");
    YVEX_TEST_ASSERT(kv == NULL, "zero shape output null");

    memset(&shape, 0, sizeof(shape));
    shape.layer_count = 1;
    shape.kv_head_count = 0;
    shape.head_dim = 4;
    shape.capacity = 1;
    rc = yvex_kv_cache_create_shape(&kv, &shape, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG, "zero heads rejected");

    shape.kv_head_count = 2;
    shape.head_dim = 0;
    rc = yvex_kv_cache_create_shape(&kv, &shape, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG, "zero head dim rejected");

    shape.head_dim = 4;
    shape.capacity = 0;
    rc = yvex_kv_cache_create_shape(&kv, &shape, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG, "zero capacity rejected");

    shape.layer_count = 1;
    shape.kv_head_count = 2;
    shape.head_dim = 4;
    shape.capacity = 1;
    rc = yvex_kv_cache_create_shape(&kv, &shape, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "create failure kv");
    fill_values(values, 16, 1.0f);
    rc = yvex_kv_cache_append_position_f32(kv, values, 15, &position, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG, "append wrong count rejected");
    rc = yvex_kv_cache_read_position_f32(kv, 0, readback, 16, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "read unwritten rejected");
    rc = yvex_kv_cache_append_position_f32(kv, values, 16, &position, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "append full capacity");
    rc = yvex_kv_cache_append_position_f32(kv, values, 16, &position, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "append overflow rejected");
    rc = yvex_kv_cache_read_position_f32(kv, 1, readback, 16, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "read beyond capacity rejected");
    yvex_kv_cache_close(kv);

    kv = NULL;
    shape.layer_count = 1ull << 63;
    shape.kv_head_count = 4;
    shape.head_dim = 4;
    shape.capacity = 1;
    rc = yvex_kv_cache_create_shape(&kv, &shape, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "value count overflow rejected");
    YVEX_TEST_ASSERT(kv == NULL, "overflow output null");

    shape.layer_count = 1;
    shape.kv_head_count = 1;
    shape.head_dim = 1;
    shape.capacity = ULLONG_MAX;
    rc = yvex_kv_cache_create_shape(&kv, &shape, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "byte count overflow rejected");
    YVEX_TEST_ASSERT(kv == NULL, "byte overflow output null");
    return 0;
}

static int test_kv_repeated_lifecycle(void)
{
    yvex_kv_shape shape;
    yvex_error err;
    unsigned int round;
    int rc;

    memset(&shape, 0, sizeof(shape));
    shape.layer_count = 1;
    shape.kv_head_count = 1;
    shape.head_dim = 2;
    shape.capacity = 2;

    for (round = 0; round < 3u; ++round) {
        yvex_kv_cache *kv = NULL;
        float values[4];
        float readback[4];
        unsigned long long position = 99ull;
        unsigned long long i;

        for (i = 0; i < 4ull; ++i) {
            values[i] = (float)(round * 10u) + (float)i;
            readback[i] = 0.0f;
        }

        rc = yvex_kv_cache_create_shape(&kv, &shape, &err);
        YVEX_TEST_ASSERT(rc == YVEX_OK, "repeat create kv");
        rc = yvex_kv_cache_append_position_f32(kv, values, 4, &position, &err);
        YVEX_TEST_ASSERT(rc == YVEX_OK, "repeat append kv");
        YVEX_TEST_ASSERT(position == 0, "repeat appended position");
        rc = yvex_kv_cache_read_position_f32(kv, 0, readback, 4, &err);
        YVEX_TEST_ASSERT(rc == YVEX_OK, "repeat read kv");
        YVEX_TEST_ASSERT(readback[0] == values[0], "repeat read first");
        YVEX_TEST_ASSERT(readback[3] == values[3], "repeat read last");
        yvex_kv_cache_close(kv);
    }
    return 0;
}

int yvex_test_kv(void)
{
    if (test_kv_unavailable_for_fixture() != 0) return 1;
    if (test_kv_invalid_context() != 0) return 1;
    if (test_kv_shape_append_read_clear() != 0) return 1;
    if (test_kv_shape_failures() != 0) return 1;
    if (test_kv_repeated_lifecycle() != 0) return 1;
    return 0;
}
