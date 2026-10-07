// Peer-scoped durable product operation receipts. This is not a computational
// scheduler: native Source/Host owners still admit and execute their own work.
use crate::{ffi, management_models, management_runtime};
use ffi::Error;
use rustix::fs::{FlockOperation, flock};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::{
    fs,
    io::Write,
    os::unix::fs::{MetadataExt, OpenOptionsExt, PermissionsExt},
    path::{Path, PathBuf},
    process::{Command, Stdio},
    time::{SystemTime, UNIX_EPOCH},
};
type Result<T> = std::result::Result<T, Error>;
const RETENTION: usize = 512;
fn fail(s: &str) -> Error {
    management_runtime::failure(s)
}
fn now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64
}
pub(crate) fn identity(s: &str) -> bool {
    s.len() == 64
        && s.bytes()
            .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
}
#[derive(Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct Job {
    pub job_id: String,
    pub operation: String,
    pub state: String,
    pub submitted_at_unix_ms: u64,
    pub updated_at_unix_ms: u64,
    pub input: Value,
    pub result: Option<Value>,
    pub reason: Option<String>,
}
#[derive(Serialize)]
struct JobSummary {
    job_id: String,
    operation: String,
    state: String,
    submitted_at_unix_ms: u64,
    updated_at_unix_ms: u64,
    reason: Option<String>,
}
impl From<Job> for JobSummary {
    fn from(job: Job) -> Self {
        Self {
            job_id: job.job_id,
            operation: job.operation,
            state: job.state,
            submitted_at_unix_ms: job.submitted_at_unix_ms,
            updated_at_unix_ms: job.updated_at_unix_ms,
            reason: job.reason,
        }
    }
}
fn private_dir(p: &Path) -> Result<()> {
    if !p.exists() {
        fs::create_dir(p).map_err(|_| fail("job_storage_unavailable"))?;
        fs::set_permissions(p, fs::Permissions::from_mode(0o700))
            .map_err(|_| fail("job_storage_unavailable"))?;
    }
    let m = fs::symlink_metadata(p).map_err(|_| fail("job_storage_unavailable"))?;
    if !m.is_dir() || m.uid() != rustix::process::geteuid().as_raw() || m.mode() & 0o077 != 0 {
        return Err(fail("unsafe_job_storage"));
    }
    Ok(())
}
fn root(peer: &str) -> Result<PathBuf> {
    if !identity(peer) {
        return Err(fail("invalid_peer_identity"));
    }
    let base = std::env::var_os("XDG_DATA_HOME")
        .map(PathBuf::from)
        .filter(|p| p.is_absolute())
        .or_else(|| std::env::var_os("HOME").map(|v| PathBuf::from(v).join(".local/share")))
        .ok_or_else(|| fail("job_storage_unavailable"))?;
    if !base.is_dir() {
        fs::create_dir_all(&base).map_err(|_| fail("job_storage_unavailable"))?;
    }
    let yvex = base.join("yvex");
    if !yvex.exists() {
        private_dir(&yvex)?;
    }
    let parent = fs::symlink_metadata(&yvex).map_err(|_| fail("job_storage_unavailable"))?;
    if !parent.is_dir()
        || parent.uid() != rustix::process::geteuid().as_raw()
        || parent.mode() & 0o022 != 0
    {
        return Err(fail("unsafe_job_parent"));
    }
    let jobs = yvex.join("management-jobs-v2");
    private_dir(&jobs)?;
    let scoped = jobs.join(peer);
    private_dir(&scoped)?;
    Ok(scoped)
}
fn lock(path: &Path, nonblocking: bool) -> Result<Option<fs::File>> {
    let f = fs::OpenOptions::new()
        .read(true)
        .write(true)
        .create(true)
        .truncate(false)
        .mode(0o600)
        .custom_flags(rustix::fs::OFlags::NOFOLLOW.bits() as i32)
        .open(path)
        .map_err(|_| fail("job_lock_unavailable"))?;
    let m = f.metadata().map_err(|_| fail("job_lock_unavailable"))?;
    if !m.is_file() || m.mode() & 0o077 != 0 || m.uid() != rustix::process::geteuid().as_raw() {
        return Err(fail("unsafe_job_lock"));
    }
    match flock(
        &f,
        if nonblocking {
            FlockOperation::NonBlockingLockExclusive
        } else {
            FlockOperation::LockExclusive
        },
    ) {
        Ok(()) => Ok(Some(f)),
        Err(rustix::io::Errno::WOULDBLOCK) if nonblocking => Ok(None),
        Err(_) => Err(fail("job_lock_unavailable")),
    }
}
fn save(path: &Path, job: &Job) -> Result<()> {
    let bytes = serde_json::to_vec(job).map_err(|_| fail("job_encoding_failed"))?;
    if bytes.len() > 1_048_576 {
        return Err(fail("job_output_bound"));
    }
    let temp = path.with_extension(format!("{}.tmp", std::process::id()));
    let mut f = fs::OpenOptions::new()
        .write(true)
        .create_new(true)
        .mode(0o600)
        .open(&temp)
        .map_err(|_| fail("job_publication_failed"))?;
    let result = (|| {
        f.write_all(&bytes)?;
        f.sync_all()?;
        fs::rename(&temp, path)?;
        fs::File::open(path.parent().unwrap())?.sync_all()
    })()
    .map_err(|_| fail("job_publication_failed"));
    if result.is_err() {
        let _ = fs::remove_file(temp);
    }
    result
}
fn load(path: &Path) -> Result<Job> {
    let m = fs::symlink_metadata(path).map_err(|_| fail("receipt_absent_do_not_redispatch"))?;
    if !m.is_file()
        || m.len() > 1_048_576
        || m.uid() != rustix::process::geteuid().as_raw()
        || m.mode() & 0o077 != 0
    {
        return Err(fail("unsafe_job_receipt"));
    }
    let f = fs::OpenOptions::new()
        .read(true)
        .custom_flags(rustix::fs::OFlags::NOFOLLOW.bits() as i32)
        .open(path)
        .map_err(|_| fail("job_read_failed"))?;
    serde_json::from_reader(f).map_err(|_| fail("invalid_job_receipt"))
}
pub(crate) fn get(peer: &str, id: &str) -> Result<Job> {
    if !identity(id) {
        return Err(fail("invalid_job_identity"));
    }
    let dir = root(peer)?;
    let p = dir.join(format!("{id}.json"));
    let mut j = load(&p)?;
    if (j.state == "running"
        || (j.state == "accepted" && now().saturating_sub(j.submitted_at_unix_ms) > 15_000))
        && let Some(_guard) = lock(&dir.join(format!("{id}.lock")), true)?
    {
        j = load(&p)?;
        if j.state == "running" || j.state == "accepted" {
            j.state = "indeterminate".into();
            j.reason = Some("worker_lost_observe_native_owner_no_replay".into());
            j.updated_at_unix_ms = now();
            save(&p, &j)?;
        }
    }
    Ok(j)
}
pub(crate) fn list(peer: &str, limit: usize) -> Result<Value> {
    let dir = root(peer)?;
    let mut ids = fs::read_dir(&dir)
        .map_err(|_| fail("job_storage_unavailable"))?
        .filter_map(|v| v.ok())
        .filter_map(|e| {
            e.file_name()
                .to_str()
                .and_then(|s| s.strip_suffix(".json"))
                .map(String::from)
        })
        .collect::<Vec<_>>();
    ids.sort();
    let mut jobs = Vec::new();
    for id in ids.into_iter().take(RETENTION) {
        jobs.push(JobSummary::from(get(peer, &id)?));
    }
    jobs.sort_by_key(|j| std::cmp::Reverse(j.submitted_at_unix_ms));
    let truncated = jobs.len() > limit;
    jobs.truncate(limit);
    Ok(json!({"jobs":jobs,"retention_limit":RETENTION,"truncated":truncated}))
}
pub(crate) fn submit(
    peer: &str,
    id: &str,
    operation: &str,
    input: &Value,
    control_root: Option<&Path>,
) -> Result<Job> {
    if !identity(id) {
        return Err(fail("invalid_job_identity"));
    }
    let dir = root(peer)?;
    let _admission = lock(&dir.join("admission.lock"), false)?.unwrap();
    let path = dir.join(format!("{id}.json"));
    if path.exists() {
        let j = load(&path)?;
        return if j.operation == operation && j.input == *input {
            Ok(j)
        } else {
            Err(fail("request_identity_conflict"))
        };
    }
    if fs::read_dir(&dir)
        .map_err(|_| fail("job_storage_unavailable"))?
        .filter_map(|e| e.ok())
        .filter(|e| e.path().extension().is_some_and(|x| x == "json"))
        .count()
        >= RETENTION
    {
        return Err(fail("receipt_retention_full"));
    }
    let j = Job {
        job_id: id.into(),
        operation: operation.into(),
        state: "accepted".into(),
        submitted_at_unix_ms: now(),
        updated_at_unix_ms: now(),
        input: input.clone(),
        result: None,
        reason: None,
    };
    save(&path, &j)?;
    // Spawn only the loaded product's internal worker. Studio never invokes a shell
    // or CLI lifecycle command; the producer worker calls typed domain functions.
    let executable = if cfg!(target_os = "linux") {
        PathBuf::from("/proc/self/exe")
    } else {
        std::env::current_exe().map_err(|_| fail("worker_unavailable"))?
    };
    let mut command = Command::new(executable);
    if let Some(root) = control_root {
        command.env("YVEX_MANAGED_HOST_ROOT", root);
    } else {
        command.env_remove("YVEX_MANAGED_HOST_ROOT");
    }
    let spawned = command
        .args(["management", "product-worker", peer, id])
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn();
    if spawned.is_err() {
        let mut j = j;
        j.state = "failed".into();
        j.reason = Some("worker_not_started".into());
        save(&path, &j)?;
        return Ok(j);
    }
    Ok(j)
}
pub(crate) fn run(peer: &str, id: &str) -> Result<()> {
    if !identity(id) {
        return Err(fail("invalid_job_identity"));
    }
    let dir = root(peer)?;
    let path = dir.join(format!("{id}.json"));
    let Some(_guard) = lock(&dir.join(format!("{id}.lock")), true)? else {
        return Err(fail("worker_already_active"));
    };
    let mut job = load(&path)?;
    if job.state != "accepted" {
        return Err(fail("job_not_dispatchable"));
    }
    job.state = "running".into();
    job.updated_at_unix_ms = now();
    save(&path, &job)?;
    let operation = job.operation.clone();
    let input = job.input.clone();
    let mut progress = |value: &Value| {
        job.result = Some(value.clone());
        job.updated_at_unix_ms = now();
        save(&path, &job)
    };
    let result = if operation.starts_with("host.") {
        crate::management_host::execute(&operation, &input, id, &mut progress)
    } else if operation.starts_with("engine.")
        || operation.starts_with("session.")
        || operation.starts_with("generation.")
    {
        management_runtime::execute(&operation, &input, &mut progress)
    } else {
        management_models::execute(&operation, &input)
    };
    job.updated_at_unix_ms = now();
    match result {
        Ok(value) => {
            job.state = "succeeded".into();
            job.result = Some(value);
        }
        Err(error) => {
            job.state = if error.owner == "management.indeterminate" {
                "indeterminate"
            } else if error.code == ffi::raw::yvex_status_YVEX_ERR_CANCELLED {
                "cancelled"
            } else {
                "failed"
            }
            .into();
            job.reason = Some(error.to_string());
        }
    }
    save(&path, &job)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn summaries_discard_prompt_and_output_bodies() {
        let summary = JobSummary::from(Job {
            job_id: "a".repeat(64),
            operation: "generation.start".into(),
            state: "running".into(),
            submitted_at_unix_ms: 1,
            updated_at_unix_ms: 2,
            input: json!({"prompt":"x".repeat(65536)}),
            result: Some(json!({"text":"x".repeat(262144)})),
            reason: Some("owner_reason".into()),
        });
        let value = serde_json::to_value(summary).unwrap();
        assert!(value.get("input").is_none() && value.get("result").is_none());
        assert_eq!(value["reason"], "owner_reason");
        assert!(value.to_string().len() < 512);
    }
}
