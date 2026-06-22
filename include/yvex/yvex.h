/*
 * YVEX - Umbrella header
 *
 * File: include/yvex/yvex.h
 * Layer: public core API
 *
 * Purpose:
 *   Provides the umbrella include for implemented public YVEX headers.
 *   It includes only real headers with backing implementation and tests.
 *
 * Owns:
 *   - public aggregation of implemented YVEX headers
 *
 * Does not own:
 *   - future backend/session APIs
 *   - implementation details
 *
 * Used by:
 *   - yvex CLI
 *   - external C consumers of the implemented public surface
 *   - tests
 *
 * Validation:
 *   - make check
 *   - build/bin/yvex info
 */
#ifndef YVEX_H
#define YVEX_H

#include <yvex/artifact.h>
#include <yvex/dtype.h>
#include <yvex/error.h>
#include <yvex/fs.h>
#include <yvex/gguf.h>
#include <yvex/graph.h>
#include <yvex/log.h>
#include <yvex/memory_plan.h>
#include <yvex/model.h>
#include <yvex/op.h>
#include <yvex/planner.h>
#include <yvex/prompt.h>
#include <yvex/status.h>
#include <yvex/tensor.h>
#include <yvex/tokenizer.h>
#include <yvex/version.h>

#endif /* YVEX_H */
