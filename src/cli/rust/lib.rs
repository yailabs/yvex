// The operator shell projects C-owned computational facts; it owns no engine state.
#![deny(unsafe_code)]
#![recursion_limit = "256"]

mod accounts;
mod acquire;
mod acquisition;
mod acquisition_report;
mod acquisition_worker;
mod artifact;
mod attention;
mod attention_projection;
mod backend;
pub mod catalog;
mod chat;
mod chat_stream;
pub mod client;
#[allow(unsafe_code)]
pub mod ffi;
mod generation;
mod generation_projection;
pub mod help;
pub mod host;
mod interaction;
mod management;
mod media;
mod paths;
mod pipeline;
mod pipeline_projection;
mod plumbing;
mod preparation;
mod quant;
mod source;
mod target;
mod tokenizer;
mod variant;
mod workflow;

pub struct Output {
    pub text: String,
    pub exit: u8,
    pub diagnostic: bool,
}

impl Output {
    pub(crate) fn standard(text: String, exit: u8) -> Self {
        Self {
            text,
            exit,
            diagnostic: false,
        }
    }
}

fn refused(reason: &str, hint: Option<&str>, width: usize, styled: bool, exit: u8) -> Output {
    let mut fields = vec![("reason", reason)];
    if let Some(hint) = hint {
        fields.push(("hint", hint));
    }
    Output {
        text: presentation::record("YVEX request refused", &fields, width, styled)
            .unwrap_or_else(|_| "YVEX request refused: invalid presentation input\n".into()),
        exit,
        diagnostic: true,
    }
}

fn refused_error(error: &(dyn std::error::Error + 'static), width: usize, styled: bool) -> Output {
    let (exit, hint) = if let Some(native) = error.downcast_ref::<ffi::Error>() {
        (native_exit(native.code), None)
    } else if let Some(grammar) = error.downcast_ref::<registry::Refusal>() {
        (2, grammar.hint.as_deref())
    } else {
        (1, None)
    };
    refused(&error.to_string(), hint, width, styled, exit)
}

fn refused_client(error: &ffi::Error, width: usize, styled: bool) -> Output {
    // Client transport/remote refusals historically exit 1; grammar still exits
    // 2. The typed native identity is retained independently of process status.
    refused(
        &error.to_string(),
        (error.code == ffi::raw::yvex_status_YVEX_ERR_IO)
            .then_some("inspect yvex host status; yvex serve starts a host"),
        width,
        styled,
        if error.code == ffi::raw::yvex_status_YVEX_ERR_INVALID_ARG {
            2
        } else {
            1
        },
    )
}

pub(crate) fn native_exit(code: i32) -> u8 {
    match code {
        0 => 0,
        -10 => 2,
        -3 => 3,
        -4 | -7 => 4,
        -5 => 5,
        _ => 1,
    }
}

// All command admission is derived from one generated grammar.
pub fn execute(words: &[String], width: usize, styled: bool) -> Output {
    let registry = match registry::Registry::embedded() {
        Ok(registry) => registry,
        Err(error) => return refused(&error.reason, None, width, styled, 1),
    };
    if words.is_empty() || words == ["--help"] || words == ["-h"] {
        return match help::render(&registry, &[], false, width, styled) {
            Ok(text) => Output::standard(text, 0),
            Err(error) => refused(&error.reason, None, width, styled, 2),
        };
    }
    // A real registry group wins over a shorter leaf's positional argument.
    // Otherwise `compile quant --help` is mistaken for `compile <target>`.
    if words
        .last()
        .is_some_and(|word| word == "--help" || word == "-h")
        && words[..words.len() - 1]
            .iter()
            .all(|word| !word.starts_with('-'))
        && let Ok(text) = help::render(&registry, &words[..words.len() - 1], false, width, styled)
    {
        return Output::standard(text, 0);
    }
    let invocation = match registry.parse(words) {
        Ok(invocation) => invocation,
        Err(error) => {
            // A group with no leaf operation still has registry-owned discovery.
            if words
                .last()
                .is_some_and(|word| word == "--help" || word == "-h")
                && let Ok(text) =
                    help::render(&registry, &words[..words.len() - 1], false, width, styled)
            {
                return Output::standard(text, 0);
            }
            return refused(&error.reason, error.hint.as_deref(), width, styled, 2);
        }
    };
    if invocation.has("--help") {
        return match help::leaf(invocation.operation, width, styled) {
            Ok(text) => Output::standard(text, 0),
            Err(error) => refused(&error.reason, None, width, styled, 2),
        };
    }
    dispatch(&invocation, &registry, width, styled)
}

fn dispatch(
    invocation: &registry::Invocation<'_>,
    registry: &registry::Registry,
    width: usize,
    styled: bool,
) -> Output {
    if let Some(result) = offline_projection(invocation, width, styled) {
        return result.unwrap_or_else(|error| refused_error(error.as_ref(), width, styled));
    }
    let json = invocation.has("--json");
    match invocation.operation.operation_id.as_str() {
        "command.discovery" => {
            if json {
                Output {
                    text: format!("{}\n", registry.discovery()),
                    exit: 0,
                    diagnostic: false,
                }
            } else {
                match help::render(
                    registry,
                    &invocation.positionals,
                    invocation.has("--advanced"),
                    width,
                    styled,
                ) {
                    Ok(text) => Output::standard(text, 0),
                    Err(error) => refused(&error.reason, None, width, styled, 2),
                }
            }
        }
        "system.version" => {
            let text = if json {
                format!(
                    "{}\n",
                    serde_json::json!({
                        "schema": "yvex.version.v1", "version": ffi::version(),
                        "local_protocol_version": ffi::LOCAL_PROTOCOL_VERSION,
                        "registry_identity": registry.registry_identity,
                        "build_commit": env!("YVEX_BUILD_COMMIT"),
                        "source_tree": env!("YVEX_BUILD_SOURCE_TREE"),
                        "source_state": env!("YVEX_BUILD_SOURCE_STATE"),
                        "build_identity": env!("YVEX_BUILD_IDENTITY"),
                        "shell_build_identity": env!("YVEX_SHELL_BUILD_IDENTITY"),
                    })
                )
            } else {
                format!(
                    "yvex {} protocol={} registry={} commit={}\n",
                    ffi::version(),
                    ffi::LOCAL_PROTOCOL_VERSION,
                    registry.registry_identity,
                    env!("YVEX_BUILD_COMMIT")
                )
            };
            Output::standard(text, 0)
        }
        "command.completion" => match registry.completion(&invocation.positionals[0]) {
            Ok(text) => Output::standard(text, 0),
            Err(error) => refused(&error.reason, None, width, styled, 2),
        },
        "host.serve" => match host::run(invocation, width, styled) {
            Ok(()) => Output {
                text: String::new(),
                exit: 0,
                diagnostic: false,
            },
            Err(error) => refused_error(&error, width, styled),
        },
        operation if operation.starts_with("management.") => {
            match management::dispatch(invocation) {
                Ok(output) => output,
                Err(reason) => refused(reason, None, width, styled, 2),
            }
        }
        "model.show" if invocation.positionals[0].starts_with("hf://") => {
            match catalog::discover(invocation, width, styled) {
                Ok(text) => Output::standard(text, 0),
                Err(error) => refused_error(error.as_ref(), width, styled),
            }
        }
        "model.list" | "model.show" => match catalog::dispatch(invocation, width, styled) {
            Ok(text) => Output::standard(text, 0),
            Err(error) => refused_error(&error, width, styled),
        },
        "host.logs" => match client::logs(invocation, width, styled) {
            Ok(()) => Output {
                text: String::new(),
                exit: 0,
                diagnostic: false,
            },
            Err(error) => refused_client(&error, width, styled),
        },
        "generation.chat" => {
            use std::io::IsTerminal;
            if !std::io::stdin().is_terminal() || !std::io::stdout().is_terminal() {
                return refused(
                    "chat requires a terminal",
                    Some("programmatic inference: use the configured provider API"),
                    width,
                    styled,
                    2,
                );
            }
            match chat::run(invocation, registry, width, styled) {
                Ok(()) => Output {
                    text: String::new(),
                    exit: 0,
                    diagnostic: false,
                },
                Err(error) => refused(
                    &error.to_string(),
                    Some("inspect yvex host status; start a host with yvex serve"),
                    width,
                    styled,
                    1,
                ),
            }
        }
        "system.paths" => match paths::dispatch(invocation, width, styled) {
            Ok(text) => Output::standard(text, 0),
            Err(error) => {
                // Preserve this command's established path-policy exit contract.
                let mut output = refused_error(error.as_ref(), width, styled);
                if let Some(native) = error.downcast_ref::<ffi::Error>() {
                    output.exit = if native.code == -10 { 2 } else { 3 };
                }
                output
            }
        },
        _ if invocation.operation.lane == registry::Lane::Client
            && !matches!(
                invocation.operation.operation_id.as_str(),
                "generation.chat"
            ) =>
        {
            match client::dispatch(invocation, width, styled) {
                Ok(text) => Output::standard(text, 0),
                Err(error) => refused_client(&error, width, styled),
            }
        }
        _ => refused(
            &format!(
                "command registry has no product dispatcher for {}",
                invocation.operation.operation_id
            ),
            None,
            width,
            styled,
            1,
        ),
    }
}

type Projection = Result<Output, Box<dyn std::error::Error>>;

fn offline_projection(
    invocation: &registry::Invocation<'_>,
    width: usize,
    styled: bool,
) -> Option<Projection> {
    let standard = |text| Output::standard(text, 0);
    Some(match invocation.operation.operation_id.as_str() {
        operation if operation.starts_with("execute.graph.attention.") => {
            attention::dispatch(invocation, width, styled)
        }
        "execute.graph.moe.execute"
        | "execute.graph.transformer.execute"
        | "execute.graph.transformer.decode"
        | "execute.graph.transformer.logits"
        | "execute.graph.transformer.sample" => pipeline::dispatch(invocation, width, styled),
        "execute.graph.transformer.generate" => generation::dispatch(invocation, width, styled),
        "execute.graph.component.audio-vae"
        | "execute.graph.component.video-vae"
        | "execute.media.publish"
        | "execute.media.generate" => media::dispatch(invocation, width, styled).map(standard),
        "quant.plan" | "quant.emit" | "quant.probe" | "quant.explain" | "quant.summarize" => {
            variant::dispatch(invocation, width, styled)
        }
        "model.search" | "source.inspect" => {
            catalog::discover(invocation, width, styled).map(standard)
        }
        "model.storage" | "model.evict" | "model.push" => {
            catalog::lifecycle(invocation, width, styled)
        }
        "model.pull" => workflow::pull(invocation, width, styled),
        "source.acquire" | "source.resume" => acquire::dispatch(invocation, width, styled),
        "source.cleanup" => acquisition::cleanup(invocation, width, styled),
        "model.acquisition.status" | "model.acquisition.stop" | "source.status" | "source.stop" => {
            acquisition::control(invocation, width, styled)
        }
        "model.prepare" => preparation::dispatch(invocation, width, styled),
        "artifact.prepare" => preparation::artifact(invocation, width, styled),
        "artifact.check" => preparation::check(invocation, width, styled),
        "artifact.registry.list" | "artifact.registry.status" => {
            artifact::inventory(invocation, width, styled)
        }
        "evidence.target" => target::dispatch(invocation, width, styled),
        operation if operation.starts_with("provider.") => {
            accounts::dispatch(invocation, width, styled)
        }
        "artifact.inspect" | "artifact.metadata" | "artifact.tensors" => {
            artifact::dispatch(invocation, width, styled).map(standard)
        }
        "artifact.verify" | "artifact.verify.report" => artifact::verify(invocation, width, styled),
        "artifact.emit.controlled" | "artifact.template" => {
            artifact::construct(invocation, width, styled).map(standard)
        }
        "artifact.materialize" => artifact::materialize(invocation, width, styled),
        "execute.materialization.full_model"
        | "context.report"
        | "evidence.moe"
        | "tensor.collection.report"
        | "inspect.model.full.report"
        | "inspect.model.full.materialization_plan"
        | "inspect.model.full.descriptor"
        | "inspect.model.full.family_runtime" => artifact::diagnostic(invocation, width, styled),
        "artifact.materialize.gate" | "artifact.model.gate" => {
            artifact::gate(invocation, width, styled)
        }
        "quant.convert"
        | "quant.qtype.support"
        | "tensor.map"
        | "quant.policy"
        | "quant.preset"
        | "quant.imatrix"
        | "quant.job" => quant::dispatch(invocation, width, styled),
        "source.list" | "source.show" | "artifact.list" | "profile.list" | "profile.show"
        | "profile.remove" | "profile.scan" => {
            plumbing::dispatch(invocation, width, styled).map(standard)
        }
        "profile.create" | "profile.verify" => plumbing::profile_command(invocation, width, styled),
        "source.manifest" | "source.payload.verify" | "source.native.inspect" => {
            source::dispatch(invocation, width, styled)
        }
        "input.prepare" => tokenizer::input(invocation, width, styled).map(standard),
        "system.backend" | "system.cuda" | "system.cuda.bandwidth" => {
            backend::dispatch(invocation, width, styled)
        }
        operation if operation.starts_with("tokenizer.") => {
            tokenizer::dispatch(invocation, width, styled).map(standard)
        }
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn native_error_identity_and_exit_remain_typed() {
        for (code, exit) in [(-10, 2), (-3, 3), (-4, 4), (-7, 4), (-5, 5), (-8, 1)] {
            let error = ffi::Error {
                code,
                owner: "fixture".into(),
                message: "bounded refusal".into(),
            };
            let result = refused_error(&error, 80, false);
            assert_eq!(result.exit, exit);
            assert!(result.diagnostic);
            assert!(result.text.contains(&ffi::status_name(code)));
        }
    }
    #[test]
    fn client_failure_exit_preserves_transport_contract() {
        for code in [-3, -4, -5, -8, -10] {
            let error = ffi::Error {
                code,
                owner: "server.protocol".into(),
                message: "bounded refusal".into(),
            };
            let output = refused_client(&error, 80, false);
            assert_eq!(output.exit, if code == -10 { 2 } else { 1 });
            assert!(output.text.contains(&ffi::status_name(code)));
            assert_eq!(output.text.contains("yvex serve"), code == -3);
        }
    }
    #[test]
    fn version_is_a_projection_not_an_independent_owner() {
        assert_eq!(ffi::version(), env!("CARGO_PKG_VERSION"));
        let output = execute(&["version".into(), "--json".into()], 80, false);
        assert_eq!(output.exit, 0);
        let json: serde_json::Value = serde_json::from_str(&output.text).unwrap();
        assert_eq!(json["schema"], "yvex.version.v1");
        assert_eq!(json["local_protocol_version"], 24);
        assert!(!output.text.contains('\u{1b}'));
    }
    #[test]
    fn nested_group_help_is_not_consumed_as_a_parent_leaf_argument() {
        for path in [["compile", "quant"], ["source", "accounts"]] {
            let words = [path[0].into(), path[1].into(), "--help".into()];
            let output = execute(&words, 180, false);
            assert_eq!(output.exit, 0);
            assert!(
                output
                    .text
                    .contains(&format!("yvex {} {}", path[0], path[1]))
            );
            assert!(!output.text.contains("operation: artifact.prepare"));
        }
        let output = execute(
            &[
                "compile".into(),
                "quant".into(),
                "plan".into(),
                "--help".into(),
            ],
            180,
            false,
        );
        assert_eq!(output.exit, 0);
        assert!(output.text.contains("operation: quant.plan"));
    }
}
pub mod presentation;
pub mod registry;
