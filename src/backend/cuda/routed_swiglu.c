/* Biased-sigmoid routing and ordered BF16 expert publication.
 * Geometry and immutable parameter lineage come from the physical program.
 * No model topology, host routing readback or weight preparation is owned here. */
#include "src/backend/cuda/private.h"
#include "src/backend/cuda/transformer_ops.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>

#define ROUTED_TILE 8u
#define ROUTED_STATUS_VALUES 64u
#define ROUTED_WARPS 4u

int yvex_cuda_routed_swiglu_workspace_values(const yvex_routed_swiglu_bf16_request *g,
    unsigned long long *values, yvex_error *err)
{
    unsigned long long scalar, row, tile, tasks;
    if (values) *values = 0u;
    int rc = yvex_routed_swiglu_workspace_values(g, &scalar, err);
    if (rc != YVEX_OK) return rc;
    tile = g->rows < ROUTED_TILE ? g->rows : ROUTED_TILE;
    if (!values || !yvex_core_u64_mul(g->selected, g->intermediate, &row) ||
        !yvex_core_u64_mul(tile, row, &tasks) || tasks > (unsigned long long)INT_MAX * ROUTED_WARPS ||
        !yvex_core_u64_mul(tile, g->hidden, &tasks) || tasks > (unsigned long long)INT_MAX * ROUTED_WARPS ||
        !yvex_core_u64_add(row, 2u * g->experts + g->selected, &row) ||
        !yvex_core_u64_mul(row, tile, &row) ||
        !yvex_core_u64_add(row, ROUTED_STATUS_VALUES, values) || *values > SIZE_MAX / sizeof(float)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.routed-swiglu.workspace",
            "selected expert tile or workspace exceeds addressable geometry");
        return YVEX_ERR_BOUNDS;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int routed_ranges_disjoint(unsigned long long a, unsigned long long an,
    unsigned long long b, unsigned long long bn)
{
    return a && b && an <= ULLONG_MAX - a && bn <= ULLONG_MAX - b &&
        (a + an <= b || b + bn <= a);
}

static int routed_bank_address(yvex_backend *backend, const yvex_component_encoded_weight *w,
    unsigned long long experts, unsigned long long rows, unsigned long long width,
    unsigned long long *address)
{
    unsigned long long count, bytes, stride;
    return w && w->encoded && w->qtype == YVEX_GGUF_QTYPE_BF16 &&
        yvex_core_u64_mul(experts, rows, &count) && count == w->row_count &&
        width == w->row_width && yvex_core_u64_mul(width, 2u, &stride) && stride == w->row_bytes &&
        yvex_core_u64_mul(count, stride, &bytes) && bytes == w->encoded_bytes &&
        bytes <= SIZE_MAX && yvex_backend_resident_resolve(backend, w->encoded, bytes, address) ==
            YVEX_BACKEND_RESIDENT_HIT;
}

static int routed_operands(yvex_backend *backend, const yvex_routed_swiglu_device_request *r,
    unsigned long long values, unsigned long long addresses[3])
{
    const yvex_routed_swiglu_bf16_request *g = &r->geometry;
    unsigned long long counts[] = {g->rows * g->hidden, g->rows * g->experts, g->experts,
        g->rows * g->hidden, values};
    const yvex_device_tensor *t[] = {r->input, r->logits, r->bias, r->output, r->workspace};
    for (size_t i = 0u; i < 5u; ++i)
        if (!backend_tensor_owner_is(backend, t[i]) || t[i]->dtype != YVEX_DTYPE_F32 ||
            (i < 3u && !t[i]->is_written) || !backend_tensor_f32_elements(t[i], counts[i])) return 0;
    for (size_t i = 3u; i < 5u; ++i)
        for (size_t j = 0u; j < i; ++j)
            if (!routed_ranges_disjoint(yvex_cuda_tensor_ptr(t[i]), t[i]->bytes,
                    yvex_cuda_tensor_ptr(t[j]), t[j]->bytes)) return 0;
    if (!routed_bank_address(backend, g->gate, g->experts, g->intermediate, g->hidden, addresses) ||
        !routed_bank_address(backend, g->up, g->experts, g->intermediate, g->hidden, addresses + 1u) ||
        !routed_bank_address(backend, g->down, g->experts, g->hidden, g->intermediate, addresses + 2u)) return 0;
    const yvex_component_encoded_weight *banks[] = {g->gate, g->up, g->down};
    for (size_t i = 0u; i < 3u; ++i)
        for (size_t j = 3u; j < 5u; ++j)
            if (!routed_ranges_disjoint(addresses[i], banks[i]->encoded_bytes,
                    yvex_cuda_tensor_ptr(t[j]), t[j]->bytes)) return 0;
    return 1;
}

static int routed_tile(yvex_backend *backend, const yvex_routed_swiglu_device_request *r,
    unsigned long long addresses[3], unsigned long long offset, unsigned long long rows,
    CUdeviceptr status, yvex_backend_operation_facts *facts, yvex_error *err)
{
    const char *where = "cuda.routed-swiglu";
    const yvex_routed_swiglu_bf16_request *g = &r->geometry;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    unsigned long long h = g->hidden, m = g->intermediate, e = g->experts, k = g->selected;
    CUdeviceptr input = yvex_cuda_tensor_ptr(r->input) + offset * h * sizeof(float);
    CUdeviceptr logits = yvex_cuda_tensor_ptr(r->logits) + offset * e * sizeof(float);
    CUdeviceptr bias = yvex_cuda_tensor_ptr(r->bias);
    CUdeviceptr output = yvex_cuda_tensor_ptr(r->output) + offset * h * sizeof(float);
    CUdeviceptr scratch = yvex_cuda_tensor_ptr(r->workspace) + ROUTED_STATUS_VALUES * sizeof(float);
    float epsilon = g->normalization_epsilon, scale = g->routed_scale;
    void *route_args[] = {&input, &logits, &bias, &scratch, &h, &m, &e, &k, &epsilon, &scale, &status};
    int rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        state->sigmoid_route_function, (unsigned int)rows, 128u, 0u, route_args, where, err);
    if (rc == YVEX_OK) {
        facts->kernel_launches++;
        void *up_args[] = {&input, addresses, addresses + 1u, &scratch, &h, &m, &e, &k, &rows, &status};
        rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            state->sigmoid_up_function, (unsigned int)((rows * k * m + ROUTED_WARPS - 1u) / ROUTED_WARPS),
            ROUTED_WARPS * 32u, 0u, up_args, where, err);
    }
    if (rc == YVEX_OK) {
        facts->kernel_launches++;
        void *down_args[] = {addresses + 2u, &scratch, &output, &h, &m, &e, &k, &rows, &status};
        rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            state->sigmoid_down_function, (unsigned int)((rows * h + ROUTED_WARPS - 1u) / ROUTED_WARPS),
            ROUTED_WARPS * 32u, 0u, down_args, where, err);
        if (rc == YVEX_OK) facts->kernel_launches++;
    }
    /* No scratch reuse, release or result publication until completion. Retain
     * the original launch failure if the required cleanup barrier also fails. */
    int device_wide = 0, code = 0;
    int sync_rc = yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        &device_wide, where, rc == YVEX_OK ? err : NULL);
    if (sync_rc == YVEX_OK) {
        facts->device_synchronizations += device_wide != 0;
        facts->queue_synchronizations += !device_wide;
    }
    if (rc == YVEX_OK) rc = sync_rc;
    if (rc == YVEX_OK) rc = yvex_cuda_status(&state->driver,
        state->driver.cuMemcpyDtoH_v2(&code, status, sizeof(code)), where, err);
    if (rc == YVEX_OK) {
        facts->download_count++; facts->d2h_bytes += sizeof(code);
        if (code) {
            yvex_error_setf(err, YVEX_ERR_FORMAT, where,
                "invalid routed BF16 operand, selection or publication (device status %d)", code);
            rc = YVEX_ERR_FORMAT;
        }
    }
    return rc;
}

int yvex_cuda_routed_swiglu_bf16(yvex_backend *backend, const yvex_routed_swiglu_device_request *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const char *where = "cuda.routed-swiglu";
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    unsigned long long values, addresses[3], offset = 0u;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!state || !r || !facts || yvex_cuda_capture_active(backend)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "owned operation outside graph capture required");
        return YVEX_ERR_INVALID_ARG;
    }
    int rc = yvex_cuda_routed_swiglu_workspace_values(&r->geometry, &values, err);
    if (rc != YVEX_OK) return rc;
    if (!routed_operands(backend, r, values, addresses)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
            "disjoint F32 carriers, admitted workspace and resident BF16 banks required");
        return YVEX_ERR_FORMAT;
    }
    r->output->is_written = r->workspace->is_written = 0;
    CUdeviceptr status = state->program_status_active ? state->program_status : yvex_cuda_tensor_ptr(r->workspace);
    rc = yvex_cuda_set_current(backend, where, err);
    if (rc == YVEX_OK && !state->program_status_active)
        rc = yvex_cuda_status(&state->driver, state->driver.cuMemsetD8_v2(status, 0, sizeof(int)), where, err);
    while (rc == YVEX_OK && offset < r->geometry.rows) {
        if (r->geometry.cancel_requested && r->geometry.cancel_requested(r->geometry.cancel_context)) {
            yvex_error_set(err, YVEX_ERR_CANCELLED, where, "routed execution cancelled before next tile");
            rc = YVEX_ERR_CANCELLED; break;
        }
        unsigned long long rows = r->geometry.rows - offset;
        if (rows > ROUTED_TILE) rows = ROUTED_TILE;
        rc = routed_tile(backend, r, addresses, offset, rows, status, facts, err);
        if (rc == YVEX_OK) offset += rows;
    }
    facts->temporary_bytes = values * sizeof(float);
    /* A selected bank population is not a device traffic or residency measure. */
    facts->compulsory_memory_facts_available = 0;
    if (rc == YVEX_OK) {
        r->output->is_written = r->workspace->is_written = 1;
        yvex_error_clear(err);
    }
    return rc;
}
