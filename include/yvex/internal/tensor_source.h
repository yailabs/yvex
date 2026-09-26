/* Admitted immutable single-shard tensor source for compiled physical programs.
 * The source owns bytes and tensor inventory; the compiler owns parameter roles. */
#ifndef INCLUDE_YVEX_INTERNAL_TENSOR_SOURCE_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_TENSOR_SOURCE_H_INCLUDED

#include <yvex/internal/core.h>
#include <yvex/source.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_TENSOR_SOURCE_SCHEMA_V1 1u
typedef struct yvex_tensor_source yvex_tensor_source;
typedef struct {
    unsigned int schema_version;
    const char *file_path;
    const char *expected_sha256;
    unsigned long long expected_tensor_count;
} yvex_tensor_source_request;
typedef struct {
    unsigned int schema_version;
    char source_identity[YVEX_SHA256_HEX_BYTES];
    unsigned long long source_bytes, tensor_count;
} yvex_tensor_source_summary;

int yvex_tensor_source_open(yvex_tensor_source **, const yvex_tensor_source_request *, yvex_error *);
const yvex_native_weight_info *yvex_tensor_source_find(const yvex_tensor_source *, const char *name);
int yvex_tensor_source_read_f32(const yvex_tensor_source *, const yvex_native_weight_info *,
    unsigned long long offset, void *output, size_t bytes, yvex_error *);
const yvex_tensor_source_summary *yvex_tensor_source_summary_get(const yvex_tensor_source *);
void yvex_tensor_source_close(yvex_tensor_source **);

#ifdef __cplusplus
}
#endif
#endif
