/*
 * YVEX - Umbrella header
 *
 * File: include/yvex/yvex.h
 * Layer: public core API
 *
 * Purpose:
 *   Provides the A0.1 umbrella include for implemented public core headers.
 *   It includes only real headers with backing implementation and tests.
 *
 * Owns:
 *   - public aggregation of version/status/error/log
 *
 * Does not own:
 *   - future artifact/model/backend/session APIs
 *   - implementation details
 *   - terminal UI types
 *
 * Used by:
 *   - yvex CLI
 *   - external C consumers of the A0.1 core surface
 *   - tests
 *
 * Validation:
 *   - make check
 *   - build/bin/yvex info
 */
#ifndef YVEX_H
#define YVEX_H

#include <yvex/error.h>
#include <yvex/log.h>
#include <yvex/status.h>
#include <yvex/version.h>

#endif /* YVEX_H */
