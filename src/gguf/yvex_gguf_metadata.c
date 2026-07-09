/*
 * yvex_gguf_metadata.c - GGUF metadata ABI boundary facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   GGUF metadata type names, key/value ABI facts, and unsupported metadata
 *   type refusal.
 *
 * Does not own:
 *   tensor byte ranges, writer emission, runtime descriptor projection,
 *   materialization, or generation.
 *
 * Invariants:
 *   unknown metadata value types refuse explicitly.
 *
 * Boundary:
 *   metadata ABI reporting is not artifact emission or runtime descriptor
 *   support.
 */
#include "yvex_gguf_private.h"

typedef struct {
    unsigned int type_id;
    const char *name;
} yvex_gguf_metadata_type_fact;

static const yvex_gguf_metadata_type_fact metadata_types[] = {
    {0u, "uint8"},
    {1u, "int8"},
    {2u, "uint16"},
    {3u, "int16"},
    {4u, "uint32"},
    {5u, "int32"},
    {6u, "float32"},
    {7u, "bool"},
    {8u, "string"},
    {9u, "array"},
    {10u, "uint64"},
    {11u, "int64"},
    {12u, "float64"}
};

/* Contract: maps GGUF metadata ABI type IDs to stable names without allocation. */
const char *yvex_gguf_metadata_type_name(unsigned int type)
{
    size_t i;

    for (i = 0u; i < sizeof(metadata_types) / sizeof(metadata_types[0]); ++i) {
        if (metadata_types[i].type_id == type) return metadata_types[i].name;
    }
    return "unknown";
}

/* Contract: refuses unsupported metadata value type IDs with a precise reason. */
int yvex_gguf_metadata_type_supported(unsigned int type, const char **reason)
{
    if (yvex_gguf_metadata_type_name(type)[0] != 'u') {
        if (reason) *reason = "metadata value type accepted by GGUF metadata ABI boundary";
        return 1;
    }
    if (reason) *reason = "unsupported GGUF metadata value type";
    return 0;
}
