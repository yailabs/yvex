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
 *   - implementation details
 *   - future sampler/server APIs
 *
 * Used by:
 *   - yvex CLI
 *   - external C consumers of the implemented public surface
 *   - tests
 *
 * Validation:
 *   - make check
 *   - ./yvex info
 */
#ifndef YVEX_H
#define YVEX_H

#include <yvex/artifact.h>
#include <yvex/artifact_identity.h>
#include <yvex/artifact_integrity.h>
#include <yvex/artifact_naming.h>
#include <yvex/backend.h>
#include <yvex/conversion.h>
#include <yvex/decode.h>
#include <yvex/dtype.h>
#include <yvex/engine.h>
#include <yvex/error.h>
#include <yvex/fs.h>
#include <yvex/gguf.h>
#include <yvex/gguf_emit.h>
#include <yvex/gguf_template.h>
#include <yvex/graph.h>
#include <yvex/imatrix.h>
#include <yvex/kv.h>
#include <yvex/logits.h>
#include <yvex/log.h>
#include <yvex/materialize_gate.h>
#include <yvex/memory_plan.h>
#include <yvex/metrics.h>
#include <yvex/model_gate.h>
#include <yvex/model_ref.h>
#include <yvex/model_registry.h>
#include <yvex/model.h>
#include <yvex/native_weights.h>
#include <yvex/op.h>
#include <yvex/planner.h>
#include <yvex/profile.h>
#include <yvex/prompt.h>
#include <yvex/quant_job.h>
#include <yvex/quant_policy.h>
#include <yvex/qtype_support.h>
#include <yvex/server.h>
#include <yvex/session.h>
#include <yvex/source_manifest.h>
#include <yvex/status.h>
#include <yvex/tensor.h>
#include <yvex/token_input.h>
#include <yvex/tokenizer.h>
#include <yvex/trace.h>
#include <yvex/version.h>
#include <yvex/weight_mapping.h>
#include <yvex/weights.h>

#endif /* YVEX_H */
