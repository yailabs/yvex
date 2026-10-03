// Host readiness and signals drive the one REPLAI terminal/restoration owner.
use replai::{Event, Interaction, Prompt, WaitInterest, Wake};
use signal_hook::{consts::signal::*, iterator::Signals};
use std::{
    io::{self, Read, Write},
    os::{fd::AsFd, unix::net::UnixStream},
    sync::{
        Arc,
        atomic::{AtomicUsize, Ordering},
    },
    thread,
    time::{Duration, Instant},
};

pub(crate) struct Notifications {
    source: UnixStream,
    pending: Arc<AtomicUsize>,
    handle: signal_hook::iterator::Handle,
    worker: Option<thread::JoinHandle<()>>,
}

impl Notifications {
    pub(crate) fn new(mut handler: impl FnMut(i32) + Send + 'static) -> io::Result<Self> {
        let (source, mut sink) = UnixStream::pair()?;
        source.set_nonblocking(true)?;
        sink.set_nonblocking(true)?;
        let pending = Arc::new(AtomicUsize::new(0));
        let events = Arc::clone(&pending);
        let mut signals = Signals::new([SIGWINCH, SIGINT, SIGTERM])?;
        let handle = signals.handle();
        let worker = thread::spawn(move || {
            for signal in signals.forever() {
                let bit = match signal {
                    SIGWINCH => 1,
                    SIGINT => 2,
                    _ => 4,
                };
                if events.fetch_or(bit, Ordering::AcqRel) == 0 {
                    // An already-full wake stream is readable; signal meaning is in pending.
                    let _ = sink.write(&[1]);
                }
                // Publish the wake before a potentially blocking domain callback.
                // A completed turn can then drain its interrupt before reopening
                // the editor, rather than receiving a delayed second interruption.
                handler(signal);
            }
        });
        Ok(Self {
            source,
            pending,
            handle,
            worker: Some(worker),
        })
    }

    pub(crate) fn take(&mut self) -> io::Result<usize> {
        let mut bytes = [0; 64];
        loop {
            match self.source.read(&mut bytes) {
                Ok(0) => break,
                Ok(_) => {}
                Err(error) if error.kind() == io::ErrorKind::WouldBlock => break,
                Err(error) if error.kind() == io::ErrorKind::Interrupted => continue,
                Err(error) => return Err(error),
            }
        }
        Ok(self.pending.swap(0, Ordering::AcqRel))
    }

    pub(crate) fn advance(
        &mut self,
        interaction: &mut Interaction,
    ) -> Result<Option<Event>, Box<dyn std::error::Error>> {
        let interest = interaction.wait_interest()?;
        let deadline = match interest {
            WaitInterest::Input { deadline } => deadline,
            _ => None,
        };
        let timeout = if interest == WaitInterest::Ready {
            Some(Duration::ZERO)
        } else {
            deadline.map(|d| d.at().saturating_duration_since(Instant::now()))
        };
        // Neither borrowed source survives advancement, close or reopen.
        let (input, notification) =
            wait(interaction.input_source()?, self.source.as_fd(), timeout)?;
        if notification {
            let events = self.take()?;
            if events & 4 != 0 {
                return Ok(Some(interaction.interrupt()?));
            }
            if events & 2 != 0 {
                return Ok(Some(interaction.interrupt()?));
            }
            if events & 1 != 0 {
                return Ok(interaction.advance(Wake::Resize)?);
            }
        }
        if input || interest == WaitInterest::Ready {
            Ok(interaction.advance(Wake::InputReady)?)
        } else if let Some(due) = deadline.filter(|d| Instant::now() >= d.at()) {
            Ok(interaction.advance(Wake::Deadline(due))?)
        } else {
            Ok(None)
        }
    }
}

impl Drop for Notifications {
    fn drop(&mut self) {
        self.handle.close();
        if let Some(worker) = self.worker.take() {
            let _ = worker.join();
        }
    }
}

#[cfg(test)]
mod lifetime_tests {
    use super::*;

    #[test]
    fn notification_lifetime_retires_workers_and_descriptors_in_isolated_process() {
        let result = std::process::Command::new(std::env::current_exe().unwrap())
            .args([
                "--exact",
                "interaction::lifetime_tests::notification_cycles",
                "--ignored",
                "--nocapture",
            ])
            .output()
            .unwrap();
        assert!(String::from_utf8_lossy(&result.stdout).contains("notification cycles: 32"));
        assert!(result.status.success(), "{:?}", result);
    }

    #[test]
    #[ignore = "executed by the isolated lifetime test; signals must not affect other unit tests"]
    fn notification_cycles() {
        let descriptors = || {
            std::fs::read_dir(if cfg!(target_os = "linux") {
                "/proc/self/fd"
            } else {
                "/dev/fd"
            })
            .unwrap()
            .count()
        };
        let before = descriptors();
        for _ in 0..32 {
            let calls = Arc::new(AtomicUsize::new(0));
            let observed = calls.clone();
            let mut scope = Notifications::new(move |_| {
                observed.fetch_add(1, Ordering::AcqRel);
            })
            .unwrap();
            for (signal, wanted, bit) in [(SIGWINCH, 1, 1), (SIGINT, 2, 2)] {
                signal_hook::low_level::raise(signal).unwrap();
                let deadline = Instant::now() + Duration::from_secs(2);
                while calls.load(Ordering::Acquire) < wanted {
                    assert!(Instant::now() < deadline, "notification callback stalled");
                    thread::yield_now();
                }
                assert_eq!(scope.take().unwrap(), bit);
                assert_eq!(scope.take().unwrap(), 0);
            }
            drop(scope);
            assert_eq!(descriptors(), before, "notification descriptors leaked");
        }
        println!("notification cycles: 32; scoped workers and descriptors retired");
        std::io::stdout().flush().unwrap();
    }
}

#[cfg(target_os = "linux")]
fn wait(
    input: std::os::fd::BorrowedFd<'_>,
    notification: std::os::fd::BorrowedFd<'_>,
    timeout: Option<Duration>,
) -> io::Result<(bool, bool)> {
    use rustix::event::{PollFd, PollFlags, Timespec, poll};
    let mut descriptors = [
        PollFd::new(&input, PollFlags::IN),
        PollFd::new(&notification, PollFlags::IN),
    ];
    let timeout = timeout
        .map(Timespec::try_from)
        .transpose()
        .map_err(io::Error::other)?;
    match poll(&mut descriptors, timeout.as_ref()) {
        Ok(_) => Ok((
            !descriptors[0].revents().is_empty(),
            !descriptors[1].revents().is_empty(),
        )),
        Err(rustix::io::Errno::INTR) => Ok((false, false)),
        Err(error) => Err(error.into()),
    }
}

#[cfg(target_os = "macos")]
fn wait(
    input: std::os::fd::BorrowedFd<'_>,
    notification: std::os::fd::BorrowedFd<'_>,
    timeout: Option<Duration>,
) -> io::Result<(bool, bool)> {
    use nix::sys::{
        select::{FD_SETSIZE, FdSet, select},
        time::{TimeVal, TimeValLike},
    };
    use std::os::fd::AsRawFd;
    // Darwin select supports terminal character devices; reject extents before FD_SET.
    if [input.as_raw_fd(), notification.as_raw_fd()]
        .iter()
        .any(|fd| *fd < 0 || *fd as usize >= FD_SETSIZE)
    {
        return Err(io::Error::other(
            "terminal readiness descriptor exceeds select extent",
        ));
    }
    let mut descriptors = FdSet::new();
    descriptors.insert(input);
    descriptors.insert(notification);
    let mut timeout = timeout.map(|duration| {
        TimeVal::microseconds(
            i64::try_from(duration.as_micros() + u128::from(duration.subsec_nanos() % 1000 != 0))
                .unwrap_or(i64::MAX),
        )
    });
    match select(None, &mut descriptors, None, None, timeout.as_mut()) {
        Ok(_) => Ok((
            descriptors.contains(input),
            descriptors.contains(notification),
        )),
        Err(nix::errno::Errno::EINTR) => Ok((false, false)),
        Err(error) => Err(io::Error::from_raw_os_error(error as i32)),
    }
}

pub(crate) fn open(
    interaction: &mut Interaction,
    label: &str,
    state: &str,
) -> Result<(), replai::Error> {
    interaction.editor_mut()?.clear();
    let prompt = Prompt::new(label)?
        .with_state(&format!(" · {state}"))?
        .with_continuation("... ")?;
    // Retain the product's explicit legacy VT assumption, not a claim of protocol discovery.
    // Native acquisition still verifies paired TTYs, dimensions and restoration.
    let config = replai::TerminalConfig::compatibility(replai::Theme::from_environment(true));
    interaction.open_with_config(&io::stdin(), &io::stdout(), prompt, config)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn wake_readiness_has_no_polling_cadence() {
        let (source, mut sink) = UnixStream::pair().unwrap();
        let (input, _other) = UnixStream::pair().unwrap();
        sink.write_all(&[1]).unwrap();
        assert_eq!(
            wait(input.as_fd(), source.as_fd(), Some(Duration::ZERO)).unwrap(),
            (false, true)
        );
    }

    #[test]
    fn wake_timeout_is_not_input_readiness() {
        let (source, _sink) = UnixStream::pair().unwrap();
        let (input, _other) = UnixStream::pair().unwrap();
        let started = Instant::now();
        let timeout = Duration::from_millis(25);
        assert_eq!(
            wait(input.as_fd(), source.as_fd(), Some(timeout)).unwrap(),
            (false, false)
        );
        assert!(started.elapsed() >= timeout);
    }
}
