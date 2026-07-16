/*
 * yvex_artifact_roundtrip_gate.h - complete artifact admission ABI.
 *
 * Owner: TRACK.ARTIFACT / TRACK.INTEGRITY.
 * Owns: typed complete-artifact classification and binding of writer,
 *   emission, native roundtrip, pinned official-reader, digest, and immutable
 *   file-snapshot evidence.
 * Does not own: GGUF writing/parsing, external checker implementation,
 *   materialization, runtime descriptors, execution, generation, or release.
 * Invariants: only one complete YVEX emission bound to one unchanged file may
 *   be admitted; proof, external, incomplete, and drifted artifacts refuse.
 * Boundary: complete-artifact admission enables materialization intake only.
 */
#ifndef YVEX_ARTIFACT_ROUNDTRIP_GATE_H
#define YVEX_ARTIFACT_ROUNDTRIP_GATE_H

#include "src/gguf/yvex_gguf_file_sink.h"
#include "src/gguf/yvex_gguf_roundtrip.h"

#include <yvex/artifact.h>

#define YVEX_GGUF_OFFICIAL_READER_REVISION \
    "af97976c7810cdabb1863172f31c432dab767de7"

typedef enum {
    YVEX_ARTIFACT_CLASS_REFUSED = 0,
    YVEX_ARTIFACT_CLASS_TENSOR_PROOF,
    YVEX_ARTIFACT_CLASS_EXTERNAL_UNADMITTED,
    YVEX_ARTIFACT_CLASS_COMPLETE_YVEX
} yvex_artifact_class;

typedef enum {
    YVEX_ARTIFACT_ADMISSION_OK = 0,
    YVEX_ARTIFACT_ADMISSION_INVALID_ARGUMENT,
    YVEX_ARTIFACT_ADMISSION_WRITER_INCOMPLETE,
    YVEX_ARTIFACT_ADMISSION_EMISSION_INCOMPLETE,
    YVEX_ARTIFACT_ADMISSION_NATIVE_ROUNDTRIP,
    YVEX_ARTIFACT_ADMISSION_OFFICIAL_READER,
    YVEX_ARTIFACT_ADMISSION_IDENTITY_MISMATCH,
    YVEX_ARTIFACT_ADMISSION_TOKENIZER_INCOMPLETE,
    YVEX_ARTIFACT_ADMISSION_TENSOR_COVERAGE,
    YVEX_ARTIFACT_ADMISSION_FILE_OPEN,
    YVEX_ARTIFACT_ADMISSION_FILE_DRIFT
} yvex_artifact_admission_code;

typedef struct {
    char revision[41];
    unsigned long long metadata_count;
    unsigned long long tensor_count;
    unsigned long long file_bytes;
    unsigned long long file_device;
    unsigned long long file_inode;
    long long file_mtime_seconds;
    long long file_mtime_nanoseconds;
    long long file_ctime_seconds;
    long long file_ctime_nanoseconds;
    int accepted;
} yvex_artifact_official_reader_fact;

typedef struct {
    yvex_artifact_admission_code code;
    unsigned long long expected;
    unsigned long long actual;
    char field[96];
} yvex_artifact_admission_failure;

typedef struct {
    const char *artifact_path;
    const yvex_gguf_writer_plan *writer_plan;
    const yvex_gguf_file_sink_summary *emission;
    const yvex_gguf_roundtrip_summary *native_roundtrip;
    const yvex_artifact_official_reader_fact *official_reader;
} yvex_artifact_admission_request;

typedef struct {
    yvex_artifact_class artifact_class;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
    unsigned long long payload_bytes;
    unsigned long long file_bytes;
    unsigned long long source_snapshot_identity;
    unsigned long long mapping_identity;
    yvex_artifact_snapshot file_snapshot;
    char artifact_path[YVEX_ARTIFACT_PATH_CAP];
    char payload_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char transform_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char profile_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char profile_name[64];
    char quant_execution_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char writer_plan_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char official_reader_revision[41];
    int tokenizer_complete;
    int native_reader_accepted;
    int official_reader_accepted;
    int payload_integrity_accepted;
    int materialization_input_ready;
    int runtime_supported;
    int complete;
} yvex_complete_artifact_admission;

int yvex_complete_artifact_admit(
    const yvex_artifact_admission_request *request,
    yvex_complete_artifact_admission *out,
    yvex_artifact_admission_failure *failure,
    yvex_error *err);
const char *yvex_artifact_class_name(yvex_artifact_class artifact_class);
const char *yvex_artifact_admission_code_name(
    yvex_artifact_admission_code code);

#endif
