/*
 * Owner: abi.accounts (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Local provider account boundary
 *
 * File: include/yvex/accounts.h
 * Layer: public runtime API
 *
 * Purpose:
 *   Defines the local non-secret provider account/status surface used by the
 *   operator CLI and source download lane.
 *
 * Owns:
 *   - provider canonicalization for external source providers
 *   - local provider CLI discovery
 *   - local auth preflight observations without token storage
 *   - provider command execution helper for source acquisition
 *
 * Does not own:
 *   - hosted credentials
 *   - YAI account state
 *   - MCP orchestration
 *   - model execution
 */
#ifndef YVEX_ACCOUNTS_H
#define YVEX_ACCOUNTS_H

#include <stddef.h>

#include <yvex/error.h>
#include <yvex/fs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_ACCOUNT_PROVIDER_CAP 32
#define YVEX_ACCOUNT_AUTH_CAP 32
#define YVEX_ACCOUNT_HINT_CAP 128
#define YVEX_ACCOUNT_SOURCE_CAP 64
#define YVEX_ACCOUNT_STATUS_CAP 64
#define YVEX_ACCOUNT_BLOCKER_CAP 128
#define YVEX_ACCOUNT_NEXT_CAP 160
#define YVEX_ACCOUNT_TOKEN_ENV_CAP 64
#define YVEX_ACCOUNT_LOGIN_METHOD_CAP 64
#define YVEX_ACCOUNT_ARG_CAP 192

typedef enum {
    YVEX_ACCOUNT_PROVIDER_UNKNOWN = 0,
    YVEX_ACCOUNT_PROVIDER_HUGGINGFACE,
    YVEX_ACCOUNT_PROVIDER_GITHUB
} yvex_account_provider;

typedef enum {
    YVEX_ACCOUNT_INTERACTIVE_AUTO = 0,
    YVEX_ACCOUNT_INTERACTIVE_ALWAYS,
    YVEX_ACCOUNT_INTERACTIVE_NEVER
} yvex_account_interactive_mode;

typedef struct {
    yvex_account_provider provider;
    const char *cli_override;
    const char *token_env_name;
} yvex_account_observe_options;

typedef struct {
    yvex_account_provider provider;
    const char *cli_override;
    const char *token_env_name;
    yvex_account_interactive_mode interactive;
    int required;
} yvex_account_ensure_options;

typedef struct {
    yvex_account_provider provider;
    char provider_name[YVEX_ACCOUNT_PROVIDER_CAP];
    char cli_path[YVEX_PATH_CAP];
    char cli_source[YVEX_ACCOUNT_SOURCE_CAP];
    char cli_status[YVEX_ACCOUNT_STATUS_CAP];
    char auth_state[YVEX_ACCOUNT_AUTH_CAP];
    char account_hint[YVEX_ACCOUNT_HINT_CAP];
    char credential_source[YVEX_ACCOUNT_SOURCE_CAP];
    char token_env_name[YVEX_ACCOUNT_TOKEN_ENV_CAP];
    char login_method[YVEX_ACCOUNT_LOGIN_METHOD_CAP];
    char status[YVEX_ACCOUNT_STATUS_CAP];
    char top_blocker[YVEX_ACCOUNT_BLOCKER_CAP];
    char next[YVEX_ACCOUNT_NEXT_CAP];
    char state_path[YVEX_PATH_CAP];
    char last_checked_at[32];
    int cli_present;
    int token_env_present;
    int token_value_redacted;
    int raw_token_stored_by_yvex;
    int command_exit_code;
} yvex_account_observation;

typedef struct {
    const char *args[YVEX_ACCOUNT_ARG_CAP];
    const char *stdout_path;
    const char *stderr_path;
} yvex_account_command_options;

int yvex_account_provider_from_name(const char *name, yvex_account_provider *out);
const char *yvex_account_provider_name(yvex_account_provider provider);
const char *yvex_account_default_token_env(yvex_account_provider provider);
int yvex_account_observe(const yvex_account_observe_options *options,
                         yvex_account_observation *out,
                         yvex_error *err);
int yvex_account_ensure(const yvex_account_ensure_options *options,
                        yvex_account_observation *out,
                        yvex_error *err);
int yvex_account_write_state(const yvex_account_observation *observations,
                             unsigned long count,
                             yvex_error *err);
int yvex_accounts_run_provider_command(const yvex_account_command_options *options,
                                       yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_ACCOUNTS_H */
