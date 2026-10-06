//! Freshness of a public read, separate from the domain state in its value.
//! Clients bind this cache to exact connection/device identity. Changing that
//! identity requires a fresh cache; observations never authorize mutations.
use super::Error;
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ReadPosture {
    NeverObserved,
    Loading,
    Current,
    Stale,
    Unavailable,
    Failed,
    Unsupported,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ReadObservation<T> {
    pub posture: ReadPosture,
    pub observed_at_unix_ms: Option<u64>,
    pub value: Option<T>,
    pub failure: Option<Error>,
}
impl<T> Default for ReadObservation<T> {
    fn default() -> Self {
        Self { posture: ReadPosture::NeverObserved, observed_at_unix_ms: None, value: None, failure: None }
    }
}
impl<T> ReadObservation<T> {
    pub fn begin(&mut self) {
        self.posture = if self.value.is_some() { ReadPosture::Stale } else { ReadPosture::Loading };
        self.failure = None;
    }
    pub fn observed(&mut self, value: T, at_unix_ms: u64) {
        self.value = Some(value);
        self.observed_at_unix_ms = Some(at_unix_ms);
        self.posture = ReadPosture::Current;
        self.failure = None;
    }
    /// Classification comes from typed producer/transport failure, never prose.
    pub fn failed(&mut self, error: Error, posture: ReadPosture) {
        self.posture = if self.value.is_some() { ReadPosture::Stale } else {
            match posture {
                ReadPosture::Unsupported => ReadPosture::Unsupported,
                ReadPosture::Failed => ReadPosture::Failed,
                _ => ReadPosture::Unavailable,
            }
        };
        self.failure = Some(error);
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn absent_and_observed_empty_are_distinct_and_failure_retains_evidence() {
        let mut read = ReadObservation::<Vec<String>>::default();
        assert!(read.value.is_none());
        assert_eq!(read.posture, ReadPosture::NeverObserved);
        read.begin();
        assert_eq!(read.posture, ReadPosture::Loading);
        read.observed(vec![], 42);
        assert_eq!(read.value, Some(vec![]));
        read.begin();
        assert_eq!(read.posture, ReadPosture::Stale);
        read.failed(Error::local("transport_unavailable"), ReadPosture::Unavailable);
        assert_eq!(read.posture, ReadPosture::Stale);
        assert_eq!(read.observed_at_unix_ms, Some(42));
        assert_eq!(read.value, Some(vec![]));
        let mut other = ReadObservation::<Vec<String>>::default();
        other.failed(Error::local("unsupported_operation"), ReadPosture::Unsupported);
        assert_eq!(other.posture, ReadPosture::Unsupported);
        assert!(other.value.is_none());
    }
}
