/* Real Metal submission owns one queue and shared system-memory buffers. Every submitted
 * command completes before host publication or resource release; no CPU operation fallback. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdlib.h>
#include <string.h>
#include <src/backend/private.h>

@interface yvex_metal_context : NSObject
@property(nonatomic, strong) id<MTLDevice> device;
@property(nonatomic, strong) id<MTLCommandQueue> queue;
@property(nonatomic, strong) id<MTLComputePipelineState> embedding;
@property(nonatomic) yvex_backend_resource_facts facts;
@property(nonatomic) yvex_backend_capability_reason failure;
@end
@implementation yvex_metal_context
@end

@interface yvex_metal_storage : NSObject
@property(nonatomic, strong) id<MTLBuffer> buffer;
@property(nonatomic) yvex_device_tensor *owner;
@end
@implementation yvex_metal_storage
@end

/* The existing F32 embedding operation selects source rows without arithmetic. Moving uint
 * representations preserves every F32 bit, including signed zero and subnormal payloads. */
static const char metal_kernel_source[] =
    "#include <metal_stdlib>\nusing namespace metal;\n"
    "kernel void yvex_embed_f32(device const uint *table [[buffer(0)]],\n"
    "device const uint *ids [[buffer(1)]], device uint *output [[buffer(2)]],\n"
    "constant ulong &width [[buffer(3)]], constant ulong &count [[buffer(4)]],\n"
    "uint index [[thread_position_in_grid]]) {\n"
    "if (ulong(index) < count) { ulong row = ulong(index) / width;\n"
    "ulong col = ulong(index) % width;\n"
    "output[index] = table[ulong(ids[row]) * width + col]; } }\n";

static yvex_metal_context *metal_context(const yvex_backend *backend)
{
    return (__bridge yvex_metal_context *)backend->impl;
}

static id<MTLBuffer> metal_buffer(const yvex_device_tensor *tensor)
{
    yvex_metal_storage *storage = (__bridge yvex_metal_storage *)tensor->backend_allocation;
    return storage.buffer;
}

static int metal_owned_storage(const yvex_backend *backend, const yvex_device_tensor *tensor,
                               yvex_error *err)
{
    if (!backend_tensor_owner_is(backend, tensor) || !tensor->backend_allocation) {
        yvex_error_set(err, YVEX_ERR_STATE, "backend.metal.storage", "native allocation owner is required");
        return YVEX_ERR_STATE;
    }
    yvex_metal_storage *storage = (__bridge yvex_metal_storage *)tensor->backend_allocation;
    if (storage.owner != tensor) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "backend.metal.storage",
                       "borrowed physical tensor views are not admitted by this foundation");
        return YVEX_ERR_UNSUPPORTED;
    }
    return YVEX_OK;
}

/* Explicit diagnostic fault hooks exercise ownership/refusal, never manufacture GPU proof. */
static int metal_fault(const char *stage)
{
    const char *selected = getenv("YVEX_TEST_METAL_FAIL_STAGE");
    return selected && strcmp(selected, stage) == 0;
}

static int metal_failure(yvex_backend *backend, yvex_backend_capability_reason reason,
                         const char *message, yvex_error *err)
{
    backend->status = YVEX_BACKEND_STATUS_FAILED;
    metal_context(backend).failure = reason;
    yvex_error_set(err, YVEX_ERR_STATE, "backend.metal.submit", message);
    return YVEX_ERR_STATE;
}

static int metal_complete(yvex_backend *backend, id<MTLCommandBuffer> command, yvex_error *err)
{
    [command commit];
    [command waitUntilCompleted];
    if (command.status != MTLCommandBufferStatusCompleted || metal_fault("completion")) {
        const char *message = command.error.localizedDescription.UTF8String;
        return metal_failure(backend, YVEX_BACKEND_CAPABILITY_REASON_SYNCHRONIZATION_FAILED,
                              message ? message : "Metal command completion refused", err);
    }
    yvex_metal_context *context = metal_context(backend);
    yvex_backend_resource_facts facts = context.facts;
    facts.command_completions++;
    context.facts = facts;
    yvex_error_clear(err);
    return YVEX_OK;
}

static id<MTLCommandBuffer> metal_command(yvex_backend *backend, yvex_error *err)
{
    id<MTLCommandBuffer> command = metal_fault("command") ? nil : [metal_context(backend).queue commandBuffer];
    if (!command) {
        metal_failure(backend, YVEX_BACKEND_CAPABILITY_REASON_LAUNCH_FAILED,
                       "Metal command buffer unavailable", err);
    }
    return command;
}

static int metal_close(yvex_backend *backend, yvex_error *err)
{
    if (backend->stats.allocation_count) {
        yvex_error_set(err, YVEX_ERR_STATE, "backend.metal.close",
                       "Metal context retains live tensors; release them before checked close retry");
        return YVEX_ERR_STATE;
    }
    @autoreleasepool {
        yvex_metal_context *context = (__bridge_transfer yvex_metal_context *)backend->impl;
        backend->impl = NULL;
        context.embedding = nil;
        context.queue = nil;
        context.device = nil;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int metal_memory_stats(const yvex_backend *backend, yvex_backend_memory_stats *out,
                              yvex_error *err)
{
    *out = backend->stats;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int metal_device_info(const yvex_backend *backend, yvex_backend_device_info *out,
                             yvex_error *err)
{
    *out = backend->device_info;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int metal_resource_facts(const yvex_backend *backend, yvex_backend_resource_facts *out,
                                yvex_error *err)
{
    yvex_metal_context *context = metal_context(backend);
    *out = context.facts;
    out->allocated_bytes = backend->stats.allocated_bytes;
    out->device_allocated_bytes = context.device.currentAllocatedSize;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int metal_query_capability(const yvex_backend *backend, yvex_backend_operation_variant variant,
                                  yvex_backend_capability_result *out, yvex_error *err)
{
    yvex_metal_context *context = metal_context(backend);
    out->context_available = context.device != nil && context.queue != nil;
    out->kernel_bundle_available = context.embedding != nil;
    out->function_available = variant == YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32 && context.embedding != nil;
    if (backend->status == YVEX_BACKEND_STATUS_FAILED) {
        out->state = YVEX_BACKEND_CAPABILITY_FAILED;
        out->reason = context.failure;
    } else if (variant <= YVEX_BACKEND_VARIANT_TENSOR_COPY || out->function_available) {
        out->state = YVEX_BACKEND_CAPABILITY_SUPPORTED;
        out->reason = YVEX_BACKEND_CAPABILITY_REASON_NONE;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int metal_new_buffer(yvex_backend *backend, unsigned long long bytes,
                            id<MTLBuffer> *out, yvex_error *err)
{
    yvex_metal_context *context = metal_context(backend);
    *out = nil;
    if (bytes > context.device.maxBufferLength || bytes > SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "backend.metal.allocate", "buffer exceeds Metal addressable extent");
        return YVEX_ERR_BOUNDS;
    }
    int rc = yvex_backend_memory_can_add(backend, bytes, "Metal", "backend.metal.allocate", err);
    if (rc != YVEX_OK) return rc;
    id<MTLBuffer> buffer = metal_fault("allocation") ? nil :
        [context.device newBufferWithLength:(NSUInteger)bytes options:MTLResourceStorageModeShared];
    if (!buffer || !buffer.contents || !buffer.gpuAddress) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "backend.metal.allocate", "shared Metal buffer allocation failed");
        return YVEX_ERR_NOMEM;
    }
    *out = buffer;
    return YVEX_OK;
}

static int metal_tensor_alloc(yvex_backend *backend, const yvex_backend_tensor_desc *desc,
                              yvex_device_tensor **out, yvex_error *err)
{
    @autoreleasepool {
        id<MTLBuffer> buffer = nil;
        int rc = metal_new_buffer(backend, desc->bytes, &buffer, err);
        if (rc != YVEX_OK) return rc;
        yvex_device_tensor *tensor = calloc(1, sizeof(*tensor));
        if (tensor) tensor->name = yvex_core_strdup(desc->name);
        if (!tensor || !tensor->name) {
            free(tensor);
            yvex_error_set(err, YVEX_ERR_NOMEM, "backend.metal.allocate", "tensor descriptor allocation failed");
            return YVEX_ERR_NOMEM;
        }
        tensor->owner = backend;
        tensor->owner_id = backend->tensor_id_next++;
        tensor->dtype = desc->dtype;
        tensor->rank = desc->rank;
        memcpy(tensor->dims, desc->dims, sizeof(tensor->dims));
        tensor->bytes = desc->bytes;
        yvex_metal_storage *storage = [yvex_metal_storage new];
        if (!storage) {
            free(tensor->name);
            free(tensor);
            yvex_error_set(err, YVEX_ERR_NOMEM, "backend.metal.allocate", "buffer owner allocation failed");
            return YVEX_ERR_NOMEM;
        }
        storage.buffer = buffer;
        storage.owner = tensor;
        tensor->backend_allocation = (__bridge_retained void *)storage;
        tensor->data = (unsigned char *)(uintptr_t)buffer.gpuAddress;
        tensor->host_data = buffer.contents;
        tensor->host_accessible = 1;
        backend_memory_acquire(backend, desc->bytes);
        yvex_metal_context *context = metal_context(backend);
        yvex_backend_resource_facts facts = context.facts;
        facts.addressable_bytes += desc->bytes;
        facts.mapped_bytes += buffer.length;
        context.facts = facts;
        *out = tensor;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int metal_tensor_free(yvex_backend *backend, yvex_device_tensor *tensor, yvex_error *err)
{
    int rc = metal_owned_storage(backend, tensor, err);
    if (rc != YVEX_OK) return rc;
    @autoreleasepool {
        yvex_metal_storage *storage = (__bridge_transfer yvex_metal_storage *)tensor->backend_allocation;
        yvex_metal_context *context = metal_context(backend);
        yvex_backend_resource_facts facts = context.facts;
        facts.addressable_bytes -= tensor->bytes;
        facts.mapped_bytes -= storage.buffer.length;
        context.facts = facts;
        backend_memory_release(backend, tensor->bytes);
        tensor->backend_allocation = NULL;
        storage.owner = NULL;
        storage.buffer = nil;
        free(tensor->name);
        free(tensor);
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int metal_tensor_write(yvex_backend *backend, yvex_device_tensor *tensor, const void *src,
                              unsigned long long len, yvex_error *err)
{
    int rc = yvex_backend_tensor_rw_validate("backend.metal.write", backend, tensor, len, err);
    if (rc != YVEX_OK) return rc;
    rc = metal_owned_storage(backend, tensor, err);
    if (rc != YVEX_OK) return rc;
    memcpy(tensor->host_data, src, (size_t)len);
    tensor->is_written = 1;
    yvex_metal_context *context = metal_context(backend);
    yvex_backend_resource_facts facts = context.facts;
    facts.host_write_copy_bytes += len;
    context.facts = facts;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int metal_tensor_read(yvex_backend *backend, const yvex_device_tensor *tensor, void *dst,
                             unsigned long long len, yvex_error *err)
{
    int rc = yvex_backend_tensor_rw_validate("backend.metal.read", backend, tensor, len, err);
    if (rc != YVEX_OK) return rc;
    rc = metal_owned_storage(backend, tensor, err);
    if (rc != YVEX_OK) return rc;
    if (!tensor->is_written) {
        yvex_error_set(err, YVEX_ERR_STATE, "backend.metal.read", "uninitialized tensor cannot be published");
        return YVEX_ERR_STATE;
    }
    memcpy(dst, tensor->host_data, (size_t)len);
    yvex_metal_context *context = metal_context(backend);
    yvex_backend_resource_facts facts = context.facts;
    facts.host_read_copy_bytes += len;
    context.facts = facts;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int metal_blit(yvex_backend *backend, yvex_device_tensor *dst,
                      const yvex_device_tensor *src, yvex_error *err)
{
    int rc = metal_owned_storage(backend, dst, err);
    if (rc == YVEX_OK && src) rc = metal_owned_storage(backend, src, err);
    if (rc != YVEX_OK) return rc;
    @autoreleasepool {
        id<MTLCommandBuffer> command = metal_command(backend, err);
        if (!command) return YVEX_ERR_STATE;
        id<MTLBlitCommandEncoder> encoder = metal_fault("encoder") ? nil : [command blitCommandEncoder];
        if (!encoder) return metal_failure(backend, YVEX_BACKEND_CAPABILITY_REASON_LAUNCH_FAILED,
                                           "Metal blit encoder unavailable", err);
        if (src) [encoder copyFromBuffer:metal_buffer(src) sourceOffset:0
                                toBuffer:metal_buffer(dst) destinationOffset:0 size:(NSUInteger)dst->bytes];
        else [encoder fillBuffer:metal_buffer(dst) range:NSMakeRange(0, (NSUInteger)dst->bytes) value:0];
        [encoder endEncoding];
        dst->is_written = 0;
        rc = metal_complete(backend, command, err);
        if (rc != YVEX_OK) return rc;
        dst->is_written = src ? src->is_written : 1;
        if (src) {
            yvex_metal_context *context = metal_context(backend);
            yvex_backend_resource_facts facts = context.facts;
            facts.device_copy_bytes += dst->bytes;
            context.facts = facts;
        }
    }
    return YVEX_OK;
}

static int metal_tensor_zero(yvex_backend *backend, yvex_device_tensor *tensor, yvex_error *err)
{
    return metal_blit(backend, tensor, NULL, err);
}

static int metal_tensor_copy(yvex_backend *backend, yvex_device_tensor *dst,
                             const yvex_device_tensor *src, yvex_error *err)
{
    int rc = yvex_backend_tensor_copy_validate(backend, dst, src, "backend.metal.copy", err);
    if (rc != YVEX_OK) return rc;
    if (dst == src) { yvex_error_clear(err); return YVEX_OK; }
    return metal_blit(backend, dst, src, err);
}

static int metal_sync(yvex_backend *backend, yvex_error *err)
{
    (void)backend;
    /* All admitted operations are synchronous; there is no outstanding queue ownership. */
    yvex_error_clear(err);
    return YVEX_OK;
}

static int metal_embed(yvex_backend *backend, const yvex_device_tensor *table,
                       const unsigned int *token_ids, unsigned long long token_count,
                       yvex_device_tensor *out, yvex_error *err)
{
    unsigned long long width, vocab;
    int rc = yvex_backend_validate_embed(backend, table, token_ids, token_count, out, &width, &vocab,
                                         "Metal embedding requires F32 table/output", "backend.metal.embed", err);
    if (rc != YVEX_OK) return rc;
    rc = metal_owned_storage(backend, table, err);
    if (rc == YVEX_OK) rc = metal_owned_storage(backend, out, err);
    if (rc != YVEX_OK) return rc;
    if (table->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "backend.metal.embed", "Metal F16 embedding is not qualified");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (table == out || !table->is_written || token_count * width > UINT_MAX ||
        token_count > SIZE_MAX / sizeof(*token_ids)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "backend.metal.embed",
                       "embedding requires initialized distinct table/output and bounded dispatch");
        return YVEX_ERR_BOUNDS;
    }
    @autoreleasepool {
        unsigned long long ids_bytes = token_count * sizeof(*token_ids), count = token_count * width;
        id<MTLBuffer> ids = nil;
        rc = metal_new_buffer(backend, ids_bytes, &ids, err);
        if (rc != YVEX_OK) return rc;
        backend_memory_acquire(backend, ids_bytes);
        yvex_metal_context *context = metal_context(backend);
        yvex_backend_resource_facts facts = context.facts;
        facts.temporary_bytes = ids_bytes;
        facts.addressable_bytes += ids_bytes;
        facts.mapped_bytes += ids.length;
        if (facts.peak_temporary_bytes < ids_bytes) facts.peak_temporary_bytes = ids_bytes;
        facts.host_write_copy_bytes += ids_bytes;
        context.facts = facts;
        memcpy(ids.contents, token_ids, (size_t)ids_bytes);
        id<MTLCommandBuffer> command = metal_command(backend, err);
        id<MTLComputeCommandEncoder> encoder = !command || metal_fault("encoder") ? nil :
            [command computeCommandEncoder];
        rc = YVEX_ERR_STATE;
        if (encoder) {
            [encoder setComputePipelineState:context.embedding];
            [encoder setBuffer:metal_buffer(table) offset:0 atIndex:0];
            [encoder setBuffer:ids offset:0 atIndex:1];
            [encoder setBuffer:metal_buffer(out) offset:0 atIndex:2];
            [encoder setBytes:&width length:sizeof(width) atIndex:3];
            [encoder setBytes:&count length:sizeof(count) atIndex:4];
            facts = context.facts;
            facts.host_write_copy_bytes += sizeof(width) + sizeof(count);
            context.facts = facts;
            NSUInteger threads = MIN((NSUInteger)256, context.embedding.maxTotalThreadsPerThreadgroup);
            [encoder dispatchThreads:MTLSizeMake((NSUInteger)count, 1, 1)
               threadsPerThreadgroup:MTLSizeMake(threads, 1, 1)];
            [encoder endEncoding];
            out->is_written = 0;
            facts = context.facts;
            facts.kernel_dispatches++;
            context.facts = facts;
            rc = metal_complete(backend, command, err);
            if (rc == YVEX_OK) out->is_written = 1;
        } else if (command) {
            rc = metal_failure(backend, YVEX_BACKEND_CAPABILITY_REASON_LAUNCH_FAILED,
                                "Metal compute encoder unavailable", err);
        }
        facts = context.facts;
        facts.temporary_bytes = 0;
        facts.addressable_bytes -= ids_bytes;
        facts.mapped_bytes -= ids.length;
        context.facts = facts;
        backend_memory_release(backend, ids_bytes);
    }
    return rc;
}

static const yvex_backend_vtable metal_vtable = {
    .close = metal_close,
    .memory_stats = metal_memory_stats,
    .device_info = metal_device_info,
    .resource_facts = metal_resource_facts,
    .tensor_alloc = metal_tensor_alloc,
    .tensor_free = metal_tensor_free,
    .tensor_write = metal_tensor_write,
    .tensor_read = metal_tensor_read,
    .tensor_zero = metal_tensor_zero,
    .tensor_copy = metal_tensor_copy,
    .sync = metal_sync,
    .query_capability = metal_query_capability,
    .op_embed = metal_embed
};

int yvex_backend_open_metal_impl(yvex_backend **out, const char *device,
                                unsigned long long memory_limit_bytes, yvex_error *err)
{
    *out = NULL;
    @autoreleasepool {
        NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
        id<MTLDevice> selected = nil;
        NSUInteger index = 0;
        if (device && device[0] && strcmp(device, "default") != 0) {
            char *end = NULL;
            unsigned long parsed = strtoul(device, &end, 10);
            if (!end || end == device || *end || parsed >= devices.count || parsed > INT_MAX) {
                yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "backend.metal.open", "Metal device index unavailable");
                return YVEX_ERR_UNSUPPORTED;
            }
            index = (NSUInteger)parsed;
            selected = devices[index];
        } else {
            selected = metal_fault("device") ? nil : MTLCreateSystemDefaultDevice();
            index = selected ? [devices indexOfObject:selected] : NSNotFound;
        }
        if (!selected || !selected.hasUnifiedMemory || index == NSNotFound) {
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "backend.metal.open",
                           "foundation requires an available unified-memory Metal device");
            return YVEX_ERR_UNSUPPORTED;
        }
        yvex_metal_context *context = [yvex_metal_context new];
        context.device = selected;
        context.queue = metal_fault("queue") ? nil : [selected newCommandQueue];
        if (!context.queue) {
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "backend.metal.open", "Metal command queue unavailable");
            return YVEX_ERR_UNSUPPORTED;
        }
        NSError *error = nil;
        MTLCompileOptions *compile = [MTLCompileOptions new];
        compile.mathMode = MTLMathModeSafe;
        id<MTLLibrary> library = [selected newLibraryWithSource:@(metal_kernel_source) options:compile error:&error];
        id<MTLFunction> function = [library newFunctionWithName:@"yvex_embed_f32"];
        context.embedding = metal_fault("pipeline") || !function ? nil :
            [selected newComputePipelineStateWithFunction:function error:&error];
        if (!context.embedding) {
            yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "backend.metal.open", "Metal pipeline rejected: %s",
                            error ? error.localizedDescription.UTF8String : "required function/pipeline absent");
            return YVEX_ERR_UNSUPPORTED;
        }
        yvex_backend *backend = calloc(1, sizeof(*backend));
        if (!backend) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "backend.metal.open", "backend owner allocation failed");
            return YVEX_ERR_NOMEM;
        }
        backend->kind = YVEX_BACKEND_KIND_METAL;
        atomic_init(&backend->status, YVEX_BACKEND_STATUS_READY);
        atomic_init(&backend->lifecycle, 0ull);
        backend->vtable = &metal_vtable;
        backend->resource_owner = backend;
        backend->tensor_id_next = 1;
        backend->stats.memory_limit_bytes = memory_limit_bytes;
        backend->device_info.kind = YVEX_BACKEND_KIND_METAL;
        snprintf(backend->device_name_storage, sizeof(backend->device_name_storage), "%s", selected.name.UTF8String);
        backend->device_info.name = backend->device_name_storage;
        backend->device_info.device_index = (int)index;
        /* Shared physical RAM does not imply identical CPU/GPU virtual addresses. */
        backend->device_info.unified_addressing = 0;
        /* Dedicated/global/free GPU memory and CUDA compute capability are unknown/inapplicable. */
        yvex_backend_resource_facts facts = {0};
        facts.schema = YVEX_BACKEND_RESOURCE_SCHEMA;
        facts.known = YVEX_BACKEND_RESOURCE_ADDRESSABLE | YVEX_BACKEND_RESOURCE_MAPPED |
            YVEX_BACKEND_RESOURCE_ALLOCATED | YVEX_BACKEND_RESOURCE_RECOMMENDED_WORKING_SET |
            YVEX_BACKEND_RESOURCE_DEVICE_ALLOCATED | YVEX_BACKEND_RESOURCE_MAX_BUFFER |
            YVEX_BACKEND_RESOURCE_TEMPORARY;
        facts.shared_system_memory = 1;
        facts.recommended_working_set_bytes = selected.recommendedMaxWorkingSetSize;
        facts.max_buffer_bytes = selected.maxBufferLength;
        context.facts = facts;
        backend->impl = (__bridge_retained void *)context;
        *out = backend;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}
