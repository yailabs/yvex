/* Bind compiled physical parameter roles to immutable admitted source tensors. */
#include <yvex/internal/tensor_parameters.h>
#include <yvex/qtype.h>
#include <stdint.h>
#include <stdlib.h>

struct yvex_tensor_parameters {
    const yvex_tensor_source *source;
    const yvex_native_weight_info **bound;
    yvex_program_kernel_parameter *parameters;
    size_t count;
    unsigned long long encoded_bytes;
};

static int parameters_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "runtime.tensor-parameters", reason);
    return status;
}

static int parameters_read(void *opaque, unsigned long long id, unsigned long long offset,
    void *output, size_t bytes, yvex_error *err)
{
    const yvex_tensor_parameters *owner = opaque;
    if (!owner || id >= owner->count || !owner->bound[id])
        return parameters_refuse(err, YVEX_ERR_BOUNDS, "compiled parameter identity is unavailable");
    return yvex_tensor_source_read_f32(owner->source, owner->bound[id], offset, output, bytes, err);
}

int yvex_tensor_parameters_open(yvex_tensor_parameters **out, const yvex_tensor_source *source,
    const yvex_program_physical *program, size_t count,
    int (*parameter_name)(void *, unsigned long long, char[256], yvex_error *),
    void *parameter_context, yvex_error *err)
{
    const yvex_program_physical_summary *summary = yvex_program_physical_summary_get(program);
    if (out) *out = NULL;
    if (!out || !source || !summary || !parameter_name || !count || count > 4096u)
        return parameters_refuse(err, YVEX_ERR_INVALID_ARG, "source and exact compiled roles are required");
    yvex_tensor_parameters *owner = calloc(1u, sizeof(*owner));
    if (!owner) return parameters_refuse(err, YVEX_ERR_NOMEM, "parameter owner allocation failed");
    owner->source = source;
    owner->count = count;
    owner->bound = calloc(count, sizeof(*owner->bound));
    owner->parameters = calloc(count, sizeof(*owner->parameters));
    if (!owner->bound || !owner->parameters) {
        yvex_tensor_parameters_close(&owner);
        return parameters_refuse(err, YVEX_ERR_NOMEM, "parameter directory allocation failed");
    }
    int rc = YVEX_OK;
    for (size_t i = 0u; rc == YVEX_OK && i < count; ++i) {
        char name[256] = {0};
        rc = parameter_name(parameter_context, i, name, err);
        const yvex_native_weight_info *info = rc == YVEX_OK ?
            yvex_tensor_source_find(source, name) : NULL;
        if (rc != YVEX_OK) break;
        if (!name[0] || !info || info->dtype != YVEX_NATIVE_DTYPE_F16 ||
            (info->rank != 1u && info->rank != 2u)) {
            rc = parameters_refuse(err, YVEX_ERR_FORMAT, "compiled role has no exact F16 source tensor");
            break;
        }
        unsigned long long rows = info->rank == 1u ? 1u : info->dims[0];
        unsigned long long width = info->dims[info->rank - 1u];
        if (!rows || !width || width > UINT64_MAX / 4u || rows > UINT64_MAX / width ||
            info->data_bytes != rows * width * 2u || info->data_bytes > UINT64_MAX / 2u ||
            owner->encoded_bytes > UINT64_MAX - info->data_bytes * 2u) {
            rc = parameters_refuse(err, YVEX_ERR_FORMAT, "source parameter geometry is malformed");
            break;
        }
        owner->bound[i] = info;
        owner->encoded_bytes += info->data_bytes * 2u;
        owner->parameters[i] = (yvex_program_kernel_parameter){.tensor_id = i,
            .weight = {.encoded_bytes = info->data_bytes * 2u, .row_count = rows,
                .row_width = width, .row_bytes = width * 4u, .qtype = YVEX_GGUF_QTYPE_F32},
            .read = parameters_read, .read_context = owner};
    }
    for (size_t value = 0u; rc == YVEX_OK && value < summary->value_count; ++value) {
        const yvex_program_physical_value *physical = yvex_program_physical_value_at(program, value);
        if (!physical || !physical->parameter) continue;
        if (physical->tensor_id >= count || physical->qtype != YVEX_GGUF_QTYPE_F32) {
            rc = parameters_refuse(err, YVEX_ERR_FORMAT, "physical parameter class differs from admitted source");
            break;
        }
        const yvex_native_weight_info *info = owner->bound[physical->tensor_id];
        if (!info || physical->type.rank != info->rank) {
            rc = parameters_refuse(err, YVEX_ERR_FORMAT, "physical parameter rank differs from source");
            break;
        }
        for (unsigned int axis = 0u; axis < info->rank; ++axis)
            if (physical->type.shape[axis].symbol != YVEX_IR_NONE ||
                physical->type.shape[axis].extent != info->dims[axis]) {
                rc = parameters_refuse(err, YVEX_ERR_FORMAT, "physical parameter extent differs from source");
                break;
            }
    }
    if (rc == YVEX_OK) {
        *out = owner;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    yvex_tensor_parameters_close(&owner);
    return rc;
}

const yvex_program_kernel_parameter *yvex_tensor_parameters_view(const yvex_tensor_parameters *owner,
    size_t *count)
{
    if (count) *count = owner ? owner->count : 0u;
    return owner ? owner->parameters : NULL;
}

unsigned long long yvex_tensor_parameters_encoded_bytes(const yvex_tensor_parameters *owner)
{
    return owner ? owner->encoded_bytes : 0u;
}

void yvex_tensor_parameters_close(yvex_tensor_parameters **owner)
{
    if (!owner || !*owner) return;
    free((*owner)->parameters);
    free((*owner)->bound);
    free(*owner);
    *owner = NULL;
}
