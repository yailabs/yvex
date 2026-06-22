/*
 * YVEX - Metrics monotonic time
 *
 * File: src/metrics/time.c
 * Layer: runtime observability implementation
 *
 * Purpose:
 *   Provides monotonic nanosecond timestamps for J0 metrics and trace events.
 */
#define _POSIX_C_SOURCE 200809L

#include "metrics_internal.h"

#include <time.h>

unsigned long long yvex_time_monotonic_ns(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }

    return ((unsigned long long)ts.tv_sec * 1000000000ull) + (unsigned long long)ts.tv_nsec;
}
