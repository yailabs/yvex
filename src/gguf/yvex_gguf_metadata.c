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

#include <stddef.h>

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
    if (type <= 12u) {
        if (reason) *reason = "metadata value type accepted by GGUF metadata ABI boundary";
        return 1;
    }
    if (reason) *reason = "unsupported GGUF metadata value type";
    return 0;
}

/* Contract: initializes metadata ABI facts to fail-closed not-evaluated state. */
void yvex_gguf_metadata_abi_init(yvex_gguf_metadata_abi *abi)
{
    if (!abi) return;
    abi->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    abi->entry_count = 0ull;
    abi->string_value_count = 0ull;
    abi->array_value_count = 0ull;
    abi->required_key_count = 0ull;
    abi->reason = "GGUF metadata ABI not evaluated";
}

/* Contract: projects parsed metadata entries into typed ABI facts only. */
int yvex_gguf_metadata_abi_from_gguf(const yvex_gguf *gguf,
                                     yvex_gguf_metadata_abi *abi,
                                     const char **reason)
{
    unsigned long long i;
    unsigned long long count;
    unsigned long long len;

    yvex_gguf_metadata_abi_init(abi);
    if (!gguf || !abi) {
        if (reason) *reason = "GGUF metadata requires a parsed GGUF view";
        return 0;
    }

    count = yvex_gguf_metadata_count(gguf);
    abi->entry_count = count;

    for (i = 0ull; i < count; ++i) {
        const char *key = yvex_gguf_metadata_key(gguf, i);
        const yvex_gguf_value *value = yvex_gguf_metadata_value(gguf, i);
        yvex_gguf_value_type type;

        if (!key || key[0] == '\0' || !value) {
            abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
            abi->reason = "metadata entry is missing key or value";
            if (reason) *reason = abi->reason;
            return 0;
        }

        type = yvex_gguf_value_type_of(value);
        if (!yvex_gguf_metadata_type_supported((unsigned int)type, reason)) {
            abi->status = YVEX_GGUF_ABI_SECTION_UNSUPPORTED;
            abi->reason = reason ? *reason : "metadata type unsupported";
            return 0;
        }

        if (type == YVEX_GGUF_VALUE_STRING) {
            const char *data = NULL;
            if (yvex_gguf_value_as_string(value, &data, &len) != YVEX_OK || !data) {
                abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
                abi->reason = "metadata string value is malformed";
                if (reason) *reason = abi->reason;
                return 0;
            }
            abi->string_value_count += 1ull;
        } else if (type == YVEX_GGUF_VALUE_ARRAY) {
            yvex_gguf_array_info info;
            if (yvex_gguf_value_array_info(value, &info) != YVEX_OK) {
                abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
                abi->reason = "metadata array value is malformed";
                if (reason) *reason = abi->reason;
                return 0;
            }
            if (!yvex_gguf_metadata_type_supported((unsigned int)info.element_type, reason)) {
                abi->status = YVEX_GGUF_ABI_SECTION_UNSUPPORTED;
                abi->reason = reason ? *reason : "metadata array element type unsupported";
                return 0;
            }
            abi->array_value_count += 1ull;
        }

        if (key && (key[0] == 'g' || key[0] == 't')) {
            abi->required_key_count += 1ull;
        }
    }

    abi->status = YVEX_GGUF_ABI_SECTION_OK;
    abi->reason = "GGUF metadata ABI accepted";
    if (reason) *reason = abi->reason;
    return 1;
}
