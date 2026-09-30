/* POSIX terminal mechanics below the platform-neutral client contract.
 * REPLAI owns editing. This adapter owns observation, temporary quiet output,
 * process interrupt capture and its bounded worker lifetime, never requests. */
#define _POSIX_C_SOURCE 200809L
#include "src/cli/io/terminal/private.h"
#include <replai.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "interrupt capture requires lock-free unsigned atomics");

struct yvex_cli_output_scope { replai_handle *handle; };
struct yvex_cli_interrupt {
    struct sigaction previous, previous_resize;
    int wake[2], watching;
    unsigned int resize_seen;
    pthread_t worker;
    atomic_int stopping;
    int (*handle)(void *context);
    void *context;
};

/* A CLI process has one terminal interaction owner. The handler never touches
 * the owner allocation; its lock-free count and pipe remain valid until the
 * worker joins and the prior handler is restored. */
static pthread_mutex_t capture_mutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_uint captured;
static atomic_uint resized;
static volatile sig_atomic_t wake_descriptor = -1;
static int capture_owned;

static int terminal_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "client.terminal", reason);
    return status;
}

int yvex_cli_terminal_interactive(FILE *stream)
{
    int descriptor = stream ? fileno(stream) : -1;
    return descriptor >= 0 && isatty(descriptor);
}

unsigned int yvex_cli_terminal_width(FILE *stream)
{
    struct winsize size = {0};
    int descriptor = stream ? fileno(stream) : -1;
    return descriptor >= 0 && ioctl(descriptor, TIOCGWINSZ, &size) == 0
               ? size.ws_col : 0u;
}

int yvex_cli_terminal_editor_open(struct replai_handle *editor)
{
    return replai_open(editor, STDIN_FILENO, STDOUT_FILENO);
}

int yvex_cli_output_scope_open(yvex_cli_output_scope **out, yvex_error *err)
{
    yvex_cli_output_scope *scope;
    replai_status status;
    replai_config config = {.struct_size = sizeof(config),
        .abi_version = REPLAI_C_ABI_VERSION, .max_input_bytes = 1u};
    if (!out) return terminal_refuse(err, YVEX_ERR_INVALID_ARG, "output scope is required");
    *out = NULL;
    if (!yvex_cli_terminal_interactive(stdin) || !yvex_cli_terminal_interactive(stdout)) return YVEX_OK;
    scope = calloc(1u, sizeof(*scope));
    if (!scope) return terminal_refuse(err, YVEX_ERR_NOMEM, "output scope allocation failed");
    status = replai_create(&config, &scope->handle);
    if (status == REPLAI_OK) status = replai_output_open(scope->handle, STDIN_FILENO, STDOUT_FILENO);
    if (status != REPLAI_OK) {
        if (scope->handle) (void)replai_destroy(&scope->handle);
        free(scope);
        return terminal_refuse(err, YVEX_ERR_IO, "quiet output admission failed");
    }
    *out = scope;
    return YVEX_OK;
}

int yvex_cli_output_scope_feedback(yvex_cli_output_scope *scope, const char *text)
{
    replai_span span = {.struct_size = sizeof(span),
        .extension_version = REPLAI_PRESENTATION_VERSION, .text = (const uint8_t *)text,
        .text_bytes = text ? strlen(text) : 0u, .role = REPLAI_ROLE_DIM};
    replai_text value = {.struct_size = sizeof(value),
        .extension_version = REPLAI_PRESENTATION_VERSION, .spans = &span, .span_count = 1u};
    if (!scope) return YVEX_OK;
    return replai_output_feedback(scope->handle, &value) == REPLAI_OK ? YVEX_OK : YVEX_ERR_IO;
}

int yvex_cli_output_scope_close(yvex_cli_output_scope **owned, yvex_error *err)
{
    yvex_cli_output_scope *scope;
    replai_status restored, retired;
    if (!owned || !*owned) return YVEX_OK;
    scope = *owned;
    /* Product does not admit a draft while a request owns output. */
    restored = replai_output_close(scope->handle, 1u);
    retired = replai_destroy(&scope->handle);
    free(scope);
    *owned = NULL;
    return restored == REPLAI_OK && retired == REPLAI_OK ? YVEX_OK
        : terminal_refuse(err, YVEX_ERR_IO, "terminal output restoration failed");
}

static void interrupt_record(void)
{
    unsigned int value = atomic_load_explicit(&captured, memory_order_relaxed);
    while (value != UINT_MAX && !atomic_compare_exchange_weak_explicit(
        &captured, &value, value + 1u, memory_order_relaxed, memory_order_relaxed)) {}
}

static void wake_send(int descriptor)
{
    const unsigned char byte = 1u;
    ssize_t written;
    do { written = write(descriptor, &byte, 1u); } while (written < 0 && errno == EINTR);
    /* EAGAIN means a wake is already queued. Counts, not pipe bytes, own events. */
}

static void interrupt_handler(int number)
{
    int saved_errno = errno;
    if (number == SIGWINCH) atomic_fetch_add_explicit(&resized, 1u, memory_order_relaxed);
    else interrupt_record();
    if (wake_descriptor >= 0) wake_send(wake_descriptor);
    errno = saved_errno;
}

static int wake_pipe_open(int descriptors[2])
{
    size_t index;
    if (pipe(descriptors) != 0) return 0;
    for (index = 0u; index < 2u; ++index)
        if (fcntl(descriptors[index], F_SETFD, FD_CLOEXEC) != 0 ||
            fcntl(descriptors[index], F_SETFL, O_NONBLOCK) != 0) {
            (void)close(descriptors[0]);
            (void)close(descriptors[1]);
            return 0;
        }
    return 1;
}

int yvex_cli_interrupt_open(yvex_cli_interrupt **out, yvex_error *err)
{
    yvex_cli_interrupt *scope;
    struct sigaction action = {0};
    int rc = YVEX_OK;
    if (!out) return terminal_refuse(err, YVEX_ERR_INVALID_ARG, "interrupt scope is required");
    *out = NULL;
    if (pthread_mutex_lock(&capture_mutex) != 0)
        return terminal_refuse(err, YVEX_ERR_STATE, "interrupt capture lock failed");
    if (capture_owned) {
        (void)pthread_mutex_unlock(&capture_mutex);
        return terminal_refuse(err, YVEX_ERR_STATE, "interrupt capture already has an owner");
    }
    scope = calloc(1u, sizeof(*scope));
    if (!scope || !wake_pipe_open(scope->wake)) {
        free(scope);
        (void)pthread_mutex_unlock(&capture_mutex);
        return terminal_refuse(err, YVEX_ERR_NOMEM, "interrupt capture allocation failed");
    }
    action.sa_handler = interrupt_handler;
    (void)sigemptyset(&action.sa_mask);
    atomic_store_explicit(&captured, 0u, memory_order_relaxed);
    wake_descriptor = scope->wake[1];
    if (sigaction(SIGINT, &action, &scope->previous) != 0) {
        wake_descriptor = -1;
        (void)close(scope->wake[0]);
        (void)close(scope->wake[1]);
        free(scope);
        rc = terminal_refuse(err, YVEX_ERR_IO, "interrupt handler admission failed");
    } else {
        if (sigaction(SIGWINCH, &action, &scope->previous_resize) != 0) {
            (void)sigaction(SIGINT, &scope->previous, NULL);
            wake_descriptor = -1;
            (void)close(scope->wake[0]); (void)close(scope->wake[1]); free(scope);
            (void)pthread_mutex_unlock(&capture_mutex);
            return terminal_refuse(err, YVEX_ERR_IO, "resize handler admission failed");
        }
        scope->resize_seen = atomic_load_explicit(&resized, memory_order_relaxed);
        atomic_init(&scope->stopping, 0);
        capture_owned = 1;
        *out = scope;
    }
    (void)pthread_mutex_unlock(&capture_mutex);
    return rc;
}

int yvex_cli_terminal_editor_advance(struct replai_handle *editor,
    yvex_cli_interrupt *interrupts, unsigned int observed_interrupts,
    struct replai_event *event)
{
    replai_interest interest = {.struct_size = sizeof(interest),
        .extension_version = REPLAI_PRESENTATION_VERSION};
    struct pollfd descriptors[2];
    unsigned int resize;
    int ready;
    replai_status status = replai_wait_interest(editor, &interest);
    if (status != REPLAI_OK) return status;
    if (yvex_cli_interrupt_count(interrupts) != observed_interrupts)
        return replai_interrupt(editor, event);
    resize = atomic_load_explicit(&resized, memory_order_relaxed);
    if (resize != interrupts->resize_seen) {
        interrupts->resize_seen = resize;
        return replai_advance(editor, REPLAI_WAKE_RESIZE, 0u, event);
    }
    if (interest.kind == REPLAI_WAIT_READY)
        return replai_advance(editor, REPLAI_WAKE_INPUT, 0u, event);
    descriptors[0] = (struct pollfd){.fd = interest.input_fd, .events = POLLIN};
    descriptors[1] = (struct pollfd){.fd = interrupts->wake[0], .events = POLLIN};
    ready = poll(descriptors, 2u, interest.timeout_ms);
    if (ready < 0) return errno == EINTR ? REPLAI_OK : REPLAI_IO;
    if (!ready) return replai_advance(editor, REPLAI_WAKE_DEADLINE, interest.deadline_ticket, event);
    if (descriptors[1].revents) {
        unsigned char bytes[64];
        while (read(interrupts->wake[0], bytes, sizeof(bytes)) > 0) {}
        if (yvex_cli_interrupt_count(interrupts) != observed_interrupts)
            return replai_interrupt(editor, event);
        resize = atomic_load_explicit(&resized, memory_order_relaxed);
        if (resize != interrupts->resize_seen) {
            interrupts->resize_seen = resize;
            return replai_advance(editor, REPLAI_WAKE_RESIZE, 0u, event);
        }
    }
    if (descriptors[0].revents) return replai_advance(editor, REPLAI_WAKE_INPUT, 0u, event);
    return REPLAI_OK;
}

unsigned int yvex_cli_interrupt_count(const yvex_cli_interrupt *scope)
{
    return scope ? atomic_load_explicit(&captured, memory_order_relaxed) : 0u;
}

void yvex_cli_interrupt_clear(yvex_cli_interrupt *scope)
{
    if (scope && !scope->watching) atomic_store_explicit(&captured, 0u, memory_order_relaxed);
}

void yvex_cli_interrupt_record(yvex_cli_interrupt *scope)
{
    if (scope) interrupt_record();
}

static void *interrupt_worker(void *opaque)
{
    yvex_cli_interrupt *scope = opaque;
    struct pollfd wake = {.fd = scope->wake[0], .events = POLLIN};
    int handled = 0;
    while (!atomic_load_explicit(&scope->stopping, memory_order_acquire)) {
        unsigned char bytes[64];
        int pending = yvex_cli_interrupt_count(scope) != 0u;
        if (pending && !handled) handled = scope->handle(scope->context);
        if (atomic_load_explicit(&scope->stopping, memory_order_acquire)) break;
        if (poll(&wake, 1u, pending && !handled ? 10 : -1) < 0 && errno != EINTR) break;
        while (read(scope->wake[0], bytes, sizeof(bytes)) > 0) {}
    }
    return NULL;
}

int yvex_cli_interrupt_watch(yvex_cli_interrupt *scope,
    int (*handle)(void *context), void *context, yvex_error *err)
{
    if (!scope || scope->watching || !handle)
        return terminal_refuse(err, YVEX_ERR_STATE, "one idle interrupt scope and callback are required");
    scope->handle = handle;
    scope->context = context;
    atomic_store_explicit(&scope->stopping, 0, memory_order_release);
    if (pthread_create(&scope->worker, NULL, interrupt_worker, scope) != 0)
        return terminal_refuse(err, YVEX_ERR_IO, "interrupt watch admission failed");
    scope->watching = 1;
    return YVEX_OK;
}

unsigned int yvex_cli_interrupt_unwatch(yvex_cli_interrupt *scope)
{
    if (!scope) return 0u;
    if (scope->watching) {
        atomic_store_explicit(&scope->stopping, 1, memory_order_release);
        wake_send(scope->wake[1]);
        (void)pthread_join(scope->worker, NULL);
        scope->watching = 0;
        scope->handle = NULL;
        scope->context = NULL;
    }
    return yvex_cli_interrupt_count(scope);
}

int yvex_cli_interrupt_close(yvex_cli_interrupt **owned, yvex_error *err)
{
    yvex_cli_interrupt *scope;
    if (!owned || !*owned) return YVEX_OK;
    scope = *owned;
    (void)yvex_cli_interrupt_unwatch(scope);
    if (sigaction(SIGINT, &scope->previous, NULL) != 0)
        return terminal_refuse(err, YVEX_ERR_IO, "interrupt handler restoration failed");
    if (sigaction(SIGWINCH, &scope->previous_resize, NULL) != 0)
        return terminal_refuse(err, YVEX_ERR_IO, "resize handler restoration failed");
    wake_descriptor = -1;
    (void)close(scope->wake[0]);
    (void)close(scope->wake[1]);
    free(scope);
    *owned = NULL;
    (void)pthread_mutex_lock(&capture_mutex);
    capture_owned = 0;
    (void)pthread_mutex_unlock(&capture_mutex);
    return YVEX_OK;
}
