// Explicitly provisioned Host process ownership. Management itself remains independent.
use crate::{
    client,
    ffi::{self, Client, Host, raw},
    management_jobs as jobs, management_runtime as runtime,
};
use rustix::fs::{FlockOperation, OFlags, flock};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::{
    fs,
    io::Write,
    os::unix::fs::{MetadataExt, OpenOptionsExt, PermissionsExt},
    path::{Path, PathBuf},
    process::{Command, Stdio},
    time::{Duration, Instant},
};
type Result<T> = std::result::Result<T, ffi::Error>;
fn fail(s: &str) -> ffi::Error {
    runtime::failure(s)
}
fn uncertain(s: &str) -> ffi::Error {
    let mut e = fail(s);
    e.owner = "management.indeterminate".into();
    e
}
#[derive(Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct Configuration {
    schema: String,
    revision: u64,
    socket: String,
    openai_port: u16,
    openai_enabled: bool,
    #[serde(default)]
    pending_phase: Option<String>,
    #[serde(default)]
    action_id: Option<String>,
}
#[derive(Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct Process {
    action_id: String,
    revision: u64,
    host_instance: Option<String>,
    state: String,
    reason: Option<String>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Input {
    expected_revision: u64,
    #[serde(default)]
    host_instance: Option<String>,
    #[serde(default)]
    acknowledge_disruption: bool,
}
fn directory(root: &Path) -> Result<()> {
    if !root.exists() {
        fs::create_dir(root).map_err(|_| fail("host_control_storage_unavailable"))?;
        fs::set_permissions(root, fs::Permissions::from_mode(0o700))
            .map_err(|_| fail("host_control_storage_unavailable"))?;
    }
    let m = fs::symlink_metadata(root).map_err(|_| fail("host_control_storage_unavailable"))?;
    if !m.is_dir() || m.uid() != rustix::process::geteuid().as_raw() || m.mode() & 0o077 != 0 {
        return Err(fail("unsafe_host_control_storage"));
    }
    Ok(())
}
fn lock(root: &Path, name: &str, wait: bool) -> Result<Option<fs::File>> {
    directory(root)?;
    let f = fs::OpenOptions::new()
        .read(true)
        .write(true)
        .create(true)
        .truncate(false)
        .mode(0o600)
        .custom_flags(OFlags::NOFOLLOW.bits() as i32)
        .open(root.join(name))
        .map_err(|_| fail("host_control_lock_unavailable"))?;
    let m = f
        .metadata()
        .map_err(|_| fail("host_control_lock_unavailable"))?;
    if !m.is_file()
        || m.nlink() != 1
        || m.uid() != rustix::process::geteuid().as_raw()
        || m.mode() & 0o077 != 0
    {
        return Err(fail("unsafe_host_control_lock"));
    }
    match flock(
        &f,
        if wait {
            FlockOperation::LockExclusive
        } else {
            FlockOperation::NonBlockingLockExclusive
        },
    ) {
        Ok(()) => Ok(Some(f)),
        Err(rustix::io::Errno::WOULDBLOCK) => Ok(None),
        Err(_) => Err(fail("host_control_lock_unavailable")),
    }
}
fn read<T: serde::de::DeserializeOwned>(root: &Path, name: &str) -> Result<Option<T>> {
    let f = match fs::OpenOptions::new()
        .read(true)
        .custom_flags(OFlags::NOFOLLOW.bits() as i32)
        .open(root.join(name))
    {
        Ok(f) => f,
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => return Ok(None),
        Err(_) => return Err(fail("host_control_read_failed")),
    };
    let m = f.metadata().map_err(|_| fail("host_control_read_failed"))?;
    if !m.is_file()
        || m.nlink() != 1
        || m.len() > 16384
        || m.uid() != rustix::process::geteuid().as_raw()
        || m.mode() & 0o077 != 0
    {
        return Err(fail("unsafe_host_control_record"));
    }
    serde_json::from_reader(f)
        .map(Some)
        .map_err(|_| fail("invalid_host_control_record"))
}
fn save(root: &Path, name: &str, value: &impl Serialize) -> Result<()> {
    let path = root.join(name);
    let temp = root.join(format!("{name}.{}.tmp", std::process::id()));
    let bytes = serde_json::to_vec(value).map_err(|_| fail("host_control_encoding_failed"))?;
    let mut f = fs::OpenOptions::new()
        .write(true)
        .create_new(true)
        .mode(0o600)
        .open(&temp)
        .map_err(|_| fail("host_control_publish_failed"))?;
    let result = (|| {
        f.write_all(&bytes)?;
        f.sync_all()?;
        fs::rename(&temp, path)?;
        fs::File::open(root)?.sync_all()
    })()
    .map_err(|_| fail("host_control_publish_failed"));
    if result.is_err() {
        let _ = fs::remove_file(temp);
    }
    result
}
fn config(root: &Path) -> Result<Option<Configuration>> {
    read(root, "configuration.json")
}
fn observed(socket: &str) -> Result<Option<(Client, String)>> {
    match Client::connect(Some(socket)) {
        Ok(mut c) => {
            c.timeout(3000)?;
            let id = c.host_identity()?;
            Ok(Some((c, id)))
        }
        Err(_) if !Path::new(socket).exists() => Ok(None),
        Err(e) => Err(e),
    }
}
pub(crate) fn configure(root: &Path, port: u16) -> Result<Value> {
    if port == 0 {
        return Err(fail("invalid_inference_port"));
    }
    let _guard = lock(root, "control.lock", true)?;
    let socket = ffi::default_socket()?;
    if observed(&socket)?.is_some() || lock(root, "process.lock", false)?.is_none() {
        return Err(fail("host_configuration_requires_stopped_host"));
    }
    let revision = config(root)?.map_or(1, |c| c.revision.saturating_add(1));
    let c = Configuration {
        schema: "yvex.host.configuration.v1".into(),
        revision,
        socket,
        openai_port: port,
        openai_enabled: true,
        pending_phase: None,
        action_id: None,
    };
    save(root, "configuration.json", &c)?;
    Ok(json!({"schema":c.schema,"revision":revision,"inference_port":port,"configured":true}))
}
pub(crate) fn observation(root: Option<&Path>, granted: bool) -> Result<Value> {
    let c = match root {
        Some(r) => config(r)?,
        None => None,
    };
    let socket = c
        .as_ref()
        .map(|c| c.socket.clone())
        .map_or_else(ffi::default_socket, Ok)?;
    let live = observed(&socket);
    let process: Option<Process> = match root {
        Some(r) => read(r, "process.json")?,
        None => None,
    };
    let instance = live
        .as_ref()
        .ok()
        .and_then(|v| v.as_ref().map(|(_, id)| id.clone()));
    let owned = instance.as_ref().is_some_and(|id| {
        process
            .as_ref()
            .is_some_and(|p| p.host_instance.as_ref() == Some(id) && p.state == "running")
    });
    let state = if live.is_err() {
        "unavailable"
    } else if instance.is_some() {
        "running"
    } else {
        "stopped"
    };
    let ownership = if instance.is_some() && !owned {
        "external"
    } else if c.is_some() {
        "managed"
    } else {
        "unconfigured"
    };
    let busy = match root.filter(|r| r.exists()) {
        Some(r) => {
            lock(r, "control.lock", false)?.is_none()
                || (state == "stopped" && lock(r, "process.lock", false)?.is_none())
        }
        None => false,
    };
    let pending = c
        .as_ref()
        .and_then(|c| c.pending_phase.as_deref())
        .is_some_and(|phase| {
            (phase == "starting" && state != "running")
                || (phase == "stopping" && state != "stopped")
        });
    let allowed = granted && c.is_some() && !busy && !pending && ownership == "managed";
    let reason = if !granted {
        Some("service_control_grant_required")
    } else if ownership == "external" {
        Some("externally_managed_host")
    } else if c.is_none() {
        Some("host_control_not_configured")
    } else if busy || pending {
        Some("host_control_operation_pending")
    } else if state == "unavailable" {
        Some("host_state_unavailable")
    } else {
        None
    };
    Ok(
        json!({"schema":"yvex.host.control.v1","ownership":ownership,"revision":c.as_ref().map_or(0,|c|c.revision),"state":state,"host_instance":instance,"configured":c.is_some(),"service_control_granted":granted,"can_start":allowed&&state=="stopped","can_stop":allowed&&state=="running","can_restart":allowed&&state=="running","reason":reason,"inference_port":c.map(|c|c.openai_port)}),
    )
}
pub(crate) fn validate(operation: &str, value: &Value) -> Result<()> {
    let input: Input =
        serde_json::from_value(value.clone()).map_err(|_| fail("invalid_host_control_input"))?;
    if input.expected_revision == 0 {
        return Err(fail("host_control_revision_required"));
    }
    match operation {
        "host.start" if input.host_instance.is_none() && !input.acknowledge_disruption => Ok(()),
        "host.stop" | "host.restart"
            if input.acknowledge_disruption
                && input.host_instance.as_deref().is_some_and(jobs::identity) =>
        {
            Ok(())
        }
        _ => Err(fail("exact_host_and_disruption_acknowledgement_required")),
    }
}
fn stop(root: &Path, c: &Configuration, expected: &str) -> Result<()> {
    let Some((mut client, id)) = observed(&c.socket)? else {
        return Err(fail("host_not_running"));
    };
    let record: Process =
        read(root, "process.json")?.ok_or_else(|| fail("externally_managed_host"))?;
    if id != expected {
        return Err(fail("stale_host_instance"));
    }
    if record.host_instance.as_deref() != Some(expected) || record.state != "running" {
        return Err(fail("externally_managed_host"));
    }
    let request = client.request(raw::yvex_client_operation_YVEX_CLIENT_OP_RUNTIME_STOP);
    client
        .send(&request)
        .map_err(|_| uncertain("host_stop_response_lost"))?;
    let reply = client::response(&mut client, &request)
        .map_err(|_| uncertain("host_stop_response_lost"))?;
    if reply.kind != raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK {
        return Err(uncertain("host_stop_response_unexpected"));
    }
    let until = Instant::now() + Duration::from_secs(20);
    while Instant::now() < until {
        if lock(root, "process.lock", false)?.is_some() {
            return match observed(&c.socket) {
                Ok(None) => Ok(()),
                Ok(Some((_, id))) if id != expected => {
                    Err(uncertain("another_host_started_after_stop"))
                }
                _ => Err(uncertain("host_stop_not_settled")),
            };
        }
        std::thread::sleep(Duration::from_millis(50));
    }
    Err(uncertain("host_shutdown_pending_observe_no_replay"))
}
fn start(root: &Path, c: &Configuration, id: &str) -> Result<String> {
    if observed(&c.socket)?.is_some() || lock(root, "process.lock", false)?.is_none() {
        return Err(fail("host_already_running_or_starting"));
    }
    let executable = if cfg!(target_os = "linux") {
        PathBuf::from("/proc/self/exe")
    } else {
        std::env::current_exe().map_err(|_| fail("host_worker_unavailable"))?
    };
    let child = Command::new(executable)
        .args(["management", "host-worker"])
        .arg(root)
        .arg(id)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .map_err(|_| fail("host_worker_not_started"))?;
    // Reap eventually, without tying the Host lifetime to the submitting request.
    std::thread::spawn(move || {
        let mut child = child;
        let _ = child.wait();
    });
    let until = Instant::now() + Duration::from_secs(15);
    while Instant::now() < until {
        if let Some(p) = read::<Process>(root, "process.json")?
            && p.action_id == id
            && p.revision == c.revision
        {
            if p.state == "failed" {
                return Err(fail(p.reason.as_deref().unwrap_or("host_start_failed")));
            }
            if let Some(expected) = p.host_instance
                && let Some((_, actual)) = observed(&c.socket)?
                && expected == actual
            {
                return Ok(actual);
            }
        }
        std::thread::sleep(Duration::from_millis(50));
    }
    Err(uncertain("host_start_pending_observe_no_replay"))
}
pub(crate) fn execute(
    operation: &str,
    input: &Value,
    id: &str,
    progress: &mut dyn FnMut(&Value) -> Result<()>,
) -> Result<Value> {
    validate(operation, input)?;
    let root = std::env::var_os("YVEX_MANAGED_HOST_ROOT")
        .map(PathBuf::from)
        .ok_or_else(|| fail("service_control_grant_required"))?;
    let _guard = lock(&root, "control.lock", true)?;
    let mut c = config(&root)?.ok_or_else(|| fail("host_control_not_configured"))?;
    let i: Input =
        serde_json::from_value(input.clone()).map_err(|_| fail("invalid_host_control_input"))?;
    if i.expected_revision != c.revision {
        return Err(fail("stale_host_control_revision"));
    }
    // Fence the physical instance and prior uncertain lifecycle before publishing an intent.
    if operation == "host.start" {
        if c.pending_phase.as_deref() == Some("starting") {
            return Err(uncertain("host_start_pending_observe_no_replay"));
        }
        if observed(&c.socket)?.is_some() || lock(&root, "process.lock", false)?.is_none() {
            return Err(fail("host_already_running_or_starting"));
        }
    } else {
        let Some((_, instance)) = observed(&c.socket)? else {
            return Err(fail("host_not_running"));
        };
        if i.host_instance.as_ref() != Some(&instance) {
            return Err(fail("stale_host_instance"));
        }
        let p: Process =
            read(&root, "process.json")?.ok_or_else(|| fail("externally_managed_host"))?;
        if p.host_instance.as_ref() != Some(&instance) || p.state != "running" {
            return Err(fail("externally_managed_host"));
        }
        if c.pending_phase.as_deref() == Some("stopping") {
            return Err(uncertain("host_shutdown_pending_observe_no_replay"));
        }
    }
    c.revision = c
        .revision
        .checked_add(1)
        .ok_or_else(|| fail("host_control_revision_exhausted"))?;
    c.action_id = Some(id.into());
    let previous = i.host_instance.clone();
    let revision = c.revision;
    let receipt = |phase: &str, instance: Option<&str>| json!({"schema":"yvex.host.control.result.v1","phase":phase,"revision":revision,"previous_host_instance":previous,"host_instance":instance,"models_restored":false});
    if operation != "host.start" {
        c.pending_phase = Some("stopping".into());
        save(&root, "configuration.json", &c)?;
        progress(&receipt("stopping", previous.as_deref()))?;
        stop(&root, &c, previous.as_deref().unwrap())?;
        c.pending_phase = None;
        c.action_id = None;
        save(&root, "configuration.json", &c)
            .map_err(|_| uncertain("host_stopped_receipt_unavailable"))?;
        progress(&receipt("stopped", None))
            .map_err(|_| uncertain("host_stopped_receipt_unavailable"))?;
    }
    if operation == "host.stop" {
        return Ok(receipt("stopped", None));
    }
    c.action_id = Some(id.into());
    c.pending_phase = Some("starting".into());
    save(&root, "configuration.json", &c)?;
    progress(&receipt("starting", None))?;
    let outcome = start(&root, &c, id);
    if outcome
        .as_ref()
        .err()
        .is_none_or(|e| e.owner != "management.indeterminate")
    {
        c.pending_phase = None;
        c.action_id = None;
        save(&root, "configuration.json", &c)
            .map_err(|_| uncertain("host_start_receipt_unavailable"))?;
    }
    outcome.map(|instance| receipt("running", Some(&instance)))
}

pub(crate) fn worker(root: &Path, id: &str) -> Result<()> {
    if !jobs::identity(id) {
        return Err(fail("invalid_host_action_identity"));
    }
    let _owned =
        lock(root, "process.lock", false)?.ok_or_else(|| fail("managed_host_already_active"))?;
    let c = config(root)?.ok_or_else(|| fail("host_control_not_configured"))?;
    if c.action_id.as_deref() != Some(id) || c.pending_phase.as_deref() != Some("starting") {
        return Err(fail("stale_host_process_intent"));
    }
    let initial = Process {
        action_id: id.into(),
        revision: c.revision,
        host_instance: None,
        state: "starting".into(),
        reason: None,
    };
    save(root, "process.json", &initial)?;
    let options = raw::yvex_server_options {
        schema_version: raw::YVEX_SERVER_OPTIONS_SCHEMA_CURRENT,
        request_queue_capacity: 16,
        worker_count: 1,
        maximum_engines: raw::YVEX_SERVER_DEFAULT_MAXIMUM_ENGINES as u64,
        openai_timeout_ms: 600000,
        openai_port: c.openai_port,
        openai_enabled: i32::from(c.openai_enabled),
        trace_level: raw::yvex_server_trace_level_YVEX_SERVER_TRACE_STAGES,
        console: raw::yvex_server_console_kind_YVEX_SERVER_CONSOLE_OFF,
        ..Default::default()
    };
    let outcome = (|| {
        let mut host = Host::create(options, &c.socket)?;
        host.start()?;
        std::thread::scope(|scope| {
            let monitor = scope.spawn(|| {
                let until = Instant::now() + Duration::from_secs(10);
                while Instant::now() < until {
                    if let Ok(Some((_, instance))) = observed(&c.socket) {
                        let p = Process {
                            host_instance: Some(instance),
                            state: "running".into(),
                            ..initial.clone()
                        };
                        return save(root, "process.json", &p);
                    }
                    std::thread::sleep(Duration::from_millis(20));
                }
                Err(uncertain("host_readiness_not_observed"))
            });
            let result = host.serve();
            let cleanup = host.finish();
            let monitor = monitor.join().map_err(|_| fail("host_monitor_failed"))?;
            result.and(cleanup).and(monitor)
        })
    })();
    let final_record = Process {
        state: if outcome.is_ok() { "stopped" } else { "failed" }.into(),
        reason: outcome.as_ref().err().map(ToString::to_string),
        ..read(root, "process.json")?.unwrap_or(initial)
    };
    save(root, "process.json", &final_record)?;
    outcome
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn control_requires_fences_and_explicit_disruption_without_process_configuration() {
        assert!(validate("host.start", &json!({"expected_revision":1})).is_ok());
        for input in [
            json!({}),
            json!({"expected_revision":0}),
            json!({"expected_revision":1,"command":"sh"}),
            json!({"expected_revision":1,"socket":"/tmp/other"}),
        ] {
            assert!(validate("host.start", &input).is_err());
        }
        assert!(validate("host.stop",&json!({"expected_revision":1,"host_instance":"a".repeat(64),"acknowledge_disruption":true})).is_ok());
        for input in [
            json!({"expected_revision":1}),
            json!({"expected_revision":1,"host_instance":"a".repeat(64)}),
            json!({"expected_revision":1,"host_instance":"invalid","acknowledge_disruption":true}),
        ] {
            assert!(validate("host.restart", &input).is_err());
        }
    }
}
