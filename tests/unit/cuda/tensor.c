/*
 * Exercises CUDA tensor allocation, zero-read, write/read, copy, and memory accounting when CUDA
 * is available. Returns 77 when CUDA is unavailable.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/api.h>
#include <yvex/internal/backend.h>
#include "src/backend/cuda/private.h"

#include "tests/test.h"

static void make_desc(yvex_backend_tensor_desc *desc,
                      const char *name,
                      unsigned long long d0,
                      unsigned long long d1)
{
    memset(desc, 0, sizeof(*desc));
    desc->name = name;
    desc->dtype = YVEX_DTYPE_F32;
    desc->rank = 2;
    desc->dims[0] = d0;
    desc->dims[1] = d1;
    desc->bytes = d0 * d1 * (unsigned long long)sizeof(float);
}

static int assert_virtual_pages(yvex_backend *backend)
{
    yvex_backend_memory_stats before, committed, released;
    yvex_backend_tensor_desc desc = {0};
    yvex_device_tensor *tensor = NULL;
    yvex_error err;
    unsigned long long granularity = 0ull, delta = 0ull, freed = 0ull;
    unsigned char value = 0x5au, observed = 0u;

    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK,
                     "snapshot memory before virtual reservation");
    desc.name = "cuda_virtual_pages";
    desc.dtype = YVEX_DTYPE_I8;
    desc.rank = 1u;
    desc.dims[0] = desc.bytes = 8ull * 1024ull * 1024ull;
    YVEX_TEST_ASSERT(yvex_backend_tensor_reserve(
                         backend, &desc, &tensor, &granularity, &err) == YVEX_OK &&
                         tensor && granularity && tensor->virtual_reserved &&
                         tensor->resident_bytes == 0ull,
                     "reserve stable CUDA virtual tensor without physical pages");
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &committed, &err) == YVEX_OK &&
                         committed.allocated_bytes == before.allocated_bytes,
                     "virtual address reservation consumes no physical memory budget");
    YVEX_TEST_ASSERT(yvex_backend_tensor_commit_range(
                         backend, tensor, granularity + 1ull, 1ull, &delta, &err) == YVEX_OK &&
                         delta == granularity && tensor->resident_bytes == granularity,
                     "commit exactly one intersecting physical granule");
    YVEX_TEST_ASSERT(yvex_backend_tensor_commit_range(
                         backend, tensor, granularity + 2ull, 1ull, &delta, &err) == YVEX_OK &&
                         delta == 0ull && tensor->resident_bytes == granularity,
                     "overlapping commitment is idempotent");
    YVEX_TEST_ASSERT(yvex_backend_tensor_commit_range(
                         backend, tensor, 3ull * granularity, 1ull, &delta, &err) == YVEX_OK &&
                         delta == granularity && tensor->resident_bytes == 2ull * granularity,
                     "commit a disjoint physical granule");
    {
        yvex_device_tensor view = *tensor;
        view.data += granularity + 1ull;
        view.bytes = view.dims[0] = 1ull;
        YVEX_TEST_ASSERT(yvex_backend_tensor_write(
                             backend, &view, &value, 1ull, &err) == YVEX_OK &&
                             yvex_backend_tensor_read(
                                 backend, &view, &observed, 1ull, &err) == YVEX_OK &&
                             observed == value,
                         "committed virtual page supports bounded tensor movement");
    }
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &committed, &err) == YVEX_OK &&
                         committed.allocated_bytes ==
                             before.allocated_bytes + 2ull * granularity,
                     "physical page commitments update exact memory accounting");
    YVEX_TEST_ASSERT(yvex_backend_tensor_decommit(
                         backend, tensor, &freed, &err) == YVEX_OK &&
                         freed == 2ull * granularity && tensor->resident_bytes == 0ull,
                     "decommit every physical page while retaining virtual ownership");
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &released, &err) == YVEX_OK &&
                         released.allocated_bytes == before.allocated_bytes,
                     "decommit restores the pre-reservation physical budget");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &tensor, &err) == YVEX_OK && !tensor,
                     "release the stable virtual address exactly once");
    return 0;
}

static CUresult (*copy_default_driver)(CUdeviceptr, CUdeviceptr, size_t);
static CUresult (*copy_stream_driver)(CUdeviceptr, CUdeviceptr, size_t, CUstream);
static unsigned int copy_default_calls, copy_stream_calls;
static CUstream copy_observed_stream;

static CUresult observe_default_copy(CUdeviceptr dst, CUdeviceptr src, size_t bytes)
{
    copy_default_calls++;
    return copy_default_driver(dst, src, bytes);
}

static CUresult observe_stream_copy(CUdeviceptr dst, CUdeviceptr src, size_t bytes, CUstream stream)
{
    copy_stream_calls++; copy_observed_stream = stream;
    return copy_stream_driver(dst, src, bytes, stream);
}

static CUresult (*host_write_driver)(CUdeviceptr, const void *, size_t, CUstream);
static CUresult (*host_read_driver)(void *, CUdeviceptr, size_t, CUstream);
static CUresult (*zero_stream_driver)(CUdeviceptr, unsigned char, size_t, CUstream);
static CUstream movement_stream;
static unsigned int movement_calls[3], movement_wrong_stream;

static CUresult observe_host_write(CUdeviceptr dst, const void *src, size_t bytes, CUstream stream)
{
    movement_calls[0]++; movement_wrong_stream += stream != movement_stream;
    return host_write_driver(dst, src, bytes, stream);
}

static CUresult observe_host_read(void *dst, CUdeviceptr src, size_t bytes, CUstream stream)
{
    movement_calls[1]++; movement_wrong_stream += stream != movement_stream;
    return host_read_driver(dst, src, bytes, stream);
}

static CUresult observe_zero_stream(CUdeviceptr dst, unsigned char value, size_t bytes, CUstream stream)
{
    movement_calls[2]++; movement_wrong_stream += stream != movement_stream;
    return zero_stream_driver(dst, value, bytes, stream);
}

static int assert_session_movement(yvex_backend *backend, yvex_device_tensor *tensor)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_device_tensor view = *tensor;
    float input[] = {1.0f, -2.0f, 3.0f, -4.0f}, output[4] = {0}, cleared[4];
    yvex_error err;
    view.bytes = sizeof(input); view.rank = 1u; view.dims[0] = 4u;
    memset(cleared, 0xff, sizeof(cleared));
    host_write_driver = state->driver.cuMemcpyHtoDAsync_v2;
    host_read_driver = state->driver.cuMemcpyDtoHAsync_v2;
    zero_stream_driver = state->driver.cuMemsetD8Async;
    movement_stream = yvex_cuda_launch_stream(backend);
    memset(movement_calls, 0, sizeof(movement_calls)); movement_wrong_stream = 0u;
    state->driver.cuMemcpyHtoDAsync_v2 = observe_host_write;
    state->driver.cuMemcpyDtoHAsync_v2 = observe_host_read;
    state->driver.cuMemsetD8Async = observe_zero_stream;
    int rc = yvex_backend_tensor_write(backend, &view, input, sizeof(input), &err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_read(backend, &view, output, sizeof(output), &err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_zero(backend, &view, &err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_read(backend, &view, cleared, sizeof(cleared), &err);
    state->driver.cuMemcpyHtoDAsync_v2 = host_write_driver;
    state->driver.cuMemcpyDtoHAsync_v2 = host_read_driver;
    state->driver.cuMemsetD8Async = zero_stream_driver;
    printf("CUDA session movement: write=%u read=%u zero=%u wrong_stream=%u "
        "values=%g,%g,%g,%g cleared=%g,%g,%g,%g\n", movement_calls[0], movement_calls[1],
        movement_calls[2], movement_wrong_stream, output[0], output[1], output[2], output[3],
        cleared[0], cleared[1], cleared[2], cleared[3]);
    YVEX_TEST_ASSERT(rc == YVEX_OK && movement_stream && movement_calls[0] == 1u &&
        movement_calls[1] == 2u && movement_calls[2] == 1u && !movement_wrong_stream,
        "host movement and zeroing must follow the session producer stream before their completion barrier");
    YVEX_TEST_ASSERT(memcmp(input, output, sizeof(input)) == 0 &&
        cleared[0] == 0.0f && cleared[1] == 0.0f && cleared[2] == 0.0f && cleared[3] == 0.0f,
        "host publication and zeroing preserve exact tensor contents");
    return 0;
}

static int assert_shared_stream_copy(yvex_backend *owner)
{
    enum { SHARED_COPY_VALUES = 1024 * 256 };
    yvex_backend *source_backend = NULL, *destination_backend = NULL;
    yvex_device_tensor *source = NULL, *destination = NULL, *replacement = NULL;
    yvex_backend_tensor_desc desc;
    yvex_error err;
    static float input[SHARED_COPY_VALUES], output[SHARED_COPY_VALUES];
    static float replacement_values[SHARED_COPY_VALUES];
    unsigned long long index;
    for (index = 0ull; index < SHARED_COPY_VALUES; ++index) {
        input[index] = (float)(index % 251ull);
        replacement_values[index] = -(float)(index % 241ull) - 1.0f;
    }
    make_desc(&desc, "cuda_shared_copy", 1024ull, 256ull);
    YVEX_TEST_ASSERT(
        yvex_backend_open_shared_cuda(&source_backend, owner, 0ull, &err) == YVEX_OK &&
            yvex_backend_open_shared_cuda(&destination_backend, owner, 0ull, &err) == YVEX_OK,
        "open two session streams over one CUDA physical owner");
    YVEX_TEST_ASSERT(
        yvex_backend_tensor_alloc(source_backend, &desc, &source, &err) == YVEX_OK &&
            yvex_backend_tensor_alloc(source_backend, &desc, &replacement, &err) == YVEX_OK &&
            yvex_backend_tensor_alloc(destination_backend, &desc, &destination, &err) == YVEX_OK,
        "allocate cross-stream source and destination tensors");
    YVEX_TEST_ASSERT(assert_session_movement(source_backend, source) == 0,
        "tensor movement has one authoritative session execution order");
    YVEX_TEST_ASSERT(
        yvex_backend_tensor_write(source_backend, source, input, desc.bytes, &err) == YVEX_OK &&
            yvex_backend_tensor_write(source_backend, replacement, replacement_values,
                                      desc.bytes, &err) == YVEX_OK,
        "prepare immutable source controls");
    yvex_cuda_backend_state *state = yvex_cuda_state(source_backend);
    copy_default_driver = state->driver.cuMemcpyDtoD_v2;
    copy_stream_driver = state->driver.cuMemcpyDtoDAsync_v2;
    copy_default_calls = copy_stream_calls = 0u; copy_observed_stream = NULL;
    state->driver.cuMemcpyDtoD_v2 = observe_default_copy;
    state->driver.cuMemcpyDtoDAsync_v2 = observe_stream_copy;
    int copied = yvex_backend_tensor_copy(source_backend, replacement, source, &err);
    state->driver.cuMemcpyDtoD_v2 = copy_default_driver;
    state->driver.cuMemcpyDtoDAsync_v2 = copy_stream_driver;
    printf("CUDA synchronous copy ordering: default_calls=%u stream_calls=%u same_producer_stream=%d\n",
        copy_default_calls, copy_stream_calls, copy_observed_stream == yvex_cuda_launch_stream(source_backend));
    YVEX_TEST_ASSERT(copied == YVEX_OK && copy_default_calls == 0u && copy_stream_calls == 1u &&
        copy_observed_stream == yvex_cuda_launch_stream(source_backend),
        "synchronous copy must enqueue after its producer, not copy on default stream before waiting");
    YVEX_TEST_ASSERT(
            yvex_backend_tensor_write(source_backend, replacement, replacement_values, desc.bytes, &err) == YVEX_OK &&
            yvex_backend_tensor_copy_shared_async(
                destination_backend, destination, source, &err) == YVEX_OK &&
            yvex_backend_tensor_copy_async(
                source_backend, source, replacement, &err) == YVEX_OK &&
            yvex_backend_tensor_release(source_backend, &source, &err) == YVEX_OK && !source &&
            yvex_backend_tensor_read(
                destination_backend, destination, output, desc.bytes, &err) == YVEX_OK &&
            memcmp(input, output, (size_t)desc.bytes) == 0,
        "source release observes cross-stream D2D consumption before storage reuse");
    YVEX_TEST_ASSERT(
        yvex_backend_tensor_release(source_backend, &replacement, &err) == YVEX_OK &&
            yvex_backend_tensor_release(destination_backend, &destination, &err) == YVEX_OK,
        "release cross-stream tensors");
    yvex_backend_close(source_backend);
    yvex_backend_close(destination_backend);
    return 0;
}


int yvex_cuda_test_tensor(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *a = NULL;
    yvex_device_tensor *b = NULL;
    yvex_backend_options options;
    yvex_backend_tensor_desc desc;
    yvex_backend_memory_stats stats;
    yvex_backend_memory_stats before_failed_release;
    yvex_backend_capability_result capability;
    yvex_error err;
    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float out[4] = {9.0f, 9.0f, 9.0f, 9.0f};
    int rc;

    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        fprintf(stderr, "SKIP: CUDA unavailable: %s\n", yvex_error_message(&err));
        return 77;
    }
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open cuda backend");
    YVEX_TEST_ASSERT(assert_virtual_pages(backend) == 0,
                     "CUDA virtual page ownership is transactional");
    YVEX_TEST_ASSERT(assert_shared_stream_copy(backend) == 0,
                     "CUDA shared physical owner supports ordered row movement");

    rc = yvex_backend_get_memory_stats(backend, &stats, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "initial stats");
    YVEX_TEST_ASSERT(stats.allocated_bytes == 0, "initial allocated bytes");

    make_desc(&desc, "cuda_a", 2, 2);
    rc = yvex_backend_tensor_alloc(backend, &desc, &a, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda tensor a");
    rc = yvex_backend_tensor_read(backend, a, out, sizeof(out), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read zero initialized");
    YVEX_TEST_ASSERT(out[0] == 0.0f && out[3] == 0.0f, "zero initialized values");

    rc = yvex_backend_tensor_write(backend, a, data, sizeof(data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write cuda tensor");
    memset(out, 0, sizeof(out));
    rc = yvex_backend_tensor_read(backend, a, out, sizeof(out), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read cuda tensor");
    YVEX_TEST_ASSERT(memcmp(data, out, sizeof(data)) == 0, "read equals written");

    make_desc(&desc, "cuda_b", 2, 2);
    rc = yvex_backend_tensor_alloc(backend, &desc, &b, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda tensor b");
    rc = yvex_backend_tensor_copy(backend, b, a, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "copy cuda tensor");
    memset(out, 0, sizeof(out));
    rc = yvex_backend_tensor_read(backend, b, out, sizeof(out), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read copied cuda tensor");
    YVEX_TEST_ASSERT(memcmp(data, out, sizeof(data)) == 0, "copy equals source");

    rc = yvex_backend_tensor_release(backend, &b, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && b == NULL, "checked release cuda tensor b");
    rc = yvex_backend_tensor_release(backend, &a, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && a == NULL, "checked release cuda tensor a");
    rc = yvex_backend_get_memory_stats(backend, &stats, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "final stats");
    YVEX_TEST_ASSERT(stats.allocated_bytes == 0, "allocated bytes return to zero");
    YVEX_TEST_ASSERT(stats.allocation_count == 0, "allocation count returns to zero");
    yvex_backend_close(backend);

    backend = NULL;
    rc = yvex_backend_open(&backend, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "reopen cuda backend for sync failure");
    make_desc(&desc, "cuda_sync_failure", 2, 2);
    rc = yvex_backend_tensor_alloc(backend, &desc, &a, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate sync failure tensor");
    YVEX_TEST_ASSERT(setenv("YVEX_TEST_CUDA_SYNC_FAILURE", "tensor-write", 1) == 0,
                     "set tensor write sync failure");
    rc = yvex_backend_tensor_write(backend, a, data, sizeof(data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BACKEND, "tensor write sync failure is returned");
    YVEX_TEST_ASSERT(!yvex_device_tensor_is_written(a),
                     "failed synchronized write remains unwritten");
    rc = yvex_backend_query_capability(backend, YVEX_BACKEND_VARIANT_TENSOR_WRITE,
                                       &capability, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK &&
                     capability.state == YVEX_BACKEND_CAPABILITY_FAILED &&
                     capability.reason ==
                         YVEX_BACKEND_CAPABILITY_REASON_SYNCHRONIZATION_FAILED,
                     "write sync failure demotes exact capability");
    rc = yvex_backend_query_capability(backend, YVEX_BACKEND_VARIANT_TENSOR_READ,
                                       &capability, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK &&
                     capability.state == YVEX_BACKEND_CAPABILITY_FAILED &&
                     capability.reason ==
                         YVEX_BACKEND_CAPABILITY_REASON_SYNCHRONIZATION_FAILED,
                     "context synchronization failure blocks other dispatch");
    YVEX_TEST_ASSERT(unsetenv("YVEX_TEST_CUDA_SYNC_FAILURE") == 0,
                     "clear tensor write sync failure");
    rc = yvex_backend_tensor_release(backend, &a, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && a == NULL, "release after sync failure");
    yvex_backend_close(backend);

    backend = NULL;
    rc = yvex_backend_open(&backend, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "reopen cuda backend for cleanup failure");
    make_desc(&desc, "cuda_cleanup_failure", 2, 2);
    rc = yvex_backend_tensor_alloc(backend, &desc, &a, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cleanup failure tensor");
    rc = yvex_backend_get_memory_stats(backend, &before_failed_release, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "stats before failed release");
    YVEX_TEST_ASSERT(setenv("YVEX_TEST_CUDA_CLEANUP_FAILURE", "tensor-alloc", 1) == 0,
                     "set tensor cleanup failure");
    rc = yvex_backend_tensor_release(backend, &a, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BACKEND && a != NULL,
                     "failed release preserves owned tensor");
    rc = yvex_backend_get_memory_stats(backend, &stats, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "stats after failed release");
    YVEX_TEST_ASSERT(stats.allocated_bytes == before_failed_release.allocated_bytes &&
                     stats.allocation_count == before_failed_release.allocation_count,
                     "failed release preserves truthful allocation accounting");
    YVEX_TEST_ASSERT(unsetenv("YVEX_TEST_CUDA_CLEANUP_FAILURE") == 0,
                     "clear tensor cleanup failure");
    rc = yvex_backend_tensor_release(backend, &a, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && a == NULL, "retry release succeeds");
    rc = yvex_backend_get_memory_stats(backend, &stats, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && stats.allocated_bytes == 0 &&
                     stats.allocation_count == 0,
                     "retry release restores zero accounting");
    yvex_backend_close(backend);
    return 0;
}
