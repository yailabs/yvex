/* Native typed computational programs. No payload, backend or runtime ownership. */
#ifndef INCLUDE_YVEX_INTERNAL_IR_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_IR_H_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include <yvex/core.h>
#include <yvex/internal/core.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t yvex_ir_id;
#define YVEX_IR_NONE UINT32_MAX
#define YVEX_IR_NAME_CAP 96u
#define YVEX_IR_RANK_CAP 8u
#define YVEX_IR_AGGREGATE_CAP 16u

typedef struct yvex_ir_module yvex_ir_module;

/* Symbols are positive admitted dimensions, not anonymous dynamic question marks.
 * Their identity is shared by every type/constraint that uses that dimension. */
typedef struct {
    char name[YVEX_IR_NAME_CAP];
    uint64_t minimum, maximum, multiple;
} yvex_ir_dimension;

typedef struct {
    yvex_ir_id symbol; /* NONE means a static extent. */
    uint64_t extent;
} yvex_ir_extent;

typedef enum {
    YVEX_IR_BOOL = 1, YVEX_IR_INDEX, YVEX_IR_I32, YVEX_IR_I64,
    YVEX_IR_F16, YVEX_IR_BF16, YVEX_IR_F32, YVEX_IR_F64
} yvex_ir_scalar;

typedef enum {
    YVEX_IR_SCALAR = 1, YVEX_IR_TENSOR, YVEX_IR_STATE,
    YVEX_IR_TUPLE, YVEX_IR_PROGRAM
} yvex_ir_type_kind;

typedef struct {
    yvex_ir_type_kind kind;
    yvex_ir_scalar scalar;
    uint32_t rank;
    yvex_ir_extent shape[YVEX_IR_RANK_CAP];
    /* State domain names semantic geometry; it is not a physical buffer kind. */
    char domain[YVEX_IR_NAME_CAP];
    uint32_t member_count;
    yvex_ir_id members[YVEX_IR_AGGREGATE_CAP];
} yvex_ir_type;

typedef enum {
    YVEX_IR_ATTR_U64 = 1, YVEX_IR_ATTR_F64, YVEX_IR_ATTR_BOOL,
    YVEX_IR_ATTR_TEXT, YVEX_IR_ATTR_TYPE, YVEX_IR_ATTR_SYMBOL
} yvex_ir_attribute_kind;

typedef struct {
    char name[YVEX_IR_NAME_CAP];
    yvex_ir_attribute_kind kind;
    union {
        uint64_t integer;
        double real;
        yvex_ir_id type;
        char text[YVEX_IR_NAME_CAP];
    } value;
} yvex_ir_attribute;

typedef enum {
    YVEX_IR_PURE = 0,
    YVEX_IR_READ_STATE = 1u << 0,
    YVEX_IR_WRITE_STATE = 1u << 1,
    YVEX_IR_RNG = 1u << 2,
    YVEX_IR_PUBLISH = 1u << 3,
    YVEX_IR_ORDERED = 1u << 4
} yvex_ir_effect;

typedef struct {
    const char *name;
    yvex_ir_attribute_kind kind;
    int required;
} yvex_ir_attribute_rule;

/* Definitions are immutable, statically owned dialect contracts. Neither an
 * imported module nor an artifact can supply callbacks or redefine semantics. */
typedef struct {
    const char *name;
    uint32_t version;
    uint32_t minimum_operands, maximum_operands;
    uint32_t minimum_results, maximum_results;
    uint32_t regions, effects;
    const yvex_ir_attribute_rule *attributes;
    size_t attribute_count;
    int terminator;
    int (*verify)(const yvex_ir_module *, yvex_ir_id, yvex_error *);
} yvex_ir_operation_definition;

typedef struct {
    const yvex_ir_operation_definition *operations;
    size_t count;
} yvex_ir_dialect;

typedef struct {
    yvex_ir_id type, block, definition;
    uint32_t ordinal; /* argument ordinal when definition is NONE; otherwise result. */
} yvex_ir_value;

typedef struct {
    yvex_ir_id block, next;
    const yvex_ir_operation_definition *definition;
    const yvex_ir_id *operands, *results, *regions;
    uint32_t operand_count, result_count, region_count;
    const yvex_ir_attribute *attributes;
    uint32_t attribute_count;
} yvex_ir_operation;

typedef struct {
    yvex_ir_id function, parent_operation;
    yvex_ir_id first_operation, last_operation;
    const yvex_ir_id *arguments;
    uint32_t argument_count;
} yvex_ir_block;

typedef struct {
    char symbol[YVEX_IR_NAME_CAP];
    yvex_ir_id body;
    const yvex_ir_id *results;
    uint32_t result_count, effects;
} yvex_ir_function;

typedef struct {
    const char *operation;
    const yvex_ir_id *operands, *result_types;
    size_t operand_count, result_count;
    const yvex_ir_attribute *attributes;
    size_t attribute_count;
} yvex_ir_operation_request;

/* Construction owns copies of all request data. Mutation is refused after seal.
 * Views may relocate on construction; IDs remain valid. Sealed views last until
 * module close. IDs are not runtime resource/publication generations. */
int yvex_ir_module_open(yvex_ir_module **out, const char *name,
                        const char *source_identity,
                        const yvex_ir_dialect *dialects, size_t dialect_count,
                        yvex_error *err);
void yvex_ir_module_close(yvex_ir_module **module);
/* Caller must hold a live reference. Only sealed modules can cross lifetimes. */
yvex_ir_module *yvex_ir_module_retain(const yvex_ir_module *, yvex_error *);
int yvex_ir_dimension_add(yvex_ir_module *, const yvex_ir_dimension *,
                          yvex_ir_id *, yvex_error *);
int yvex_ir_type_intern(yvex_ir_module *, const yvex_ir_type *,
                        yvex_ir_id *, yvex_error *);
int yvex_ir_function_add(yvex_ir_module *, const char *symbol,
                         const yvex_ir_id *arguments, size_t argument_count,
                         const yvex_ir_id *results, size_t result_count,
                         uint32_t effects, yvex_ir_id *, yvex_error *);
int yvex_ir_operation_add(yvex_ir_module *, yvex_ir_id block,
                          const yvex_ir_operation_request *, yvex_ir_id *,
                          yvex_error *);
int yvex_ir_region_add(yvex_ir_module *, yvex_ir_id operation,
                       const yvex_ir_id *arguments, size_t argument_count,
                       yvex_ir_id *, yvex_error *);

const yvex_ir_dimension *yvex_ir_dimension_at(const yvex_ir_module *, yvex_ir_id);
const yvex_ir_type *yvex_ir_type_at(const yvex_ir_module *, yvex_ir_id);
const yvex_ir_value *yvex_ir_value_at(const yvex_ir_module *, yvex_ir_id);
const yvex_ir_operation *yvex_ir_operation_at(const yvex_ir_module *, yvex_ir_id);
const yvex_ir_block *yvex_ir_block_at(const yvex_ir_module *, yvex_ir_id);
const yvex_ir_function *yvex_ir_function_at(const yvex_ir_module *, yvex_ir_id);
const yvex_ir_attribute *yvex_ir_attribute_get(
    const yvex_ir_module *, yvex_ir_id operation, const char *name);
size_t yvex_ir_function_count(const yvex_ir_module *);
size_t yvex_ir_operation_count(const yvex_ir_module *);
int yvex_ir_function_find(const yvex_ir_module *, const char *, yvex_ir_id *);
int yvex_ir_type_equal(const yvex_ir_type *, const yvex_ir_type *);
int yvex_ir_extent_equal(yvex_ir_extent, yvex_ir_extent);
uint32_t yvex_ir_operation_effects(const yvex_ir_module *, yvex_ir_id);
int yvex_ir_verify(const yvex_ir_module *, yvex_error *);
int yvex_ir_seal(yvex_ir_module *, yvex_error *);
const char *yvex_ir_identity(const yvex_ir_module *);
const char *yvex_ir_source_identity(const yvex_ir_module *);
int yvex_ir_print(const yvex_ir_module *, yvex_core_bytes *, yvex_error *);
int yvex_ir_encode(const yvex_ir_module *, yvex_core_bytes *, yvex_error *);
int yvex_ir_decode(yvex_ir_module **, const unsigned char *, size_t,
                    const yvex_ir_dialect *, size_t, yvex_error *);
const yvex_ir_dialect *yvex_ir_core_dialect(void);
const yvex_ir_dialect *yvex_ir_neural_dialect(void);
const yvex_ir_dialect *yvex_ir_sequence_dialect(void);

/* A pass consumes an immutable module and publishes a distinct verified module.
 * Failure never mutates its input or exposes a partially rewritten program. */
typedef struct {
    const char *name;
    uint32_t version;
    int (*run)(const yvex_ir_module *, yvex_ir_module **, yvex_error *);
} yvex_ir_pass;

typedef struct {
    char pass[YVEX_IR_NAME_CAP];
    uint32_t version;
    char input_identity[YVEX_SHA256_HEX_BYTES];
    char output_identity[YVEX_SHA256_HEX_BYTES];
    size_t input_operations, output_operations;
} yvex_ir_pass_evidence;

int yvex_ir_pass_pipeline(const yvex_ir_module *, const yvex_ir_pass *, size_t,
                           yvex_ir_module **, yvex_ir_pass_evidence *, yvex_error *);
const yvex_ir_pass *yvex_ir_canonical_pass(void);
const yvex_ir_pass *yvex_ir_dead_code_pass(void);
const yvex_ir_pass *yvex_ir_inline_pass(void);

#ifdef __cplusplus
}
#endif
#endif
