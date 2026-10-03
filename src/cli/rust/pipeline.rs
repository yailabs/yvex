// Engineering command workflows consume native computational records, not another CLI.
use crate::{
    Output,
    attention::{Signals, expanded},
    ffi::{
        self,
        pipeline::{self, Kind},
        raw,
    },
    pipeline_projection, presentation,
    registry::Invocation,
};
use std::sync::atomic::Ordering;
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn invalid(message: &str) -> ffi::Error {
    ffi::Error {
        code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
        owner: "pipeline.grammar".into(),
        message: message.into(),
    }
}

pub(crate) fn required<'a>(invocation: &'a Invocation<'_>, name: &str) -> Result<&'a str> {
    invocation
        .value(name)
        .filter(|v| !v.is_empty())
        .ok_or_else(|| invalid(&format!("{name} is required")).into())
}

pub(crate) fn number(
    invocation: &Invocation<'_>,
    name: &str,
    default: u64,
    zero: bool,
) -> Result<u64> {
    let Some(value) = invocation.value(name) else {
        return Ok(default);
    };
    value
        .parse::<u64>()
        .ok()
        .filter(|v| zero || *v != 0)
        .ok_or_else(|| {
            invalid(&format!(
                "{name} requires a {} integer",
                if zero { "non-negative" } else { "positive" }
            ))
            .into()
        })
}

fn real(invocation: &Invocation<'_>, name: &str, default: f64) -> Result<f64> {
    let Some(value) = invocation.value(name) else {
        return Ok(default);
    };
    value
        .parse::<f64>()
        .ok()
        .filter(|v| v.is_finite())
        .ok_or_else(|| invalid(&format!("{name} requires a finite number")).into())
}

fn kind(invocation: &Invocation<'_>) -> Result<Kind> {
    Ok(match invocation.operation.operation_id.as_str() {
        "execute.graph.moe.execute" => Kind::Moe,
        "execute.graph.transformer.execute" => Kind::Transformer,
        "execute.graph.transformer.decode" => Kind::Decode,
        "execute.graph.transformer.logits" => Kind::Logits,
        "execute.graph.transformer.sample" => Kind::Sample,
        _ => return Err(invalid("registry operation has no computational adapter").into()),
    })
}

pub(crate) fn sampling(
    invocation: &Invocation<'_>,
    kind: Kind,
) -> Result<raw::yvex_runtime_sampling_policy> {
    let mut result = pipeline::sampling_defaults();
    result.temperature = real(invocation, "--temperature", result.temperature)?;
    result.top_k = number(invocation, "--top-k", result.top_k, true)?;
    result.top_p = real(invocation, "--top-p", result.top_p)?;
    result.min_p = real(invocation, "--min-p", result.min_p)?;
    result.typical_p = real(invocation, "--typical-p", result.typical_p)?;
    result.seed_present = invocation.has("--seed").into();
    result.seed = number(invocation, "--seed", 0, true)?;
    let strategy = invocation.value("--strategy").unwrap_or("greedy");
    let neutral = result.seed_present == 0
        && result.temperature == 1.0
        && result.top_k == 0
        && result.top_p == 1.0
        && result.min_p == 0.0
        && result.typical_p == 1.0;
    if kind != Kind::Sample && (strategy != "greedy" || !neutral) {
        return Err(
            invalid("sampling options require bench transformer sample or generate").into(),
        );
    }
    match strategy {
        "greedy" if neutral => {}
        "greedy" => {
            return Err(invalid("greedy strategy requires neutral filters and no seed").into());
        }
        "stochastic"
            if result.seed_present != 0
                && result.temperature > 0.0
                && result.top_p > 0.0
                && result.top_p <= 1.0
                && result.min_p >= 0.0
                && result.min_p <= 1.0
                && result.typical_p > 0.0
                && result.typical_p <= 1.0 =>
        {
            result.strategy = raw::yvex_sampling_strategy_YVEX_SAMPLING_STRATEGY_STOCHASTIC;
        }
        "stochastic" => return Err(invalid("stochastic strategy parameters are invalid").into()),
        _ => return Err(invalid("--strategy requires greedy or stochastic").into()),
    }
    Ok(result)
}

fn preflight(invocation: &Invocation<'_>, kind: Kind) -> Result<()> {
    let expected = if kind == Kind::Moe {
        "tensor-file"
    } else {
        "token-ids"
    };
    if invocation
        .value("--input")
        .unwrap_or(if kind == Kind::Moe { "" } else { expected })
        != expected
    {
        return Err(invalid(&format!("this operation requires --input {expected}")).into());
    }
    if invocation.value("--progress").unwrap_or("off") != "off" {
        return Err(invalid("this direct computational operation requires --progress off").into());
    }
    if kind == Kind::Moe && invocation.value("--scope").unwrap_or("full") != "full" {
        return Err(invalid("bench moe supports only --scope full").into());
    }
    if kind != Kind::Moe {
        if invocation.value("--phase").unwrap_or("prefill") != "prefill" {
            return Err(invalid("graph transformer supports only --phase prefill").into());
        }
        if ["--text", "--user", "--system", "--max-new-tokens"]
            .iter()
            .any(|v| invocation.has(v))
            || invocation
                .value("--generation-mode")
                .unwrap_or("target-only")
                != "target-only"
        {
            return Err(
                invalid("prompt generation options require bench transformer generate").into(),
            );
        }
    }
    if !matches!(
        invocation.value("--output").unwrap_or("normal"),
        "normal" | "table" | "audit" | "json" | "csv"
    ) {
        return Err(invalid("unsupported computational output mode").into());
    }
    Ok(())
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let kind = kind(invocation)?;
    preflight(invocation, kind)?;
    let paths = [
        required(invocation, "--target")?.to_owned(),
        expanded(required(invocation, "--artifact")?)?,
        expanded(required(invocation, "--runtime-binding")?)?,
        expanded(required(invocation, "--input-file")?)?,
    ];
    let backend = pipeline::backend(required(invocation, "--backend")?)?;
    let context = if kind == Kind::Moe {
        0
    } else {
        number(invocation, "--context-capacity", 0, false)?
    };
    if kind != Kind::Moe && context == 0 {
        return Err(invalid("--context-capacity is required").into());
    }
    let chunk_flag = if kind == Kind::Transformer {
        "--chunk-tokens"
    } else {
        "--prefill-chunk-tokens"
    };
    let chunk = number(invocation, chunk_flag, 0, false)?;
    let prefill = number(invocation, "--prefill-tokens", 0, false)?;
    if kind != Kind::Moe && (chunk == 0 || (kind != Kind::Transformer && prefill == 0)) {
        return Err(invalid(
            "explicit chunk tokens and, for decode/logits/sample, prefill tokens are required",
        )
        .into());
    }
    // Validate every explicitly supplied bound even if the selected operator does not use it.
    for name in [
        "--chunk-tokens",
        "--prefill-tokens",
        "--prefill-chunk-tokens",
        "--context-capacity",
        "--max-new-tokens",
        "--max-output-bytes",
    ] {
        number(invocation, name, 0, false)?;
    }
    let device_bytes = number(invocation, "--max-device-bytes", 0, false)?;
    if device_bytes != 0 && backend == raw::yvex_backend_kind_YVEX_BACKEND_KIND_CPU {
        return Err(invalid("--max-device-bytes requires an accelerator backend").into());
    }
    let signals = Signals::new()?;
    let mut run = pipeline::run(
        pipeline::Input {
            kind,
            paths: [&paths[0], &paths[1], &paths[2], &paths[3]],
            backend,
            chunk,
            prefill,
            context,
            host_bytes: number(invocation, "--max-host-bytes", 0, false)?,
            device_bytes,
            policy: sampling(invocation, kind)?,
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
            owner: "pipeline.publication".into(),
            message: "execution cancelled before command publication".into(),
        });
    }
    if run.failure.is_none() && !run.completed() {
        run.refuse(ffi::Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "pipeline.publication".into(),
            message: "execution returned an incomplete result".into(),
        });
    }
    let fields = pipeline_projection::fields(&run)?;
    let text = pipeline_projection::present(
        &fields,
        invocation.value("--output").unwrap_or("normal"),
        width,
        styled,
    )?;
    if let Some(error) = run.failure.as_ref() {
        use std::io::Write;
        let diagnostic = presentation::record(
            "EXECUTION refused",
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
