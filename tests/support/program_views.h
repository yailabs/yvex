/* Shared compiler contract controls, executed by CPU and CUDA stage consumers. */
#ifndef TESTS_SUPPORT_PROGRAM_VIEWS_H
#define TESTS_SUPPORT_PROGRAM_VIEWS_H

#include "tests/test.h"
#include <yvex/internal/program_physical.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int test_program_input_views(yvex_program_physical **owner, unsigned long long rows,
    size_t count, unsigned long long bytes)
{
    yvex_program_physical *p = NULL, *again = NULL, *decoded = NULL, *refused = NULL;
    yvex_program_view_facts facts, repeated;
    yvex_core_bytes wire = {.maximum = 1048576u};
    yvex_error err = {0};
    const yvex_program_physical_summary *before = yvex_program_physical_summary_get(*owner);
    int rc = yvex_program_physical_views_compile(&p, *owner, rows, &facts, &err);
    if (rc != YVEX_OK) fprintf(stderr, "view legalization: %s: %s\n",
        yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && facts.view_count == count && facts.copy_bytes == bytes,
        "input-backed legalization derives exact eliminated population from the program");
    const yvex_program_physical_summary *after = yvex_program_physical_summary_get(p);
    YVEX_TEST_ASSERT(strcmp(before->identity, after->identity) &&
        !strcmp(before->semantic_identity, after->semantic_identity) &&
        !strcmp(before->execution_identity, after->execution_identity) &&
        !strcmp(before->parameter_identity, after->parameter_identity) &&
        before->step_count == after->step_count && before->storage_count > after->storage_count,
        "physical realization changes, semantic graph/parameters/effects remain exact and storage falls");
    YVEX_TEST_ASSERT(yvex_program_physical_views_compile(&again, p, rows, &repeated, &err) == YVEX_OK &&
        !strcmp(after->identity, yvex_program_physical_summary_get(again)->identity) &&
        repeated.view_count == count && repeated.copy_bytes == bytes,
        "view legalization is deterministic and idempotent");
    YVEX_TEST_ASSERT(yvex_program_physical_encode(p, &wire, &err) == YVEX_OK &&
        yvex_program_physical_decode(&decoded, wire.data, wire.count, &err) == YVEX_OK &&
        !strcmp(after->identity, yvex_program_physical_summary_get(decoded)->identity),
        "view contract and zero-slot decisions survive authenticated import");
    size_t negatives = 0u;
    for (size_t i = 0u; i < after->step_count; ++i) {
        const yvex_program_physical_step *s = yvex_program_physical_step_at(p, i);
        const char *view = !strcmp(s->implementation, "reshape.f32-storage.v1") ? "reshape.input_view.v1" :
            !strcmp(s->implementation, "cast.exact.f32.v1") ? "cast.input_view.f32.v1" : NULL;
        if (!view) continue;
        yvex_program_target_choice choice = {i, view};
        YVEX_TEST_ASSERT(yvex_program_physical_target_compile(&refused, p, &choice, 1u, &err) != YVEX_OK &&
            !refused, "computed/returned values cannot borrow an input lifetime by caller assertion");
        negatives++;
    }
    YVEX_TEST_ASSERT(negatives &&
        yvex_program_physical_views_compile(&refused, p, 0u, &repeated, &err) != YVEX_OK && !refused &&
        !repeated.copy_bytes && !repeated.view_count, "invalid population refuses without partial facts");
    printf("Input views: count=%zu copy_bytes=%llu storage_slots=%zu->%zu negatives=%zu; "
        "semantic/execution/parameter identity unchanged\n", count, bytes,
        before->storage_count, after->storage_count, negatives);
    yvex_program_physical_close(&again);
    yvex_program_physical_close(&decoded);
    yvex_program_physical_close(owner);
    free(wire.data);
    *owner = p;
    return 0;
}

#endif
