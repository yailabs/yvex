/* Exact bounded multiple-choice allocation. No quality or timing is inferred
 * from source retention. This algorithm is independent of model architecture. */
#include <yvex/optimization.h>
#include <yvex/internal/core.h>
#include <stdlib.h>
#include <string.h>

static int allocation_refuse(yvex_error *err, yvex_status code, const char *message)
{
    yvex_error_set(err, code, "compiler.allocation", message);
    return code;
}

static int allocation_order(const void *left, const void *right)
{
    const yvex_optimization_allocation *a = left, *b = right;
    if (a->encoded_bytes != b->encoded_bytes)
        return a->encoded_bytes < b->encoded_bytes ? -1 : 1;
    if (a->source_elements != b->source_elements)
        return a->source_elements > b->source_elements ? -1 : 1;
    return a->choices < b->choices ? -1 : a->choices > b->choices;
}

int yvex_optimization_allocate(const yvex_optimization_allocation_request *r,
    yvex_optimization_allocation *out, unsigned int capacity, unsigned int *count,
    yvex_error *err)
{
    yvex_optimization_allocation *current = NULL, *next = NULL;
    unsigned long long remaining[YVEX_OPTIMIZATION_MAX_GROUPS + 1u] = {0};
    unsigned int population = 1u;
    int rc = YVEX_OK;
    if (!r || r->schema_version != YVEX_OPTIMIZATION_ALLOCATION_SCHEMA_V1 ||
        !r->groups || !r->group_count || r->group_count > YVEX_OPTIMIZATION_MAX_GROUPS ||
        !r->maximum_states || r->maximum_states > 65536u || !r->maximum_encoded_bytes ||
        !out || !capacity || !count)
        return allocation_refuse(err, YVEX_ERR_INVALID_ARG, "bounded allocation request required");
    for (unsigned int i = r->group_count; i; --i) {
        const yvex_optimization_allocation_group *g = &r->groups[i - 1u];
        unsigned long long least = g->encoded_bytes[0] < g->encoded_bytes[1] ?
            g->encoded_bytes[0] : g->encoded_bytes[1];
        if (!least || !yvex_core_u64_add(remaining[i], least, &remaining[i - 1u]))
            return allocation_refuse(err, YVEX_ERR_BOUNDS, "invalid group size or allocation overflow");
    }
    unsigned long long minimum;
    if (!yvex_core_u64_add(r->fixed_bytes, remaining[0], &minimum))
        return allocation_refuse(err, YVEX_ERR_BOUNDS, "fixed allocation overflow");
    if (minimum > r->maximum_encoded_bytes)
        return allocation_refuse(err, YVEX_ERR_NOMEM, "no encoding satisfies the declared weight budget");
    current = calloc(r->maximum_states, sizeof(*current));
    next = calloc((size_t)r->maximum_states * 2u, sizeof(*next));
    if (!current || !next) {
        rc = allocation_refuse(err, YVEX_ERR_NOMEM, "allocation frontier storage unavailable");
        goto done;
    }
    current[0].encoded_bytes = r->fixed_bytes;
    for (unsigned int i = 0u; i < r->group_count; ++i) {
        unsigned int expanded = 0u, kept = 0u;
        for (unsigned int j = 0u; j < population; ++j) {
            for (unsigned int option = 0u; option < 2u; ++option) {
                yvex_optimization_allocation row = current[j];
                unsigned long long complete_minimum;
                if (!yvex_core_u64_add(row.encoded_bytes, r->groups[i].encoded_bytes[option],
                                      &row.encoded_bytes) ||
                    !yvex_core_u64_add(row.source_elements, r->groups[i].source_elements[option],
                                      &row.source_elements) ||
                    !yvex_core_u64_add(row.encoded_bytes, remaining[i + 1u], &complete_minimum)) {
                    rc = allocation_refuse(err, YVEX_ERR_BOUNDS, "candidate allocation overflow");
                    goto done;
                }
                if (complete_minimum > r->maximum_encoded_bytes) continue;
                if (option) row.choices |= 1ull << i;
                next[expanded++] = row;
            }
        }
        qsort(next, expanded, sizeof(*next), allocation_order);
        for (unsigned int j = 0u; j < expanded; ++j) {
            /* With ascending cost, a non-increasing retention is dominated.
             * This is safe because remaining independent group choices have
             * identical costs regardless of the preceding partial assignment. */
            if (kept && next[j].source_elements <= current[kept - 1u].source_elements) continue;
            if (kept == r->maximum_states) {
                rc = allocation_refuse(err, YVEX_ERR_BOUNDS,
                    "exact allocation frontier exceeds search budget; increase maximum states");
                goto done;
            }
            current[kept++] = next[j];
        }
        population = kept;
    }
    if (population > capacity) {
        rc = allocation_refuse(err, YVEX_ERR_BOUNDS, "output capacity cannot hold exact allocation frontier");
        goto done;
    }
    memcpy(out, current, population * sizeof(*out));
    *count = population;
    yvex_error_clear(err);
done:
    free(next);
    free(current);
    return rc;
}
