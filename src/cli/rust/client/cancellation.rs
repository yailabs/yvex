//! Admission-aware cancellation lifetime shared by native product consumers.
//! Callers own exact session/socket meaning; the response stream owns settlement.
use std::sync::{
    Mutex,
    atomic::{AtomicBool, AtomicUsize, Ordering},
};
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

// A cancellation before TURN_STARTED is pending intent, not an idle-session
// cancellation. Both the signal and acknowledgement paths claim one dispatch.
#[derive(Default)]
pub(crate) struct Interrupts {
    active: AtomicBool,
    admitted: AtomicBool,
    dispatched: AtomicBool,
    count: AtomicUsize,
    terminate: AtomicBool,
    worker: Mutex<Option<std::thread::JoinHandle<()>>>,
}

impl Interrupts {
    pub(crate) fn begin(&self) {
        self.admitted.store(false, Ordering::Release);
        self.dispatched.store(false, Ordering::Release);
        self.count.store(0, Ordering::Release);
        self.active.store(true, Ordering::Release);
    }

    pub(crate) fn request(&self, terminating: bool) {
        if terminating {
            self.terminate.store(true, Ordering::Release);
        }
        if self.active.load(Ordering::Acquire) && self.count.fetch_add(1, Ordering::AcqRel) != 0 {
            self.terminate.store(true, Ordering::Release);
        }
    }

    fn claim(&self) -> bool {
        self.active.load(Ordering::Acquire)
            && self.admitted.load(Ordering::Acquire)
            && self.count.load(Ordering::Acquire) != 0
            && self
                .dispatched
                .compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire)
                .is_ok()
    }

    pub(crate) fn admit(&self) {
        self.admitted.store(true, Ordering::Release);
    }

    pub(crate) fn terminating(&self) -> bool {
        self.terminate.load(Ordering::Acquire)
    }

    pub(crate) fn dispatch(&self, cancel: impl FnOnce() -> Result<()> + Send + 'static) {
        let Ok(mut worker) = self.worker.lock() else {
            eprintln!("yvex: cancellation unconfirmed: worker ownership poisoned");
            return;
        };
        if !self.claim() {
            return;
        }
        // The control connection must never block the only generation reader.
        match std::thread::Builder::new()
            .name("yvex-request-cancel".into())
            .spawn(move || {
                if let Err(error) = cancel() {
                    eprintln!("yvex: cancellation unconfirmed: {error}");
                }
            }) {
            Ok(handle) => *worker = Some(handle),
            Err(error) => eprintln!("yvex: cancellation unconfirmed: {error}"),
        }
    }

    pub(crate) fn finish(&self) -> Result<()> {
        let worker = {
            let mut worker = self.worker.lock().map_err(|_| "cancel worker poisoned")?;
            self.active.store(false, Ordering::Release);
            worker.take()
        };
        // No cancellation from this turn may survive into the next prompt.
        if let Some(worker) = worker {
            worker.join().map_err(|_| "cancel worker panicked")?;
        }
        Ok(())
    }
}

impl Drop for Interrupts {
    fn drop(&mut self) {
        if let Err(error) = self.finish() {
            eprintln!("yvex: cancellation cleanup unconfirmed: {error}");
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Arc;
    #[test]
    fn turn_finish_retires_its_cancel_worker_before_another_turn() {
        let state = Interrupts::default();
        state.begin();
        let completed = Arc::new(AtomicBool::new(false));
        let observed = completed.clone();
        *state.worker.lock().unwrap() = Some(std::thread::spawn(move || {
            observed.store(true, Ordering::Release);
        }));
        state.finish().unwrap();
        assert!(completed.load(Ordering::Acquire));
        assert!(state.worker.lock().unwrap().is_none());
        assert!(!state.active.load(Ordering::Acquire));
        state.begin();
        assert_eq!(state.count.load(Ordering::Acquire), 0);
        assert!(!state.dispatched.load(Ordering::Acquire));
    }
    #[test]
    fn early_cancel_is_pending_until_admission_and_dispatches_once() {
        let state = Interrupts::default();
        state.begin();
        state.request(false);
        assert!(!state.claim());
        state.admitted.store(true, Ordering::Release);
        assert!(state.claim());
        assert!(!state.claim());
        state.request(false);
        assert!(state.terminate.load(Ordering::Acquire));
        state.active.store(false, Ordering::Release);
        assert!(!state.claim());
    }
}
