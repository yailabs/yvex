/*
 * yvex_prefill_cli.c - prefill CLI command adapter boundary.
 *
 * Owner:
 *   src/cli/commands
 *
 * Owns:
 *   command adapter file presence for CLI topology guards.
 *
 * Does not own:
 *   domain facts, output policy, runtime behavior, eval, benchmark, or capability claims.
 *
 * Invariants:
 *   command anchors do not change command behavior.
 *
 * Boundary:
 *   CLI topology anchor only; prefill behavior stays in the existing owner.
 */

void yvex_prefill_cli_boundary(void)
{
}
