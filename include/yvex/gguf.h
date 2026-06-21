/*
 * YVEX - GGUF header probe
 *
 * File: include/yvex/gguf.h
 * Layer: public format API
 *
 * Purpose:
 *   Defines the implemented C0 GGUF probe/header API. This surface identifies
 *   GGUF files and reads the fixed header only; metadata, tensor directory,
 *   tokenizer, and model loading remain future work.
 *
 * Owns:
 *   - YVEX_GGUF_MAGIC
 *   - yvex_gguf_header
 *   - yvex_gguf_probe
 *   - yvex_gguf_probe_file
 *   - yvex_gguf_read_header
 *
 * Does not own:
 *   - GGUF metadata entries
 *   - tensor directory parsing
 *   - qtype support
 *   - model execution
 *
 * Used by:
 *   - yvex inspect
 *   - tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_gguf
 */
#ifndef YVEX_GGUF_H
#define YVEX_GGUF_H

#include <yvex/artifact.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_GGUF_MAGIC 0x46554747u

typedef struct {
    unsigned int version;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
} yvex_gguf_header;

typedef struct {
    int is_gguf;
    yvex_gguf_header header;
} yvex_gguf_probe;

int yvex_gguf_probe_file(const yvex_artifact *artifact, yvex_gguf_probe *out, yvex_error *err);
int yvex_gguf_read_header(const yvex_artifact *artifact, yvex_gguf_header *out, yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_GGUF_H */
