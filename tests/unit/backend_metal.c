/* Qualify real embedding dispatch, exact bit publication, shared-buffer facts and retained
 * cleanup through the common backend API. Injected faults are separate from GPU execution. */
#include <stdint.h>
#include <stdlib.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/backend_resource.h>
#include <tests/test.h>

static int metal_open(yvex_backend **out, unsigned long long limit, yvex_error *err)
{
    yvex_backend_options options = {YVEX_BACKEND_KIND_METAL, NULL, limit};
    return yvex_backend_open(out, &options, err);
}

static int metal_alloc(yvex_backend *backend, yvex_device_tensor **out,
                       unsigned long long rows, unsigned long long cols, yvex_error *err)
{
    yvex_backend_tensor_desc desc = {0};
    desc.name = "foundation";
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = 2;
    desc.dims[0] = rows;
    desc.dims[1] = cols;
    desc.bytes = rows * cols * sizeof(float);
    return yvex_backend_tensor_alloc(backend, &desc, out, err);
}

int yvex_test_backend_metal_refusal(void)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options = {YVEX_BACKEND_KIND_METAL, "invalid-device", 0};
    yvex_error err;
    for (int i = 0; i <= YVEX_BACKEND_KIND_ROCM; i++) {
        yvex_backend_kind kind;
        YVEX_TEST_ASSERT(yvex_backend_kind_parse(yvex_backend_kind_name((yvex_backend_kind)i), &kind, &err) ==
                             YVEX_OK && kind == (yvex_backend_kind)i,
                         "canonical backend identities round-trip independent of admitted implementations");
    }
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_ERR_UNSUPPORTED && !backend,
                     "invalid/unavailable Metal selector never opens CPU");
#if !defined(__APPLE__) || !defined(__aarch64__)
    options.device = NULL;
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_ERR_UNSUPPORTED && !backend,
                     "non-native build refuses explicit Metal without Apple tools");
#endif
    yvex_backend_resource_facts facts;
    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open CPU resource control");
    memset(&facts, 0xff, sizeof(facts));
    YVEX_TEST_ASSERT(yvex_backend_get_resource_facts(backend, &facts, &err) == YVEX_ERR_UNSUPPORTED &&
                         facts.known == 0 && facts.schema == 0, "unobserved CPU facts remain unavailable");
    YVEX_TEST_ASSERT(yvex_backend_close_checked(&backend, &err) == YVEX_OK && !backend, "close CPU control");
    return 0;
}

static int metal_embedding_case(yvex_backend *metal, yvex_backend *cpu,
                                unsigned long long width, unsigned int tokens)
{
    const unsigned int vocab = 37;
    unsigned int ids[63];
    size_t table_bytes = (size_t)(width * vocab * sizeof(float));
    size_t output_bytes = (size_t)(width * tokens * sizeof(float));
    uint32_t *table = malloc(table_bytes), *expected = malloc(output_bytes);
    uint32_t *actual = malloc(output_bytes), *cpu_actual = malloc(output_bytes);
    yvex_device_tensor *mt = NULL, *mo = NULL, *mc = NULL, *ct = NULL, *co = NULL;
    yvex_error err;
    YVEX_TEST_ASSERT(table && expected && actual && cpu_actual, "allocate independent reference");
    for (size_t i = 0; i < table_bytes / sizeof(*table); i++) {
        static const uint32_t edge[] = {0, 0x80000000u, 1, 0x807fffffu, 0x7f7fffffu,
                                        0xff7fffffu, 0x7fc12345u, 0x7f800000u, 0xff800000u};
        table[i] = i % 29 < sizeof(edge) / sizeof(edge[0]) ? edge[i % 29] :
            (0x3f000000u | (uint32_t)((i * 719u) & 0x007fffffu));
    }
    for (unsigned int row = 0; row < tokens; row++) {
        ids[row] = row % 5 == 0 ? vocab - 1 : (row * 17u) % vocab;
        /* Reference addresses rows directly in original host bytes, without backend validators. */
        memcpy(expected + row * width, table + ids[row] * width, (size_t)width * sizeof(*table));
    }
    YVEX_TEST_ASSERT(metal_alloc(metal, &mt, width, vocab, &err) == YVEX_OK &&
                         metal_alloc(metal, &mo, tokens, width, &err) == YVEX_OK &&
                         metal_alloc(metal, &mc, tokens, width, &err) == YVEX_OK &&
                         metal_alloc(cpu, &ct, width, vocab, &err) == YVEX_OK &&
                         metal_alloc(cpu, &co, tokens, width, &err) == YVEX_OK, "allocate both backend paths");
    YVEX_TEST_ASSERT(yvex_backend_tensor_read(metal, mo, actual, output_bytes, &err) == YVEX_ERR_STATE,
                     "uninitialized shared storage cannot publish");
    YVEX_TEST_ASSERT(yvex_backend_tensor_write(metal, mt, table, table_bytes, &err) == YVEX_OK &&
                         yvex_backend_tensor_write(cpu, ct, table, table_bytes, &err) == YVEX_OK,
                     "copy independent source bytes into each backend");
    yvex_device_tensor view;
    YVEX_TEST_ASSERT(yvex_backend_tensor_f32_subview(mt, 1, 1, &view) &&
                         yvex_backend_tensor_read(metal, &view, actual, sizeof(float), &err) == YVEX_ERR_UNSUPPORTED,
                     "unqualified borrowed physical views refuse without reading the wrong buffer offset");
    yvex_device_tensor *borrowed = &view;
    YVEX_TEST_ASSERT(yvex_backend_tensor_copy(metal, borrowed, borrowed, &err) == YVEX_ERR_UNSUPPORTED,
                     "self-copy cannot admit an unqualified borrowed physical view");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(metal, &borrowed, &err) == YVEX_ERR_UNSUPPORTED &&
                         borrowed == &view, "borrowed view release preserves the owning buffer and stack view");
    YVEX_TEST_ASSERT(yvex_backend_op_embed(metal, mt, ids, tokens, mo, &err) == YVEX_OK &&
                         yvex_backend_op_embed(cpu, ct, ids, tokens, co, &err) == YVEX_OK,
                     "real Metal pipeline and CPU operation execute");
    YVEX_TEST_ASSERT(yvex_backend_sync(metal, &err) == YVEX_OK &&
                         yvex_backend_tensor_read(metal, mo, actual, output_bytes, &err) == YVEX_OK &&
                         yvex_backend_tensor_read(cpu, co, cpu_actual, output_bytes, &err) == YVEX_OK &&
                         memcmp(actual, expected, output_bytes) == 0 &&
                         memcmp(cpu_actual, expected, output_bytes) == 0,
                     "GPU/CPU/reference agree bitwise including exceptional F32 payloads");
    YVEX_TEST_ASSERT(yvex_backend_tensor_copy(metal, mc, mo, &err) == YVEX_OK &&
                         yvex_backend_tensor_read(metal, mc, actual, output_bytes, &err) == YVEX_OK &&
                         memcmp(actual, expected, output_bytes) == 0, "GPU blit copy completes before publication");
    YVEX_TEST_ASSERT(yvex_backend_tensor_zero(metal, mc, &err) == YVEX_OK &&
                         yvex_backend_tensor_read(metal, mc, actual, output_bytes, &err) == YVEX_OK,
                     "GPU blit zero completes");
    for (size_t i = 0; i < output_bytes / sizeof(*actual); i++)
        YVEX_TEST_ASSERT(actual[i] == 0, "zero covers complete extent");
    ids[0] = vocab;
    YVEX_TEST_ASSERT(yvex_backend_op_embed(metal, mt, ids, tokens, mo, &err) == YVEX_ERR_BOUNDS,
                     "invalid token refuses before command submission");
    YVEX_TEST_ASSERT(yvex_backend_op_embed(metal, ct, ids, tokens, mo, &err) == YVEX_ERR_STATE,
                     "foreign CPU tensor never acts as a hidden fallback");
    YVEX_TEST_ASSERT(yvex_backend_op_matmul(metal, mo, mo, mc, &err) == YVEX_ERR_UNSUPPORTED &&
                         yvex_backend_op_rms_norm(metal, mo, mo, 1e-6f, mc, &err) == YVEX_ERR_UNSUPPORTED,
                     "unqualified numerical classes fail closed");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(metal, &mt, &err) == YVEX_OK &&
                         yvex_backend_tensor_release(metal, &mo, &err) == YVEX_OK &&
                         yvex_backend_tensor_release(metal, &mc, &err) == YVEX_OK &&
                         yvex_backend_tensor_release(cpu, &ct, &err) == YVEX_OK &&
                         yvex_backend_tensor_release(cpu, &co, &err) == YVEX_OK, "release exact buffer ownership");
    free(table); free(expected); free(actual); free(cpu_actual);
    return 0;
}

int yvex_test_metal_foundation(void)
{
    yvex_backend *metal = NULL, *cpu = NULL;
    yvex_backend_device_info device;
    yvex_backend_resource_facts initial, facts;
    yvex_backend_memory_stats stats;
    yvex_error err;
    YVEX_TEST_ASSERT(!getenv("YVEX_TEST_METAL_FAIL_STAGE"), "real GPU evidence forbids diagnostic fault hooks");
    YVEX_TEST_ASSERT(metal_open(&metal, 16ull * 1024 * 1024, &err) == YVEX_OK,
                     "admit native Metal device and compiled pipeline");
    YVEX_TEST_ASSERT(yvex_backend_kind_of(metal) == YVEX_BACKEND_KIND_METAL &&
                         yvex_backend_status_of(metal) == YVEX_BACKEND_STATUS_READY &&
                         yvex_backend_get_device_info(metal, &device, &err) == YVEX_OK &&
                         !device.unified_addressing && !device.free_memory_bytes && !device.total_memory_bytes,
                     "identity and unified addressability are truthful without invented free GPU bytes");
    for (int i = 0; i < YVEX_BACKEND_VARIANT_COUNT; i++) {
        yvex_backend_capability_result capability;
        int supported = i <= YVEX_BACKEND_VARIANT_TENSOR_COPY || i == YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32;
        YVEX_TEST_ASSERT(yvex_backend_query_capability(metal, (yvex_backend_operation_variant)i,
                                                      &capability, &err) == YVEX_OK &&
                             (capability.state == YVEX_BACKEND_CAPABILITY_SUPPORTED) == supported &&
                             capability.backend_kind == YVEX_BACKEND_KIND_METAL,
                         "capability publishes exactly the earned variants");
    }
    YVEX_TEST_ASSERT(yvex_backend_get_resource_facts(metal, &initial, &err) == YVEX_OK &&
                         initial.schema == YVEX_BACKEND_RESOURCE_SCHEMA && initial.shared_system_memory &&
                         !(initial.known & (YVEX_BACKEND_RESOURCE_RESIDENT | YVEX_BACKEND_RESOURCE_WORKING_SET)) &&
                         initial.max_buffer_bytes && initial.recommended_working_set_bytes,
                     "owned/API observations distinguish unknown residency and actual working set");
    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&cpu, &err) == YVEX_OK, "open CPU oracle");
    const unsigned long long widths[] = {1, 257, 1025};
    const unsigned int tokens[] = {1, 13, 63};
    for (unsigned int round = 0; round < 4; round++)
        for (unsigned int i = 0; i < 3; i++)
            YVEX_TEST_ASSERT(metal_embedding_case(metal, cpu, widths[i], tokens[i]) == 0,
                             "qualified boundary/duplicate/partial-threadgroup embedding case");
    YVEX_TEST_ASSERT(yvex_backend_get_resource_facts(metal, &facts, &err) == YVEX_OK &&
                         yvex_backend_get_memory_stats(metal, &stats, &err) == YVEX_OK &&
                         facts.kernel_dispatches == 12 && facts.command_completions == 36 &&
                         !facts.addressable_bytes && !facts.mapped_bytes && !facts.allocated_bytes &&
                         !facts.temporary_bytes && facts.peak_temporary_bytes == 63 * sizeof(unsigned int) &&
                         !stats.allocation_count && stats.allocation_events == stats.release_events &&
                         !stats.h2d_bytes && !stats.d2h_bytes,
                     "all synchronous kernels/blits and temporary/tensor ownership reconcile");
    printf("Metal device=%s index=%d unified=1 recommended_bytes=%llu max_buffer_bytes=%llu\n",
           device.name, device.device_index, facts.recommended_working_set_bytes, facts.max_buffer_bytes);
    printf("Metal kernels=%llu completions=%llu bit_mismatches=0 widths=1,257,1025 tokens=1,13,63 repeats=4\n",
           facts.kernel_dispatches, facts.command_completions);
    printf("Metal allocated=%llu mapped=%llu temporary=%llu peak_temporary=%llu device_allocated_start=%llu end=%llu\n",
           facts.allocated_bytes, facts.mapped_bytes, facts.temporary_bytes, facts.peak_temporary_bytes,
           initial.device_allocated_bytes, facts.device_allocated_bytes);
    printf("Metal host_write_copies=%llu host_read_copies=%llu device_copies=%llu allocation_events=%llu releases=%llu\n",
           facts.host_write_copy_bytes, facts.host_read_copy_bytes, facts.device_copy_bytes,
           stats.allocation_events, stats.release_events);
    YVEX_TEST_ASSERT(yvex_backend_close_checked(&metal, &err) == YVEX_OK && !metal &&
                         yvex_backend_close_checked(&cpu, &err) == YVEX_OK && !cpu, "complete both backend lifecycles");
    return 0;
}

static int metal_failure_case(const char *stage)
{
    yvex_backend *metal = NULL;
    yvex_device_tensor *table = NULL, *out = NULL;
    yvex_backend_capability_result capability;
    yvex_error err;
    const float data[] = {1, 2, 3, 4};
    unsigned int id = 0;
    YVEX_TEST_ASSERT(metal_open(&metal, 1024, &err) == YVEX_OK &&
                         metal_alloc(metal, &table, 2, 2, &err) == YVEX_OK &&
                         metal_alloc(metal, &out, 1, 2, &err) == YVEX_OK &&
                         yvex_backend_tensor_write(metal, table, data, sizeof(data), &err) == YVEX_OK,
                     "prepare native failure lifecycle");
    YVEX_TEST_ASSERT(setenv("YVEX_TEST_METAL_FAIL_STAGE", stage, 1) == 0, "select diagnostic fault");
    YVEX_TEST_ASSERT(yvex_backend_op_embed(metal, table, &id, 1, out, &err) != YVEX_OK &&
                         !yvex_device_tensor_is_written(out), "failed operation publishes no initialized output");
    YVEX_TEST_ASSERT(unsetenv("YVEX_TEST_METAL_FAIL_STAGE") == 0, "clear diagnostic fault");
    if (strcmp(stage, "allocation") == 0) {
        YVEX_TEST_ASSERT(yvex_backend_status_of(metal) == YVEX_BACKEND_STATUS_READY &&
                             yvex_backend_op_embed(metal, table, &id, 1, out, &err) == YVEX_OK,
                         "allocation refusal leaves a reusable healthy context");
    } else {
        YVEX_TEST_ASSERT(yvex_backend_status_of(metal) == YVEX_BACKEND_STATUS_FAILED &&
                             yvex_backend_query_capability(metal, YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32,
                                                           &capability, &err) == YVEX_OK &&
                             capability.state == YVEX_BACKEND_CAPABILITY_FAILED &&
                             yvex_backend_op_embed(metal, table, &id, 1, out, &err) == YVEX_ERR_STATE,
                         "launch/completion failure poisons dispatch with no fallback");
    }
    YVEX_TEST_ASSERT(yvex_backend_close_checked(&metal, &err) == YVEX_ERR_STATE && metal,
                     "checked close retains context while tensors remain");
    YVEX_TEST_ASSERT(yvex_backend_tensor_write(metal, table, data, sizeof(data), &err) == YVEX_ERR_STATE,
                     "retained context admits cleanup only");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(metal, &out, &err) == YVEX_OK &&
                         yvex_backend_tensor_release(metal, &table, &err) == YVEX_OK &&
                         yvex_backend_close_checked(&metal, &err) == YVEX_OK && !metal,
                     "cleanup drains buffers/temporary resources and close retry nulls ownership");
    return 0;
}

static int metal_storage_case(void)
{
    yvex_backend *metal = NULL;
    yvex_device_tensor *a = NULL, *b = NULL, *half = NULL, *out = NULL;
    yvex_backend_tensor_desc desc = {0};
    yvex_error err;
    const unsigned char bytes[] = {7, 11, 29};
    unsigned char actual[3];
    const unsigned short source[] = {0x3c00, 0x4000, 0x4200, 0x4400};
    unsigned int id = 0;
    desc.name = "byte-storage"; desc.dtype = YVEX_DTYPE_I8; desc.rank = 1;
    desc.dims[0] = 3; desc.bytes = 3;
    YVEX_TEST_ASSERT(metal_open(&metal, 4096, &err) == YVEX_OK &&
                         yvex_backend_tensor_alloc(metal, &desc, &a, &err) == YVEX_OK &&
                         yvex_backend_tensor_alloc(metal, &desc, &b, &err) == YVEX_OK &&
                         yvex_backend_tensor_write(metal, a, bytes, sizeof(bytes), &err) == YVEX_OK &&
                         yvex_backend_tensor_copy(metal, b, a, &err) == YVEX_OK &&
                         yvex_backend_tensor_read(metal, b, actual, sizeof(actual), &err) == YVEX_OK &&
                         memcmp(bytes, actual, sizeof(bytes)) == 0 &&
                         yvex_backend_tensor_zero(metal, b, &err) == YVEX_OK &&
                         yvex_backend_tensor_read(metal, b, actual, sizeof(actual), &err) == YVEX_OK &&
                         !actual[0] && !actual[1] && !actual[2], "byte storage blits cover odd extents");
    desc.dtype = YVEX_DTYPE_F16; desc.rank = 2;
    desc.dims[0] = 2; desc.dims[1] = 2; desc.bytes = sizeof(source);
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(metal, &desc, &half, &err) == YVEX_OK &&
                         metal_alloc(metal, &out, 1, 2, &err) == YVEX_OK &&
                         yvex_backend_tensor_write(metal, half, source, sizeof(source), &err) == YVEX_OK &&
                         yvex_backend_op_embed(metal, half, &id, 1, out, &err) == YVEX_ERR_UNSUPPORTED &&
                         !yvex_device_tensor_is_written(out) &&
                         yvex_backend_status_of(metal) == YVEX_BACKEND_STATUS_READY,
                     "valid F16 geometry refuses without conversion, fallback or poisoned context");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(metal, &a, &err) == YVEX_OK &&
                         yvex_backend_tensor_release(metal, &b, &err) == YVEX_OK &&
                         yvex_backend_tensor_release(metal, &half, &err) == YVEX_OK &&
                         yvex_backend_tensor_release(metal, &out, &err) == YVEX_OK &&
                         yvex_backend_close_checked(&metal, &err) == YVEX_OK && !metal, "release storage fixture");
    return 0;
}

int yvex_test_metal_failure(void)
{
    yvex_backend *metal = NULL;
    yvex_device_tensor *table = NULL, *out = NULL, *extra = NULL;
    yvex_backend_memory_stats stats;
    yvex_error err;
    YVEX_TEST_ASSERT(!getenv("YVEX_TEST_METAL_FAIL_STAGE"), "fault qualification owns its diagnostic hook");
    YVEX_TEST_ASSERT(metal_storage_case() == 0, "storage and precision refusal qualified");
    const char *open_faults[] = {"device", "queue", "pipeline"};
    for (unsigned int i = 0; i < 3; i++) {
        YVEX_TEST_ASSERT(setenv("YVEX_TEST_METAL_FAIL_STAGE", open_faults[i], 1) == 0 &&
                             metal_open(&metal, 0, &err) == YVEX_ERR_UNSUPPORTED && !metal &&
                             unsetenv("YVEX_TEST_METAL_FAIL_STAGE") == 0,
                         "partial device/queue/pipeline admission releases without publishing an owner");
    }
    const char *stages[] = {"allocation", "command", "encoder", "completion"};
    for (unsigned int i = 0; i < 4; i++)
        YVEX_TEST_ASSERT(metal_failure_case(stages[i]) == 0, "fault stage qualified");
    const float data[] = {1, 2, 3, 4};
    unsigned int id = 0;
    YVEX_TEST_ASSERT(metal_open(&metal, 24, &err) == YVEX_OK &&
                         metal_alloc(metal, &table, 2, 2, &err) == YVEX_OK &&
                         metal_alloc(metal, &out, 1, 2, &err) == YVEX_OK &&
                         yvex_backend_tensor_write(metal, table, data, sizeof(data), &err) == YVEX_OK,
                     "exact declared byte budget admits only permanent storage");
    YVEX_TEST_ASSERT(metal_alloc(metal, &extra, 1, 1, &err) != YVEX_OK && !extra &&
                         yvex_backend_op_embed(metal, table, &id, 1, out, &err) != YVEX_OK &&
                         yvex_backend_get_memory_stats(metal, &stats, &err) == YVEX_OK &&
                         stats.allocated_bytes == 24 && stats.allocation_count == 2 &&
                         yvex_backend_status_of(metal) == YVEX_BACKEND_STATUS_READY,
                     "permanent and temporary capacity refusal leaves accounting unchanged");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(metal, &table, &err) == YVEX_OK &&
                         yvex_backend_tensor_release(metal, &out, &err) == YVEX_OK &&
                         yvex_backend_close_checked(&metal, &err) == YVEX_OK && !metal,
                     "capacity refusal remains cleanup-safe");
    printf("Metal diagnostic faults=device,queue,pipeline,allocation,command,encoder,completion budget_refusal=PASS\n");
    return 0;
}
