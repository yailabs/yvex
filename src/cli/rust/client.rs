// Runtime reads use the existing C client; the shell never reconstructs the private wire.
pub(crate) mod cancellation;
use crate::ffi::{self, Client, Error, raw};
use crate::{catalog, presentation, registry::Invocation};
use replai::Alignment;
use serde_json::{Value, json};

fn invalid(owner: &str, reason: &str) -> Error {
    Error {
        code: raw::yvex_status_YVEX_ERR_FORMAT,
        owner: owner.into(),
        message: reason.into(),
    }
}

pub(crate) fn response(
    client: &mut Client,
    request: &raw::yvex_client_request,
) -> Result<Box<raw::yvex_client_message>, Error> {
    let reply = client.receive()?;
    if reply.request_number != request.request_number {
        return Err(invalid(
            "client.correlation",
            "reply belongs to another request",
        ));
    }
    if reply.kind == raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ERROR {
        return Err(Error {
            code: reply.status,
            owner: "server.request".into(),
            message: ffi::text(&reply.reason),
        });
    }
    Ok(reply)
}

pub(crate) fn stop_host(socket: &str) -> Result<(), Error> {
    let mut client = Client::connect(Some(socket))?;
    let request = client.request(raw::yvex_client_operation_YVEX_CLIENT_OP_RUNTIME_STOP);
    client.send(&request)?;
    let reply = response(&mut client, &request)?;
    if reply.kind != raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK {
        return Err(invalid("host.stop", "unexpected stop response"));
    }
    Ok(())
}

pub(crate) fn logs(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<(), Error> {
    use std::io;
    let json = invocation.has("--json");
    let verbose = invocation.has("--verbose");
    let follow = invocation.has("--follow");
    let mut client = Client::connect(None)?;
    let mut request = client.request(if follow {
        raw::yvex_client_operation_YVEX_CLIENT_OP_RUNTIME_WATCH
    } else {
        raw::yvex_client_operation_YVEX_CLIENT_OP_RUNTIME_TRACE
    });
    request.trace_level = if json || verbose {
        raw::yvex_server_trace_level_YVEX_SERVER_TRACE_FULL
    } else {
        raw::yvex_server_trace_level_YVEX_SERVER_TRACE_STAGES
    };
    client.send(&request)?;
    let mut output = io::stdout().lock();
    let mut cadence = EventCadence::default();
    if !json {
        let title = if follow {
            "YVEX logs · UTC · Ctrl-C to detach"
        } else {
            "YVEX logs · UTC"
        };
        let title = presentation::spans(&[(replai::Role::Accent, title)], width, styled)
            .map_err(|error| invalid("presentation", &error.to_string()))?;
        if !write_log(&mut output, &title)? {
            return Ok(());
        }
    }
    loop {
        let reply = response(&mut client, &request)?;
        if reply.kind == raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK {
            break;
        }
        if reply.kind != raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_EVENT {
            return Err(invalid("host.logs", "unexpected log response"));
        }
        if !json && !cadence.admit(&reply.event, verbose) {
            continue;
        }
        let rendered = if json {
            format!("{}\n", event_json(&reply.event))
        } else {
            event_render(&reply.event, verbose, width, styled)
                .map_err(|error| invalid("presentation", &error.to_string()))?
        };
        if !write_log(&mut output, &rendered)? {
            break;
        }
        if reply.event.kind
            == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_RUNTIME_SHUTDOWN_COMPLETE
        {
            break;
        }
    }
    // Dropping this read-only subscription never stops the host or its engines.
    Ok(())
}

fn write_log(output: &mut impl std::io::Write, text: &str) -> Result<bool, Error> {
    match output
        .write_all(text.as_bytes())
        .and_then(|()| output.flush())
    {
        Ok(()) => Ok(true),
        Err(error) if error.kind() == std::io::ErrorKind::BrokenPipe => Ok(false),
        Err(error) => Err(Error {
            code: raw::yvex_status_YVEX_ERR_IO,
            owner: "host.logs.output".into(),
            message: error.to_string(),
        }),
    }
}

pub(crate) fn event_json(event: &raw::yvex_server_event) -> Value {
    let m = &event.measurement;
    // Preserve the established JSON projection's nine significant digit precision.
    let decimal = |number: f64| format!("{number:.8e}").parse::<f64>().unwrap();
    json!({ "schema": event.schema_version, "sequence": event.sequence, "process": event.process_id,
        "wall_time_ns": event.wall_time_ns, "monotonic_time_ns": event.monotonic_time_ns,
        "kind": ffi::event_name(event.kind), "severity": event.severity,
        "session": ffi::text(&event.session_id), "request": ffi::text(&event.request_id),
        "turn": ffi::text(&event.turn_id), "phase": ffi::text(&event.phase),
        "provider": ffi::text(&event.provider_adapter),
        "provider_request_identity": ffi::text(&event.provider_request_identity),
        "external_correlation_id": ffi::text(&event.external_correlation_id),
        "a": event.value_a, "b": event.value_b, "c": event.value_c,
        "engine_kind": event.engine_kind, "execution_strategy": event.execution_strategy,
        "speculative_cycle": event.speculative_cycle, "proposed_tokens": event.proposed_tokens,
        "selected_verification_tokens": event.selected_verification_tokens,
        "accepted_tokens": event.accepted_tokens, "rejected_tokens": event.rejected_tokens,
        "discarded_tokens": event.discarded_tokens, "verification_count": event.verification_count,
        "confidence_logit_count": event.confidence_logit_count,
        "confidence_logit_minimum": decimal(event.confidence_logit_minimum),
        "confidence_logit_maximum": decimal(event.confidence_logit_maximum),
        "confidence_logit_mean": decimal(event.confidence_logit_mean),
        "speculation_policy_identity": ffi::text(&event.speculation_policy_identity),
        "seconds": decimal(event.seconds), "rate": decimal(event.rate),
        "runtime_model_identity": ffi::text(&event.runtime_model_identity),
        "artifact_identity": ffi::text(&event.artifact_identity),
        "variant_identity": ffi::text(&event.variant_identity),
        "measurement_schema": m.schema_version, "measurement_scope": m.scope,
        "measurement_clock": m.clock, "measurement_composition": m.composition,
        "measurement_unit": m.work_unit, "measurement_available": m.available,
        "measurement_completed": m.completed_units, "measurement_total": m.total_units,
        "measurement_duration_ns": m.duration_ns, "measurement_rolling_units": m.rolling_units,
        "measurement_rolling_duration_ns": m.rolling_duration_ns,
        "measurement_rolling_window_units": m.rolling_window_units,
        "measurement_cumulative_rate": decimal(m.cumulative_rate), "measurement_rolling_rate": decimal(m.rolling_rate),
        "identity": ffi::text(&event.event_identity) })
}

fn engine_event_human(event: &raw::yvex_server_event) -> Option<(&'static str, String)> {
    let category = match event.kind {
        raw::yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_READY => "MODEL",
        raw::yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_LOAD_REQUESTED => "LOAD",
        raw::yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_LOAD_FAILED
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_UNLOAD_FAILED => "FAIL",
        raw::yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_UNLOAD_STARTED => "UNLOAD",
        raw::yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_UNLOADED => "UNLOADED",
        _ => return None,
    };
    let backend = u32::try_from(event.value_c)
        .map(ffi::backend_name)
        .unwrap_or_else(|_| "unknown".into())
        .to_uppercase();
    let strategy = match event.execution_strategy {
        raw::yvex_server_execution_strategy_YVEX_SERVER_EXECUTION_TARGET_ONLY => {
            " strategy=target-only"
        }
        raw::yvex_server_execution_strategy_YVEX_SERVER_EXECUTION_SPECULATIVE => {
            " strategy=speculative"
        }
        _ => "",
    };
    Some((
        category,
        format!(
            "{} generation={} backend={backend}{strategy}",
            ffi::text(&event.phase),
            event.value_a
        ),
    ))
}

fn http_event_human(event: &raw::yvex_server_event, verbose: bool) -> String {
    use raw::*;
    let phase = ffi::text(&event.phase);
    let (port, status, code) = (event.value_a, event.value_b, event.value_c);
    let mut detail = format!("openai {}", &phase[5..]);
    if verbose {
        detail.push_str(&format!(" peer=127.0.0.1:{port}"));
    }
    if event.kind == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_REQUEST_RECEIVED {
        detail.push_str(" received");
        return detail;
    }
    let outcome = if code == (-yvex_status_YVEX_ERR_CANCELLED) as u64 {
        "cancelled"
    } else if status == 0 {
        "incomplete"
    } else if status >= 500 {
        "failed"
    } else if status >= 400 {
        "rejected"
    } else if code != 0 {
        "failed"
    } else {
        "complete"
    };
    detail.push_str(&format!(
        " closed status={} outcome={outcome} elapsed={:.3}s",
        if status == 0 {
            "unavailable".into()
        } else {
            status.to_string()
        },
        event.seconds
    ));
    if code != 0 {
        detail.push_str(&format!(" error={}", ffi::status_name(-(code as i32))));
    }
    detail
}

fn generation_progress(event: &raw::yvex_server_event, verbose: bool) -> (&'static str, String) {
    let phase = ffi::text(&event.phase);
    if event.engine_kind != raw::yvex_server_engine_kind_YVEX_SERVER_ENGINE_TEXT {
        return (
            "generate",
            format!(
                "phase={phase}{}",
                progress_human(&event.measurement, verbose)
            ),
        );
    }
    let detail = if verbose {
        let reasoning = if phase == "reasoning" || event.value_c == 0 {
            String::new()
        } else {
            format!(" reasoning={}", event.value_c)
        };
        format!(
            "generated={} position={} phase={phase}{reasoning}{}",
            event.value_a,
            event.value_b,
            token_rates(&event.measurement, event.rate, true)
        )
    } else {
        format!(
            "gen={} ctx={}{}",
            event.value_a,
            event.value_b,
            token_rates(&event.measurement, event.rate, false)
        )
    };
    (
        if verbose {
            "generate"
        } else if phase == "reasoning" {
            "reasoning"
        } else {
            "decode"
        },
        detail,
    )
}

fn event_parts(event: &raw::yvex_server_event, verbose: bool) -> (&'static str, String) {
    let kind = event.kind;
    let phase = ffi::text(&event.phase);
    let (a, b, c) = (event.value_a, event.value_b, event.value_c);
    let (category, detail) = if let Some(engine) = engine_event_human(event) {
        engine
    } else {
        match kind {
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_REQUEST_RECEIVED
            | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_CLIENT_DISCONNECTED
                if phase.starts_with("http:") =>
            {
                ("HTTP", http_event_human(event, verbose))
            }
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_STARTED
            | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_PROGRESS
            | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_COMPLETED => (
                "PREFILL",
                format!(
                    "{}{}",
                    if kind == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_STARTED {
                        "started"
                    } else if kind
                        == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_COMPLETED
                    {
                        "completed"
                    } else {
                        "progress"
                    },
                    progress_human(&event.measurement, verbose)
                ),
            ),
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_LOAD_PROGRESS => (
                "LOAD",
                format!(
                    "phase={phase}{}",
                    progress_human(&event.measurement, verbose)
                ),
            ),
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_PROGRESS => {
                generation_progress(event, verbose)
            }
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED
            | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_CANCELLED
            | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FAILED => {
                let category = if kind
                    == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED
                {
                    "DONE"
                } else if kind == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_CANCELLED
                {
                    "CANCELLED"
                } else {
                    "FAIL"
                };
                let detail = if event.engine_kind
                    == raw::yvex_server_engine_kind_YVEX_SERVER_ENGINE_MEDIA
                {
                    format!("frames={a} bytes={b} audio_samples={c}")
                } else {
                    let stop = [
                        "none",
                        "EOS",
                        "tokenizer stop",
                        "maximum tokens",
                        "context capacity",
                        "cancelled",
                        "model failure",
                        "tokenizer failure",
                        "output failure",
                    ]
                    .get(c as usize)
                    .copied()
                    .unwrap_or("unknown");
                    let speculation = if event.proposed_tokens == 0 {
                        String::new()
                    } else {
                        format!(
                            " spec-accepted={}/{}",
                            event.accepted_tokens, event.proposed_tokens
                        )
                    };
                    if verbose {
                        format!(
                            "generated={a} position={b} stop={stop:?} elapsed={:.3}s{speculation}{}",
                            event.seconds,
                            token_rates(&event.measurement, event.rate, true)
                        )
                    } else {
                        format!(
                            "gen={a} finish={stop:?}{} {:.3}s",
                            token_rates(&event.measurement, event.rate, false),
                            event.seconds
                        )
                    }
                };
                (category, detail)
            }
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_SPECULATIVE_CYCLE_COMMITTED => (
                "SPECULATION",
                format!(
                    "cycle={} accepted={}/{} rejected={} discarded={}",
                    event.speculative_cycle,
                    event.accepted_tokens,
                    event.proposed_tokens,
                    event.rejected_tokens,
                    event.discarded_tokens
                ),
            ),
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_REQUEST_STARTED => (
                "REQUEST",
                format!(
                    "{}={a} prefix_tokens={b} max_tokens={c}",
                    if ffi::text(&event.provider_request_identity).is_empty() {
                        "input_bytes"
                    } else {
                        "messages"
                    }
                ),
            ),
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_TOKENIZER_COMPLETED => (
                "PROMPT",
                if verbose {
                    format!("tokens={a} reused={b}")
                } else {
                    format!("{a} tokens, {b} reused")
                },
            ),
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_SESSION_CREATED => {
                ("SESSION", format!("created active_sessions={c}"))
            }
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_SESSION_CLOSED => {
                ("SESSION", format!("closed active_sessions={b}"))
            }
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_SESSION_RESET => {
                ("SESSION", "reset".into())
            }
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_SESSION_ATTACHED => {
                ("SESSION", format!("attached clients={a}"))
            }
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_SESSION_DETACHED => {
                ("SESSION", format!("detached clients={a}"))
            }
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_REQUEST_QUEUED => {
                ("QUEUE", format!("waiting depth={a} capacity={b}"))
            }
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FIRST_TOKEN => (
                "FIRST-TOKEN",
                if verbose {
                    format!("first committed elapsed={:.3}s ordinal={a}", event.seconds)
                } else {
                    format!("{:.3}s", event.seconds)
                },
            ),
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_TELEMETRY_DROPPED => (
                "WARNING",
                format!("telemetry coalesced={c} dropped={a} capacity={b}"),
            ),
            _ => (
                "RUNTIME",
                if verbose {
                    format!(
                        "{} phase={phase} value_a={a} value_b={b} value_c={c}",
                        ffi::event_name(kind)
                    )
                } else {
                    ffi::event_name(kind)
                },
            ),
        }
    };
    (category, detail)
}

fn event_role(event: &raw::yvex_server_event) -> replai::Role {
    use raw::*;
    use replai::Role;
    if event.severity >= yvex_server_event_severity_YVEX_SERVER_SEVERITY_ERROR {
        return Role::Error;
    }
    if event.severity == yvex_server_event_severity_YVEX_SERVER_SEVERITY_WARNING {
        return Role::Warning;
    }
    match event.kind {
        raw::yvex_server_event_kind_YVEX_SERVER_EVENT_RUNTIME_READY
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_LISTENER_READY
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_PROGRESS
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_SPECULATIVE_CYCLE_COMMITTED
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_RUNTIME_SHUTDOWN_COMPLETE
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_READY
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_UNLOADED => Role::Success,
        raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_CANCELLED
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_TELEMETRY_DROPPED => Role::Warning,
        raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FAILED
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_LOAD_FAILED
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_UNLOAD_FAILED => Role::Error,
        raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_STARTED
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_PROGRESS
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_COMPLETED
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FIRST_TOKEN
        | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_LOAD_PROGRESS => Role::Accent,
        _ => Role::Strong,
    }
}

pub(crate) fn event_render(
    event: &raw::yvex_server_event,
    verbose: bool,
    width: usize,
    styled: bool,
) -> Result<String, replai::EditError> {
    let (category, detail) = event_parts(event, verbose);
    render_log_record(event, category, detail, verbose, width, styled)
}

fn render_log_record(
    event: &raw::yvex_server_event,
    category: &str,
    mut detail: String,
    verbose: bool,
    width: usize,
    styled: bool,
) -> Result<String, replai::EditError> {
    let timestamp = ffi::event_timestamp(event.wall_time_ns, verbose);
    let timestamp = if verbose {
        timestamp.as_str()
    } else {
        &timestamp[..8]
    };
    let severity = match event.severity {
        raw::yvex_server_event_severity_YVEX_SERVER_SEVERITY_DEBUG => "DEBUG",
        raw::yvex_server_event_severity_YVEX_SERVER_SEVERITY_INFO => "INFO",
        raw::yvex_server_event_severity_YVEX_SERVER_SEVERITY_WARNING => "WARN",
        raw::yvex_server_event_severity_YVEX_SERVER_SEVERITY_ERROR => "ERROR",
        raw::yvex_server_event_severity_YVEX_SERVER_SEVERITY_FATAL => "FATAL",
        _ => "UNKNOWN",
    };
    let session = ffi::text(&event.session_id);
    let request = ffi::text(&event.request_id);
    let identity = match (session.is_empty(), request.is_empty()) {
        (false, false) => format!("{session}/{request}"),
        (false, true) => session,
        (true, false) => request,
        (true, true) => "host".into(),
    };
    if verbose {
        let turn = ffi::text(&event.turn_id);
        if !turn.is_empty() {
            detail.push_str(&format!(" turn={turn}"));
        }
    }
    let sequence = if verbose {
        format!(" #{}", event.sequence)
    } else {
        String::new()
    };
    let label = category.to_ascii_lowercase();
    let level = if verbose
        || event.severity >= raw::yvex_server_event_severity_YVEX_SERVER_SEVERITY_WARNING
    {
        severity
    } else {
        ""
    };
    presentation::log_record(
        &[
            (replai::Role::Dim, timestamp),
            (event_role(event), "yvex:"),
            (event_role(event), level),
            (replai::Role::Dim, &identity),
            (replai::Role::Dim, &sequence),
            (event_role(event), &label),
        ],
        &detail,
        event_role(event),
        width,
        styled,
    )
}

#[cfg(test)]
fn event_human(event: &raw::yvex_server_event, verbose: bool) -> String {
    event_render(event, verbose, 4096, false).unwrap()
}

fn token_rates(
    measurement: &raw::yvex_execution_measurement,
    fallback: f64,
    verbose: bool,
) -> String {
    use raw::*;
    if measurement.schema_version != YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1 {
        return if fallback.is_finite() && fallback > 0.0 {
            format!(" rate={fallback:.2}tok/s")
        } else {
            String::new()
        };
    }
    if measurement.work_unit != raw::yvex_execution_work_unit_YVEX_EXECUTION_WORK_TOKENS {
        return String::new();
    }
    let mut text = String::new();
    if measurement.available & u64::from(YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE) != 0
        && measurement.cumulative_rate.is_finite()
        && measurement.cumulative_rate > 0.0
    {
        let scope = match measurement.scope {
            raw::yvex_execution_measurement_scope_YVEX_EXECUTION_SCOPE_SUBSEQUENT_DECODE => {
                "decode-avg"
            }
            raw::yvex_execution_measurement_scope_YVEX_EXECUTION_SCOPE_TOTAL_OPERATION => {
                "total-avg"
            }
            _ => "avg",
        };
        text = if verbose {
            format!(" {scope}={:.2}tok/s", measurement.cumulative_rate)
        } else {
            format!(
                " {}={:.2}t/s",
                scope.strip_suffix("-avg").unwrap_or(scope),
                measurement.cumulative_rate
            )
        };
    }
    if verbose
        && measurement.available & u64::from(YVEX_EXECUTION_MEASUREMENT_ROLLING_RATE_AVAILABLE) != 0
        && measurement.rolling_rate.is_finite()
    {
        let population = if measurement.rolling_units < measurement.rolling_window_units {
            format!(
                "{}/{}",
                measurement.rolling_units, measurement.rolling_window_units
            )
        } else {
            measurement.rolling_window_units.to_string()
        };
        text.push_str(&format!(
            " rolling[{population}]={:.2}tok/s",
            measurement.rolling_rate
        ));
    }
    text
}

fn progress_human(measurement: &raw::yvex_execution_measurement, verbose: bool) -> String {
    use raw::*;
    if measurement.schema_version != YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1 {
        return " progress=unavailable".into();
    }
    let units = match measurement.work_unit {
        raw::yvex_execution_work_unit_YVEX_EXECUTION_WORK_TOKENS => "tokens",
        raw::yvex_execution_work_unit_YVEX_EXECUTION_WORK_BYTES => "bytes",
        raw::yvex_execution_work_unit_YVEX_EXECUTION_WORK_TENSORS => "tensors",
        raw::yvex_execution_work_unit_YVEX_EXECUTION_WORK_PLANS => "plans",
        raw::yvex_execution_work_unit_YVEX_EXECUTION_WORK_COMPONENTS => "components",
        raw::yvex_execution_work_unit_YVEX_EXECUTION_WORK_EVALUATIONS => "evaluations",
        raw::yvex_execution_work_unit_YVEX_EXECUTION_WORK_FRAMES => "frames",
        raw::yvex_execution_work_unit_YVEX_EXECUTION_WORK_SAMPLES => "samples",
        _ => "work",
    };
    let total =
        if measurement.available & u64::from(YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE) != 0
            && measurement.total_units != 0
        {
            measurement.total_units.to_string()
        } else {
            "unknown".into()
        };
    let mut text = if verbose {
        format!(" {units}={}/{total}", measurement.completed_units)
    } else {
        format!(" {}/{total} {units}", measurement.completed_units)
    };
    if measurement.available & u64::from(YVEX_EXECUTION_MEASUREMENT_DURATION_AVAILABLE) != 0 {
        let seconds = measurement.duration_ns as f64 / 1e9;
        text.push_str(&if verbose {
            format!(" elapsed={seconds:.2}s")
        } else {
            format!(" {seconds:.3}s")
        });
    }
    if measurement.work_unit == raw::yvex_execution_work_unit_YVEX_EXECUTION_WORK_TOKENS {
        text.push_str(&token_rates(measurement, 0.0, verbose));
    } else if measurement.available
        & u64::from(YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE)
        != 0
        && measurement.cumulative_rate.is_finite()
        && measurement.cumulative_rate > 0.0
    {
        text.push_str(&format!(" avg={:.2}{units}/s", measurement.cumulative_rate));
    }
    text
}

#[derive(Default)]
pub(crate) struct EventCadence(
    std::collections::BTreeMap<(String, String, u32), (String, u64, u64)>,
);
impl EventCadence {
    pub(crate) fn admit(&mut self, event: &raw::yvex_server_event, verbose: bool) -> bool {
        use raw::*;
        if verbose || event.severity >= yvex_server_event_severity_YVEX_SERVER_SEVERITY_WARNING {
            return true;
        }
        let kind = event.kind;
        let phase = ffi::text(&event.phase);
        if matches!(
            kind,
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_SESSION_ATTACHED
                | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_SESSION_DETACHED
        ) || (kind == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_REQUEST_QUEUED
            && event.value_a <= 1)
        {
            return false;
        }
        // Routine successful polling is diagnostic noise; failures stay visible.
        if matches!(phase.as_str(), "http:GET /health" | "http:GET /v1/models")
            && (kind == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_REQUEST_RECEIVED
                || (kind == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_CLIENT_DISCONNECTED
                    && (200..400).contains(&event.value_b)
                    && event.value_c == 0))
        {
            return false;
        }
        // A final chunk and the completed event describe one boundary.
        // Keep the authoritative completed event and its full measurement.
        if kind == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_PROGRESS
            && event.measurement.schema_version == YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1
            && event.measurement.available
                & u64::from(YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE)
                != 0
            && event.measurement.total_units != 0
            && event.measurement.completed_units == event.measurement.total_units
        {
            return false;
        }
        if kind == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_STARTED {
            self.remember(
                event,
                raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_PROGRESS,
            );
            return true;
        }
        if matches!(
            kind,
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_REQUEST_RECEIVED
                | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_CLIENT_DISCONNECTED
        ) && !ffi::text(&event.phase).starts_with("http:")
        {
            return false;
        }
        if (raw::yvex_server_event_kind_YVEX_SERVER_EVENT_DRAFT_STARTED
            ..=raw::yvex_server_event_kind_YVEX_SERVER_EVENT_CANDIDATE_REJECTED)
            .contains(&kind)
            || matches!(
                kind,
                raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FRAGMENT
                    | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_PROFILE
            )
        {
            return false;
        }
        if !matches!(
            kind,
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_PROGRESS
                | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_PROGRESS
        ) {
            return true;
        }
        let key = (
            ffi::text(&event.session_id),
            ffi::text(&event.request_id),
            kind,
        );
        let next = Self::position(event);
        if let Some(previous) = self.0.get(&key) {
            if previous == &next {
                return false;
            }
            let complete = event.measurement.available
                & u64::from(YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE)
                != 0
                && event.measurement.total_units != 0
                && next.2 == event.measurement.total_units;
            if next.0 == previous.0
                && !complete
                && next.1.saturating_sub(previous.1) < 1_000_000_000
            {
                return false;
            }
        }
        if self.0.len() >= 128 && !self.0.contains_key(&key) {
            self.0.pop_first();
        }
        self.0.insert(key, next);
        true
    }

    fn position(event: &raw::yvex_server_event) -> (String, u64, u64) {
        let typed = event.measurement.schema_version == raw::YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1;
        let duration = if typed
            && event.measurement.available
                & u64::from(raw::YVEX_EXECUTION_MEASUREMENT_DURATION_AVAILABLE)
                != 0
        {
            event.measurement.duration_ns
        } else if event.seconds.is_finite() && event.seconds >= 0.0 {
            (event.seconds * 1e9) as u64
        } else {
            event.monotonic_time_ns
        };
        (
            ffi::text(&event.phase),
            duration,
            if typed {
                event.measurement.completed_units
            } else {
                event.value_a
            },
        )
    }

    fn remember(&mut self, event: &raw::yvex_server_event, kind: u32) {
        if self.0.len() >= 128 {
            self.0.pop_first();
        }
        self.0.insert(
            (
                ffi::text(&event.session_id),
                ffi::text(&event.request_id),
                kind,
            ),
            Self::position(event),
        );
    }
}

#[derive(Default)]
pub(crate) struct ResourceCadence(Option<u64>);
impl ResourceCadence {
    pub(crate) fn due(&mut self, event: &raw::yvex_server_event) -> bool {
        let terminal = matches!(
            event.kind,
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED
                | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_CANCELLED
                | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FAILED
        );
        if !terminal
            && event.kind != raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_PROGRESS
        {
            return false;
        }
        let stamp = event.monotonic_time_ns;
        if !terminal
            && self
                .0
                .is_some_and(|previous| stamp >= previous && stamp - previous < 10_000_000_000)
        {
            return false;
        }
        self.0 = Some(stamp);
        true
    }
}

pub(crate) fn resource_human(summary: &raw::yvex_server_summary) -> Option<String> {
    use raw::*;
    let resource = &summary.metrics.resources;
    if resource.schema_version != YVEX_EXECUTION_RESOURCE_SCHEMA_V1 || resource.available == 0 {
        return None;
    }
    let mut text = String::from("host snapshot");
    for (name, value, flag) in [
        (
            "process-rss",
            resource.process_rss_current_bytes,
            YVEX_EXECUTION_RESOURCE_PROCESS_AVAILABLE,
        ),
        (
            "workspace",
            resource.workspace_current_bytes,
            YVEX_EXECUTION_RESOURCE_WORKSPACE_AVAILABLE,
        ),
        (
            "session-state",
            resource.session_physical_state_bytes,
            YVEX_EXECUTION_RESOURCE_SESSION_AVAILABLE,
        ),
    ] {
        if resource.available & u64::from(flag) != 0 {
            text.push_str(&format!(" {name}={:.1}GiB", value as f64 / 1073741824.0));
        }
    }
    text.push_str(&format!(
        " active_requests={} queued={}",
        summary.metrics.active_requests, summary.metrics.queue_depth
    ));
    Some(text)
}

pub(crate) fn resource_render(
    event: &raw::yvex_server_event,
    summary: &raw::yvex_server_summary,
    width: usize,
    styled: bool,
) -> Result<Option<String>, replai::EditError> {
    resource_human(summary)
        .map(|message| {
            // Snapshot facts are collected in response to this producer event.
            // They do not inherit its session, request, failure level or category.
            let context = raw::yvex_server_event {
                wall_time_ns: event.wall_time_ns,
                severity: raw::yvex_server_event_severity_YVEX_SERVER_SEVERITY_INFO,
                ..Default::default()
            };
            render_log_record(&context, "RESOURCES", message, false, width, styled)
        })
        .transpose()
}

pub struct HostStatus {
    snapshot: raw::yvex_server_summary,
}

pub(crate) fn management_status() -> Result<raw::yvex_server_summary, Error> {
    let mut client = Client::connect(None)?;
    client.timeout(2000)?;
    let request = client.request(raw::yvex_client_operation_YVEX_CLIENT_OP_RUNTIME_STATUS);
    client.send(&request)?;
    let reply = response(&mut client, &request)?;
    if reply.kind != raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_STATUS {
        return Err(invalid("management.status", "unexpected runtime response"));
    }
    Ok(reply.runtime)
}

pub fn host_status(socket: Option<&str>) -> Result<HostStatus, Error> {
    let mut client = Client::connect(socket)?;
    let request = client.request(raw::yvex_client_operation_YVEX_CLIENT_OP_RUNTIME_STATUS);
    client.send(&request)?;
    let reply = response(&mut client, &request)?;
    if reply.kind != raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_STATUS {
        return Err(Error {
            code: -4,
            owner: "client.status".into(),
            message: "unexpected response kind".into(),
        });
    }
    Ok(HostStatus {
        snapshot: reply.runtime,
    })
}

impl HostStatus {
    pub(crate) fn from_snapshot(snapshot: raw::yvex_server_summary) -> Self {
        Self { snapshot }
    }
    pub fn memory_json(&self) -> Value {
        let metrics = &self.snapshot.metrics;
        json!({ "schema": "yvex.host.memory.v2", "rss_bytes": metrics.current_rss_bytes,
            "peak_rss_bytes": metrics.peak_rss_bytes, "mapped_artifact_bytes": metrics.mapped_artifact_bytes,
            "resident_host_bytes": metrics.resident_host_bytes, "resident_device_bytes": metrics.resident_device_bytes,
            "resources": resource_json(&metrics.resources) })
    }

    fn memory(&self, width: usize, styled: bool) -> Result<String, Error> {
        let resource = &self.snapshot.metrics.resources;
        let reported = |value: u64, flag: u64| {
            if resource.available & flag != 0 {
                format!("{} MiB", value / 1048576)
            } else {
                "not reported".into()
            }
        };
        let lines = vec![
            format!(
                "MEMORY  {:.2} GiB RSS · {:.2} GiB peak",
                self.snapshot.metrics.current_rss_bytes as f64 / 1073741824.0,
                self.snapshot.metrics.peak_rss_bytes as f64 / 1073741824.0
            ),
            format!(
                "MODEL  {} mapped · {} prepared",
                reported(
                    resource.model_mapped_bytes,
                    raw::YVEX_EXECUTION_RESOURCE_MODEL_AVAILABLE as u64
                ),
                reported(
                    resource.model_prepared_bytes,
                    raw::YVEX_EXECUTION_RESOURCE_MODEL_AVAILABLE as u64
                )
            ),
            format!(
                "STATE  {} physical · {} workspace · {} transient",
                reported(
                    resource.session_physical_state_bytes,
                    raw::YVEX_EXECUTION_RESOURCE_SESSION_AVAILABLE as u64
                ),
                reported(
                    resource.workspace_current_bytes,
                    raw::YVEX_EXECUTION_RESOURCE_WORKSPACE_AVAILABLE as u64
                ),
                reported(
                    resource.transient_current_bytes,
                    raw::YVEX_EXECUTION_RESOURCE_TRANSIENT_AVAILABLE as u64
                )
            ),
            format!(
                "PLACEMENT  {} · physical residency {}",
                resource_json(resource)["placement"].as_str().unwrap(),
                if resource.available
                    & raw::YVEX_EXECUTION_RESOURCE_PHYSICAL_RESIDENCY_AVAILABLE as u64
                    != 0
                {
                    "reported"
                } else {
                    "not reported"
                }
            ),
        ];
        presentation::lines(&lines, width, styled)
            .map_err(|error| invalid("presentation", &error.to_string()))
    }
    pub fn json(&self) -> Value {
        let status = &self.snapshot;
        let metrics = &status.metrics;
        json!({
            "schema": "yvex.host.status.v1", "protocol": ffi::LOCAL_PROTOCOL_VERSION,
            "status": status.status, "host_ready": status.host_ready != 0,
            "engine_count": status.engine_count, "loaded_engine_count": status.loaded_engine_count,
            "draining_engine_count": status.draining_engine_count, "maximum_engines": status.maximum_engines,
            "workers": status.worker_count, "session_count": status.session_count,
            "requests": status.request_count, "uptime_ns": metrics.uptime_ns,
            "model_open_count": metrics.model_open_count, "model_close_count": metrics.model_close_count,
            "artifact_open_count": metrics.artifact_open_count, "binding_open_count": metrics.binding_open_count,
            "materialization_count": metrics.materialization_count,
            "residency_build_count": metrics.residency_build_count,
            "output_head_upload_count": metrics.output_head_upload_count,
            "sessions": status.session_count, "active_sessions": metrics.active_sessions,
            "total_sessions": metrics.total_sessions, "queue_depth": metrics.queue_depth,
            "queue_capacity": metrics.queue_capacity, "active_requests": metrics.active_requests,
            "completed_requests": metrics.completed_requests, "failed_requests": metrics.failed_requests,
            "cancelled_requests": metrics.cancelled_requests,
            "openai_enabled": status.openai_listener_enabled != 0,
            "openai_ready": status.openai_listener_ready != 0, "openai_port": status.openai_port,
            "active_http_requests": metrics.active_http_requests,
            "completed_http_requests": metrics.completed_http_requests,
            "failed_http_requests": metrics.failed_http_requests,
            "cancelled_http_requests": metrics.cancelled_http_requests,
            "telemetry_dropped": metrics.telemetry_dropped,
            "rss_bytes": metrics.current_rss_bytes, "peak_rss_bytes": metrics.peak_rss_bytes,
            "mapped_artifact_bytes": metrics.mapped_artifact_bytes,
            "resident_host_bytes": metrics.resident_host_bytes,
            "resident_device_bytes": metrics.resident_device_bytes,
            "resources": resource_json(&metrics.resources),
        })
    }

    pub fn compact(&self, width: usize, styled: bool) -> Result<String, replai::EditError> {
        use replai::Role;
        let status = &self.snapshot;
        let metrics = &status.metrics;
        let seconds = metrics.uptime_ns / 1_000_000_000;
        let ready = status.host_ready != 0;
        let detail = format!(
            " · up {:02}:{:02}:{:02} · workers {} · queue {}/{}",
            seconds / 3600,
            seconds / 60 % 60,
            seconds % 60,
            status.worker_count,
            metrics.queue_depth,
            metrics.queue_capacity
        );
        let mut output = presentation::spans(
            &[
                (Role::Accent, "HOST  "),
                (
                    if ready { Role::Success } else { Role::Warning },
                    if ready { "ready" } else { "starting" },
                ),
                (Role::Dim, &detail),
            ],
            width,
            styled,
        )?;
        for (label, value) in [
            (
                "MODELS  ",
                format!(
                    "{} loaded · {} known · capacity {}",
                    status.loaded_engine_count, status.engine_count, status.maximum_engines
                ),
            ),
            (
                "SESSIONS  ",
                format!(
                    "{} active · {} total",
                    metrics.active_sessions, metrics.total_sessions
                ),
            ),
            (
                "MEMORY  ",
                format!(
                    "{:.2} GiB RSS · {:.2} MiB work",
                    metrics.current_rss_bytes as f64 / (1024.0 * 1024.0 * 1024.0),
                    metrics.resources.workspace_current_bytes as f64 / (1024.0 * 1024.0)
                ),
            ),
        ] {
            output.push_str(&presentation::spans(
                &[(Role::Accent, label), (Role::Default, &value)],
                width,
                styled,
            )?);
        }
        Ok(output)
    }
}

pub(crate) fn engines(socket: Option<&str>) -> Result<Vec<raw::yvex_server_engine_summary>, Error> {
    let mut client = Client::connect(socket)?;
    let request = client.request(raw::yvex_client_operation_YVEX_CLIENT_OP_ENGINE_LIST);
    client.send(&request)?;
    let mut engines = Vec::new();
    loop {
        let reply = response(&mut client, &request)?;
        if reply.kind == raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK {
            break;
        }
        if reply.kind != raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ENGINE {
            return Err(invalid(
                "client.engines",
                "unexpected engine catalog response",
            ));
        }
        if engines.len() >= raw::YVEX_SERVER_IMPLEMENTATION_MAXIMUM_ENGINES as usize {
            return Err(invalid(
                "client.engines",
                "engine catalog exceeds the native host bound",
            ));
        }
        engines.push(reply.engine);
    }
    Ok(engines)
}

fn generation(socket: Option<&str>, alias: &str) -> Result<u64, Error> {
    engines(socket)?
        .into_iter()
        .find(|engine| {
            ffi::text(&engine.alias) == alias
                && engine.state == raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_LOADED
        })
        .map(|engine| engine.generation)
        .ok_or_else(|| Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "client.engine-generation".into(),
            message: "requested engine is not loaded".into(),
        })
}

fn engine_state(state: raw::yvex_server_engine_state) -> &'static str {
    match state {
        raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_UNLOADED => "unloaded",
        raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_LOADING => "loading",
        raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_LOADED => "loaded",
        raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_DRAINING => "draining",
        raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_UNLOADING => "unloading",
        raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_FAILED => "failed",
        _ => "unknown",
    }
}

pub(crate) fn engine_kind(kind: raw::yvex_server_engine_kind) -> &'static str {
    match kind {
        raw::yvex_server_engine_kind_YVEX_SERVER_ENGINE_TEXT => "text",
        raw::yvex_server_engine_kind_YVEX_SERVER_ENGINE_MEDIA => "media",
        raw::yvex_server_engine_kind_YVEX_SERVER_ENGINE_FINITE_DECISION => "finite-decision",
        _ => "none",
    }
}

pub(crate) fn strategy(strategy: raw::yvex_server_execution_strategy) -> &'static str {
    match strategy {
        raw::yvex_server_execution_strategy_YVEX_SERVER_EXECUTION_TARGET_ONLY => "target-only",
        raw::yvex_server_execution_strategy_YVEX_SERVER_EXECUTION_SPECULATIVE => "speculative",
        _ => "n/a",
    }
}

pub(crate) fn engine_json(engine: &raw::yvex_server_engine_summary) -> Value {
    let capacity = &engine.capacity;
    let kinds = |mask: u64| {
        ["text", "image", "audio", "video", "file", "tensor"]
            .into_iter()
            .enumerate()
            .filter_map(|(index, name)| (mask & (1 << index) != 0).then_some(name))
            .collect::<Vec<_>>()
    };
    json!({ "alias": ffi::text(&engine.alias), "generation": engine.generation,
        "state": engine_state(engine.state), "backend": engine.backend,
        "engine_kind": engine_kind(engine.engine_kind), "execution_strategy": strategy(engine.execution_strategy),
        "execution_ready": engine.execution_ready != 0, "continuous_batching": engine.continuous_batching_ready != 0,
        "context_capacity": engine.context_capacity, "prefill_chunk_tokens": engine.prefill_chunk_tokens,
        "maximum_new_tokens": engine.maximum_new_tokens, "maximum_sessions": engine.maximum_sessions,
        "configured_physical_sequence_width": engine.concurrent_sequences, "active_work": engine.active_work,
        "sessions": engine.session_count, "attached_clients": engine.attached_client_count,
        "model_leases": engine.model_lease_count,
        "activity": if engine.active_work != 0 || engine.session_count != 0 || engine.model_lease_count != 0 {
            "active" } else { "idle" },
        "mapped_package_bytes": engine.mapped_package_bytes, "resident_host_bytes": engine.resident_host_bytes,
        "resident_device_bytes": engine.resident_device_bytes, "prepared_bytes": engine.prepared_bytes,
        "capacity": { "sessions": capacity.session_capacity, "runnable_work": capacity.runnable_work_capacity,
            "physical_sequence_width": capacity.physical_sequence_width,
            "cooperative_scheduling": capacity.cooperative_scheduling_ready != 0,
            "compatible_operation_batching": capacity.compatible_operation_batching_ready != 0,
            "continuous_batching": capacity.continuous_batching_ready != 0 },
        "resources": resource_json(&engine.resources),
        "capabilities": { "input_mask": engine.capabilities.input_kinds,
            "output_mask": engine.capabilities.output_kinds, "inputs": kinds(engine.capabilities.input_kinds),
            "outputs": kinds(engine.capabilities.output_kinds), "properties": engine.capabilities.execution_properties,
            "maximum_input_parts": engine.capabilities.maximum_input_parts },
        "target": ffi::text(&engine.target_id), "model_identity": ffi::text(&engine.runtime_model_identity),
        "runtime_binding_identity":ffi::text(&engine.runtime_binding_identity),
        "artifact_identity":ffi::text(&engine.artifact_identity),
        "capacity_plan_identity":ffi::text(&engine.capacity_plan_identity),
        "specialization_identity": ffi::text(&engine.specialization_identity) })
}

fn engine_human(engine: &raw::yvex_server_engine_summary) -> String {
    format!(
        "{}  {} · {} · {} · generation {} · sessions {} · work {}",
        ffi::text(&engine.alias),
        engine_state(engine.state),
        ffi::backend_name(engine.backend),
        engine_kind(engine.engine_kind),
        engine.generation,
        engine.session_count,
        engine.active_work
    )
}

pub(crate) fn dispatch(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<String, Error> {
    let operation = invocation.operation.operation_id.as_str();
    let json = invocation.has("--json");
    if operation == "model.load" || operation == "model.unload" {
        use std::io::IsTerminal;
        if operation == "model.load"
            && invocation.positionals.is_empty()
            && (!std::io::stdin().is_terminal() || !std::io::stdout().is_terminal())
        {
            return Err(Error {
                code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
                owner: "model.arguments".into(),
                message: "model load requires MODEL when input is not a terminal".into(),
            });
        }
        // Grammar validation precedes catalog and transport work.
        let context = invocation.value("--ctx").map(positive).transpose()?;
        let loaded = if operation == "model.unload" {
            Some(engines(None)?)
        } else {
            None
        };
        let mut selected = catalog::select_runtime(
            invocation,
            invocation.positionals.first().map(String::as_str),
            loaded.as_deref(),
            width,
            styled,
        )?;
        let mut client = Client::connect(None)?;
        let mut request = client.request(if operation == "model.load" {
            raw::yvex_client_operation_YVEX_CLIENT_OP_ENGINE_LOAD
        } else {
            raw::yvex_client_operation_YVEX_CLIENT_OP_ENGINE_UNLOAD
        });
        ffi::put_text(&mut request.model_alias, &selected.alias)?;
        if let Some(value) = context {
            request.load_context_capacity = value;
        }
        if operation == "model.unload" {
            request.engine_generation = loaded
                .as_deref()
                .unwrap()
                .iter()
                .find(|engine| ffi::text(&engine.alias) == selected.alias)
                .map_or(0, |engine| engine.generation);
        }
        client.send(&request)?;
        let reply = response(&mut client, &request)?;
        if reply.kind != raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ENGINE {
            return Err(invalid("model.runtime", "unexpected engine response"));
        }
        selected.contract["schema"] = json!("yvex.model.runtime.v1");
        selected.contract["operation"] = json!(if operation == "model.load" {
            "load"
        } else {
            "unload"
        });
        selected.contract["generation"] = json!(reply.engine.generation);
        selected.contract["state"] = json!(engine_state(reply.engine.state));
        return if json {
            Ok(format!("{}\n", selected.contract))
        } else {
            presentation::lines(
                &[format!(
                    "MODEL {} · {} · {}",
                    selected.contract["model"].as_str().unwrap(),
                    engine_state(reply.engine.state),
                    ffi::backend_name(reply.engine.backend)
                )],
                width,
                styled,
            )
            .map_err(|error| invalid("presentation", &error.to_string()))
        };
    }
    if operation == "host.status" || operation == "host.memory" {
        let status = host_status(None)?;
        return if json {
            Ok(format!(
                "{}\n",
                if operation == "host.status" {
                    status.json()
                } else {
                    status.memory_json()
                }
            ))
        } else if operation == "host.memory" {
            status.memory(width, styled)
        } else {
            status
                .compact(width, styled)
                .map_err(|error| invalid("presentation", &error.to_string()))
        };
    }
    if matches!(operation, "engine.list" | "engine.show" | "model.active") {
        let filter = invocation.positionals.first().map(String::as_str);
        let selected: Vec<_> = engines(None)?
            .into_iter()
            .filter(|engine| {
                filter.is_none_or(|alias| ffi::text(&engine.alias) == alias)
                    && (operation != "model.active"
                        || matches!(
                            engine.state,
                            raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_LOADED
                                | raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_DRAINING
                                | raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_UNLOADING
                        ))
            })
            .collect();
        if filter.is_some() && selected.is_empty() {
            return Err(Error {
                code: raw::yvex_status_YVEX_ERR_STATE,
                owner: "client.engine".into(),
                message: "requested engine is not known to the host".into(),
            });
        }
        if json {
            let value = if operation == "engine.show" {
                json!({"schema":"yvex.engine.v1", "engine":engine_json(&selected[0])})
            } else {
                json!({"schema": if operation == "model.active" { "yvex.model.active.v1" } else {
                    "yvex.engine.list.v1" }, "engines": selected.iter().map(engine_json).collect::<Vec<_>>()})
            };
            return Ok(format!("{value}\n"));
        }
        if selected.is_empty() {
            return Ok("no engines known to this host\n".into());
        }
        return presentation::lines(
            &selected.iter().map(engine_human).collect::<Vec<_>>(),
            width,
            styled,
        )
        .map_err(|error| invalid("presentation", &error.to_string()));
    }
    if operation == "engine.load" || operation == "engine.unload" {
        let alias = invocation.positionals.first().ok_or_else(|| Error {
            code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
            owner: "engine.arguments".into(),
            message: if operation == "engine.load" {
                "engine load requires PROFILE; use yvex model load [MODEL] for selection"
            } else {
                "engine unload requires ENGINE"
            }
            .into(),
        })?;
        let mut client = Client::connect(None)?;
        let mut request = client.request(if operation == "engine.load" {
            raw::yvex_client_operation_YVEX_CLIENT_OP_ENGINE_LOAD
        } else {
            raw::yvex_client_operation_YVEX_CLIENT_OP_ENGINE_UNLOAD
        });
        ffi::put_text(&mut request.model_alias, alias)?;
        request.load_context_capacity = invocation
            .value("--ctx")
            .map_or(Ok(0), str::parse::<u64>)
            .map_err(|_| invalid("engine.load", "invalid context capacity"))?;
        if invocation.has("--ctx") && request.load_context_capacity == 0 {
            return Err(invalid("engine.load", "context capacity must be positive"));
        }
        if operation == "engine.unload" {
            request.engine_generation = generation(None, alias)?;
        }
        client.send(&request)?;
        let reply = response(&mut client, &request)?;
        if reply.kind != raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ENGINE {
            return Err(invalid("client.engine", "unexpected engine response"));
        }
        return if json {
            Ok(format!(
                "{}\n",
                json!({"schema":"yvex.engine.v1", "engine":engine_json(&reply.engine)})
            ))
        } else {
            presentation::lines(&[engine_human(&reply.engine)], width, styled)
                .map_err(|error| invalid("presentation", &error.to_string()))
        };
    }
    session_dispatch(invocation, width, styled)
}

fn session_dispatch(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<String, Error> {
    let operation = invocation.operation.operation_id.as_str();
    let native_operation = match operation {
        "host.stop" => raw::yvex_client_operation_YVEX_CLIENT_OP_RUNTIME_STOP,
        "session.new" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_NEW,
        "session.list" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_LIST,
        "session.show" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_SHOW,
        "session.attach" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_ATTACH,
        "session.detach" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_DETACH,
        "session.reset" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_RESET,
        "session.fork" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_FORK,
        "session.close" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_CLOSE,
        "session.state.save" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_STATE_SAVE,
        "session.state.restore" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_STATE_RESTORE,
        "generation.cancel" => raw::yvex_client_operation_YVEX_CLIENT_OP_GENERATION_CANCEL,
        _ => {
            return Err(invalid(
                "client.dispatch",
                "operation has no runtime request projection",
            ));
        }
    };
    let mut client = Client::connect(None)?;
    let mut request = client.request(native_operation);
    if let Some(alias) = invocation.value("--model") {
        ffi::put_text(&mut request.model_alias, alias)?;
        request.engine_generation = generation(None, alias)?;
    }
    if let Some(name) = invocation.positionals.first() {
        ffi::put_text(&mut request.session_name, name)?;
    }
    if operation == "session.fork" {
        ffi::put_text(&mut request.fork_session_name, &invocation.positionals[1])?;
        request.maximum_prefix_bytes = positive(&invocation.positionals[2])?;
    } else if operation.starts_with("session.state.") {
        ffi::put_text(&mut request.state_path, &invocation.positionals[1])?;
        if operation == "session.state.restore" {
            request.maximum_state_file_bytes = positive(&invocation.positionals[2])?;
        }
    }
    client.send(&request)?;
    let mut sessions = Vec::new();
    loop {
        let reply = response(&mut client, &request)?;
        if reply.kind == raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK {
            if operation == "session.list" {
                break;
            }
            let reason = ffi::text(&reply.reason);
            if reply.state_checkpoint.schema_version != 0 {
                return Ok(format!(
                    "{} · position {} · {} bytes · digest {}\n",
                    reason,
                    reply.state_checkpoint.committed_sequence_length,
                    reply.state_checkpoint.file_bytes,
                    ffi::text(&reply.state_checkpoint.file_digest)
                ));
            }
            return Ok(format!(
                "{}\n",
                if reason.is_empty() { "ok" } else { &reason }
            ));
        }
        if reply.kind != raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_SESSION {
            return Err(invalid("client.session", "unexpected session response"));
        }
        sessions.push(json!({ "name": ffi::text(&reply.session_name), "identity": ffi::text(&reply.session_identity),
            "state": ffi::session_state_name(reply.session_state), "position": reply.final_position,
            "turns": reply.turn_count }));
        if operation != "session.list" {
            break;
        }
    }
    if invocation.has("--json") {
        let value = if operation == "session.list" {
            json!({ "schema": "yvex.session.list.v1", "sessions": sessions })
        } else {
            json!({ "schema": "yvex.session.v1", "session": sessions.first() })
        };
        return Ok(format!("{value}\n"));
    }
    if sessions.is_empty() {
        return Ok("no sessions known to this engine\n".into());
    }
    let rows = sessions
        .iter()
        .map(|session| {
            vec![
                session["name"].as_str().unwrap().into(),
                session["state"].as_str().unwrap().into(),
                session["position"].to_string(),
                session["turns"].to_string(),
            ]
        })
        .collect::<Vec<_>>();
    presentation::table(
        &[
            ("SESSION", Alignment::Left),
            ("STATE", Alignment::Left),
            ("POS", Alignment::Right),
            ("TURNS", Alignment::Right),
        ],
        &rows,
        width,
        styled,
    )
    .map_err(|error| invalid("presentation", &error.to_string()))
}

fn positive(value: &str) -> Result<u64, Error> {
    value
        .parse::<u64>()
        .ok()
        .filter(|value| *value != 0)
        .ok_or_else(|| Error {
            code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
            owner: "client.bound".into(),
            message: "an explicit positive bound is required".into(),
        })
}

pub(crate) fn resource_json(resource: &raw::yvex_execution_resource_summary) -> Value {
    let placement = match resource.placement {
        raw::yvex_execution_resource_placement_YVEX_EXECUTION_PLACEMENT_EXPLICIT_HOST => "explicit-host",
        raw::yvex_execution_resource_placement_YVEX_EXECUTION_PLACEMENT_MANAGED_UNIFIED => "managed-unified",
        raw::yvex_execution_resource_placement_YVEX_EXECUTION_PLACEMENT_ARTIFACT_MAPPED => "artifact-mapped",
        raw::yvex_execution_resource_placement_YVEX_EXECUTION_PLACEMENT_ARTIFACT_MAPPED_DEVICE_ADDRESSABLE =>
            "mapped-device-addressable",
        raw::yvex_execution_resource_placement_YVEX_EXECUTION_PLACEMENT_COMPOSITE => "composite",
        _ => "unknown",
    };
    json!({
        "schema_version": resource.schema_version, "available": resource.available, "placement": placement,
        "physical_residency_known": resource.available
            & raw::YVEX_EXECUTION_RESOURCE_PHYSICAL_RESIDENCY_AVAILABLE as u64 != 0,
        "unified_memory": resource.available & raw::YVEX_EXECUTION_RESOURCE_UNIFIED_MEMORY as u64 != 0,
        "components": resource.component_count,
        "model_artifact_bytes": resource.model_artifact_bytes, "model_mapped_bytes": resource.model_mapped_bytes,
        "model_prepared_bytes": resource.model_prepared_bytes,
        "model_explicit_host_bytes": resource.model_explicit_host_bytes,
        "model_explicit_device_bytes": resource.model_explicit_device_bytes,
        "model_device_addressable_bytes": resource.model_device_addressable_bytes,
        "session_attention_allocated_bytes": resource.session_attention_allocated_bytes,
        "session_attention_resident_bytes": resource.session_attention_resident_bytes,
        "session_attention_virtual_bytes": resource.session_attention_virtual_bytes,
        "session_attention_page_table_bytes": resource.session_attention_page_table_bytes,
        "session_recurrent_state_bytes": resource.session_recurrent_state_bytes,
        "session_convolution_state_bytes": resource.session_convolution_state_bytes,
        "session_candidate_state_bytes": resource.session_candidate_state_bytes,
        "session_physical_state_bytes": resource.session_physical_state_bytes,
        "activation_arena_current_bytes": resource.activation_arena_current_bytes,
        "activation_arena_peak_bytes": resource.activation_arena_peak_bytes,
        "workspace_current_bytes": resource.workspace_current_bytes,
        "workspace_peak_bytes": resource.workspace_peak_bytes,
        "transient_current_bytes": resource.transient_current_bytes,
        "transient_peak_bytes": resource.transient_peak_bytes,
        "process_rss_current_bytes": resource.process_rss_current_bytes,
        "process_rss_peak_bytes": resource.process_rss_peak_bytes,
        "logical_upload_bytes": resource.logical_upload_bytes,
        "logical_download_bytes": resource.logical_download_bytes,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use raw::*;

    #[test]
    fn logs_preserve_metric_fields_and_resource_context_at_narrow_widths() {
        let mut event = decode();
        event.wall_time_ns = 1_700_000_000_123_000_000;
        event.severity = yvex_server_event_severity_YVEX_SERVER_SEVERITY_INFO;
        event.kind = yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_COMPLETED;
        event.value_a = 22;
        event.value_b = 22;
        event.seconds = 4.142;
        event.measurement = yvex_execution_measurement {
            schema_version: YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1,
            scope: yvex_execution_measurement_scope_YVEX_EXECUTION_SCOPE_PREFILL,
            work_unit: yvex_execution_work_unit_YVEX_EXECUTION_WORK_TOKENS,
            available: u64::from(
                YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE
                    | YVEX_EXECUTION_MEASUREMENT_DURATION_AVAILABLE
                    | YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE,
            ),
            completed_units: 22,
            total_units: 22,
            duration_ns: 4_142_000_000,
            cumulative_rate: 22.0 / 4.142,
            ..Default::default()
        };
        for width in [40, 80, 180] {
            let text = event_render(&event, false, width, false).unwrap();
            assert!(text.contains("22/22 tokens"), "{width}: {text}");
            assert!(text.contains("4.142s"), "{width}: {text}");
            assert!(text.contains("avg=5.31t/s"), "{width}: {text}");
            assert!(!text.contains('\x1b'));
        }
        event.severity = yvex_server_event_severity_YVEX_SERVER_SEVERITY_ERROR;
        let mut summary = yvex_server_summary::default();
        assert!(
            resource_render(&event, &summary, 80, false)
                .unwrap()
                .is_none()
        );
        summary.metrics.resources.available =
            u64::from(YVEX_EXECUTION_RESOURCE_WORKSPACE_AVAILABLE);
        summary.metrics.resources.schema_version = YVEX_EXECUTION_RESOURCE_SCHEMA_V1;
        summary.metrics.resources.workspace_current_bytes = 1_073_741_824;
        let text = resource_render(&event, &summary, 180, false)
            .unwrap()
            .unwrap();
        assert!(text.contains("22:13:20 yvex: host resources"), "{text}");
        assert!(text.contains("workspace=1.0GiB"), "{text}");
        assert!(
            !text.contains("main/r5") && !text.contains("ERROR"),
            "{text}"
        );
    }

    fn decode() -> yvex_server_event {
        let mut event = yvex_server_event {
            kind: raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_PROGRESS,
            engine_kind: raw::yvex_server_engine_kind_YVEX_SERVER_ENGINE_TEXT,
            value_a: 280,
            value_b: 307,
            value_c: 97,
            seconds: 40.0,
            rate: 999.0,
            ..Default::default()
        };
        ffi::put_text(&mut event.session_id, "main").unwrap();
        ffi::put_text(&mut event.request_id, "r5").unwrap();
        ffi::put_text(&mut event.phase, "answer").unwrap();
        event.measurement = yvex_execution_measurement {
            schema_version: YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1,
            scope: raw::yvex_execution_measurement_scope_YVEX_EXECUTION_SCOPE_SUBSEQUENT_DECODE,
            work_unit: raw::yvex_execution_work_unit_YVEX_EXECUTION_WORK_TOKENS,
            available: u64::from(
                YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE
                    | YVEX_EXECUTION_MEASUREMENT_ROLLING_RATE_AVAILABLE,
            ),
            cumulative_rate: 6.98,
            rolling_rate: 5.38,
            rolling_units: 32,
            rolling_window_units: 32,
            ..Default::default()
        };
        event
    }

    #[test]
    fn logs_bind_one_envelope_to_producer_time_identity_and_level() {
        let mut event = decode();
        event.wall_time_ns = 1_700_000_000_123_000_000;
        event.sequence = 42;
        event.severity = yvex_server_event_severity_YVEX_SERVER_SEVERITY_INFO;
        let stamp = ffi::event_timestamp(event.wall_time_ns, false);
        assert_eq!(stamp, "22:13:20.123Z");
        assert_eq!(
            ffi::event_timestamp(event.wall_time_ns, true),
            "2023-11-14T22:13:20.123Z"
        );
        let plain = event_human(&event, false);
        assert!(
            plain.starts_with("22:13:20 yvex: main/r5 decode"),
            "{plain}"
        );
        assert_eq!(plain.matches("main/r5").count(), 1);
        assert!(!plain.contains("#42"));
        assert!(event_human(&event, true).contains("main/r5 #42"));
        assert_eq!(plain, event_human(&event, false));
        event.wall_time_ns = 0;
        assert!(event_human(&event, false).starts_with("--:--:--"));
        event.kind = yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FIRST_TOKEN;
        let first = event_human(&event, false);
        assert!(first.contains("first-token 40.000s"));
        assert!(!first.contains("value_b=") && !first.contains(" a="));
        event.kind = yvex_server_event_kind_YVEX_SERVER_EVENT_SESSION_ATTACHED;
        assert!(event_human(&event, false).contains("attached clients=280"));
        event.kind = yvex_server_event_kind_YVEX_SERVER_EVENT_REQUEST_QUEUED;
        assert!(event_human(&event, false).contains("waiting depth=280 capacity=307"));
    }

    #[test]
    fn logs_default_noise_policy_keeps_failures_and_verbose_facts() {
        let mut cadence = EventCadence::default();
        let mut event = decode();
        event.kind = yvex_server_event_kind_YVEX_SERVER_EVENT_REQUEST_QUEUED;
        event.value_a = 1;
        assert!(!cadence.admit(&event, false));
        assert!(cadence.admit(&event, true));
        event.value_a = 2;
        assert!(cadence.admit(&event, false));
        event.kind = yvex_server_event_kind_YVEX_SERVER_EVENT_SESSION_ATTACHED;
        assert!(!cadence.admit(&event, false));
        event.severity = yvex_server_event_severity_YVEX_SERVER_SEVERITY_WARNING;
        assert!(cadence.admit(&event, false));
        event.severity = yvex_server_event_severity_YVEX_SERVER_SEVERITY_INFO;
        event.kind = yvex_server_event_kind_YVEX_SERVER_EVENT_CLIENT_DISCONNECTED;
        ffi::put_text(&mut event.phase, "http:GET /health").unwrap();
        event.value_b = 200;
        event.value_c = 0;
        assert!(!cadence.admit(&event, false));
        assert!(cadence.admit(&event, true));
        event.value_b = 503;
        assert!(cadence.admit(&event, false));
        event.value_b = 200;
        event.value_c = (-yvex_status_YVEX_ERR_CANCELLED) as u64;
        assert!(cadence.admit(&event, false));
        assert!(event_human(&event, false).contains("outcome=cancelled"));
    }

    #[test]
    fn logs_use_typed_severity_and_keep_plain_event_content() {
        use replai::Role;
        let mut event = decode();
        for (kind, role) in [
            (
                yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_PROGRESS,
                Role::Accent,
            ),
            (
                yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED,
                Role::Success,
            ),
            (
                yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_CANCELLED,
                Role::Warning,
            ),
            (
                yvex_server_event_kind_YVEX_SERVER_EVENT_ENGINE_LOAD_FAILED,
                Role::Error,
            ),
            (
                yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FAILED,
                Role::Error,
            ),
        ] {
            event.kind = kind;
            assert_eq!(event_role(&event), role);
        }
        event.kind = yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED;
        event.severity = yvex_server_event_severity_YVEX_SERVER_SEVERITY_ERROR;
        assert_eq!(event_role(&event), Role::Error);
        event.severity = yvex_server_event_severity_YVEX_SERVER_SEVERITY_WARNING;
        assert_eq!(event_role(&event), Role::Warning);
        let machine = event_json(&event);
        for width in [40, 80, 180] {
            let plain = event_render(&event, true, width, false).unwrap();
            assert!(!plain.contains('\x1b'));
            assert!(plain.contains("done"));
            assert!(plain.contains("generated=280"));
            assert_eq!(machine, event_json(&event));
        }
        ffi::put_text(&mut event.session_id, "unsafe\x1b[2J").unwrap();
        assert!(
            !event_render(&event, false, 80, false)
                .unwrap()
                .contains('\x1b')
        );
    }

    #[test]
    fn logs_keep_typed_rates_populations_and_failure_identity() {
        let mut event = decode();
        let compact = event_human(&event, false);
        assert!(compact.contains("gen=280 ctx=307 decode=6.98t/s"));
        assert!(!compact.contains("rolling[") && !compact.contains("reasoning=97"));
        let human = event_human(&event, true);
        assert!(human.contains("generated=280 position=307 phase=answer reasoning=97"));
        assert!(human.contains("decode-avg=6.98tok/s rolling[32]=5.38tok/s"));
        assert!(!human.contains("999"));
        event.measurement.rolling_units = 10;
        assert!(event_human(&event, true).contains("rolling[10/32]"));
        event.measurement.available = 0;
        assert!(!event_human(&event, false).contains("tok/s"));
        event.measurement.schema_version = 0;
        event.rate = 4.0;
        assert!(event_human(&event, false).contains("rate=4.00tok/s"));
        for (kind, stop, name) in [
            (
                raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_CANCELLED,
                5,
                "cancelled",
            ),
            (
                raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FAILED,
                6,
                "fail",
            ),
        ] {
            event.kind = kind;
            event.value_c = stop;
            assert!(event_human(&event, false).contains(name));
        }
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED;
        event.engine_kind = raw::yvex_server_engine_kind_YVEX_SERVER_ENGINE_MEDIA;
        assert!(event_human(&event, false).contains("frames=280 bytes=307 audio_samples=6"));
        assert!(!event_human(&event, false).contains("tok/s"));
        event.engine_kind = raw::yvex_server_engine_kind_YVEX_SERVER_ENGINE_TEXT;
        event.measurement = decode().measurement;
        event.measurement.scope =
            raw::yvex_execution_measurement_scope_YVEX_EXECUTION_SCOPE_TOTAL_OPERATION;
        event.proposed_tokens = 100;
        event.accepted_tokens = 39;
        let human = event_human(&event, true);
        assert!(human.contains("total-avg=6.98") && human.contains("spec-accepted=39/100"));
        assert!(!human.contains("decode-avg"));
        let compact = event_human(&event, false);
        assert!(compact.contains("total=6.98t/s") && !compact.contains("spec-accepted"));
    }

    #[test]
    fn logs_preserve_http_outcomes_and_owner_counts() {
        let mut event = decode();
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_REQUEST_RECEIVED;
        ffi::put_text(&mut event.phase, "http:GET /v1/models").unwrap();
        event.value_a = 44150;
        assert!(event_human(&event, false).contains("openai GET /v1/models received"));
        assert!(!event_human(&event, false).contains("peer="));
        assert!(event_human(&event, true).contains("peer=127.0.0.1:44150 received"));
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_CLIENT_DISCONNECTED;
        event.value_b = 200;
        event.value_c = 0;
        assert!(event_human(&event, false).contains("status=200 outcome=complete"));
        event.value_c = (-yvex_status_YVEX_ERR_CANCELLED) as u64;
        assert!(event_human(&event, false).contains("status=200 outcome=cancelled"));
        event.value_b = 504;
        event.value_c = (-yvex_status_YVEX_ERR_TIMEOUT) as u64;
        let human = event_human(&event, false);
        assert!(human.contains("outcome=failed") && human.contains("YVEX_ERR_TIMEOUT"));
        event.value_b = 0;
        assert!(event_human(&event, false).contains("status=unavailable outcome=incomplete"));
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_SESSION_CREATED;
        event.value_c = 1;
        assert!(event_human(&event, false).contains("created active_sessions=1"));
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_SESSION_CLOSED;
        assert!(event_human(&event, false).contains("closed active_sessions=0"));
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_SESSION_RESET;
        let reset = event_human(&event, false);
        assert!(reset.contains("main/r5") && reset.contains("reset"));
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_REQUEST_STARTED;
        assert!(event_human(&event, false).contains("input_bytes=44150"));
        ffi::put_text(&mut event.provider_request_identity, "provider").unwrap();
        assert!(event_human(&event, false).contains("messages=44150"));
    }

    #[test]
    fn logs_do_not_invent_progress_denominators_or_rates() {
        let mut event = decode();
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_PROGRESS;
        event.measurement.available = u64::from(
            YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE
                | YVEX_EXECUTION_MEASUREMENT_DURATION_AVAILABLE
                | YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE,
        );
        event.measurement.scope =
            raw::yvex_execution_measurement_scope_YVEX_EXECUTION_SCOPE_PREFILL;
        event.measurement.completed_units = 330;
        event.measurement.total_units = 330;
        event.measurement.duration_ns = 28_070_000_000;
        event.measurement.cumulative_rate = 11.76;
        assert!(event_human(&event, true).contains("tokens=330/330 elapsed=28.07s avg=11.76tok/s"));
        event.measurement.available = 0;
        let human = event_human(&event, false);
        assert!(human.contains("330/unknown") && !human.contains("tok/s"));
        event.measurement.work_unit = raw::yvex_execution_work_unit_YVEX_EXECUTION_WORK_BYTES;
        assert!(event_human(&event, false).contains("330/unknown bytes"));
        assert!(!event_human(&event, false).contains("tok/s"));
    }

    #[test]
    fn logs_coalesce_chunks_not_phase_transitions_or_terminal_events() {
        let mut event = decode();
        let mut cadence = EventCadence::default();
        assert!(cadence.admit(&event, false));
        assert!(!cadence.admit(&event, false));
        ffi::put_text(&mut event.phase, "reasoning").unwrap();
        assert!(cadence.admit(&event, false));
        event.seconds += 1.0;
        assert!(cadence.admit(&event, false));
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_STARTED;
        event.measurement.available = u64::from(
            YVEX_EXECUTION_MEASUREMENT_DURATION_AVAILABLE
                | YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE,
        );
        event.measurement.duration_ns = 0;
        event.measurement.completed_units = 0;
        event.measurement.total_units = 100;
        assert!(cadence.admit(&event, false));
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_PROGRESS;
        let shown = (1..=100)
            .filter(|index| {
                event.measurement.duration_ns = index * 100_000_000;
                event.measurement.completed_units = *index;
                cadence.admit(&event, false)
            })
            .count();
        assert_eq!(shown, 9);
        assert!(cadence.admit(&event, true));
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_COMPLETED;
        assert!(cadence.admit(&event, false));
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_CANCELLED;
        assert!(cadence.admit(&event, false));
    }

    #[test]
    fn logs_host_resource_snapshots_never_infer_gpu_residency() {
        let mut event = decode();
        let mut cadence = ResourceCadence::default();
        let snapshots = (0..100)
            .filter(|index| {
                event.monotonic_time_ns = index * 1_000_000_000;
                cadence.due(&event)
            })
            .count();
        assert_eq!(snapshots, 10);
        event.kind = raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED;
        assert!(cadence.due(&event));
        let mut summary = yvex_server_summary::default();
        assert!(resource_human(&summary).is_none());
        summary.metrics.resources.schema_version = YVEX_EXECUTION_RESOURCE_SCHEMA_V1;
        summary.metrics.resources.available = u64::from(YVEX_EXECUTION_RESOURCE_PROCESS_AVAILABLE);
        summary.metrics.resources.process_rss_current_bytes = 98_784_247_808;
        let human = resource_human(&summary).unwrap();
        assert!(human.contains("process-rss=92.0GiB"));
        assert!(
            !human.contains("workspace=")
                && !human.contains("resident=")
                && !human.contains("device-alloc=")
        );
    }
    #[test]
    fn no_runtime_does_not_become_capability() {
        assert!(host_status(Some("/nonexistent-yvex-shell-test/socket")).is_err());
    }
    #[test]
    fn projection_does_not_invent_residency() {
        let report = HostStatus {
            snapshot: Default::default(),
        };
        assert_eq!(
            report.json()["resources"]["physical_residency_known"],
            false
        );
        assert_eq!(report.json()["schema"], "yvex.host.status.v1");
        assert!(report.compact(120, false).unwrap().contains("starting"));
        let human = report.memory(160, false).unwrap();
        assert!(human.contains("not reported"));
        assert!(!human.contains("0 MiB physical"));
    }
}
