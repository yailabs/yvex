/*
 * YVEX - Error helpers
 *
 * File: src/core/error.c
 * Layer: core implementation
 *
 * Purpose:
 *   Implements caller-owned error helper functions for fixed-size YVEX error
 *   objects. This module does not allocate, abort, log, or print.
 *
 * Implements:
 *   - yvex_error_clear
 *   - yvex_error_set
 *   - yvex_error_setf
 *   - yvex_error_is_set
 *   - yvex_error_code
 *   - yvex_error_where
 *   - yvex_error_message
 *
 * Invariants:
 *   - error strings are always null-terminated
 *   - null yvex_error pointers are tolerated by clear/set helpers
 *   - helpers do not allocate heap memory
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_error
 */
#include <yvex/error.h>

#include <stdarg.h>
#include <stdio.h>

static const char yvex_empty_string[] = "";

static void yvex_copy_text(char *dst, unsigned long cap, const char *src)
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
