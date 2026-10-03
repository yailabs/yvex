// Provider account UX projects the native observation/persistence/process owners.
use crate::{
    Output,
    ffi::{self, raw},
    presentation,
    registry::Invocation,
};
use replai::Alignment;
use serde_json::{Value, json};
use std::ffi::OsString;

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn authenticated(observation: &raw::yvex_account_observation) -> bool {
    matches!(
        ffi::text(&observation.auth_state).as_str(),
        "logged-in" | "env-token-present"
    )
}

fn persist(observations: &[raw::yvex_account_observation]) {
    // An optional local observation receipt must never pretend persistence succeeded.
    if let Err(error) = ffi::account_state(observations) {
        eprintln!("yvex: account observation not persisted: {error}");
    }
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let operation = invocation.operation.operation_id.as_str();
    if matches!(operation, "provider.list" | "provider.account.status") {
        let observations = [
            ffi::Account::open("huggingface", None, None)?.observe()?,
            ffi::Account::open("github", None, None)?.observe()?,
        ];
        if operation == "provider.account.status" {
            persist(&observations);
        }
        let schema = if operation == "provider.list" {
            "yvex.provider.list.v1"
        } else {
            "yvex.provider.status.v1"
        };
        let machine = json!({"schema": schema, "providers": observations.iter().map(projection).collect::<Vec<_>>()});
        return Ok(Output::standard(
            render(invocation, &observations, machine, width, styled)?,
            0,
        ));
    }
    let provider = invocation.positionals.first().ok_or("provider required")?;
    let account = ffi::Account::open(
        provider,
        invocation.value("--cli"),
        invocation.value("--token-env"),
    )?;
    let (observation, exit) = if operation == "provider.account.ensure" {
        let interactive = match invocation.value("--interactive").unwrap_or("auto") {
            "auto" => raw::yvex_account_interactive_mode_YVEX_ACCOUNT_INTERACTIVE_AUTO,
            "always" => raw::yvex_account_interactive_mode_YVEX_ACCOUNT_INTERACTIVE_ALWAYS,
            "never" => raw::yvex_account_interactive_mode_YVEX_ACCOUNT_INTERACTIVE_NEVER,
            _ => return Err("unadmitted interactive policy".into()),
        };
        let mut observed = account.ensure(interactive, invocation.has("--required"))?;
        let exit = if authenticated(&observed) {
            ffi::put_text(&mut observed.status, "account-ensure-pass")?;
            0
        } else {
            if matches!(
                ffi::text(&observed.status).as_str(),
                "" | "account-provider-blocked"
            ) {
                ffi::put_text(&mut observed.status, "account-ensure-blocked")?;
            }
            5
        };
        (observed, exit)
    } else {
        let mut observed = account.observe()?;
        let exit = match operation {
            "provider.account.whoami" => {
                let passed = authenticated(&observed);
                ffi::put_text(
                    &mut observed.status,
                    if passed {
                        "account-whoami-pass"
                    } else {
                        "account-whoami-blocked"
                    },
                )?;
                if passed {
                    persist(std::slice::from_ref(&observed));
                }
                if passed { 0 } else { 5 }
            }
            "provider.account.login" | "provider.account.logout" => {
                login_logout(invocation, &account, &mut observed)?
            }
            _ => return Err("unprojected provider account operation".into()),
        };
        (observed, exit)
    };
    // A violated native redaction invariant is refused, never masked in projection.
    if observation.raw_token_stored_by_yvex != 0 {
        return Err("provider observation violates the no-raw-token contract".into());
    }
    let surface = operation
        .rsplit('.')
        .next()
        .ok_or("account operation identity missing")?;
    let machine = json!({"schema": "yvex.provider.account.v1", "operation": surface,
        "provider": projection(&observation)});
    Ok(Output::standard(
        render(invocation, &[observation], machine, width, styled)?,
        exit,
    ))
}

fn login_logout(
    invocation: &Invocation<'_>,
    account: &ffi::Account,
    observed: &mut raw::yvex_account_observation,
) -> Result<u8> {
    let login = invocation.operation.operation_id == "provider.account.login";
    let action = if login { "login" } else { "logout" };
    if observed.cli_present == 0 {
        ffi::put_text(&mut observed.status, &format!("account-{action}-blocked"))?;
        return Ok(5);
    }
    let token = std::env::var_os(account.token_environment()).filter(|value| !value.is_empty());
    if login
        && account.provider == raw::yvex_account_provider_YVEX_ACCOUNT_PROVIDER_GITHUB
        && token.is_some()
    {
        ffi::put_text(&mut observed.auth_state, "env-token-present")?;
        ffi::put_text(&mut observed.credential_source, "environment")?;
        ffi::put_text(
            &mut observed.account_hint,
            &format!("env:{}", account.token_environment()),
        )?;
        ffi::put_text(&mut observed.login_method, "token-env")?;
        ffi::put_text(&mut observed.status, "account-login-pass")?;
        observed.token_value_redacted = 1;
        persist(std::slice::from_ref(observed));
        return Ok(0);
    }
    let mut arguments: Vec<OsString> = vec![
        ffi::text(&observed.cli_path).into(),
        "auth".into(),
        action.into(),
    ];
    if login {
        if account.provider == raw::yvex_account_provider_YVEX_ACCOUNT_PROVIDER_HUGGINGFACE {
            for flag in ["--force", "--add-to-git-credential", "--device"] {
                if invocation.has(flag) {
                    arguments.push(flag.into());
                }
            }
            if let Some(value) = &token {
                arguments.extend(["--token".into(), value.clone()]);
                observed.token_value_redacted = 1;
            }
        } else {
            for flag in ["--web", "--skip-ssh-key"] {
                if invocation.has(flag) {
                    arguments.push(flag.into());
                }
            }
            for flag in ["--hostname", "--git-protocol", "--scopes"] {
                if let Some(value) = invocation.value(flag) {
                    arguments.extend([flag.into(), value.into()]);
                }
            }
        }
        ffi::put_text(
            &mut observed.login_method,
            if token.is_some() {
                "token-env"
            } else {
                "provider-cli"
            },
        )?;
    }
    let exit = ffi::run_provider(&arguments, invocation.has("--json"))?;
    observed.command_exit_code = exit;
    ffi::put_text(
        &mut observed.status,
        &format!(
            "account-{action}-{}",
            if exit == 0 { "pass" } else { "fail" }
        ),
    )?;
    if exit != 0 {
        if login {
            ffi::put_text(&mut observed.top_blocker, "provider-login-failed")?;
            ffi::put_text(&mut observed.next, "retry provider login")?;
        }
        return Ok(1);
    }
    if login {
        let checked = account.observe();
        if let Ok(mut checked) = checked
            && authenticated(&checked)
        {
            ffi::put_text(&mut checked.status, "account-login-pass")?;
            persist(std::slice::from_ref(&checked));
            *observed = checked;
            return Ok(0);
        }
        ffi::put_text(&mut observed.auth_state, "unknown")?;
        persist(std::slice::from_ref(observed));
    }
    Ok(0)
}

fn projection(observation: &raw::yvex_account_observation) -> Value {
    json!({"provider": ffi::text(&observation.provider_name),
        "cli_path": ffi::text(&observation.cli_path), "cli_source": ffi::text(&observation.cli_source),
        "cli_status": ffi::text(&observation.cli_status), "auth_state": ffi::text(&observation.auth_state),
        "account": ffi::text(&observation.account_hint), "credential_source": ffi::text(&observation.credential_source),
        "token_env_name": ffi::text(&observation.token_env_name), "login_method": ffi::text(&observation.login_method),
        "status": ffi::text(&observation.status), "blocker": ffi::text(&observation.top_blocker),
        "next": ffi::text(&observation.next), "state_path": ffi::text(&observation.state_path),
        "last_checked_at": ffi::text(&observation.last_checked_at), "cli_present": observation.cli_present != 0,
        "token_env_present": observation.token_env_present != 0,
        "token_value_redacted": observation.token_value_redacted != 0,
        "raw_token_stored_by_yvex": false, "command_exit_code": observation.command_exit_code})
}

fn render(
    invocation: &Invocation<'_>,
    observations: &[raw::yvex_account_observation],
    machine: Value,
    width: usize,
    styled: bool,
) -> Result<String> {
    if invocation.has("--json") {
        return Ok(format!("{machine}\n"));
    }
    let rows = observations
        .iter()
        .map(|observed| {
            vec![
                ffi::text(&observed.provider_name),
                ffi::text(&observed.cli_status),
                ffi::text(&observed.auth_state),
                ffi::text(&observed.account_hint),
            ]
        })
        .collect::<Vec<_>>();
    let mut output = presentation::table(
        &[
            ("PROVIDER", Alignment::Left),
            ("CLI", Alignment::Left),
            ("AUTH", Alignment::Left),
            ("ACCOUNT", Alignment::Left),
        ],
        &rows,
        width,
        styled,
    )?;
    for observed in observations {
        if invocation.value("--output") == Some("audit") {
            let facts = projection(observed)
                .as_object()
                .unwrap()
                .iter()
                .map(|(name, value)| {
                    (
                        name.clone(),
                        value
                            .as_str()
                            .map(str::to_owned)
                            .unwrap_or_else(|| value.to_string()),
                    )
                })
                .collect::<Vec<_>>();
            let borrowed = facts
                .iter()
                .map(|(key, value)| (key.as_str(), value.as_str()))
                .collect::<Vec<_>>();
            output.push_str(&presentation::record(
                &ffi::text(&observed.provider_name),
                &borrowed,
                width,
                styled,
            )?);
        } else {
            let mut lines = Vec::new();
            if observed.top_blocker[0] != 0 {
                lines.push(format!(
                    "BLOCKED  {} · {}",
                    ffi::text(&observed.provider_name),
                    ffi::text(&observed.top_blocker)
                ));
            }
            if observed.next[0] != 0 {
                lines.push(format!("NEXT  {}", ffi::text(&observed.next)));
            }
            output.push_str(&presentation::lines(&lines, width, styled)?);
        }
    }
    output
        .push_str("Authentication is delegated to provider tooling; YVEX stores no raw tokens.\n");
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn provider_projection_is_typed_and_redacted_not_a_command_transcript() {
        let mut observation = raw::yvex_account_observation {
            token_value_redacted: 1,
            ..Default::default()
        };
        ffi::put_text(&mut observation.provider_name, "github").unwrap();
        ffi::put_text(&mut observation.auth_state, "env-token-present").unwrap();
        let json = projection(&observation);
        assert_eq!(json["raw_token_stored_by_yvex"], false);
        assert_eq!(json["token_value_redacted"], true);
        assert!(authenticated(&observation));
        assert_eq!(json.as_object().unwrap().len(), 19);
    }
}
