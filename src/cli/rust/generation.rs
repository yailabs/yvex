// Prompt construction and target/speculative execution remain native model/runtime authority.
use crate::{
    Output,
    attention::{Signals, expanded},
    ffi::{
        self, generation,
        pipeline::{self, Kind},
        raw,
    },
    pipeline::{number, required, sampling},
    pipeline_projection, presentation,
    registry::Invocation,
};
use std::sync::atomic::Ordering;
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn invalid(message: &str) -> ffi::Error {
    ffi::Error {
        code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
        owner: "generation.grammar".into(),
        message: message.into(),
    }
}

fn preflight(invocation: &Invocation<'_>) -> Result<()> {
    if invocation.has("--text") == invocation.has("--user")
        || (invocation.has("--system") && !invocation.has("--user"))
    {
        return Err(invalid(
            "generation requires exactly one text or user prompt; system requires user",
        )
        .into());
    }
    if invocation.value("--phase").unwrap_or("prefill") != "prefill"
        || invocation.value("--progress").unwrap_or("off") != "off"
    {
        return Err(
            invalid("direct engineering generation requires prefill and progress off").into(),
        );
    }
    for name in [
        "--context-capacity",
        "--prefill-chunk-tokens",
        "--max-new-tokens",
    ] {
        if number(invocation, name, 0, false)? == 0 {
            return Err(invalid(&format!("{name} is required")).into());
        }
    }
    for name in [
        "--chunk-tokens",
        "--prefill-tokens",
        "--max-output-bytes",
        "--max-host-bytes",
        "--max-device-bytes",
    ] {
        number(invocation, name, 0, false)?;
    }
    if !matches!(
        invocation.value("--output").unwrap_or("normal"),
        "normal" | "table" | "audit" | "json" | "csv"
    ) {
        return Err(invalid("unsupported engineering generation output mode").into());
    }
    Ok(())
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    preflight(invocation)?;
    let artifact = expanded(required(invocation, "--artifact")?)?;
    let binding = expanded(required(invocation, "--runtime-binding")?)?;
    let backend = pipeline::backend(required(invocation, "--backend")?)?;
    let device_bytes = number(invocation, "--max-device-bytes", 0, false)?;
    if device_bytes > 0 && backend == raw::yvex_backend_kind_YVEX_BACKEND_KIND_CPU {
        return Err(invalid("--max-device-bytes requires an accelerator backend").into());
    }
    let mode = match invocation
        .value("--generation-mode")
        .unwrap_or("target-only")
    {
        "target-only" => raw::yvex_runtime_generation_mode_YVEX_GENERATION_MODE_TARGET_ONLY,
        "dspark" => raw::yvex_runtime_generation_mode_YVEX_GENERATION_MODE_SPECULATIVE,
        _ => return Err(invalid("--generation-mode requires target-only or dspark").into()),
    };
    let signals = Signals::new()?;
    let mut run = generation::run(
        generation::Input {
            target: required(invocation, "--target")?,
            artifact: &artifact,
            binding: &binding,
            backend,
            mode,
            text: invocation.value("--text"),
            system: invocation.value("--system"),
            user: invocation.value("--user"),
            context: number(invocation, "--context-capacity", 0, false)?,
            chunk: number(invocation, "--prefill-chunk-tokens", 0, false)?,
            new_tokens: number(invocation, "--max-new-tokens", 0, false)?,
            output_bytes: number(invocation, "--max-output-bytes", 1048576, false)?,
            host_bytes: number(invocation, "--max-host-bytes", 0, false)?,
            device_bytes,
            policy: sampling(invocation, Kind::Sample)?,
        },
        signals.cancel.clone(),
    )?;
    run.command(&invocation.operation.command_path.join(" "));
    if let Err(error) = run.finish() {
        run.refuse(error);
    }
    if run.failure.is_none() && signals.cancel.load(Ordering::Relaxed) {
        run.refuse(ffi::Error {
            code: raw::yvex_status_YVEX_ERR_CANCELLED,
            owner: "generation.publication".into(),
            message: "generation cancelled before command publication".into(),
        });
    }
    if run.failure.is_none() && !run.completed() {
        run.refuse(ffi::Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "generation.publication".into(),
            message: "generation returned an incomplete result".into(),
        });
    }
    let text = pipeline_projection::present(
        &pipeline_projection::fields(&run)?,
        invocation.value("--output").unwrap_or("normal"),
        width,
        styled,
    )?;
    if let Some(error) = run.failure.as_ref() {
        use std::io::Write;
        let diagnostic = presentation::record(
            "GENERATION refused",
            &[
                ("code", &ffi::status_name(error.code)),
                ("owner", &error.owner),
                ("reason", &error.message),
            ],
            width,
            styled,
        )?;
        std::io::stderr().write_all(diagnostic.as_bytes())?;
    }
    Ok(Output::standard(
        text,
        run.failure
            .as_ref()
            .map_or(0, |v| crate::native_exit(v.code)),
    ))
}
