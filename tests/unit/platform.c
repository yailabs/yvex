#define _GNU_SOURCE
#include <yvex/internal/platform.h>
#include <yvex/internal/core.h>
#include "src/runtime/private.h"
#include "tests/test.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif

static int test_native_identity(void)
{
    char boot[128], second[128], executable[4096];
    unsigned long long start, repeated, current = 0ull, peak = 0ull, total, available;
    int limited, peers[2];
    YVEX_TEST_ASSERT(yvex_platform_boot_id(boot, sizeof(boot)) &&
                     yvex_platform_boot_id(second, sizeof(second)) && !strcmp(boot, second),
                     "native boot identity is stable");
    YVEX_TEST_ASSERT(yvex_platform_process_start(getpid(), &start) && start &&
                     yvex_platform_process_start(getpid(), &repeated) && start == repeated &&
                     !yvex_platform_process_start(-1, &repeated),
                     "native process start identity is stable and refuses invalid PID");
    YVEX_TEST_ASSERT(yvex_platform_executable(executable, sizeof(executable)) > 0 &&
                     executable[0] == '/' && access(executable, X_OK) == 0,
                     "self executable is a native absolute path");
    YVEX_TEST_ASSERT(yvex_platform_process_memory(&current, &peak) && current &&
                     peak >= current && peak < (1ull << 48),
                     "RSS is measured in bytes with consistent peak");
    YVEX_TEST_ASSERT(yvex_runtime_private_memory_capacity(&total, &available, &limited) &&
                     total && available <= total, "native capacity extent is measured");
    YVEX_TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, peers) == 0,
                     "native local credential fixture");
    YVEX_TEST_ASSERT(yvex_platform_peer_owned(peers[0]) &&
                     yvex_platform_peer_owned(peers[1]) && !yvex_platform_peer_owned(-1),
                     "only authenticated local peers pass");
    close(peers[0]); close(peers[1]);
    return 0;
}

static int test_native_publication(void)
{
    char root[] = "/tmp/yvex-platform-XXXXXX", source[256], destination[256], unsafe[256];
    unsigned char *bytes = NULL;
    size_t count = 0;
    yvex_core_file_result result;
    yvex_error err;
    struct stat status;
    int fd;
    YVEX_TEST_ASSERT(mkdtemp(root) != NULL, "native publication fixture");
    snprintf(source, sizeof(source), "%s/source", root);
    snprintf(destination, sizeof(destination), "%s/destination", root);
    snprintf(unsafe, sizeof(unsafe), "%s/alias", root);
    fd = open(source, O_RDWR | O_CREAT | O_EXCL, 0600);
    YVEX_TEST_ASSERT(fd >= 0 && yvex_platform_file_preallocate(fd, 32768) == 0 &&
                     fstat(fd, &status) == 0 && status.st_size == 32768 &&
                     pwrite(fd, "source", 6u, 0) == 6, "native allocation establishes full file extent");
    close(fd);
    fd = open(destination, O_WRONLY | O_CREAT | O_EXCL, 0600);
    YVEX_TEST_ASSERT(fd >= 0 && write(fd, "keep", 4u) == 4, "conflicting destination fixture");
    close(fd);
    YVEX_TEST_ASSERT(yvex_platform_rename_noreplace(source, destination) != 0 && errno == EEXIST,
                     "atomic publication cannot replace a competing destination");
    YVEX_TEST_ASSERT(yvex_core_file_read_snapshot(destination, 16u, &bytes, &count,
                     &result, &err) == YVEX_OK && count == 4u && !memcmp(bytes, "keep", 4u),
                     "competing bytes are preserved through OS aliases");
    free(bytes);
    YVEX_TEST_ASSERT(symlink(destination, unsafe) == 0 &&
                     (fd = yvex_core_file_open_readonly(unsafe)) < 0,
                     "application symlink is refused");
    unlink(unsafe); unlink(destination);
    YVEX_TEST_ASSERT(yvex_platform_rename_noreplace(source, destination) == 0 &&
                     access(source, F_OK) != 0, "native atomic publication installs a new name");
    unlink(destination); rmdir(root);
    return 0;
}

static int test_native_state_mapping(void)
{
    yvex_platform_backing backing = {-1, -1};
    long page = sysconf(_SC_PAGESIZE);
    size_t length;
    int fd;
    unsigned char *source, *destination, *other, *invalid;
    YVEX_TEST_ASSERT(page > 0, "native system page geometry");
    length = (size_t)page * 2u;
    YVEX_TEST_ASSERT(yvex_platform_backing_open(&backing) == 0 &&
                     ftruncate(backing.write_fd, (off_t)length) == 0 &&
                     pwrite(backing.write_fd, "prefix", 6u, 0) == 6,
                     "private state backing is populated before publication");
    fd = yvex_platform_backing_finish(&backing);
    YVEX_TEST_ASSERT(fd >= 0 && backing.write_fd == -1 && backing.read_fd == -1 &&
                     pwrite(fd, "mutate", 6u, 0) < 0 && ftruncate(fd, 1) < 0,
                     "published state backing refuses write and truncation");
    source = mmap(NULL, length, PROT_NONE, MAP_PRIVATE, fd, 0);
    other = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    destination = mmap(NULL, length, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    YVEX_TEST_ASSERT(source != MAP_FAILED && other != MAP_FAILED && destination != MAP_FAILED &&
                     mprotect(source, (size_t)page, PROT_READ) == 0,
                     "state fixture retains a protected sparse extent");
    destination[0] = 42;
    YVEX_TEST_ASSERT(yvex_platform_mapping_replace(source + 1, destination, length) != 0 &&
                     destination[0] == 42, "unaligned replacement preserves old state");
    invalid = mmap(NULL, length, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    YVEX_TEST_ASSERT(invalid != MAP_FAILED && munmap(invalid, length) == 0 &&
                     yvex_platform_mapping_replace(invalid, destination, length) != 0 &&
                     destination[0] == 42, "kernel refusal preserves old state");
    YVEX_TEST_ASSERT(yvex_platform_mapping_replace(source, destination, length) == 0 &&
                     !memcmp(destination, "prefix", 6u), "state moves to its stable destination");
#ifdef __APPLE__
    mach_vm_address_t address = (mach_vm_address_t)(uintptr_t)(destination + page);
    mach_vm_size_t region_size;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    YVEX_TEST_ASSERT(mach_vm_region(mach_task_self(), &address, &region_size,
                     VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &info_count,
                     &object) == KERN_SUCCESS && info.protection == VM_PROT_NONE,
                     "Darwin remapping preserves the uncommitted page protection");
    if (object != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), object);
#endif
    YVEX_TEST_ASSERT(mprotect(destination, (size_t)page, PROT_READ | PROT_WRITE) == 0,
                     "admitted state page becomes writable");
    destination[0] = 'X';
    YVEX_TEST_ASSERT(other[0] == 'p', "copy on write isolates sibling state mappings");
    munmap(destination, length); munmap(other, length); close(fd);
    return 0;
}

int yvex_test_platform(void)
{
    if (test_native_identity()) return 1;
    if (test_native_publication()) return 1;
    return test_native_state_mapping();
}
