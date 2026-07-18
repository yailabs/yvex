/*
 * roundtrip_gate.h - complete artifact admission ABI.
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

#include "src/gguf/file_sink.h"
#include "src/gguf/roundtrip.h"

#include <yvex/artifact.h>

#define YVEX_GGUF_OFFICIAL_READER_REVISION \
    "af97976c7810cdabb1863172f31c432dab767de7"
#define YVEX_SELECTED_DEEPSEEK_ARTIFACT_FILENAME \
    "deepseek-v4-flash-q8_0-q2_k-v1.gguf"
#define YVEX_SELECTED_DEEPSEEK_ARTIFACT_IDENTITY \
    "01b2bed4f070d0a3fdb02e546764b3a49cb69886eebe17b4877d20294725682c"
#define YVEX_SELECTED_DEEPSEEK_PROFILE_NAME \
    "deepseek-v4-flash-q8_0-q2_k-v1"
#define YVEX_SELECTED_DEEPSEEK_PROFILE_IDENTITY \
    "04be09e124fd997ae3b785d0d3018f9d571cb6b96df5488d0ab21de3345bce25"
#define YVEX_SELECTED_DEEPSEEK_EXECUTION_IDENTITY \
    "b81f3c5d670737bf20c938e635a1bffdbb0d60f885f994225a02225bb7ba51db"
#define YVEX_SELECTED_DEEPSEEK_PAYLOAD_IDENTITY \
    "e22b3678d131d334f154a93214bdddfafc172c9869f4c52db28fea198eaa9165"
#define YVEX_SELECTED_DEEPSEEK_TRANSFORM_IDENTITY \
    "1c5ceab43fa9f9bf437aacc3b4b3c246ff26446ab0d7abd22ea642ce726017f5"
#define YVEX_SELECTED_DEEPSEEK_WRITER_PLAN_IDENTITY \
    "4b47814e06c43b3426efcaab72b836596c42358a7c59ea5619ddd70c0eefe9fd"
#define YVEX_SELECTED_DEEPSEEK_FILE_BYTES 102408545440ull
#define YVEX_SELECTED_DEEPSEEK_PAYLOAD_BYTES 102396843592ull
#define YVEX_SELECTED_DEEPSEEK_TENSOR_COUNT 1360ull
#define YVEX_SELECTED_DEEPSEEK_METADATA_COUNT 68ull
#define YVEX_SELECTED_DEEPSEEK_SOURCE_IDENTITY 0x818f3e5c5eaf9ffcull
#define YVEX_SELECTED_DEEPSEEK_MAPPING_IDENTITY 0x1aecbbe25b04de0dull

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

typedef enum {
    YVEX_ARTIFACT_DESCRIPTOR_REFUSED = 0,
    YVEX_ARTIFACT_DESCRIPTOR_REPORT_ONLY = 1,
    YVEX_ARTIFACT_DESCRIPTOR_COMPLETE_ADMITTED = 2
} yvex_artifact_descriptor_status;

typedef struct {
    yvex_artifact_descriptor_status status;
    const char *format;
    const char *reason;
    const char *next_row;
    const char *artifact_identity;
    unsigned long long tensor_count;
    int materialization_input_ready;
    int runtime_supported;
} yvex_artifact_descriptor_fact;

int yvex_complete_artifact_admit(
    const yvex_artifact_admission_request *request,
    yvex_complete_artifact_admission *out,
    yvex_artifact_admission_failure *failure,
    yvex_error *err);
int yvex_artifact_admit_deepseek(
    const yvex_artifact *artifact,
    yvex_complete_artifact_admission *out,
    yvex_artifact_admission_failure *failure,
    yvex_error *err);
const char *yvex_artifact_class_name(yvex_artifact_class artifact_class);
const char *yvex_artifact_admission_code_name(
    yvex_artifact_admission_code code);
void yvex_artifact_descriptor_refuse_missing_gguf(
    yvex_artifact_descriptor_fact *fact);
int yvex_artifact_descriptor_from_admission(
    const yvex_complete_artifact_admission *admission,
    yvex_artifact_descriptor_fact *fact);
const char *yvex_artifact_descriptor_status_name(
    yvex_artifact_descriptor_status status);

#endif
