/* Immutable physical SSA work. Import/verification is a cold trust boundary;
 * invocation consumes slots and admitted implementation contracts, not model topology. */
#ifndef INCLUDE_YVEX_INTERNAL_PROGRAM_PHYSICAL_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_PROGRAM_PHYSICAL_H_INCLUDED

#include <yvex/internal/program.h>
#include <yvex/internal/sequence_mixer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_PROGRAM_OPERAND_CAP 16u
#define YVEX_PROGRAM_RESULT_CAP 4u
#define YVEX_PROGRAM_ATTRIBUTE_CAP 12u

typedef struct yvex_program_physical yvex_program_physical;

typedef struct {
    yvex_ir_id semantic_value;
    unsigned long long tensor_id;
    unsigned int qtype;
} yvex_program_parameter_binding;

typedef struct {
    /* Only symbol 0 (the admitted row population) or NONE survives lowering.
     * BF16 activations publish BF16 values in F32 execution storage. Parameter
     * qtypes describe encoded artifact storage, never activation semantics. */
    yvex_ir_type type;
    yvex_ir_id definition, last_use, storage, state_root;
    unsigned long long tensor_id;
    unsigned int qtype;
    int parameter;
} yvex_program_physical_value;

typedef struct {
    const char *implementation;
    yvex_ir_id operands[YVEX_PROGRAM_OPERAND_CAP];
    yvex_ir_id results[YVEX_PROGRAM_RESULT_CAP];
    yvex_ir_attribute attributes[YVEX_PROGRAM_ATTRIBUTE_CAP];
    unsigned int operand_count, result_count, attribute_count, effects;
} yvex_program_physical_step;

typedef struct {
    char entry[YVEX_IR_NAME_CAP];
    char semantic_identity[YVEX_SHA256_HEX_BYTES];
    char execution_identity[YVEX_SHA256_HEX_BYTES];
    char parameter_identity[YVEX_SHA256_HEX_BYTES];
    char identity[YVEX_SHA256_HEX_BYTES];
    /* Dynamic leading extent, or exactly 1/1/1 for fixed-shape invocation.
     * Instruction populations remain their own verified tensor extents. */
    unsigned long long minimum_rows, maximum_rows, row_multiple;
    size_t input_count, value_count, step_count, result_count, storage_count;
} yvex_program_physical_summary;

/* Binding ordinals have already been joined to exact artifact parameters by
 * compilation. Every used constant must resolve once; no name/role lookup is
 * deferred to invocation. No allocation, payload read or backend call occurs. */
int yvex_program_physical_compile(yvex_program_physical **,
    const yvex_program_execution *, const char *entry,
    const yvex_program_parameter_binding *, size_t binding_count,
    const char *parameter_identity, yvex_error *);
typedef struct {
    size_t step;
    const char *implementation;
} yvex_program_target_choice;
/* Cold, immutable target specialization. Only an admitted implementation of
 * the same operation/precision may replace a step. Semantic/execution lineage
 * is retained, physical identity changes; the source object is never mutated. */
int yvex_program_physical_target_compile(yvex_program_physical **,
    const yvex_program_physical *, const yvex_program_target_choice *, size_t, yvex_error *);
int yvex_program_physical_encode(const yvex_program_physical *, yvex_core_bytes *, yvex_error *);
int yvex_program_physical_decode(yvex_program_physical **, const unsigned char *, size_t, yvex_error *);
/* Cold admission joins the sealed work to its exact package physical records. */
int yvex_program_physical_parameters_validate(const yvex_program_physical *,
    const struct yvex_physical_execution_ir *, yvex_error *);
/* Cold contiguous package view: only insertion/removal of singleton axes is
 * representationally neutral here. No transpose, factorization, qtype change
 * or block reinterpretation is inferred from equal element counts. */
int yvex_program_physical_parameter_view(const yvex_program_physical_value *,
    unsigned int rank, const unsigned long long *dims, unsigned int qtype,
    unsigned long long encoded_bytes, unsigned long long *rows, unsigned long long *width, yvex_error *);
/* Source-order compatibility packages preserve three leading axes and fold
 * the contiguous tail into container axis four. This explicit import profile
 * is scalar-only; ordinary parameter views never infer such factorization. */
int yvex_program_physical_source_parameter_view(const yvex_program_physical_value *,
    unsigned int rank, const unsigned long long *dims, unsigned int qtype,
    unsigned long long encoded_bytes, unsigned long long *rows, unsigned long long *width, yvex_error *);
/* Immutable program lifetime is shared by executable resource owners. This
 * does not extend encoded parameter, backend or transient result lifetimes. */
yvex_program_physical *yvex_program_physical_retain(const yvex_program_physical *, yvex_error *);
void yvex_program_physical_close(yvex_program_physical **);
const yvex_program_physical_summary *yvex_program_physical_summary_get(const yvex_program_physical *);
const yvex_program_physical_value *yvex_program_physical_value_at(const yvex_program_physical *, size_t);
const yvex_program_physical_step *yvex_program_physical_step_at(const yvex_program_physical *, size_t);
/* Invocation view of an admitted value, not a second serialized layout. The
 * compiler resolves its one population symbol and physical carrier. BF16/F32
 * activations use F32 storage; INDEX streams use exact host U32 storage.
 * Parameters and state handles are not activation allocations. This query
 * admits geometry only, never available memory, residency or a reservation. */
typedef struct {
    yvex_ir_scalar storage_scalar;
    unsigned int rank;
    unsigned long long dims[YVEX_IR_RANK_CAP], elements, bytes;
    int dynamic_rows;
} yvex_program_value_layout;
int yvex_program_physical_value_layout(const yvex_program_physical *, size_t value,
    unsigned long long entry_rows, yvex_program_value_layout *, yvex_error *);
/* Operation population derives from verified result geometry, not necessarily
 * the entrypoint population. There is no independent serialized copy. */
unsigned long long yvex_program_physical_step_population(const yvex_program_physical *, size_t,
    unsigned long long entry_rows);
yvex_ir_id yvex_program_physical_result_at(const yvex_program_physical *, size_t);
const yvex_ir_attribute *yvex_program_physical_attribute(const yvex_program_physical_step *, const char *);
/* A runner-facing projection of executable operands/results, not another model
 * topology. Only the admitted token/index -> hidden/state signature qualifies.
 * The token bound is the intersection of executable embedding vocabularies;
 * output width need not equal embedding width. Every state input must have one
 * produced successor in the result signature. Row capacity is not a model
 * context or a runtime resource reservation. The view is not serialized. */
typedef struct {
    unsigned long long vocabulary_size, hidden_width;
    size_t state_inputs, attention_operations, recurrent_operations;
} yvex_program_token_interface;
int yvex_program_physical_token_interface(const yvex_program_physical *,
    yvex_program_token_interface *, yvex_error *);
/* Derived operation/provider views. Numeric geometry comes only from verified
 * instruction attributes; provider handles are state input slots, not layers. */
const yvex_gated_delta_plan *yvex_program_physical_delta_at(const yvex_program_physical *, size_t step);
const yvex_selective_ssd_geometry *yvex_program_physical_ssd_at(
    const yvex_program_physical *, size_t step);
int yvex_program_physical_sequence_state(const yvex_program_physical *, yvex_sequence_state_plan *);

#ifdef __cplusplus
}
#endif
#endif
