/* Bounded host buffers and exact resident weight views for admitted component execution. */
#ifndef INCLUDE_YVEX_INTERNAL_COMPONENT_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_COMPONENT_H_INCLUDED

#include <yvex/backend.h>
#include <yvex/internal/artifact.h>
#include <yvex/internal/execution_transaction.h>
#include <yvex/internal/latent.h>
#include <yvex/internal/text_program.h>
#include <yvex/internal/neural_operations.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_runtime_residency yvex_runtime_residency;
typedef struct yvex_program_physical yvex_program_physical;
typedef struct yvex_program_stage yvex_program_stage;
typedef struct yvex_joint_program yvex_joint_program;
typedef struct yvex_component_program_binding yvex_component_program_binding;
typedef struct yvex_runtime_component_session yvex_runtime_component_session;
typedef struct yvex_runtime_av_layout_output yvex_runtime_av_layout_output;
typedef struct yvex_runtime_av_layout_result yvex_runtime_av_layout_result;
typedef struct yvex_transformer_linear_physical_plan yvex_transformer_linear_physical_plan;
typedef struct yvex_transformer_joint_request yvex_transformer_joint_request;
typedef struct yvex_transformer_joint_result yvex_transformer_joint_result;
typedef struct yvex_media_condition yvex_media_condition;

typedef struct {
    float *data;
    unsigned long long count;
} yvex_component_f32_buffer;

/* A runtime-owned component session lends this view for synchronous family/backend execution.
 * A runtime transaction may retain the owner through the typed resource lease; backend and
 * family consumers still cannot acquire, invalidate, or release resources directly. */
#define YVEX_COMPONENT_EXECUTION_SCHEMA_V2 2u
typedef int (*yvex_component_weight_view_fn)(
    void *, const char *, yvex_component_encoded_weight *, yvex_error *);
typedef int (*yvex_component_workspace_reserve_fn)(void *, unsigned long long, yvex_error *);
typedef struct yvex_component_execution {
    unsigned int schema_version;
    yvex_materialization_session *materialization;
    yvex_backend *backend;
    char residency_identity[YVEX_SHA256_HEX_CAP];
    unsigned long long resident_encoded_bytes;
    void *owner_context;
    yvex_component_weight_view_fn weight_view;
    yvex_component_workspace_reserve_fn workspace_reserve;
    /* Session-owned cleanup slot. A failed checked release remains reachable
     * and prevents another execution until the owner discharges it. */
    yvex_program_stage **program_stage;
} yvex_component_execution;

/* Cold binding resolves compiled parameter IDs once. Invocation consumes only
 * verified program signatures, tensor views and the component's lifetime. */
typedef int (*yvex_component_parameter_name_fn)(void *, unsigned long long, char[256], yvex_error *);
typedef enum {
    YVEX_COMPONENT_PARAMETER_GGUF_ORDER = 0,
    YVEX_COMPONENT_PARAMETER_SOURCE_ORDER = 1
} yvex_component_parameter_axis_order;
typedef struct yvex_component_program_request {
    const yvex_program_physical *program;
    yvex_component_parameter_name_fn parameter_name;
    /* Derived from the admitted package profile, never guessed from shapes.
     * Source-order compatibility packages retain outermost-first dimensions;
     * ordinary GGUF directories list the innermost dimension first. */
    yvex_component_parameter_axis_order parameter_axis_order;
    void *parameter_context;
    const float *const *inputs;
    /* Exactly one representation per entry, selected by the IR signature. */
    const unsigned int *const *index_inputs;
    const unsigned long long *input_capacity;
    float *const *outputs;
    const unsigned long long *output_capacity;
    size_t input_count, output_count;
    unsigned long long rows, host_limit, device_limit;
    int (*cancel_requested)(void *);
    void *cancel_context;
} yvex_component_program_request;
typedef struct {
    yvex_backend_operation_facts facts;
    unsigned long long host_bytes, device_bytes, parameter_reads, parameter_bytes;
    char execution_identity[YVEX_SHA256_HEX_BYTES];
    int complete;
} yvex_component_program_result;
int yvex_component_tensor_program_execute(const yvex_component_execution *,
    const yvex_component_program_request *, yvex_component_program_result *, yvex_error *);
/* CPU source-backed execution. The materialization remains caller-owned;
 * parameter names are resolved once, then bounded reads use exact bindings. */
int yvex_component_materialized_program_execute(yvex_materialization_session *,
    const yvex_component_program_request *, yvex_component_program_result *, yvex_error *);

typedef struct {
    unsigned long long token_count, hidden_width, layer_count, resident_bytes;
    unsigned long long kernel_launches, h2d_bytes, d2h_bytes, device_bytes;
    char residency_identity[YVEX_SHA256_HEX_BYTES];
    char execution_identity[YVEX_SHA256_HEX_BYTES];
    int complete;
} yvex_backend_text_execution_result;

typedef struct {
    const unsigned long long *position_ids;
    unsigned long long position_capacity;
    const unsigned int *visual_token_indices;
    unsigned long long visual_token_count;
    const float *visual_embeddings;
    unsigned long long visual_embedding_capacity;
    const float *deepstack_embeddings;
    unsigned long long deepstack_layer_count, deepstack_embedding_capacity;
    unsigned long long mrope_sections[3];
    const char *vision_execution_identity;
} yvex_backend_text_multimodal_input;


typedef enum {
    YVEX_COMPONENT_EXECUTION_NONE = 0,
    YVEX_COMPONENT_EXECUTION_INVALID_ARGUMENT,
    YVEX_COMPONENT_EXECUTION_LIFECYCLE,
    YVEX_COMPONENT_EXECUTION_MISSING_TENSOR,
    YVEX_COMPONENT_EXECUTION_TENSOR_CONTRACT,
    YVEX_COMPONENT_EXECUTION_BUDGET,
    YVEX_COMPONENT_EXECUTION_MATERIALIZATION,
    YVEX_COMPONENT_EXECUTION_NUMERIC,
    YVEX_COMPONENT_EXECUTION_CANCELLED
} yvex_component_execution_code;

typedef struct yvex_component_execution_failure {
    unsigned int code;
    char tensor_name[256];
    unsigned long long expected, actual;
    const char *reason;
} yvex_component_execution_failure;

typedef struct yvex_runtime_av_conditioning_result {
    unsigned long long token_count, hidden_width, layer_count;
    unsigned long long condition_count, condition_rows;
    unsigned long long condition_latent_height, condition_latent_width;
    unsigned long long condition_latent_values;
    unsigned long long resident_bytes, kernel_launches, h2d_bytes, d2h_bytes, device_bytes;
    unsigned long long peak_workspace_bytes;
    char prompt_identity[YVEX_SHA256_HEX_CAP];
    char processor_identity[YVEX_SHA256_HEX_CAP];
    char vision_identity[YVEX_SHA256_HEX_CAP];
    char condition_identity[YVEX_SHA256_HEX_CAP];
    char media_identities[2][YVEX_SHA256_HEX_CAP];
    char latent_identities[2][YVEX_SHA256_HEX_CAP];
    char residency_identity[YVEX_SHA256_HEX_CAP];
    char execution_identity[YVEX_SHA256_HEX_CAP];
    int complete;
} yvex_runtime_av_conditioning_result;

typedef struct yvex_runtime_av_keyframe_result {
    unsigned long long condition_count, condition_rows;
    unsigned long long latent_channels, latent_height, latent_width, latent_values;
    unsigned long long resident_bytes, kernel_launches, h2d_bytes, d2h_bytes, device_bytes;
    unsigned long long peak_workspace_bytes;
    char residency_identity[YVEX_SHA256_HEX_CAP];
    char media_identities[2][YVEX_SHA256_HEX_CAP];
    char latent_identities[2][YVEX_SHA256_HEX_CAP];
    char execution_identity[YVEX_SHA256_HEX_CAP];
    int complete;
} yvex_runtime_av_keyframe_result;

/* Invocation consumes a compiled signature and exact parameter IDs. Source
 * recipes, layer counts and parameter roles do not cross this boundary. */
typedef struct {
    const yvex_program_physical *program;
    const yvex_backend_text_multimodal_input *multimodal;
    yvex_component_parameter_name_fn parameter_name;
    void *parameter_context;
    const unsigned int *token_ids;
    unsigned long long token_count;
    float *output;
    unsigned long long output_capacity, maximum_host_bytes, maximum_device_bytes;
    int (*cancelled)(void *);
    void *cancellation_context;
} yvex_component_text_request;
/* Execute a cold-compiled component using the shared physical SSA executor.
 * Bindings are exact artifact handles, not executable layer descriptions. */
int yvex_component_text_program_execute(const yvex_component_execution *, const yvex_component_text_request *,
    const yvex_component_encoded_weight *, size_t, yvex_backend_text_execution_result *, yvex_error *);

#define YVEX_COMPONENT_RESOURCE_SUMMARY_SCHEMA_V2 2u
typedef struct {
    unsigned int schema_version;
    unsigned long long host_arena_bytes, device_arena_bytes;
    unsigned long long request_prepared_bytes, condition_prepared_bytes;
    unsigned long long preparation_nanoseconds, preparation_count;
    unsigned long long use_count, reuse_count, rebuild_count;
    unsigned long long allocation_count, execution_allocation_events;
    unsigned long long last_execution_allocation_events;
    unsigned long long resource_count, resource_generation;
    /* Aggregate retained resources across exact prepared programs. The
     * identity below remains the most recently selected program, not a set. */
    unsigned long long prepared_program_count, metadata_host_bytes;
    char prepared_identity[YVEX_SHA256_HEX_CAP];
    int ready, request_ready, condition_ready, retained_by_transaction;
} yvex_component_resource_summary;

typedef struct yvex_runtime_av_audio_decode_options {
    const float *latent;
    unsigned long long batch, latent_channels, latent_steps;
    float *output;
    unsigned long long output_capacity, max_workspace_bytes;
    int (*cancelled)(void *);
    void *cancellation_context;
} yvex_runtime_av_audio_decode_options;

typedef struct yvex_runtime_av_audio_decode_result {
    unsigned long long batch, samples_per_channel, output_values;
    unsigned long long tensor_reads, payload_bytes_read, peak_workspace_bytes, kernel_launches;
    unsigned long long h2d_bytes, d2h_bytes, device_bytes;
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char execution_identity[YVEX_SHA256_HEX_CAP];
    char residency_identity[YVEX_SHA256_HEX_CAP];
    int complete;
} yvex_runtime_av_audio_decode_result;

typedef struct yvex_runtime_av_video_decode_options {
    const float *latent;
    float *output;
    unsigned long long batch, latent_channels;
    unsigned long long latent_frames, latent_height, latent_width;
    unsigned long long output_capacity, max_workspace_bytes;
    int (*cancelled)(void *);
    void *cancellation_context;
} yvex_runtime_av_video_decode_options;

typedef struct yvex_runtime_av_video_decode_result {
    unsigned long long batch, frames, height, width, output_values;
    unsigned long long tensor_reads, payload_bytes_read, peak_workspace_bytes, kernel_launches;
    unsigned long long h2d_bytes, d2h_bytes, device_bytes;
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char execution_identity[YVEX_SHA256_HEX_CAP];
    char residency_identity[YVEX_SHA256_HEX_CAP];
    int complete;
} yvex_runtime_av_video_decode_result;

typedef struct yvex_runtime_av_latent_context {
    const yvex_component_execution *transformer_component;
    const float *conditioning;
    unsigned long long conditioning_capacity;
    const float *condition_latents;
    unsigned long long condition_latent_capacity;
    const yvex_runtime_av_keyframe_result *keyframes;
    const yvex_media_condition *conditions;
    unsigned long long condition_count;
    const yvex_runtime_av_layout_output *layout;
    const yvex_runtime_av_layout_result *layout_result;
    const yvex_transformer_linear_physical_plan *video_output_specialization;
    const yvex_transformer_linear_physical_plan *audio_output_specialization;
    unsigned int *timestep_indices;
    unsigned long long timestep_capacity, block_count;
    const char *conditioning_identity;
    int (*cancelled)(void *);
    void *cancellation_context;
    const yvex_execution_yield_control *yield_control;
    yvex_runtime_latent_observe_fn observe;
    void *observer_context;
} yvex_runtime_av_latent_context;

int yvex_component_buffer_open(
    yvex_component_f32_buffer *, unsigned long long, unsigned long long,
    unsigned long long *, unsigned long long *, const char *, const char *, yvex_error *);
void yvex_component_buffer_close(yvex_component_f32_buffer *, unsigned long long *);
int yvex_runtime_component_session_open(
    yvex_runtime_component_session **, const yvex_complete_artifact_admission *,
    const yvex_artifact *, const yvex_gguf *, const yvex_tensor_table *, yvex_backend_kind,
    unsigned long long, unsigned long long, yvex_error *);
int yvex_runtime_component_session_close(yvex_runtime_component_session **, yvex_error *);
int yvex_runtime_component_session_borrow(
    yvex_runtime_component_session *, yvex_component_execution *, yvex_error *);
int yvex_component_execution_resource_lease(
    const yvex_component_execution *, yvex_execution_resource_lease *, yvex_error *);
int yvex_component_execution_resource_summary(
    const yvex_component_execution *, yvex_component_resource_summary *, yvex_error *);
int yvex_component_execution_weight_view(
    const yvex_component_execution *, const char *, yvex_component_encoded_weight *,
    yvex_error *);
int yvex_component_text_execute(
    const yvex_component_execution *, const yvex_component_text_request *,
    yvex_runtime_av_conditioning_result *, yvex_error *);
int yvex_runtime_component_text_artifact_execute(
    const yvex_complete_artifact_admission *, const yvex_artifact *, const yvex_gguf *,
    const yvex_tensor_table *, yvex_backend_kind, const yvex_component_text_request *,
    yvex_runtime_av_conditioning_result *, yvex_error *);
/* Cold linkage retains the component session and immutable physical program
 * until close. Source-name callbacks are consumed only by open, never run.
 * Backend operand compatibility is verified separately by stage admission. */
int yvex_component_program_binding_open(yvex_component_program_binding **,
    const yvex_component_execution *, const yvex_program_physical *,
    yvex_component_parameter_name_fn, void *, yvex_error *);
void yvex_component_program_binding_close(yvex_component_program_binding **);
int yvex_component_joint_program_execute(
    const yvex_component_program_binding *, const yvex_joint_program *, const yvex_transformer_joint_request *,
    yvex_transformer_joint_result *, yvex_error *);

#ifdef __cplusplus
}
#endif
#endif
