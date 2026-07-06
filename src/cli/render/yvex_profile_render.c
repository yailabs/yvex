/*
 * yvex_profile_render.c - profile CLI renderer boundary.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   renderer file presence for CLI topology guards.
 *
 * Does not own:
 *   domain facts, command parsing, output policy, runtime behavior, or capability claims.
 *
 * Invariants:
 *   renderer anchors do not change command behavior.
 *
 * Boundary:
 *   renderer topology anchor only; behavior is preserved by command quarantine.
 */

void yvex_profile_render_boundary(void)
{
}
