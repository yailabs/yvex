#define _GNU_SOURCE
#include <yvex/internal/platform.h>

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/sysctl.h>
#else
#include <sys/syscall.h>
#endif

static int platform_argument_matches(const char *bytes, size_t length, const char *argument)
{
    size_t cursor = 0u, wanted = strlen(argument);
    while (cursor < length) {
        const char *ending = memchr(bytes + cursor, '\0', length - cursor);
        size_t count;
        if (!ending) return -1;
        count = (size_t)(ending - bytes - cursor);
        if (count == wanted && !memcmp(bytes + cursor, argument, wanted)) return 1;
        cursor += count + 1u;
    }
    return 0;
}

int yvex_platform_process_argument_count(const char *argument, unsigned long long *out)
{
    const size_t capacity = 256u * 1024u;
    unsigned long long matched = 0ull;
    char *bytes;
    int rc = 0;
    if (!argument || !argument[0] || !out) return -1;
    bytes = malloc(capacity);
    if (!bytes) return -1;
#ifdef __APPLE__
    {
        int total = proc_listallpids(NULL, 0), count, index;
        pid_t *pids;
        if (total <= 0 || total > 1000000) { free(bytes); return -1; }
        total += 256;
        pids = calloc((size_t)total, sizeof(*pids));
        if (!pids) { free(bytes); return -1; }
        count = proc_listallpids(pids, total * (int)sizeof(*pids));
        if (count <= 0 || count >= total) rc = -1;
        for (index = 0; rc == 0 && index < count; ++index) {
            struct proc_bsdinfo info;
            int mib[3] = {CTL_KERN, KERN_PROCARGS2, pids[index]}, argc, item, match;
            size_t length = capacity, cursor = sizeof(argc), end;
            if (pids[index] <= 0 || pids[index] == getpid()) continue;
            if (proc_pidinfo(pids[index], PROC_PIDTBSDINFO, 0, &info, sizeof(info)) != sizeof(info)) continue;
            if (info.pbi_uid != geteuid()) continue;
            if (sysctl(mib, 3, bytes, &length, NULL, 0) != 0) {
                if (errno != ESRCH) rc = -1;
                continue;
            }
            if (length <= sizeof(argc)) { rc = -1; break; }
            memcpy(&argc, bytes, sizeof(argc));
            if (argc <= 0 || argc > 65536) { rc = -1; break; }
            while (cursor < length && bytes[cursor]) ++cursor;
            while (cursor < length && !bytes[cursor]) ++cursor;
            end = cursor;
            for (item = 0; item < argc; ++item) {
                const char *ending = memchr(bytes + end, '\0', length - end);
                if (!ending) { rc = -1; break; }
                end = (size_t)(ending - bytes) + 1u;
            }
            if (rc) break;
            match = platform_argument_matches(bytes + cursor, end - cursor, argument);
            if (match < 0) rc = -1;
            else matched += (unsigned int)match;
        }
        free(pids);
    }
#else
    {
        DIR *directory = opendir("/proc");
        struct dirent *entry;
        if (!directory) { free(bytes); return -1; }
        for (;;) {
            char path[128], *ending;
            struct stat status;
            long pid;
            int descriptor, match;
            ssize_t length;
            errno = 0;
            entry = readdir(directory);
            if (!entry) { if (errno) rc = -1; break; }
            if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
            pid = strtol(entry->d_name, &ending, 10);
            if (*ending || pid <= 0 || pid == (long)getpid()) continue;
            if (snprintf(path, sizeof(path), "/proc/%ld", pid) >= (int)sizeof(path)) { rc = -1; break; }
            if (stat(path, &status) != 0) { if (errno == ENOENT) continue; rc = -1; break; }
            if (status.st_uid != geteuid()) continue;
            if (snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid) >= (int)sizeof(path)) { rc = -1; break; }
            descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
            if (descriptor < 0) { if (errno == ENOENT) continue; rc = -1; break; }
            do { length = read(descriptor, bytes, capacity); } while (length < 0 && errno == EINTR);
            close(descriptor);
            if (length < 0 || (size_t)length == capacity) { rc = -1; break; }
            match = platform_argument_matches(bytes, (size_t)length, argument);
            if (match < 0) { rc = -1; break; }
            matched += (unsigned int)match;
        }
        closedir(directory);
    }
#endif
    free(bytes);
    if (rc == 0) *out = matched;
    return rc;
}

struct timespec yvex_platform_stat_mtime(const struct stat *status)
{
#ifdef __APPLE__
    return status->st_mtimespec;
#else
    return status->st_mtim;
#endif
}

struct timespec yvex_platform_stat_ctime(const struct stat *status)
{
#ifdef __APPLE__
    return status->st_ctimespec;
#else
    return status->st_ctim;
#endif
}

int yvex_platform_cache_release_available(void)
{
#ifdef __APPLE__
    return 0;
#else
    return 1;
#endif
}

int yvex_platform_file_preallocate(int fd, off_t length)
{
#ifdef __APPLE__
    fstore_t extent;
    if (length < 0) return EINVAL;
    if (!length) return 0;
    memset(&extent, 0, sizeof(extent));
    extent.fst_flags = F_ALLOCATEALL;
    extent.fst_posmode = F_PEOFPOSMODE;
    extent.fst_length = length;
    if (fcntl(fd, F_PREALLOCATE, &extent) != 0) return errno;
    return ftruncate(fd, length) == 0 ? 0 : errno;
#else
    return posix_fallocate(fd, 0, length);
#endif
}

int yvex_platform_file_cache_release(int fd, off_t offset, off_t length)
{
#ifdef __APPLE__
    /* F_NOCACHE changes future caching; it does not evict the requested extent. */
    (void)fd; (void)offset; (void)length;
    return ENOTSUP;
#else
    return posix_fadvise(fd, offset, length, POSIX_FADV_DONTNEED);
#endif
}

int yvex_platform_rename_noreplace(const char *source, const char *destination)
{
#ifdef __APPLE__
    return renamex_np(source, destination, RENAME_EXCL);
#elif defined(SYS_renameat2)
    return (int)syscall(SYS_renameat2, AT_FDCWD, source, AT_FDCWD, destination, 1u);
#else
    (void)source; (void)destination;
    errno = ENOTSUP;
    return -1;
#endif
}

int yvex_platform_peer_owned(int fd)
{
#ifdef __APPLE__
    uid_t uid;
    gid_t gid;
    return getpeereid(fd, &uid, &gid) == 0 && uid == geteuid();
#elif defined(SO_PEERCRED)
    struct ucred credentials;
    socklen_t count = sizeof(credentials);
    memset(&credentials, 0, sizeof(credentials));
    return getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &count) == 0 &&
           count == sizeof(credentials) && credentials.uid == geteuid();
#else
    (void)fd;
    return 0;
#endif
}

int yvex_platform_boot_id(char *out, size_t capacity)
{
    size_t length;
    if (!out || capacity < 2u) return 0;
    out[0] = '\0';
#ifdef __APPLE__
    length = capacity;
    if (sysctlbyname("kern.bootsessionuuid", out, &length, NULL, 0) != 0 ||
        !length || length > capacity || out[length - 1u] != '\0') return 0;
#else
    FILE *stream = fopen("/proc/sys/kernel/random/boot_id", "rb");
    if (!stream) return 0;
    if (capacity > INT_MAX || !fgets(out, (int)capacity, stream)) {
        (void)fclose(stream);
        return 0;
    }
    (void)fclose(stream);
#endif
    length = strlen(out);
    while (length && (out[length - 1u] == '\n' || out[length - 1u] == '\r'))
        out[--length] = '\0';
    return length > 0u;
}

int yvex_platform_process_start(pid_t pid, unsigned long long *out)
{
    if (pid <= 0 || !out) return 0;
#ifdef __APPLE__
    struct proc_bsdinfo info;
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) != sizeof(info) ||
        info.pbi_start_tvsec > ULLONG_MAX / 1000000ull ||
        info.pbi_start_tvusec >= 1000000ull) return 0;
    *out = info.pbi_start_tvsec * 1000000ull + info.pbi_start_tvusec;
    return *out != 0ull;
#else
    char path[64], buffer[4096], *cursor, *end, *tail;
    FILE *stream;
    unsigned int field = 3u;
    if (snprintf(path, sizeof(path), "/proc/%lld/stat", (long long)pid) >=
        (int)sizeof(path)) return 0;
    stream = fopen(path, "rb");
    if (!stream) return 0;
    if (!fgets(buffer, sizeof(buffer), stream)) { fclose(stream); return 0; }
    fclose(stream);
    cursor = strrchr(buffer, ')');
    if (!cursor || cursor[1] != ' ') return 0;
    cursor += 2;
    while (field <= 22u) {
        while (*cursor == ' ') cursor++;
        if (!*cursor) return 0;
        end = cursor;
        while (*end && *end != ' ') end++;
        if (field == 22u) {
            char saved = *end;
            unsigned long long value;
            *end = '\0'; errno = 0;
            value = strtoull(cursor, &tail, 10);
            if (errno || tail == cursor || *tail) { *end = saved; return 0; }
            *end = saved; *out = value;
            return value != 0ull;
        }
        cursor = end; field++;
    }
    return 0;
#endif
}

ssize_t yvex_platform_executable(char *out, size_t capacity)
{
    if (!out || capacity < 2u) { errno = EINVAL; return -1; }
#ifdef __APPLE__
    int count;
    if (capacity > UINT32_MAX) { errno = EOVERFLOW; return -1; }
    count = proc_pidpath(getpid(), out, (uint32_t)capacity);
    if (count <= 0) return -1;
    return (ssize_t)strlen(out);
#else
    ssize_t count = readlink("/proc/self/exe", out, capacity - 1u);
    if (count < 0 || (size_t)count >= capacity - 1u) return -1;
    out[count] = '\0';
    return count;
#endif
}

int yvex_platform_process_write_bytes(pid_t pid, unsigned long long *out)
{
    if (pid <= 0 || !out) return 0;
#ifdef __APPLE__
    struct rusage_info_v2 usage;
    if (proc_pid_rusage(pid, RUSAGE_INFO_V2, (rusage_info_t *)&usage) != 0) return 0;
    *out = usage.ri_diskio_byteswritten;
    return 1;
#else
    char path[64], line[256];
    FILE *stream;
    if (snprintf(path, sizeof(path), "/proc/%lld/io", (long long)pid) >=
        (int)sizeof(path) || !(stream = fopen(path, "r"))) return 0;
    while (fgets(line, sizeof(line), stream))
        if (sscanf(line, "write_bytes: %llu", out) == 1) { fclose(stream); return 1; }
    fclose(stream);
    return 0;
#endif
}

int yvex_platform_system_memory(unsigned long long *total,
                                unsigned long long *available)
{
#ifdef __APPLE__
    uint64_t bytes;
    size_t size = sizeof(bytes);
    unsigned int level;
    size_t level_size = sizeof(level);
    vm_statistics64_data_t statistics;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_size_t page_size;
    host_t host;
    kern_return_t result;
    if (!total || !available ||
        sysctlbyname("hw.memsize", &bytes, &size, NULL, 0) != 0 ||
        size != sizeof(bytes) || !bytes) return 0;
    /* XNU publishes its free/reclaimable page estimate as a rounded-down percentage.
     * This is advisory capacity, not a claim that all pages are currently unused. */
    if (sysctlbyname("kern.memorystatus_level", &level, &level_size, NULL, 0) == 0 &&
        level_size == sizeof(level) && level <= 100u) {
        *total = bytes;
        *available = (bytes / 100ull) * level;
        return 1;
    }
    host = mach_host_self();
    result = host_page_size(host, &page_size);
    if (result == KERN_SUCCESS)
        result = host_statistics64(host, HOST_VM_INFO64,
                                    (host_info64_t)&statistics, &count);
    (void)mach_port_deallocate(mach_task_self(), host);
    if (result != KERN_SUCCESS || !page_size) return 0;
    /* Estimated free/reclaimable resident pages, like Linux MemAvailable.
     * The capacity owner subtracts its system reserve; compressed pages are excluded. */
    *total = bytes;
    *available = ((unsigned long long)statistics.free_count + statistics.inactive_count) * page_size;
    if (*available > *total) *available = *total;
    return 1;
#else
    (void)total; (void)available;
    return 0; /* Linux capacity owner additionally accounts for cgroups. */
#endif
}

int yvex_platform_process_memory(unsigned long long *current,
                                 unsigned long long *peak)
{
    struct rusage usage;
    if (!current || !peak) return 0;
#ifdef __APPLE__
    struct proc_taskinfo info;
    if (proc_pidinfo(getpid(), PROC_PIDTASKINFO, 0, &info, sizeof(info)) != sizeof(info) ||
        getrusage(RUSAGE_SELF, &usage) != 0 || usage.ru_maxrss < 0) return 0;
    *current = info.pti_resident_size;
    *peak = (unsigned long long)usage.ru_maxrss; /* Darwin reports bytes. */
#else
    FILE *stream = fopen("/proc/self/statm", "r");
    unsigned long long pages, resident;
    long page_size = sysconf(_SC_PAGESIZE);
    int parsed;
    if (!stream) return 0;
    parsed = fscanf(stream, "%llu %llu", &pages, &resident);
    fclose(stream);
    if (parsed != 2 || page_size <= 0 || resident > ULLONG_MAX / (unsigned long long)page_size ||
        getrusage(RUSAGE_SELF, &usage) != 0 || usage.ru_maxrss < 0 ||
        (unsigned long long)usage.ru_maxrss > ULLONG_MAX / 1024ull) return 0;
    *current = resident * (unsigned long long)page_size;
    *peak = (unsigned long long)usage.ru_maxrss * 1024ull;
#endif
    if (*peak < *current) *peak = *current;
    return 1;
}

int yvex_platform_mapping_replace(void *source, void *destination, size_t length)
{
    long page = sysconf(_SC_PAGESIZE);
    uintptr_t from = (uintptr_t)source, to = (uintptr_t)destination;
    if (!source || !destination || !length || source == MAP_FAILED ||
        destination == MAP_FAILED || page <= 0 || from % (uintptr_t)page ||
        to % (uintptr_t)page || length % (size_t)page ||
        length > UINTPTR_MAX - from || length > UINTPTR_MAX - to ||
        (from < to + length && to < from + length)) { errno = EINVAL; return -1; }
#ifdef __APPLE__
    mach_vm_address_t address = (mach_vm_address_t)(uintptr_t)destination;
    vm_prot_t current, maximum;
    kern_return_t result = mach_vm_remap(mach_task_self(), &address, length, 0,
        VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, mach_task_self(),
        (mach_vm_address_t)(uintptr_t)source, FALSE, &current, &maximum, VM_INHERIT_COPY);
    if (result != KERN_SUCCESS) { errno = ENOMEM; return -1; }
    (void)munmap(source, length);
    return 0;
#else
    return mremap(source, length, length, MREMAP_MAYMOVE | MREMAP_FIXED,
                  destination) == MAP_FAILED ? -1 : 0;
#endif
}

void yvex_platform_backing_close(yvex_platform_backing *backing)
{
    if (!backing) return;
    if (backing->write_fd >= 0) (void)close(backing->write_fd);
    if (backing->read_fd >= 0) (void)close(backing->read_fd);
    backing->write_fd = backing->read_fd = -1;
}

int yvex_platform_backing_open(yvex_platform_backing *backing)
{
    if (!backing) { errno = EINVAL; return -1; }
    backing->write_fd = backing->read_fd = -1;
#ifdef __APPLE__
    char path[] = "/tmp/yvex-state-prefix.XXXXXX";
    struct stat written, read;
    backing->write_fd = mkstemp(path);
    if (backing->write_fd < 0) return -1;
    if (fcntl(backing->write_fd, F_SETFD, FD_CLOEXEC) != 0 ||
        fchmod(backing->write_fd, 0400) != 0 ||
        (backing->read_fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW)) < 0 ||
        fstat(backing->write_fd, &written) != 0 || fstat(backing->read_fd, &read) != 0 ||
        written.st_dev != read.st_dev || written.st_ino != read.st_ino) {
        (void)unlink(path); yvex_platform_backing_close(backing); return -1;
    }
    if (unlink(path) != 0) { yvex_platform_backing_close(backing); return -1; }
#else
    backing->write_fd = memfd_create("yvex-state-prefix", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (backing->write_fd < 0) return -1;
#endif
    return 0;
}

int yvex_platform_backing_finish(yvex_platform_backing *backing)
{
    int fd;
    if (!backing || backing->write_fd < 0) { errno = EINVAL; return -1; }
#ifdef __APPLE__
    if (backing->read_fd < 0) { errno = EINVAL; return -1; }
    fd = backing->read_fd;
    backing->read_fd = -1;
    (void)close(backing->write_fd);
#else
    if (fcntl(backing->write_fd, F_ADD_SEALS,
              F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_SEAL) != 0) return -1;
    fd = backing->write_fd;
#endif
    backing->write_fd = -1;
    return fd;
}
