/*
 * yvex_model_target_sidecar_write.c - model-target sidecar writer boundary.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   explicit local sidecar writer module status and future sidecar writer
 *   entry points.
 *
 * Does not own:
 *   CLI operator streams, command dispatch, rendering, runtime execution,
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   sidecar writer APIs write explicit local files only and never process
 *   operator streams.
 *
 * Boundary:
 *   sidecar writer availability does not create artifact emission capability,
 *   runtime support, generation support, benchmark evidence, or release
 *   readiness.
 */
#include "yvex_model_target_sidecar_write.h"

static int model_target_sidecar_writer_file_scope(void)
{
    return 1;
}

static int model_target_sidecar_writer_operator_scope(void)
{
    return 0;
}

static int model_target_sidecar_writer_boundary_ok(void)
{
    return model_target_sidecar_writer_file_scope() &&
           !model_target_sidecar_writer_operator_scope();
}

/*
 * yvex_model_target_sidecar_writer_available()
 *
 * Purpose:
 *   expose whether the sidecar writer module is present as an explicit file-IO
 *   owner.
 *
 * Inputs:
 *   none.
 *
 * Effects:
 *   none; this status helper does not open files or write output.
 *
 * Failure:
 *   none.
 *
 * Boundary:
 *   module availability is an ownership fact only. It does not emit artifacts,
 *   write operator output, execute runtime paths, generate, evaluate,
 *   benchmark, or mark release readiness.
 */
int yvex_model_target_sidecar_writer_available(void)
{
    return model_target_sidecar_writer_boundary_ok();
}
