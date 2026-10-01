/* Resource observations distinguish owned logical storage from unmeasured physical facts. */
#ifndef INCLUDE_YVEX_INTERNAL_BACKEND_RESOURCE_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_BACKEND_RESOURCE_H_INCLUDED
#include <yvex/backend.h>
#ifdef __cplusplus
extern "C" {
#endif
#define YVEX_BACKEND_RESOURCE_SCHEMA 1u
typedef enum {
    YVEX_BACKEND_RESOURCE_ADDRESSABLE = 1u << 0,
    YVEX_BACKEND_RESOURCE_MAPPED = 1u << 1,
    YVEX_BACKEND_RESOURCE_ALLOCATED = 1u << 2,
    YVEX_BACKEND_RESOURCE_RESIDENT = 1u << 3,
    YVEX_BACKEND_RESOURCE_WORKING_SET = 1u << 4,
    YVEX_BACKEND_RESOURCE_RECOMMENDED_WORKING_SET = 1u << 5,
    YVEX_BACKEND_RESOURCE_DEVICE_ALLOCATED = 1u << 6,
    YVEX_BACKEND_RESOURCE_MAX_BUFFER = 1u << 7,
    YVEX_BACKEND_RESOURCE_TEMPORARY = 1u << 8
} yvex_backend_resource_fact;
typedef struct {
    unsigned int schema, known;
    int shared_system_memory;
    unsigned long long addressable_bytes, mapped_bytes, allocated_bytes;
    unsigned long long resident_bytes, working_set_bytes, recommended_working_set_bytes;
    unsigned long long device_allocated_bytes, max_buffer_bytes;
    unsigned long long temporary_bytes, peak_temporary_bytes;
    unsigned long long host_write_copy_bytes, host_read_copy_bytes, device_copy_bytes;
    unsigned long long kernel_dispatches, command_completions;
} yvex_backend_resource_facts;
/* Unset known bits mean unavailable/unmeasured, never a measured zero. API/storage copies
 * count separately from physical bus transfers. Device allocation excludes other processes. */
int yvex_backend_get_resource_facts(const yvex_backend *backend,
                                   yvex_backend_resource_facts *out, yvex_error *err);
#ifdef __cplusplus
}
#endif
#endif
