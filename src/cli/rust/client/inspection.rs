// Finite, bounded runtime observations shared by management and terminal consumers.
use super::{invalid, response};
use crate::ffi::{self, Client, Error, raw};

pub(crate) fn engines(socket: Option<&str>) -> Result<Vec<raw::yvex_server_engine_summary>, Error> {
    Ok(engine_catalog(socket, None)?.1)
}

pub(crate) fn engine_catalog(
    socket: Option<&str>,
    timeout: Option<u64>,
) -> Result<(String, Vec<raw::yvex_server_engine_summary>), Error> {
    let mut client = Client::connect(socket)?;
    if let Some(timeout) = timeout {
        client.timeout(timeout)?;
    }
    let host = client.host_identity()?;
    let deadline =
        timeout.map(|ms| std::time::Instant::now() + std::time::Duration::from_millis(ms));
    let request = client.request(raw::yvex_client_operation_YVEX_CLIENT_OP_ENGINE_LIST);
    client.send(&request)?;
    let mut engines = Vec::new();
    loop {
        if let Some(deadline) = deadline {
            let remaining = deadline
                .saturating_duration_since(std::time::Instant::now())
                .as_millis();
            if remaining == 0 {
                return Err(invalid(
                    "client.engines",
                    "engine inspection deadline exceeded",
                ));
            }
            client.timeout(remaining as u64)?;
        }
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
    Ok((host, engines))
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

/// Finite server-authored trace, not a subscription or a second telemetry source.
pub(crate) fn recent_events() -> Result<Vec<raw::yvex_server_event>, Error> {
    let mut client = Client::connect(None)?;
    client.timeout(2000)?;
    let mut request = client.request(raw::yvex_client_operation_YVEX_CLIENT_OP_RUNTIME_TRACE);
    request.trace_level = raw::yvex_server_trace_level_YVEX_SERVER_TRACE_STAGES;
    client.send(&request)?;
    let deadline = std::time::Instant::now() + std::time::Duration::from_secs(2);
    let mut events = std::collections::VecDeque::new();
    for _ in 0..8192 {
        let remaining = deadline
            .saturating_duration_since(std::time::Instant::now())
            .as_millis();
        if remaining == 0 {
            break;
        }
        client.timeout(remaining as u64)?;
        let reply = response(&mut client, &request)?;
        if reply.kind == raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK {
            return Ok(events.into());
        }
        if reply.kind != raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_EVENT {
            return Err(invalid("host.trace", "unexpected trace response"));
        }
        if events.len() == 32 {
            events.pop_front();
        }
        events.push_back(reply.event);
        if std::time::Instant::now() >= deadline {
            break;
        }
    }
    Err(invalid(
        "host.trace",
        "trace exceeds bounded inspection budget",
    ))
}

/// Preserve the inspected generation, including across the confirmation prompt.
/// The native lifecycle alone decides whether retirement is admitted.
pub(crate) fn unload_selected(
    engine: &raw::yvex_server_engine_summary,
    host: &str,
) -> Result<(), Error> {
    let mut client = Client::connect(None)?;
    if client.host_identity()? != host {
        return Err(invalid(
            "engine.unload",
            "selected host lifetime changed; retirement not dispatched",
        ));
    }
    let mut request = client.request(raw::yvex_client_operation_YVEX_CLIENT_OP_ENGINE_UNLOAD);
    ffi::put_text(&mut request.model_alias, &ffi::text(&engine.alias))?;
    request.engine_generation = engine.generation;
    client.send(&request)?;
    let reply = response(&mut client, &request)?;
    if reply.kind != raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ENGINE
        || reply.engine.generation != engine.generation
        || ffi::text(&reply.engine.alias) != ffi::text(&engine.alias)
    {
        return Err(invalid(
            "engine.unload",
            "unexpected retirement response identity; do not retry blindly",
        ));
    }
    Ok(())
}
