/* Exact source-backed physical parameter admission, independent of model family. */
#define _POSIX_C_SOURCE 200809L
#include <yvex/internal/tensor_source.h>
#include <yvex/internal/source.h>
#include <yvex/internal/core.h>
#include <yvex/source.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct yvex_tensor_source {
    int fd;
    const unsigned char *mapped;
    size_t mapped_bytes;
    unsigned long long payload_offset;
    yvex_native_weight_table *table;
    yvex_tensor_source_summary summary;
};

static int source_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "artifact.tensor-source", reason);
    return status;
}

static float source_f16_to_f32(uint16_t bits)
{
    uint32_t sign = ((uint32_t)bits & 0x8000u) << 16;
    uint32_t exponent = ((uint32_t)bits >> 10) & 31u;
    uint32_t fraction = bits & 1023u;
    uint32_t result;
    if (exponent == 0u) {
        if (fraction == 0u) result = sign;
        else {
            exponent = 113u;
            while ((fraction & 1024u) == 0u) { fraction <<= 1; exponent--; }
            result = sign | (exponent << 23) | ((fraction & 1023u) << 13);
        }
    } else if (exponent == 31u) result = sign | 0x7f800000u | (fraction << 13);
    else result = sign | ((exponent + 112u) << 23) | (fraction << 13);
    float value;
    memcpy(&value, &result, sizeof(value));
    return value;
}

int yvex_tensor_source_read_f32(const yvex_tensor_source *source,
    const yvex_native_weight_info *info, unsigned long long offset,
    void *output, size_t bytes, yvex_error *err)
{
    int owned = 0;
    if (source && info)
        for (unsigned long long i = 0u; i < source->summary.tensor_count; ++i)
            if (info == yvex_native_weight_table_at(source->table, i)) { owned = 1; break; }
    if (!owned || !output || info->dtype != YVEX_NATIVE_DTYPE_F16 ||
        offset % sizeof(float) || bytes % sizeof(float) ||
        info->data_bytes > UINT64_MAX / 2u ||
        offset > info->data_bytes * 2u || bytes > info->data_bytes * 2u - offset ||
        source->payload_offset > source->mapped_bytes ||
        info->data_start > source->mapped_bytes - source->payload_offset)
        return source_refuse(err, YVEX_ERR_BOUNDS, "parameter source extent is unavailable");
    unsigned long long start = source->payload_offset + info->data_start;
    unsigned long long source_offset = offset / 2u, source_bytes = bytes / 2u;
    if (start > source->mapped_bytes || source_offset > source->mapped_bytes - start ||
        source_bytes > source->mapped_bytes - start - source_offset)
        return source_refuse(err, YVEX_ERR_BOUNDS, "parameter source extent exceeds immutable payload");
    const unsigned char *encoded = source->mapped + start + source_offset;
    float *decoded = output;
    for (size_t i = 0u; i < bytes / sizeof(float); ++i) {
        uint16_t half = (uint16_t)encoded[i * 2u] | (uint16_t)((uint16_t)encoded[i * 2u + 1u] << 8);
        decoded[i] = source_f16_to_f32(half);
    }
    return YVEX_OK;
}

int yvex_tensor_source_open(yvex_tensor_source **out,
    const yvex_tensor_source_request *request, yvex_error *err)
{
    yvex_tensor_source *source = NULL;
    struct stat st;
    yvex_safetensors_file_facts facts = {0};
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    int rc = YVEX_ERR_INVALID_ARG;
    if (out) *out = NULL;
    if (!out || !request || request->schema_version != YVEX_TENSOR_SOURCE_SCHEMA_V1 ||
        !request->file_path || !yvex_sha256_hex_valid(request->expected_sha256) ||
        !request->expected_tensor_count)
        return source_refuse(err, rc, "exact file, digest and tensor inventory are required");
    source = calloc(1u, sizeof(*source));
    if (!source) return source_refuse(err, YVEX_ERR_NOMEM, "source owner allocation failed");
    source->fd = -1;
    source->fd = open(request->file_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (source->fd < 0 || fstat(source->fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 8) {
        rc = source_refuse(err, YVEX_ERR_IO, "regular immutable source file cannot be opened");
        goto fail;
    }
    source->mapped_bytes = (size_t)st.st_size;
    if ((off_t)source->mapped_bytes != st.st_size) {
        rc = source_refuse(err, YVEX_ERR_BOUNDS, "source file exceeds addressable extent");
        goto fail;
    }
    source->mapped = mmap(NULL, source->mapped_bytes, PROT_READ, MAP_PRIVATE, source->fd, 0);
    if (source->mapped == MAP_FAILED) {
        source->mapped = NULL;
        rc = source_refuse(err, YVEX_ERR_NOMEM, "source mapping failed");
        goto fail;
    }
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update(&hash, source->mapped, source->mapped_bytes) ||
        !yvex_sha256_final(&hash, digest)) {
        rc = source_refuse(err, YVEX_ERR_STATE, "source digest is unavailable");
        goto fail;
    }
    yvex_sha256_hex(digest, source->summary.source_identity);
    if (strcmp(source->summary.source_identity, request->expected_sha256)) {
        rc = source_refuse(err, YVEX_ERR_FORMAT, "source digest differs from immutable authority");
        goto fail;
    }
    source->table = calloc(1u, sizeof(*source->table));
    if (!source->table) { rc = source_refuse(err, YVEX_ERR_NOMEM, "tensor directory allocation failed"); goto fail; }
    rc = yvex_safetensors_read_header_file_with_facts(request->file_path, "weights",
        source->table, &facts, err);
    if (rc == YVEX_OK) rc = yvex_native_weight_table_finalize(source->table, err);
    if (rc != YVEX_OK) goto fail;
    if (facts.file_bytes != source->mapped_bytes ||
        yvex_native_weight_table_count(source->table) != request->expected_tensor_count) {
        rc = source_refuse(err, YVEX_ERR_FORMAT, "source tensor inventory differs from admission request");
        goto fail;
    }
    source->payload_offset = facts.data_region_offset;
    source->summary.schema_version = YVEX_TENSOR_SOURCE_SCHEMA_V1;
    source->summary.source_bytes = source->mapped_bytes;
    source->summary.tensor_count = request->expected_tensor_count;
    *out = source;
    yvex_error_clear(err);
    return YVEX_OK;
fail:
    yvex_tensor_source_close(&source);
    return rc;
}

const yvex_native_weight_info *yvex_tensor_source_find(const yvex_tensor_source *source,
    const char *name)
{
    return source && name ? yvex_native_weight_table_find(source->table, name) : NULL;
}

const yvex_tensor_source_summary *yvex_tensor_source_summary_get(const yvex_tensor_source *source)
{
    return source ? &source->summary : NULL;
}

void yvex_tensor_source_close(yvex_tensor_source **source)
{
    if (!source || !*source) return;
    yvex_native_weight_table_close((*source)->table);
    if ((*source)->mapped) munmap((void *)(*source)->mapped, (*source)->mapped_bytes);
    if ((*source)->fd >= 0) close((*source)->fd);
    free(*source);
    *source = NULL;
}
