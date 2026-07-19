/* Owner: gguf.container
 * Owns: GGUF magic, version, count, and metadata-value admission facts.
 * Does not own: tensor descriptors, payload ranges, writer emission, or runtime binding.
 * Invariants: unsupported versions, counts, and metadata types always refuse explicitly.
 * Boundary: container and metadata admission do not imply a complete artifact.
 * Purpose: centralize the small parser-facing contracts for the GGUF structural prefix.
 * Inputs: immutable parsed headers and metadata views.
 * Effects: mutates only caller-owned ABI summaries and optional reason pointers.
 * Failure: malformed or unsupported facts remain typed and never become accepted state. */
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <yvex/internal/gguf.h>

/* Purpose: initialize a container summary to fail-closed state.
 * Inputs: optional caller-owned summary.
 * Effects: clears counts and installs a stable reason.
 * Failure: a null summary is ignored.
 * Boundary: initialization performs no input read. */
void yvex_gguf_container_abi_init(yvex_gguf_container_abi *abi) {
    if (!abi)
        return;
    abi->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    abi->magic = 0u;
    abi->version = 0u;
    abi->metadata_count = 0ull;
    abi->tensor_count = 0ull;
    abi->reason = "GGUF container ABI not evaluated";
}

/* Purpose: admit only the pinned GGUF container version.
 * Inputs: raw version and optional reason sink.
 * Effects: writes borrowed immutable reason text when requested.
 * Failure: unsupported versions return zero.
 * Boundary: version admission does not validate metadata or tensors. */
static int container_version_supported(unsigned int version, const char **reason) {
    if (version == 3u) {
        if (reason)
            *reason = "container version accepted by current parser boundary";
        return 1;
    }
    if (reason)
        *reason = "unsupported GGUF container version";
    return 0;
}

/* Purpose: validate header record counts before host-size conversion.
 * Inputs: metadata count, tensor count, and optional reason sink.
 * Effects: writes borrowed immutable reason text when requested.
 * Failure: excessive counts return zero without narrowing.
 * Boundary: count admission does not allocate record arrays. */
static int container_counts_supported(unsigned long long metadata_count,
                                      unsigned long long tensor_count, const char **reason) {
    if (metadata_count > (unsigned long long)(SIZE_MAX / sizeof(void *))) {
        if (reason)
            *reason = "metadata count exceeds parser allocation boundary";
        return 0;
    }
    if (tensor_count > (unsigned long long)(SIZE_MAX / sizeof(void *))) {
        if (reason)
            *reason = "tensor count exceeds parser allocation boundary";
        return 0;
    }
    if (reason)
        *reason = "GGUF metadata and tensor counts accepted";
    return 1;
}

/* Purpose: project one parsed header into canonical container ABI facts.
 * Inputs: immutable parsed header and caller-owned summary.
 * Effects: initializes then conditionally accepts the summary.
 * Failure: missing or unsupported header facts leave a typed non-success result.
 * Boundary: projection never reads beyond the supplied header. */
void yvex_gguf_container_abi_from_header(const yvex_gguf_header *header,
                                         yvex_gguf_container_abi *abi) {
    const char *reason = NULL;

    yvex_gguf_container_abi_init(abi);
    if (!abi)
        return;
    if (!header) {
        abi->status = YVEX_GGUF_ABI_SECTION_NOT_PRESENT;
        abi->reason = "GGUF header not present";
        return;
    }

    abi->magic = YVEX_GGUF_MAGIC;
    abi->version = header->version;
    abi->metadata_count = header->metadata_count;
    abi->tensor_count = header->tensor_count;

    if (!container_version_supported(header->version, &reason)) {
        abi->status = YVEX_GGUF_ABI_SECTION_UNSUPPORTED;
        abi->reason = reason;
        return;
    }
    if (!container_counts_supported(header->metadata_count, header->tensor_count, &reason)) {
        abi->status = YVEX_GGUF_ABI_SECTION_REFUSED;
        abi->reason = reason;
        return;
    }

    abi->status = YVEX_GGUF_ABI_SECTION_OK;
    abi->reason = reason;
}

/* Purpose: admit metadata value types implemented by the canonical parser.
 * Inputs: raw type identifier and optional reason sink.
 * Effects: writes borrowed immutable reason text when requested.
 * Failure: unknown types return zero.
 * Boundary: type admission does not parse a value payload. */
static int metadata_type_supported(unsigned int type, const char **reason) {
    if (type <= 12u) {
        if (reason)
            *reason = "metadata value type accepted by GGUF metadata ABI boundary";
        return 1;
    }
    if (reason)
        *reason = "unsupported GGUF metadata value type";
    return 0;
}

/* Purpose: initialize a metadata summary to fail-closed state.
 * Inputs: optional caller-owned summary.
 * Effects: clears counters and installs a stable reason.
 * Failure: a null summary is ignored.
 * Boundary: initialization performs no metadata traversal. */
void yvex_gguf_metadata_abi_init(yvex_gguf_metadata_abi *abi) {
    if (!abi)
        return;
    abi->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    abi->entry_count = 0ull;
    abi->string_value_count = 0ull;
    abi->array_value_count = 0ull;
    abi->reason = "GGUF metadata ABI not evaluated";
}

/* Purpose: summarize metadata entries from one immutable parsed GGUF view.
 * Inputs: parsed container, caller-owned summary, and optional reason sink.
 * Effects: scans metadata types and updates summary counters.
 * Failure: null input or unsupported metadata types produce typed refusal.
 * Boundary: traversal validates structure only and reads no tensor payload. */
int yvex_gguf_metadata_abi_from_gguf(const yvex_gguf *gguf, yvex_gguf_metadata_abi *abi,
                                     const char **reason) {
    unsigned long long i;
    unsigned long long count;
    unsigned long long len;

    yvex_gguf_metadata_abi_init(abi);
    if (!gguf || !abi) {
        if (reason)
            *reason = "GGUF metadata requires a parsed GGUF view";
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
            if (reason)
                *reason = abi->reason;
            return 0;
        }

        type = yvex_gguf_value_type_of(value);
        if (!metadata_type_supported((unsigned int)type, reason)) {
            abi->status = YVEX_GGUF_ABI_SECTION_UNSUPPORTED;
            abi->reason = reason ? *reason : "metadata type unsupported";
            return 0;
        }

        if (type == YVEX_GGUF_VALUE_STRING) {
            const char *data = NULL;
            if (yvex_gguf_value_as_string(value, &data, &len) != YVEX_OK || !data) {
                abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
                abi->reason = "metadata string value is malformed";
                if (reason)
                    *reason = abi->reason;
                return 0;
            }
            abi->string_value_count += 1ull;
        } else if (type == YVEX_GGUF_VALUE_ARRAY) {
            yvex_gguf_array_info info;
            if (yvex_gguf_value_array_info(value, &info) != YVEX_OK) {
                abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
                abi->reason = "metadata array value is malformed";
                if (reason)
                    *reason = abi->reason;
                return 0;
            }
            if (!metadata_type_supported((unsigned int)info.element_type, reason)) {
                abi->status = YVEX_GGUF_ABI_SECTION_UNSUPPORTED;
                abi->reason = reason ? *reason : "metadata array element type unsupported";
                return 0;
            }
            abi->array_value_count += 1ull;
        }
    }

    abi->status = YVEX_GGUF_ABI_SECTION_OK;
    abi->reason = "GGUF metadata ABI accepted";
    if (reason)
        *reason = abi->reason;
    return 1;
}
