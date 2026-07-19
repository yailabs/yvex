/* Owner: core.status (core).
 * Owns: status names, error text, version facts, process-local logging, checked arithmetic, canonical string
 *   ownership, and deterministic index hashing.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: errors remain typed, copied text is terminated, arithmetic is checked, hashes use explicit byte
 *   encodings, and logging stays process-local.
 * Boundary: core status utilities do not infer model, backend, or runtime capability.
 * Purpose: provide status, error, checked scalar, owned-string, index-hash, and logging primitives.
 * Inputs: typed core values, borrowed byte ranges, and caller-owned outputs.
 * Effects: mutates only declared core state or caller-owned outputs.
 * Failure: returns typed refusal or null allocation with caller-owned state defined. */

#include <yvex/internal/core.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char yvex_empty_string[] = "";

/* Purpose: Compute copy text for its core invariant (`copy_text`). */
static void copy_text(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) {
        return;
    }

    if (!src) {
        src = "";
    }

    snprintf(dst, cap, "%s", src);
    dst[cap - 1] = '\0';
}

/* Reads a test-only process flag without allowing domain owners to duplicate
 * environment parsing or reinterpret false values. */
/* Purpose: Compute core test flag for its core invariant (`yvex_core_test_flag`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_core_test_flag(const char *name)
{
    const char *value = name ? getenv(name) : NULL;

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

/* Purpose: duplicate nullable text through one checked ownership mechanism.
 * Inputs: borrowed text; null is the canonical empty string.
 * Effects: returns one caller-owned allocation.
 * Failure: allocation or length overflow returns null without side effects.
 * Boundary: copying bytes does not validate or reinterpret domain text. */
char *yvex_core_strdup(const char *text)
{
    size_t length;
    char *copy;

    if (!text) {
        text = "";
    }
    length = strlen(text);
    if (length == SIZE_MAX) {
        return NULL;
    }
    copy = (char *)malloc(length + 1u);
    if (copy) {
        memcpy(copy, text, length + 1u);
    }
    return copy;
}

/* Purpose: mix an ordered byte range into a non-authoritative FNV-1a index hash.
 * Inputs: current hash plus a borrowed range; null is admitted only for zero length.
 * Effects: none.
 * Failure: invalid nonempty input preserves the incoming hash.
 * Boundary: lookup hashing only; semantic cryptographic identities use SHA-256. */
unsigned long long yvex_core_hash_mix_bytes(unsigned long long hash,
                                            const void *data,
                                            size_t length)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t index;

    if (!bytes && length != 0u) {
        return hash;
    }
    for (index = 0u; index < length; ++index) {
        hash ^= (unsigned long long)bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

/* Purpose: mix one explicit-width integer in canonical little-endian byte order.
 * Inputs: current non-authoritative hash and one unsigned 64-bit value.
 * Effects: none.
 * Failure: all inputs have a deterministic result.
 * Boundary: lookup hashing only; semantic cryptographic identities use SHA-256. */
unsigned long long yvex_core_hash_mix_u64(unsigned long long hash,
                                          unsigned long long value)
{
    unsigned char bytes[8];
    unsigned int index;

    for (index = 0u; index < sizeof(bytes); ++index) {
        bytes[index] = (unsigned char)((value >> (index * 8u)) & 0xffull);
    }
    return yvex_core_hash_mix_bytes(hash, bytes, sizeof(bytes));
}

/* Purpose: hash one nullable key for deterministic process-local indexes.
 * Inputs: borrowed UTF-8/byte text, with null equivalent to an empty key.
 * Effects: none.
 * Failure: all inputs produce a nonzero deterministic index hash.
 * Boundary: index placement only; this value is never a semantic identity. */
unsigned long long yvex_core_index_hash(const char *text)
{
    const char *key = text ? text : "";
    unsigned long long hash = yvex_core_hash_mix_bytes(
        1469598103934665603ull, key, strlen(key));

    return hash ? hash : 1ull;
}

/* Purpose: add two 64-bit geometry facts without wraparound.
 * Inputs: immutable operands and required caller-owned output.
 * Effects: writes the exact sum only on success.
 * Failure: null output or overflow returns false without writing output.
 * Boundary: generic checked arithmetic; it owns no domain accounting policy. */
int yvex_core_u64_add(unsigned long long left,
                      unsigned long long right,
                      unsigned long long *out)
{
    if (!out || left > ULLONG_MAX - right) {
        return 0;
    }
    *out = left + right;
    return 1;
}

/* Purpose: multiply two 64-bit geometry facts without wraparound.
 * Inputs: immutable operands and required caller-owned output.
 * Effects: writes the exact product only on success.
 * Failure: null output or overflow returns false without writing output.
 * Boundary: generic checked arithmetic; it owns no domain accounting policy. */
int yvex_core_u64_mul(unsigned long long left,
                      unsigned long long right,
                      unsigned long long *out)
{
    if (!out || (left != 0ull && right > ULLONG_MAX / left)) {
        return 0;
    }
    *out = left * right;
    return 1;
}

/* Purpose: Release or reset owned error clear state (`yvex_error_clear`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
void yvex_error_clear(yvex_error *err)
{
    if (!err) {
        return;
    }

    err->code = YVEX_OK;
    err->where[0] = '\0';
    err->message[0] = '\0';
}

/* Purpose: Compute error set for its core invariant (`yvex_error_set`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
void yvex_error_set(yvex_error *err, yvex_status code, const char *where, const char *message)
{
    if (!err) {
        return;
    }

    err->code = code;
    copy_text(err->where, YVEX_ERROR_WHERE_CAP, where ? where : "unknown");
    copy_text(err->message, YVEX_ERROR_MESSAGE_CAP, message);
}

/* Purpose: Compute error setf for its core invariant (`yvex_error_setf`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
void yvex_error_setf(yvex_error *err, yvex_status code, const char *where, const char *fmt, ...)
{
    va_list ap;

    if (!err) {
        return;
    }

    err->code = code;
    copy_text(err->where, YVEX_ERROR_WHERE_CAP, where ? where : "unknown");

    if (!fmt) {
        err->message[0] = '\0';
        return;
    }

    va_start(ap, fmt);
    vsnprintf(err->message, YVEX_ERROR_MESSAGE_CAP, fmt, ap);
    va_end(ap);
    err->message[YVEX_ERROR_MESSAGE_CAP - 1] = '\0';
}

/* Purpose: Compute error is set for its core invariant (`yvex_error_is_set`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_error_is_set(const yvex_error *err)
{
    if (!err) {
        return 0;
    }

    return err->code != YVEX_OK;
}

/* Purpose: Compute error code for its core invariant (`yvex_error_code`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
yvex_status yvex_error_code(const yvex_error *err)
{
    if (!err) {
        return YVEX_OK;
    }

    return err->code;
}

/* Purpose: Compute error where for its core invariant (`yvex_error_where`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
const char *yvex_error_where(const yvex_error *err)
{
    if (!err) {
        return yvex_empty_string;
    }

    return err->where;
}

/* Purpose: Compute error message for its core invariant (`yvex_error_message`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
const char *yvex_error_message(const yvex_error *err)
{
    if (!err) {
        return yvex_empty_string;
    }

    return err->message;
}

/* Purpose: Compute log level name for its core invariant (`yvex_log_level_name`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
const char *yvex_log_level_name(yvex_log_level level)
{
    switch (level) {
    case YVEX_LOG_ERROR:
        return "error";
    case YVEX_LOG_WARN:
        return "warn";
    case YVEX_LOG_INFO:
        return "info";
    case YVEX_LOG_DEBUG:
        return "debug";
    case YVEX_LOG_TRACE:
        return "trace";
    default:
        return "unknown";
    }
}

/* Purpose: Compute log domain name for its core invariant (`yvex_log_domain_name`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
const char *yvex_log_domain_name(yvex_log_domain domain)
{
    switch (domain) {
    case YVEX_LOG_CORE:
        return "core";
    case YVEX_LOG_CLI:
        return "cli";
    default:
        return "unknown";
    }
}

/* Purpose: Compute status name for its core invariant (`yvex_status_name`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
const char *yvex_status_name(yvex_status status)
{
    switch (status) {
    case YVEX_OK:
        return "YVEX_OK";
    case YVEX_ERR:
        return "YVEX_ERR";
    case YVEX_ERR_NOMEM:
        return "YVEX_ERR_NOMEM";
    case YVEX_ERR_IO:
        return "YVEX_ERR_IO";
    case YVEX_ERR_FORMAT:
        return "YVEX_ERR_FORMAT";
    case YVEX_ERR_UNSUPPORTED:
        return "YVEX_ERR_UNSUPPORTED";
    case YVEX_ERR_BACKEND:
        return "YVEX_ERR_BACKEND";
    case YVEX_ERR_BOUNDS:
        return "YVEX_ERR_BOUNDS";
    case YVEX_ERR_STATE:
        return "YVEX_ERR_STATE";
    case YVEX_ERR_CANCELLED:
        return "YVEX_ERR_CANCELLED";
    case YVEX_ERR_INVALID_ARG:
        return "YVEX_ERR_INVALID_ARG";
    default:
        return "YVEX_STATUS_UNKNOWN";
    }
}

/* Purpose: Compute status is ok for its core invariant (`yvex_status_is_ok`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_status_is_ok(yvex_status status)
{
    return status == YVEX_OK;
}

/* Purpose: Compute status is error for its core invariant (`yvex_status_is_error`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_status_is_error(yvex_status status)
{
    return status != YVEX_OK;
}

/* Purpose: Compute version string for its core invariant (`yvex_version_string`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
const char *yvex_version_string(void)
{
    return "0.1.0";
}

/* Purpose: Compute version major for its core invariant (`yvex_version_major`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_version_major(void)
{
    return YVEX_VERSION_MAJOR;
}

/* Purpose: Compute version minor for its core invariant (`yvex_version_minor`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_version_minor(void)
{
    return YVEX_VERSION_MINOR;
}

/* Purpose: Compute version patch for its core invariant (`yvex_version_patch`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_version_patch(void)
{
    return YVEX_VERSION_PATCH;
}
