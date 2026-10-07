use super::{event_parts, event_render, render_log_record};
use crate::ffi::{self, raw};
type PhaseKey = (u64, String, String);
type PhaseFacts = (Option<(f64, f64)>, Option<f64>);
// Join server-authored facts, never client arrival clocks. Missing history stays unknown.
#[derive(Default)]
pub(crate) struct TurnPhases(std::collections::BTreeMap<PhaseKey, PhaseFacts>);
impl TurnPhases {
    pub(crate) fn render(
        &mut self,
        event: &raw::yvex_server_event,
        verbose: bool,
        width: usize,
        styled: bool,
    ) -> Result<String, replai::EditError> {
        use raw::*;
        let key = (
            event.process_id,
            ffi::text(&event.session_id),
            ffi::text(&event.request_id),
        );
        let kind = event.kind;
        if kind == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_TELEMETRY_DROPPED {
            self.0.clear();
        }
        if matches!(
            kind,
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_COMPLETED
                | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FIRST_TOKEN
        ) {
            if self.0.len() >= 128 && !self.0.contains_key(&key) {
                self.0.pop_first();
            }
            let entry = self.0.entry(key.clone()).or_default();
            let m = &event.measurement;
            if kind == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_COMPLETED
                && m.schema_version == YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1
                && m.scope == yvex_execution_measurement_scope_YVEX_EXECUTION_SCOPE_PREFILL
                && m.available & u64::from(YVEX_EXECUTION_MEASUREMENT_DURATION_AVAILABLE) != 0
                && m.available & u64::from(YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE)
                    != 0
                && m.cumulative_rate.is_finite()
                && m.cumulative_rate >= 0.0
            {
                entry.0 = Some((m.duration_ns as f64 / 1e9, m.cumulative_rate));
            }
            if kind == raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FIRST_TOKEN
                && event.seconds.is_finite()
                && event.seconds >= 0.0
            {
                entry.1 = Some(event.seconds);
            }
        }
        if matches!(
            kind,
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED
                | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_CANCELLED
                | raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FAILED
        ) {
            let (category, mut detail) = event_parts(event, verbose);
            let (prefill, first) = self.0.remove(&key).unwrap_or_default();
            if let Some((seconds, rate)) = prefill {
                detail.push_str(&format!(" prefill={rate:.2}t/s/{seconds:.3}s"));
            }
            if let Some(seconds) = first {
                detail.push_str(&format!(" ttft-server={seconds:.3}s"));
            }
            let m = &event.measurement;
            if m.schema_version == YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1
                && m.scope
                    == yvex_execution_measurement_scope_YVEX_EXECUTION_SCOPE_SUBSEQUENT_DECODE
                && m.available & u64::from(YVEX_EXECUTION_MEASUREMENT_DURATION_AVAILABLE) != 0
            {
                detail.push_str(&format!(" decode-time={:.3}s", m.duration_ns as f64 / 1e9));
            }
            return render_log_record(event, category, detail, verbose, width, styled);
        }
        event_render(event, verbose, width, styled)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn event(
        kind: raw::yvex_server_event_kind,
        process: u64,
        request: &str,
    ) -> raw::yvex_server_event {
        let mut e = raw::yvex_server_event {
            kind,
            process_id: process,
            ..Default::default()
        };
        ffi::put_text(&mut e.session_id, "fixture").unwrap();
        ffi::put_text(&mut e.request_id, request).unwrap();
        e
    }

    #[test]
    fn summary_uses_only_correlated_server_facts_and_retires_them() {
        let mut phases = TurnPhases::default();
        let mut first = event(
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FIRST_TOKEN,
            7,
            "r1",
        );
        first.seconds = 2.8;
        phases.render(&first, false, 200, false).unwrap();
        let other = event(
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED,
            8,
            "r1",
        );
        assert!(
            !phases
                .render(&other, false, 200, false)
                .unwrap()
                .contains("ttft-server")
        );
        let done = event(
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED,
            7,
            "r1",
        );
        assert!(
            phases
                .render(&done, false, 200, false)
                .unwrap()
                .contains("ttft-server=2.800s")
        );
        assert!(
            !phases
                .render(&done, false, 200, false)
                .unwrap()
                .contains("ttft-server")
        );
    }

    #[test]
    fn phase_summary_never_substitutes_total_prompt_or_client_clocks() {
        use raw::*;
        let mut phases = TurnPhases::default();
        let mut prefill = event(
            yvex_server_event_kind_YVEX_SERVER_EVENT_PREFILL_COMPLETED,
            7,
            "r1",
        );
        prefill.measurement.schema_version = YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1;
        prefill.measurement.scope = yvex_execution_measurement_scope_YVEX_EXECUTION_SCOPE_PREFILL;
        prefill.measurement.available = u64::from(
            YVEX_EXECUTION_MEASUREMENT_DURATION_AVAILABLE
                | YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE,
        );
        prefill.measurement.duration_ns = 1_000_000_000;
        prefill.measurement.cumulative_rate = 10.0;
        phases.render(&prefill, false, 200, false).unwrap();
        let done = event(
            yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED,
            7,
            "r1",
        );
        let summary = phases.render(&done, false, 200, false).unwrap();
        assert!(summary.contains("prefill=10.00t/s/1.000s"));
        assert!(!summary.contains("ttft-server") && !summary.contains("decode-time="));
        prefill.measurement.cumulative_rate = f64::NAN;
        phases.render(&prefill, false, 200, false).unwrap();
        assert!(
            !phases
                .render(&done, false, 200, false)
                .unwrap()
                .contains("prefill=")
        );
    }

    #[test]
    fn dropped_history_does_not_fill_missing_phases_with_zero() {
        let mut phases = TurnPhases::default();
        let first = event(
            raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_FIRST_TOKEN,
            7,
            "r1",
        );
        phases.render(&first, false, 200, false).unwrap();
        phases
            .render(
                &event(
                    raw::yvex_server_event_kind_YVEX_SERVER_EVENT_TELEMETRY_DROPPED,
                    7,
                    "",
                ),
                false,
                200,
                false,
            )
            .unwrap();
        let text = phases
            .render(
                &event(
                    raw::yvex_server_event_kind_YVEX_SERVER_EVENT_GENERATION_COMPLETED,
                    7,
                    "r1",
                ),
                false,
                200,
                false,
            )
            .unwrap();
        assert!(!text.contains("ttft-server") && !text.contains("prefill="));
    }
}
