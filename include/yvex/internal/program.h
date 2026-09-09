/* Compiler joins between semantic values and independently sealed physical truth. */
#ifndef INCLUDE_YVEX_INTERNAL_PROGRAM_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_PROGRAM_H_INCLUDED

#include <yvex/internal/ir.h>

#ifdef __cplusplus
extern "C" {
#endif

struct yvex_transform_ir;
struct yvex_physical_execution_ir;
typedef struct yvex_program_parameters yvex_program_parameters;
typedef struct yvex_program_execution yvex_program_execution;
typedef struct yvex_program_tensor_plan yvex_program_tensor_plan;

/* Canonical, straight-line execution form. IDs here are entry-local value/step
 * slots, not semantic module IDs or device handles. Region/call legalization
 * must precede this form; unsupported control flow fails closed. */
typedef struct {
    yvex_ir_id semantic_value, definition, last_use;
} yvex_program_value;

typedef struct {
    yvex_ir_id semantic_operation;
    const yvex_ir_id *operands, *results, *dependencies;
    size_t operand_count, result_count, dependency_count;
} yvex_program_step;

typedef struct {
    const char *symbol;
    const yvex_program_value *values;
    const yvex_program_step *steps;
    const yvex_ir_id *results;
    size_t input_count, value_count, step_count, result_count;
} yvex_program_entry;

/* Serial baseline lowering preserves data dependencies and the declared effect
 * order. It proves value lifetimes, not an asynchronous or target schedule.
 * Parameters remain symbolic constants; state slots are semantic versions and
 * never imply fresh physical allocations. */
int yvex_program_execution_compile(yvex_program_execution **, const yvex_ir_module *, yvex_error *);
void yvex_program_execution_close(yvex_program_execution **);
const yvex_ir_module *yvex_program_execution_module(const yvex_program_execution *);
const char *yvex_program_execution_identity(const yvex_program_execution *);
size_t yvex_program_execution_entry_count(const yvex_program_execution *);
const yvex_program_entry *yvex_program_execution_entry_at(const yvex_program_execution *, size_t);
int yvex_program_execution_print(const yvex_program_execution *, yvex_core_bytes *, yvex_error *);

/* Bounded physical legalization of pure rank-2 BF16 tensor programs. Values
 * publish BF16 numerics in F32 activation storage; parameters remain encoded
 * BF16 rows. Other types/effects require another explicit legalization. */
typedef struct {
    unsigned long long rows, width;
    int parameter; /* Static encoded input, never a computed tensor. */
} yvex_program_tensor_value;

typedef struct {
    const char *implementation;
    yvex_ir_id operands[2], result;
} yvex_program_tensor_step;

typedef struct {
    char entry[YVEX_IR_NAME_CAP];
    char semantic_identity[YVEX_SHA256_HEX_BYTES];
    char execution_identity[YVEX_SHA256_HEX_BYTES];
    char identity[YVEX_SHA256_HEX_BYTES];
    unsigned long long minimum_rows, maximum_rows, row_multiple;
    size_t input_count, value_count, step_count, result_count;
} yvex_program_tensor_summary;

int yvex_program_tensor_compile(yvex_program_tensor_plan **, const yvex_program_execution *,
                                const char *entry, yvex_error *);
int yvex_program_tensor_encode(const yvex_program_tensor_plan *, yvex_core_bytes *, yvex_error *);
int yvex_program_tensor_decode(yvex_program_tensor_plan **, const unsigned char *, size_t, yvex_error *);
const yvex_program_tensor_summary *yvex_program_tensor_summary_get(const yvex_program_tensor_plan *);
const yvex_program_tensor_value *yvex_program_tensor_value_at(const yvex_program_tensor_plan *, size_t);
const yvex_program_tensor_step *yvex_program_tensor_step_at(const yvex_program_tensor_plan *, size_t);
yvex_ir_id yvex_program_tensor_result_at(const yvex_program_tensor_plan *, size_t);
void yvex_program_tensor_close(yvex_program_tensor_plan **);

typedef struct {
    yvex_ir_id value;
    unsigned long long terminal;
} yvex_program_parameter;

/* Compile exact parameter references through Transformation IR to Physical IR.
 * A source symbol must have one shape/type-preserving identity realization.
 * Other transformations require explicit computation legalization, not guessed
 * aliasing. Payload bytes, device allocation and target selection are absent.
 * The result retains its immutable program and owns compact terminal handles;
 * source and physical compiler objects may close after successful construction. */
int yvex_program_parameters_compile(yvex_program_parameters **,
    const yvex_ir_module *, const struct yvex_transform_ir *,
    const struct yvex_physical_execution_ir *, yvex_error *);
void yvex_program_parameters_close(yvex_program_parameters **);
const yvex_ir_module *yvex_program_parameters_module(const yvex_program_parameters *);
const char *yvex_program_parameters_identity(const yvex_program_parameters *);
size_t yvex_program_parameters_count(const yvex_program_parameters *);
const yvex_program_parameter *yvex_program_parameter_at(const yvex_program_parameters *, size_t);

#ifdef __cplusplus
}
#endif
#endif
