/* Native filesystem, process, credential and virtual-memory mechanisms.
 * Domain admission and lifecycle accounting remain with each consumer. */
#ifndef YVEX_INTERNAL_PLATFORM_H
#define YVEX_INTERNAL_PLATFORM_H

#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Native OS mechanisms only. Admission, accounting and policy remain with callers. */
struct timespec yvex_platform_stat_mtime(const struct stat *status);
struct timespec yvex_platform_stat_ctime(const struct stat *status);

int yvex_platform_file_preallocate(int fd, off_t length);
int yvex_platform_cache_release_available(void);
int yvex_platform_file_cache_release(int fd, off_t offset, off_t length);
int yvex_platform_rename_noreplace(const char *source, const char *destination);
int yvex_platform_peer_owned(int fd);
int yvex_platform_boot_id(char *out, size_t capacity);
int yvex_platform_process_start(pid_t pid, unsigned long long *out);
ssize_t yvex_platform_executable(char *out, size_t capacity);
int yvex_platform_process_write_bytes(pid_t pid, unsigned long long *out);
/* Same-user process snapshot, matching a whole argv element, never prose.
 * 0 success, -1 incomplete/unavailable; output is unchanged on failure.
 * This observation does not authorize signalling a PID. */
int yvex_platform_process_argument_count(const char *argument, unsigned long long *out);
int yvex_platform_system_memory(unsigned long long *total,
                                unsigned long long *available);
int yvex_platform_process_memory(unsigned long long *current,
                                 unsigned long long *peak);

/* Successful replacement consumes source; failure leaves both mappings owned. */
int yvex_platform_mapping_replace(void *source, void *destination, size_t length);

/* Only a read-only, unlinked descriptor is published on Darwin; Linux seals its memfd. */
typedef struct {
    int write_fd, read_fd;
} yvex_platform_backing;
int yvex_platform_backing_open(yvex_platform_backing *backing);
int yvex_platform_backing_finish(yvex_platform_backing *backing);
void yvex_platform_backing_close(yvex_platform_backing *backing);

#endif
