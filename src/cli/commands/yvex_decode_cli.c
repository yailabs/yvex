/*
 * decode CLI adapter slot.
 *
 * Owner: src/cli/commands.
 * Owns: command-file presence for CLI topology and future argv/help adapters.
 * Does not own: domain structs, domain APIs, backend opens, runtime state, or rendering.
 * Invariants: this file must not hide domain implementation under src/cli.
 * Boundary: topology adapter slot; command symbols remain with their restored owner until a later real extraction moves behavior and tests together.
 *
 * Purpose: keep the command adapter slot explicit after domain ownership restoration.
 * Inputs: none.
 * Effects: none.
 * Failure: none.
 */

void decode_cli_adapter_file(void)
{
}
