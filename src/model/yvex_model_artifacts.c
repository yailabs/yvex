/*
 * yvex_model_artifacts.c - model artifact compatibility compilation unit.
 *
 * Owner:
 *   src/model
 *
 * Owns:
 *   compatibility anchor for the historic model artifact compilation unit.
 *
 * Does not own:
 *   model registry storage, model reference resolution, gate algorithms,
 *   report construction, explicit file writing, CLI parsing, command dispatch,
 *   rendering, stdout/stderr, artifact emission, runtime generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   public model artifact behavior is implemented under src/model/artifacts;
 *   this file must stay small and must not regain command or rendering logic.
 *
 * Boundary:
 *   the compatibility compilation unit is not artifact emission, runtime
 *   support, generation readiness, benchmark evidence, or release readiness.
 */

int yvex_model_artifacts_translation_unit_anchor(void)
{
    return 0;
}
