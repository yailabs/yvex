// Rust owns foreground mode and signals. C owns deployment admission and work.
use crate::{
    client,
    ffi::{self, Error, Host, raw},
    presentation,
    registry::Invocation,
};
use signal_hook::{
    consts::{SIGINT, SIGTERM},
    iterator::Signals,
};
use std::{
    io::{self, Write},
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
    },
    time::Duration,
};

fn failure(owner: &str, reason: impl ToString) -> Error {
    Error {
        code: raw::yvex_status_YVEX_ERR_IO,
        owner: owner.into(),
        message: reason.to_string(),
    }
}

fn positive(invocation: &Invocation<'_>, flag: &str, default: u64) -> Result<u64, Error> {
    match invocation.value(flag) {
        None => Ok(default),
        Some(value) => value
            .parse::<u64>()
            .ok()
            .filter(|number| *number != 0)
            .ok_or_else(|| Error {
                code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
                owner: "serve.options".into(),
                message: format!("{flag} requires a positive integer"),
            }),
    }
}

fn options(invocation: &Invocation<'_>) -> Result<raw::yvex_server_options, Error> {
    let trace = match invocation.value("--trace-level").unwrap_or("stages") {
        "summary" => raw::yvex_server_trace_level_YVEX_SERVER_TRACE_SUMMARY,
        "stages" => raw::yvex_server_trace_level_YVEX_SERVER_TRACE_STAGES,
        "tokens" => raw::yvex_server_trace_level_YVEX_SERVER_TRACE_TOKENS,
        "full" => raw::yvex_server_trace_level_YVEX_SERVER_TRACE_FULL,
        _ => unreachable!("registry rejected the trace spelling"),
    };
    let console = match invocation.value("--logs").unwrap_or("human") {
        "human" => raw::yvex_server_console_kind_YVEX_SERVER_CONSOLE_HUMAN,
        "json" => raw::yvex_server_console_kind_YVEX_SERVER_CONSOLE_RAW,
        "off" => raw::yvex_server_console_kind_YVEX_SERVER_CONSOLE_OFF,
        _ => unreachable!("registry rejected the console spelling"),
    };
    Ok(raw::yvex_server_options {
        schema_version: raw::YVEX_SERVER_OPTIONS_SCHEMA_CURRENT,
        request_queue_capacity: 16,
        worker_count: positive(invocation, "--workers", 1)?,
        maximum_engines: positive(
            invocation,
            "--max-engines",
            raw::YVEX_SERVER_DEFAULT_MAXIMUM_ENGINES as u64,
        )?,
        openai_timeout_ms: positive(invocation, "--openai-timeout-ms", 600000)?,
        openai_port: u16::try_from(positive(invocation, "--openai-port", 8001)?)
            .map_err(|_| failure("serve.options", "port exceeds 65535"))?,
        openai_enabled: i32::from(invocation.value("--openai") != Some("off")),
        trace_level: trace,
        console,
        trace_content: i32::from(invocation.has("--trace-content")),
        ..Default::default()
    })
}

fn output(text: &str) -> Result<(), Error> {
    let mut stdout = io::stdout().lock();
    stdout
        .write_all(text.as_bytes())
        .and_then(|()| stdout.flush())
        .map_err(|error| failure("serve.output", error))
}

pub(crate) fn run(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<(), Error> {
    let options = options(invocation)?;
    let socket = invocation
        .value("--socket")
        .map(String::from)
        .map_or_else(ffi::default_socket, Ok)?;
    if client::host_status(Some(&socket)).is_ok() {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "serve.admission".into(),
            message: "host already running; inspect with yvex host status".into(),
        });
    }
    // Install before creating C workers. Shutdown always uses the typed lifecycle.
    let mut signals =
        Signals::new([SIGINT, SIGTERM]).map_err(|error| failure("serve.signals", error))?;
    let signal_handle = signals.handle();
    let mut host = Host::create(options, &socket)?;
    host.start()?;
    let header = format!(
        "YVEX {}  HOST ready · {} workers · capacity {}\n\
        native {} · protocol {}\n{}\nCtrl-C to stop\n",
        ffi::version(),
        options.worker_count,
        options.maximum_engines,
        socket,
        ffi::LOCAL_PROTOCOL_VERSION,
        if options.openai_enabled != 0 {
            format!("OpenAI 127.0.0.1:{} · loopback", options.openai_port)
        } else {
            "OpenAI disabled".into()
        }
    );
    output(
        &presentation::lines(
            &header.lines().map(String::from).collect::<Vec<_>>(),
            width,
            styled,
        )
        .map_err(|error| failure("presentation", error))?,
    )?;
    let done = Arc::new(AtomicBool::new(false));
    let reader = host.events();
    let log_socket = socket.as_str();
    let console = options.console;
    std::thread::scope(|scope| {
        let signal_thread = scope.spawn(|| {
            if signals.forever().next().is_some() {
                client::stop_host(&socket)?;
            }
            Ok::<(), Error>(())
        });
        let log_done = done.clone();
        let log_thread = scope.spawn(move || {
            let mut cursor = 0;
            let mut cadence = client::EventCadence::default();
            let mut resources = client::ResourceCadence::default();
            loop {
                match reader.next(cursor) {
                    Ok(event) => {
                        cursor = event.sequence;
                        if console != raw::yvex_server_console_kind_YVEX_SERVER_CONSOLE_OFF {
                            let text = if console == raw::yvex_server_console_kind_YVEX_SERVER_CONSOLE_RAW {
                                format!("{}\n", client::event_json(&event))
                            } else if cadence.admit(&event, false) {
                                let rendered = client::event_render(&event, false, width, styled)
                                    .and_then(|mut text| {
                                        if resources.due(&event)
                                            && let Ok(summary) = reader.summary()
                                        {
                                            let resource = client::resource_render(&event, &summary, width, styled)?;
                                            if let Some(line) = resource {
                                                text.push_str(&line);
                                            }
                                        }
                                        Ok(text)
                                    });
                                match rendered {
                                    Ok(text) => text,
                                    Err(error) => {
                                        let _ = client::stop_host(log_socket);
                                        return Err(failure("presentation", error));
                                    }
                                }
                            } else { String::new() };
                            if let Err(error) = output(&text) {
                                let _ = client::stop_host(log_socket);
                                return Err(error);
                            }
                        }
                        if event.kind == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_RUNTIME_SHUTDOWN_COMPLETE {
                            break;
                        }
                    }
                    Err(error) if error.code == raw::yvex_status_YVEX_ERR_STATE => {
                        if log_done.load(Ordering::Acquire) { break; }
                        std::thread::sleep(Duration::from_millis(10));
                    }
                    Err(error) => { let _ = client::stop_host(log_socket); return Err(error); }
                }
            }
            Ok(())
        });
        let result = host.serve();
        let cleanup = host.finish();
        done.store(true, Ordering::Release);
        signal_handle.close();
        let signal_result = signal_thread
            .join()
            .map_err(|_| failure("serve.signals", "coordinator panicked"))?;
        let log_result = log_thread
            .join()
            .map_err(|_| failure("serve.logs", "observer panicked"))?;
        result.and(cleanup).and(signal_result).and(log_result)
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::registry::Registry;
    #[test]
    fn options_match_existing_defaults() {
        let registry = Registry::embedded().unwrap();
        let invocation = registry
            .parse(&["serve".into(), "--openai".into(), "off".into()])
            .unwrap();
        let options = options(&invocation).unwrap();
        assert_eq!(options.openai_enabled, 0);
        assert_eq!(options.worker_count, 1);
        assert_eq!(options.maximum_engines, 8);
        assert_eq!(
            options.schema_version,
            raw::YVEX_SERVER_OPTIONS_SCHEMA_CURRENT
        );
    }
}
