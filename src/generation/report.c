/* Owner: src/generation
 * Owns: small report helper APIs for diagnostic generation facts.
 * Does not own: CLI parsing, command dispatch, rendering, graph execution, provider generation, eval, benchmark, or
 *   release decisions.
 * Invariants: helper names preserve the lowest true diagnostic stage and never imply full-model generation
 *   readiness.
 * Boundary: report helper APIs are not runtime generation support.
 * Purpose: Provide canonical names for typed diagnostic generation report enums.
 * Inputs: A typed report enum value.
 * Effects: Does not mutate state.
 * Failure: Unknown values return the canonical unknown label. */
#include <yvex/internal/generation.h>

#include <string.h>

typedef struct {
    yvex_status status;
    int kv;
    int ownership;
    int sampling;
} generation_exit_row;

static const generation_exit_row generation_exit_rows[] = {
    {YVEX_OK, 1, 1, 0},
    {YVEX_ERR_INVALID_ARG, 2, 2, 2},
    {YVEX_ERR_IO, 3, 3, 3},
    {YVEX_ERR_NOMEM, 1, 4, 3},
    {YVEX_ERR_FORMAT, 4, 4, 4},
    {YVEX_ERR_BOUNDS, 4, 4, 4},
    {YVEX_ERR_UNSUPPORTED, 5, 5, 5},
};

static const char *const generation_trace_names[] = {
    "none", "tokens", "steps", "kv", "logits", "sampling", "full",
};

/* Purpose: publish one exact typed generation refusal and return its status.
 * Inputs: optional error storage plus the stable status, owner, and message.
 * Effects: replaces only caller error state.
 * Failure: the supplied status is returned even when error storage is absent.
 * Boundary: common generation failure projection; it owns no lifecycle cleanup. */
int yvex_generation_refuse(yvex_error *err,
                           yvex_status status,
                           const char *where,
                           const char *message)
{
    yvex_error_set(err, status, where, message);
    return status;
}

/* Purpose: activate one named generation fault seam without duplicating refusal code.
 * Inputs: test flag and the exact refusal facts for its historical checkpoint.
 * Effects: reads the process-local test seam and may replace caller error state.
 * Failure: returns the requested status only while the seam is enabled.
 * Boundary: internal fault injection only; production behavior is otherwise inert. */
int yvex_generation_test_refuse(const char *flag,
                                yvex_error *err,
                                yvex_status status,
                                const char *where,
                                const char *message)
{
    return yvex_core_test_flag(flag)
               ? yvex_generation_refuse(err, status, where, message)
               : YVEX_OK;
}

/* Purpose: map one typed status through the report surface's historical exit policy.
 * Inputs: status and explicit KV, ownership, or sampling policy.
 * Effects: none.
 * Failure: unknown statuses map to the stable generic failure exit code.
 * Boundary: CLI-facing report fact only; it does not classify domain capability. */
int yvex_generation_exit_code(yvex_status status,
                              yvex_generation_exit_policy policy)
{
    size_t index;

    for (index = 0; index < sizeof(generation_exit_rows) / sizeof(generation_exit_rows[0]);
         ++index) {
        const generation_exit_row *row = &generation_exit_rows[index];

        if (row->status == status) {
            if (policy == YVEX_GENERATION_EXIT_SAMPLING) return row->sampling;
            if (policy == YVEX_GENERATION_EXIT_KV_OWNERSHIP) return row->ownership;
            return row->kv;
        }
    }
    return 1;
}

/* Purpose: copy an admitted typed field projection without repeating report policy.
 * Inputs: source/destination objects and immutable checked offset/size rows.
 * Effects: copies only the named destination ranges in table order.
 * Failure: missing objects or tables perform no partial projection.
 * Boundary: internal by-value fact projection; rows never retain borrowed storage. */
void yvex_generation_project_fields(
    void *destination,
    const void *source,
    const yvex_generation_field_projection *fields,
    size_t count)
{
    size_t index;

    if (!destination || !source || !fields) return;
    for (index = 0; index < count; ++index) {
        memcpy((unsigned char *)destination + fields[index].destination_offset,
               (const unsigned char *)source + fields[index].source_offset,
               fields[index].size);
    }
}

/* Purpose: Return the canonical diagnostic label for trace level name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
const char *yvex_generation_trace_level_name(yvex_generation_trace_level level)
{
    return level >= YVEX_GENERATION_TRACE_NONE &&
                   (size_t)level < sizeof(generation_trace_names) /
                                       sizeof(generation_trace_names[0])
               ? generation_trace_names[level]
               : generation_trace_names[YVEX_GENERATION_TRACE_NONE];
}
