/*
 * yvex_core.c - Errors, status names, logging, and version reporting.
 *
 * This file owns the small core utilities used by every other YVEX module.
 */

#include <yvex/error.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

static const char yvex_empty_string[] = "";

static void yvex_copy_text(char *dst, size_t cap, const char *src)
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

void yvex_error_clear(yvex_error *err)
{
    if (!err) {
        return;
    }

    err->code = YVEX_OK;
    err->where[0] = '\0';
    err->message[0] = '\0';
}

void yvex_error_set(yvex_error *err, yvex_status code, const char *where, const char *message)
{
    if (!err) {
        return;
    }

    err->code = code;
    yvex_copy_text(err->where, YVEX_ERROR_WHERE_CAP, where ? where : "unknown");
    yvex_copy_text(err->message, YVEX_ERROR_MESSAGE_CAP, message);
}

void yvex_error_setf(yvex_error *err, yvex_status code, const char *where, const char *fmt, ...)
{
    va_list ap;

    if (!err) {
        return;
    }

    err->code = code;
    yvex_copy_text(err->where, YVEX_ERROR_WHERE_CAP, where ? where : "unknown");

    if (!fmt) {
        err->message[0] = '\0';
        return;
    }

    va_start(ap, fmt);
    vsnprintf(err->message, YVEX_ERROR_MESSAGE_CAP, fmt, ap);
    va_end(ap);
    err->message[YVEX_ERROR_MESSAGE_CAP - 1] = '\0';
}

int yvex_error_is_set(const yvex_error *err)
{
    if (!err) {
        return 0;
    }

    return err->code != YVEX_OK;
}

yvex_status yvex_error_code(const yvex_error *err)
{
    if (!err) {
        return YVEX_OK;
    }

    return err->code;
}

const char *yvex_error_where(const yvex_error *err)
{
    if (!err) {
        return yvex_empty_string;
    }

    return err->where;
}

const char *yvex_error_message(const yvex_error *err)
{
    if (!err) {
        return yvex_empty_string;
    }

    return err->message;
}

#include <yvex/log.h>

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

#include <yvex/status.h>

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

int yvex_status_is_ok(yvex_status status)
{
    return status == YVEX_OK;
}

int yvex_status_is_error(yvex_status status)
{
    return status != YVEX_OK;
}

#include <yvex/version.h>

const char *yvex_version_string(void)
{
    return "0.1.0";
}

int yvex_version_major(void)
{
    return YVEX_VERSION_MAJOR;
}

int yvex_version_minor(void)
{
    return YVEX_VERSION_MINOR;
}

int yvex_version_patch(void)
{
    return YVEX_VERSION_PATCH;
}
