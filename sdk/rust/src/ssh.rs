//! Bounded mechanical SSH transport shared by the two public YVEX protocols.
//! A transport loss never proves that a computation was not dispatched.
use crate::SshConnection;
use std::io::{Read, Write};
use std::process::{Child, Command, Stdio};
use std::sync::{
    atomic::{AtomicBool, Ordering},
    mpsc,
};
use std::time::{Duration, Instant};
use wait_timeout::ChildExt;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum FailureKind {
    Unavailable,
    Timeout,
    Cancelled,
    ResponseTooLarge,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) struct Failure {
    pub kind: FailureKind,
    /// False only when no child was started. Once started, loss is conservative.
    pub started: bool,
}

pub(crate) fn invoke(
    connection: &SshConnection,
    request: Vec<u8>,
    timeout: Duration,
    cancel: &AtomicBool,
    max_response: usize,
) -> Result<Vec<u8>, Failure> {
    let mut command = Command::new("ssh");
    command.args(["-T", "-F", "/dev/null"]);
    for option in [
        "BatchMode=yes",
        "PreferredAuthentications=publickey",
        "PasswordAuthentication=no",
        "KbdInteractiveAuthentication=no",
        "GSSAPIAuthentication=no",
        "ClearAllForwardings=yes",
        "ForwardAgent=no",
        "StrictHostKeyChecking=yes",
        "IdentitiesOnly=yes",
        "ConnectionAttempts=1",
        "ConnectTimeout=5",
        "ControlMaster=no",
        "ControlPath=none",
        "GlobalKnownHostsFile=/dev/null",
        "HostKeyAlgorithms=ssh-ed25519",
        "PubkeyAcceptedAlgorithms=ssh-ed25519",
        "UpdateHostKeys=no",
    ] {
        command.args(["-o", option]);
    }
    command
        .arg("-o")
        .arg(format!(
            "UserKnownHostsFile={}",
            connection.pinned_known_hosts.display()
        ))
        .arg("-i")
        .arg(&connection.enrolled_client_key)
        .arg("-p")
        .arg(connection.port.to_string())
        // No remote command: the enrolled finite/management grant selects it.
        .arg(format!("{}@{}", connection.user, connection.address));
    run(command, request, timeout, cancel, max_response)
}

struct Reap(Child);
impl Drop for Reap {
    fn drop(&mut self) {
        let _ = self.0.kill();
        let _ = self.0.wait();
    }
}

fn run(
    mut command: Command,
    request: Vec<u8>,
    timeout: Duration,
    cancel: &AtomicBool,
    max_response: usize,
) -> Result<Vec<u8>, Failure> {
    let fail = |kind, started| Failure { kind, started };
    if cancel.load(Ordering::Acquire) {
        return Err(fail(FailureKind::Cancelled, false));
    }
    let started = Instant::now();
    let mut child = Reap(
        command
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::null())
            .spawn()
            .map_err(|_| fail(FailureKind::Unavailable, false))?,
    );
    let mut stdin = child.0.stdin.take().unwrap();
    let stdout = child.0.stdout.take().unwrap();
    let (written_tx, written_rx) = mpsc::channel();
    let (output_tx, output_rx) = mpsc::channel();
    // Drain while SSH runs: waiting for exit before reading can deadlock on a
    // full pipe. Writing is also asynchronous so an unresponsive peer is bounded.
    std::thread::spawn(move || {
        let result = stdin.write_all(&request);
        drop(stdin);
        let _ = written_tx.send(result);
    });
    std::thread::spawn(move || {
        let mut bytes = Vec::new();
        let result = stdout
            .take((max_response + 1) as u64)
            .read_to_end(&mut bytes)
            .map(|_| bytes);
        let _ = output_tx.send(result);
    });
    let mut written = false;
    let mut output = None;
    let mut status: Option<std::process::ExitStatus> = None;
    loop {
        if cancel.load(Ordering::Acquire) {
            return Err(fail(FailureKind::Cancelled, true));
        }
        if started.elapsed() >= timeout {
            return Err(fail(FailureKind::Timeout, true));
        }
        if !written {
            match written_rx.try_recv() {
                Ok(Ok(())) => written = true,
                Ok(Err(_)) | Err(mpsc::TryRecvError::Disconnected) => {
                    return Err(fail(FailureKind::Unavailable, true))
                }
                Err(mpsc::TryRecvError::Empty) => {}
            }
        }
        if output.is_none() {
            match output_rx.try_recv() {
                Ok(Ok(bytes)) if bytes.len() > max_response => {
                    return Err(fail(FailureKind::ResponseTooLarge, true))
                }
                Ok(Ok(bytes)) => output = Some(bytes),
                Ok(Err(_)) | Err(mpsc::TryRecvError::Disconnected) => {
                    return Err(fail(FailureKind::Unavailable, true))
                }
                Err(mpsc::TryRecvError::Empty) => {}
            }
        }
        if let Some(exit) = status {
            if !exit.success() {
                return Err(fail(FailureKind::Unavailable, true));
            }
            if written {
                if let Some(bytes) = output {
                    return Ok(bytes);
                }
            }
            std::thread::sleep(Duration::from_millis(1));
        } else {
            status = child
                .0
                .wait_timeout(
                    Duration::from_millis(20).min(timeout.saturating_sub(started.elapsed())),
                )
                .map_err(|_| fail(FailureKind::Unavailable, true))?;
        }
    }
}

#[cfg(all(test, unix))]
mod tests {
    use super::*;
    fn shell(script: &str) -> Command {
        let mut c = Command::new("sh");
        c.args(["-c", script]);
        c
    }
    #[test]
    fn drains_large_response_without_pipe_deadlock() {
        let output = run(
            shell("cat >/dev/null; head -c 24000 /dev/zero"),
            b"{}\n".to_vec(),
            Duration::from_secs(2),
            &AtomicBool::new(false),
            32768,
        )
        .unwrap();
        assert_eq!(output.len(), 24000);
    }
    #[test]
    fn caps_output_and_classifies_loss_without_retry() {
        let failure = run(
            shell("cat >/dev/null; head -c 100000 /dev/zero"),
            b"{}\n".to_vec(),
            Duration::from_secs(2),
            &AtomicBool::new(false),
            1024,
        )
        .unwrap_err();
        assert_eq!(
            failure,
            Failure {
                kind: FailureKind::ResponseTooLarge,
                started: true
            }
        );
        assert_eq!(
            run(
                shell("exit 1"),
                b"{}\n".to_vec(),
                Duration::from_secs(2),
                &AtomicBool::new(false),
                1024
            )
            .unwrap_err()
            .kind,
            FailureKind::Unavailable
        );
    }
    #[test]
    fn bounds_blocked_stdin_and_distinguishes_pre_dispatch_cancellation() {
        let started = Instant::now();
        assert_eq!(
            run(
                shell("exec sleep 10"),
                vec![0; 32768],
                Duration::from_millis(60),
                &AtomicBool::new(false),
                1024
            )
            .unwrap_err(),
            Failure {
                kind: FailureKind::Timeout,
                started: true
            }
        );
        assert!(started.elapsed() < Duration::from_secs(2));
        assert_eq!(
            run(
                shell("exit 99"),
                vec![],
                Duration::from_secs(2),
                &AtomicBool::new(true),
                1024
            )
            .unwrap_err(),
            Failure {
                kind: FailureKind::Cancelled,
                started: false
            }
        );
        let cancel = std::sync::Arc::new(AtomicBool::new(false));
        let trigger = cancel.clone();
        std::thread::spawn(move || {
            std::thread::sleep(Duration::from_millis(40));
            trigger.store(true, Ordering::Release);
        });
        assert_eq!(
            run(
                shell("exec sleep 10"),
                vec![],
                Duration::from_secs(2),
                &cancel,
                1024
            )
            .unwrap_err(),
            Failure {
                kind: FailureKind::Cancelled,
                started: true
            }
        );
    }
}
