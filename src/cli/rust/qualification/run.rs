//! Bounded local qualification execution; existing native client owns the wire.
//! Never replace an engine/profile or touch an operator's session.
use super::{RULES, Result, WORKLOADS, require, statistics, target_identity};
use crate::{
    catalog, client,
    client::cancellation::Interrupts,
    ffi::{self, Client, raw},
    interaction,
    registry::Invocation,
};
use serde_json::{Value, json};
use std::os::unix::fs::{DirBuilderExt, OpenOptionsExt};
use std::{
    fs::{File, OpenOptions},
    io::Write,
    path::Path,
    sync::{Arc, Mutex},
    time::Instant,
};

#[derive(Clone)]
struct Session {
    socket: Option<String>,
    alias: String,
    generation: u64,
    name: String,
}

fn authenticate_lineage(
    path: &str,
    engine: &raw::yvex_server_engine_summary,
) -> Result<ffi::execution::BindingLineage> {
    let lineage = ffi::execution::binding_lineage(path)?;
    require(
        ffi::text(&lineage.summary.identity) == ffi::text(&engine.runtime_binding_identity)
            && ffi::text(&lineage.summary.artifact_identity)
                == ffi::text(&engine.artifact_identity),
        "local binding does not authenticate the exact loaded producer",
    )?;
    Ok(lineage)
}

impl Session {
    fn close_checked(&self, expected_sessions: u64) -> Result<()> {
        self.operation(
            raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_CLOSE,
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK,
        )?;
        let after = client::engines(self.socket.as_deref())?;
        let current = after
            .iter()
            .find(|e| ffi::text(&e.alias) == self.alias)
            .ok_or("engine retired during measurement")?;
        require(
            current.generation == self.generation
                && current.session_count == expected_sessions
                && current.active_work == 0,
            "engine identity/cleanup changed during measurement",
        )
    }

    fn cancel(&self, interrupts: &Interrupts) {
        let session = self.clone();
        interrupts.dispatch(move || {
            let mut connection = Client::connect(session.socket.as_deref())?;
            connection.timeout(2000)?;
            let request = session.request(
                &mut connection,
                raw::yvex_client_operation_YVEX_CLIENT_OP_GENERATION_CANCEL,
            )?;
            connection.send(&request)?;
            let reply = client::response(&mut connection, &request)?;
            require(
                reply.kind == raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK,
                "unexpected cancellation acknowledgement",
            )
        });
    }

    fn request(
        &self,
        client: &mut Client,
        op: raw::yvex_client_operation,
    ) -> Result<raw::yvex_client_request> {
        let mut request = client.request(op);
        ffi::put_text(&mut request.model_alias, &self.alias)?;
        ffi::put_text(&mut request.session_name, &self.name)?;
        request.engine_generation = self.generation;
        Ok(request)
    }

    fn operation(
        &self,
        op: raw::yvex_client_operation,
        expected: raw::yvex_client_message_kind,
    ) -> Result<()> {
        let mut connection = Client::connect(self.socket.as_deref())?;
        let request = self.request(&mut connection, op)?;
        connection.send(&request)?;
        let reply = client::response(&mut connection, &request)?;
        require(reply.kind == expected, "unexpected session lifecycle reply")
    }
}

struct Cancellation {
    // Retire signal callbacks before releasing their shared request lifetime.
    _notifications: interaction::Notifications,
    interrupts: Arc<Interrupts>,
    current: Arc<Mutex<Option<Session>>>,
}

impl Cancellation {
    fn new() -> Result<Self> {
        let interrupts = Arc::new(Interrupts::default());
        let current: Arc<Mutex<Option<Session>>> = Arc::new(Mutex::new(None));
        let state = Arc::clone(&interrupts);
        let session = Arc::clone(&current);
        let notifications = interaction::Notifications::new(move |signal| {
            if !matches!(
                signal,
                signal_hook::consts::SIGINT | signal_hook::consts::SIGTERM
            ) {
                return;
            }
            state.request(true); // Stop the entire suite, not just this sample.
            if let Ok(current) = session.lock()
                && let Some(session) = current.as_ref()
            {
                session.cancel(&state);
            }
        })?;
        Ok(Self {
            _notifications: notifications,
            interrupts,
            current,
        })
    }

    fn begin(&self, session: &Session) -> Result<()> {
        *self
            .current
            .lock()
            .map_err(|_| "qualification signal ownership poisoned")? = Some(session.clone());
        self.interrupts.begin();
        // A signal racing idle -> dispatch must stay pending until admission.
        if self.interrupts.terminating() {
            self.interrupts.request(true);
        }
        Ok(())
    }

    fn finish(&self) -> Result<()> {
        self.interrupts.finish()?;
        *self
            .current
            .lock()
            .map_err(|_| "qualification signal ownership poisoned")? = None;
        Ok(())
    }
}

fn journal(file: &mut File, value: &Value) -> Result<()> {
    serde_json::to_writer(&mut *file, value)?;
    file.write_all(b"\n")?;
    file.flush()?;
    Ok(())
}

fn input_identity(prompt: &str, history: &[(String, String)]) -> Result<String> {
    // Reused input includes actual preceding continuations, not only the new text.
    // Token-ledger identities originate in the runtime; no prose reconstruction.
    Ok(ffi::digest(&serde_json::to_vec(&json!({
        "prompt": prompt, "history": history
    }))?)?)
}

fn terminal_refusal(reply: &raw::yvex_client_message, session: &Session) -> Value {
    let cancelled = reply.status == raw::yvex_status_YVEX_ERR_CANCELLED
        && reply.generation_phase == raw::yvex_client_generation_phase_YVEX_CLIENT_PHASE_CANCELLED
        && reply.cancellation_class
            == raw::yvex_client_cancellation_class_YVEX_CLIENT_CANCELLATION_COMPLETED;
    let partial = &reply.partial_turn;
    json!({"kind":if cancelled {"cancelled"} else {"refused"},
        "session":session.name,"status":reply.status,"status_name":ffi::status_name(reply.status),
        "reason":ffi::text(&reply.reason),"phase":reply.generation_phase,
        "session_state":reply.session_state,"cancellation_class":reply.cancellation_class,
        "partial":(partial.available != 0).then(|| json!({
            "schema":partial.schema_version,"committed_tokens":partial.committed_token_count,
            "final_position":partial.final_committed_position,"reset_required":partial.reset_required,
            "token_ledger_identity":ffi::text(&partial.token_ledger_identity),
            "published_text_identity":ffi::text(&partial.published_text_identity)})),
        "measurement":false,"retry":false})
}

fn turn(
    session: &Session,
    prompt: &str,
    reasoning: &str,
    bound: u64,
    greedy: bool,
    log: &mut File,
    cancellation: &Cancellation,
) -> Result<Value> {
    let started = Instant::now();
    let mut connection = Client::connect(session.socket.as_deref())?;
    connection.timeout(1_800_000)?;
    let mut request = session.request(
        &mut connection,
        raw::yvex_client_operation_YVEX_CLIENT_OP_GENERATION_TURN,
    )?;
    request.maximum_new_tokens = bound;
    request.reasoning_policy = match reasoning {
        "none" => raw::yvex_reasoning_policy_YVEX_REASONING_DISABLED,
        "high" => raw::yvex_reasoning_policy_YVEX_REASONING_ENABLED,
        "maximum" => raw::yvex_reasoning_policy_YVEX_REASONING_MAXIMUM,
        _ => return Err("qualification: unsupported reasoning mode".into()),
    };
    if greedy {
        request.stochastic = 0;
        request.temperature = 0.0;
    }
    let sampling = json!({"stochastic":request.stochastic,"temperature":request.temperature,
        "top_k":request.top_k,"top_p":request.top_p,"min_p":request.min_p,"typical_p":request.typical_p,
        "seed_present":request.seed_present,"seed":request.seed_present.ne(&0).then_some(request.seed)});
    journal(
        log,
        &json!({"kind":"dispatch","session":session.name,"prompt_sha256":ffi::digest(prompt.as_bytes())?,
        "reasoning":reasoning,"maximum_output":bound,"sampling":sampling}),
    )?;
    connection.send_text(&request, prompt, &[])?;
    let mut first_visible = None;
    let mut first_reasoning = None;
    let mut first_final = None;
    let mut admitted = None;
    loop {
        let reply = connection.receive()?;
        require(
            reply.request_number == request.request_number,
            "qualification response correlation mismatch",
        )?;
        match reply.kind {
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_TURN_STARTED => {
                cancellation.interrupts.admit();
                session.cancel(&cancellation.interrupts);
                admitted = Some(started.elapsed().as_secs_f64());
                journal(
                    log,
                    &json!({"kind":"admitted","seconds":admitted,"session":session.name}),
                )?;
            }
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ERROR => {
                // A correlated terminal refusal is settled failure, not lost
                // delivery. Preserve its partial-state facts before cleanup.
                return Ok(terminal_refusal(&reply, session));
            }
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_EVENT => {
                journal(
                    log,
                    &json!({"kind":"server-event","event":client::event_json(&reply.event)}),
                )?;
            }
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_FRAGMENT => {
                let count = usize::try_from(reply.byte_count)?;
                require(count <= reply.bytes.len(), "fragment exceeds native bound")?;
                let visible = count > 0 && matches!(reply.stream_channel, 1 | 2);
                let seconds = started.elapsed().as_secs_f64();
                if visible {
                    first_visible.get_or_insert(seconds);
                    if reply.stream_channel == 1 {
                        first_final.get_or_insert(seconds);
                    }
                    if reply.stream_channel == 2 {
                        first_reasoning.get_or_insert(seconds);
                    }
                }
                // No model prose in the canonical receipt. Raw fragment identity only.
                journal(
                    log,
                    &json!({"kind":"fragment","seconds":seconds,"channel":reply.stream_channel,
                    "bytes":count,"sha256":ffi::digest(&reply.bytes[..count])?}),
                )?;
            }
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_TURN_COMPLETE => {
                let complete = started.elapsed().as_secs_f64();
                require(
                    reply.reused_tokens <= reply.prompt_tokens
                        && reply.prefill_tokens == reply.prompt_tokens - reply.reused_tokens,
                    "prefill population mismatch",
                )?;
                let m = &reply.measurement;
                let post_first = (m.schema_version == raw::YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1
                    && m.scope == raw::yvex_execution_measurement_scope_YVEX_EXECUTION_SCOPE_SUBSEQUENT_DECODE
                    && m.available & u64::from(raw::YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE) != 0)
                    .then_some(m.cumulative_rate);
                return Ok(
                    json!({"kind":"turn","session":session.name,"client_complete_seconds":complete,
                    "client_admitted_seconds":admitted,"sampling":sampling,
                    "client_first_visible_seconds":first_visible,"client_first_reasoning_seconds":first_reasoning,
                    "client_first_final_seconds":first_final,"server_first_token_seconds":reply.first_token_seconds,
                    "first_fragment_publication_seconds":null,"prompt_tokens":reply.prompt_tokens,
                    "reused_tokens":reply.reused_tokens,"prefill_tokens":reply.prefill_tokens,
                    "prefill_seconds":reply.prefill_seconds,"prefill_rate":reply.prefill_rate,
                    "generated_tokens":reply.generated_tokens,"post_first_decode_rate":post_first,
                    "post_first_decode_units":m.completed_units,"post_first_decode_ns":m.duration_ns,
                    "reasoning_tokens":reply.reasoning_tokens,"final_tokens":reply.final_tokens,
                    "reasoning_seconds":reply.reasoning_seconds,"final_seconds":reply.final_seconds,
                    "server_first_reasoning_seconds":
                        (reply.reasoning_tokens>0).then_some(reply.first_reasoning_seconds),
                    "server_first_final_seconds":(reply.final_tokens>0).then_some(reply.first_final_seconds),
                    "reasoning_phase_rate":
                        (reply.reasoning_tokens>0 && reply.reasoning_seconds>0.0).then_some(reply.reasoning_rate),
                    "final_phase_rate":(reply.final_tokens>0 && reply.final_seconds>0.0).then_some(reply.final_rate),
                    "reasoning_to_final_transition_seconds":null,
                    "total_completion_seconds":reply.total_completion_seconds,
                    "draft_cycles":reply.draft_cycle_count,"draft_forwards":reply.draft_forward_count,
                    "proposed_tokens":reply.proposed_tokens,
                    "selected_verification_tokens":reply.selected_verification_tokens,
                    "target_verifications":reply.target_verification_count,
                    "accepted_tokens":reply.accepted_draft_tokens,
                    "rejected_tokens":reply.rejected_draft_tokens,"discarded_tokens":reply.discarded_draft_tokens,
                    "correction_or_bonus_tokens":reply.target_correction_or_bonus_tokens,
                    "maximum_accepted_prefix":reply.maximum_accepted_prefix,
                    "mean_accepted_prefix":reply.mean_accepted_prefix,"draft_seconds":reply.draft_seconds,
                    "verification_seconds":reply.verification_seconds,"commit_seconds":reply.speculative_commit_seconds,
                    "stop_reason":reply.stop_reason,"turn_identity":ffi::text(&reply.turn_identity),
                    "token_identity":ffi::text(&reply.generated_token_identity)}),
                );
            }
            _ => {
                return Err(
                    "qualification: unexpected turn reply; reconcile session, do not retry".into(),
                );
            }
        }
    }
}

fn locks() -> Result<Vec<File>> {
    use std::os::unix::fs::{MetadataExt, OpenOptionsExt, PermissionsExt};
    let uid = rustix::process::geteuid().as_raw();
    let base = std::env::var_os("XDG_RUNTIME_DIR")
        .map(std::path::PathBuf::from)
        .unwrap_or_else(std::env::temp_dir);
    require(
        base.is_absolute() && base.is_dir() && !base.is_symlink(),
        "unsafe QA runtime root",
    )?;
    let root = base.join(format!("yvex-qa-{uid}"));
    std::fs::create_dir_all(&root)?;
    require(
        !root.is_symlink() && root.metadata()?.uid() == uid,
        "unsafe QA owner",
    )?;
    let root = root.join("locks");
    std::fs::create_dir_all(&root)?;
    require(
        !root.is_symlink() && root.metadata()?.uid() == uid,
        "unsafe QA locks",
    )?;
    std::fs::set_permissions(&root, std::fs::Permissions::from_mode(0o700))?;
    let mut held = Vec::new();
    for name in [
        "benchmark-directory",
        "cuda-device",
        "gb10-live-model",
        "metal-device",
    ] {
        let fd = rustix::fs::open(
            root.join(format!("{name}.lock")),
            rustix::fs::OFlags::CREATE
                | rustix::fs::OFlags::RDWR
                | rustix::fs::OFlags::NOFOLLOW
                | rustix::fs::OFlags::CLOEXEC,
            rustix::fs::Mode::from_raw_mode(0o600),
        )?;
        let file = File::from(fd);
        require(
            file.metadata()?.is_file() && file.metadata()?.uid() == uid,
            "unsafe QA lock file",
        )?;
        rustix::fs::flock(&file, rustix::fs::FlockOperation::NonBlockingLockExclusive)?;
        held.push(file);
    }
    // OpenOptionsExt is also used for the caller-owned journal below.
    let _ = OpenOptions::new().mode(0o600);
    Ok(held)
}

fn prompts(case: &Value) -> Result<Vec<String>> {
    require(
        case.get("native_disposition").is_none(),
        "suite case requires an adapter not admitted by this native text lane",
    )?;
    let mut result = case["turns"]
        .as_array()
        .ok_or("qualification: missing native turns")?
        .iter()
        .map(|v| {
            v.as_str()
                .map(str::to_owned)
                .ok_or("qualification: non-text turn")
        })
        .collect::<std::result::Result<Vec<_>, _>>()?;
    if let Some(repeat) = case.get("repeat") {
        let count = repeat["count"]
            .as_u64()
            .ok_or("qualification: invalid repeat")?;
        require(
            count <= 100_000 && !result.is_empty(),
            "repeat bound exceeded",
        )?;
        let template = repeat["template"]
            .as_str()
            .ok_or("qualification: template missing")?;
        for index in 0..count {
            result[0].push_str(&template.replace("{index:04d}", &format!("{index:04}")));
        }
    }
    require(
        !result.is_empty()
            && result
                .iter()
                .all(|s| !s.is_empty() && s.len() <= 1024 * 1024),
        "prompt extent invalid",
    )?;
    Ok(result)
}

pub(super) fn execute(inv: &Invocation<'_>, width: usize, styled: bool) -> Result<Value> {
    let catalog: Value = serde_json::from_str(WORKLOADS)?;
    let suite = catalog["suites"]
        .as_array()
        .unwrap()
        .iter()
        .find(|s| s["id"].as_str() == inv.value("--suite"))
        .ok_or("qualification: unknown suite")?;
    let case = suite["cases"]
        .as_array()
        .unwrap()
        .iter()
        .find(|c| c["id"].as_str() == inv.value("--case"))
        .ok_or("qualification: unknown case")?;
    let reasoning = inv
        .value("--reasoning")
        .ok_or("qualification: reasoning mode required")?;
    require(
        case["reasoning_modes"]
            .as_array()
            .unwrap()
            .iter()
            .any(|r| r == reasoning),
        "reasoning mode not in suite",
    )?;
    let texts = prompts(case)?;
    let repeats: usize = inv.value("--samples").unwrap_or("3").parse()?;
    require((1..=20).contains(&repeats), "sample count outside 1..20")?;
    let _locks = locks()?;
    let socket = inv.value("--socket");
    let status = client::host_status(socket)?.json();
    for key in ["active_requests", "active_http_requests", "queue_depth"] {
        require(
            status[key].as_u64() == Some(0),
            "host busy or activity unavailable",
        )?;
    }
    let engines = client::engines(socket)?;
    require(
        engines.iter().all(|e| {
            e.active_work == 0 && e.attached_client_count == 0 && e.model_lease_count == 0
        }),
        "operator work/client/lease present on host",
    )?;
    let choice = catalog::select_runtime(
        inv,
        Some(&inv.positionals[0]),
        Some(&engines),
        width,
        styled,
    )?;
    let engine = engines
        .iter()
        .find(|e| ffi::text(&e.alias) == choice.alias)
        .ok_or("qualification: loaded variant not found")?;
    require(
        engine.active_work == 0
            && engine.attached_client_count == 0
            && engine.model_lease_count == 0,
        "operator work/client/lease present",
    )?;
    let published: Vec<Value> = serde_json::from_str(super::CATALOG)?;
    require(
        published.iter().any(|r| {
            r["target"]["artifact_set"] == ffi::text(&engine.artifact_identity)
                && r["target"]["upstream_repository"] == suite["applicability"]["repository"]
                && r["target"]["checkpoint"] == suite["applicability"]["revision"]
        }),
        "no exact artifact/checkpoint relationship for this suite; family name is insufficient",
    )?;
    require(
        case["execution_strategies"]
            .as_array()
            .unwrap()
            .iter()
            .any(|v| v == client::strategy(engine.execution_strategy)),
        "loaded strategy not in suite",
    )?;
    let lineage = authenticate_lineage(&choice.binding_path, engine)?;
    let dir = Path::new(
        inv.value("--receipt-dir")
            .ok_or("receipt directory required")?,
    );
    std::fs::DirBuilder::new().mode(0o700).create(dir)?; // Exclusive; never overwrite evidence.
    let mut log = OpenOptions::new()
        .create_new(true)
        .write(true)
        .mode(0o600)
        .open(dir.join("events.jsonl"))?;
    let mut observations = Vec::new();
    let cancellation = Cancellation::new()?;
    for sample in 0..repeats {
        require(
            !cancellation.interrupts.terminating(),
            "qualification interrupted before next sample",
        )?;
        let nonce = format!(
            "{}-{sample}-{:?}",
            std::process::id(),
            std::time::SystemTime::now()
        );
        let session = Session {
            socket: socket.map(str::to_owned),
            alias: choice.alias.clone(),
            generation: engine.generation,
            name: format!("bench-{}", &ffi::digest(nonce.as_bytes())?[..24]),
        };
        journal(
            &mut log,
            &json!({"kind":"session-planned","session":session.name,"engine":client::engine_json(engine)}),
        )?;
        session.operation(
            raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_NEW,
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_SESSION,
        )?;
        let mut history = Vec::new();
        for (index, prompt) in texts.iter().enumerate() {
            cancellation.begin(&session)?;
            let outcome = turn(
                &session,
                prompt,
                reasoning,
                case["maximum_output"]
                    .as_u64()
                    .ok_or("output bound missing")?,
                inv.value("--sampling") == Some("greedy"),
                &mut log,
                &cancellation,
            );
            cancellation.finish()?;
            let observation = match outcome {
                Ok(mut value) => {
                    if value["kind"] != "turn" || cancellation.interrupts.terminating() {
                        journal(&mut log, &value)?;
                        session.close_checked(engine.session_count)?;
                        journal(
                            &mut log,
                            &json!({"kind":"nonmeasurement-session-closed",
                            "session":session.name,"measurement":false,"retry":false}),
                        )?;
                        log.sync_all()?;
                        return Err(format!(
                            "qualification stopped: {}; owned session closed; no performance receipt",
                            value["reason"].as_str().unwrap_or("interrupted")
                        ).into());
                    }
                    value["sample"] = json!(sample);
                    value["turn_index"] = json!(index);
                    value["input_identity"] = json!(input_identity(prompt, &history)?);
                    history.push((
                        ffi::digest(prompt.as_bytes())?,
                        value["token_identity"]
                            .as_str()
                            .filter(|v| !v.is_empty())
                            .ok_or("qualification: missing committed token identity")?
                            .to_owned(),
                    ));
                    value
                }
                Err(error) => {
                    journal(
                        &mut log,
                        &json!({"kind":"unsettled","session":session.name,"reason":error.to_string(),"retry":false}),
                    )?;
                    return Err(format!(
                        "{error}; reconcile owned session {}; no retry or blind close performed",
                        session.name
                    )
                    .into());
                }
            };
            journal(&mut log, &observation)?;
            observations.push(observation);
        }
        session.close_checked(engine.session_count)?;
    }
    log.sync_all()?;
    let result = receipt(
        (suite, case, &texts),
        engine,
        &choice.contract,
        (reasoning, inv.value("--sampling").unwrap_or("product")),
        &lineage,
        &observations,
        dir,
    )?;
    super::validate(&result)?;
    let mut file = OpenOptions::new()
        .create_new(true)
        .write(true)
        .mode(0o600)
        .open(dir.join("receipt.json"))?;
    serde_json::to_writer_pretty(&mut file, &result)?;
    file.sync_all()?;
    Ok(json!({"schema":"yvex.qualification.catalog.v1","targets":[result]}))
}

// Different continuations create different later inputs. Keep exact groups;
// never discard the completed turns or average their incompatible histories.
fn input_groups(rows: &[Value], index: usize) -> Result<Vec<(String, Vec<&Value>)>> {
    let mut groups = std::collections::BTreeMap::<String, Vec<&Value>>::new();
    for row in rows
        .iter()
        .filter(|r| r["turn_index"].as_u64() == Some(index as u64))
    {
        let identity = row["input_identity"]
            .as_str()
            .filter(|s| !s.is_empty())
            .ok_or("qualification: missing exact input identity")?;
        groups.entry(identity.to_owned()).or_default().push(row);
    }
    require(
        !groups.is_empty(),
        "qualification: no completed input group",
    )?;
    Ok(groups.into_iter().collect())
}

fn measurements(
    case: &Value,
    prompts: &[String],
    rows: &[Value],
    rules: &Value,
    dir: &Path,
) -> Result<(Vec<Value>, Vec<Value>)> {
    let mut measurements = Vec::new();
    let mut separated_inputs = Vec::new();
    for index in 0..prompts.len() {
        let groups = input_groups(rows, index)?;
        let separated = groups.len() > 1;
        for (identity, group) in groups {
            let case_id = format!("{}/turn-{index}", case["id"].as_str().unwrap());
            let case_id = if separated {
                let case_id = format!("{case_id}/input-{identity}");
                separated_inputs.push(json!({"case":case_id,"input_identity":identity,
                    "samples":group.len(),"reason":"actual prior continuations differ; no cross-input average"}));
                case_id
            } else {
                case_id
            };
            for (metric, field) in [
                ("admission.client", "client_admitted_seconds"),
                ("request.client-complete", "client_complete_seconds"),
                ("ttft.client-visible", "client_first_visible_seconds"),
                ("ttft.server", "server_first_token_seconds"),
                ("reasoning.first.server", "server_first_reasoning_seconds"),
                ("reasoning.first.client", "client_first_reasoning_seconds"),
                ("final.first.server", "server_first_final_seconds"),
                ("final.first.client", "client_first_final_seconds"),
                ("reasoning.phase-rate", "reasoning_phase_rate"),
                ("final.phase-rate", "final_phase_rate"),
                ("decode.post-first.committed", "post_first_decode_rate"),
                ("prefill.uncached", "prefill_rate"),
                ("prefill.wall", "prefill_seconds"),
            ] {
                let samples = group.iter().map(|r| r[field].clone()).collect::<Vec<_>>();
                if samples.iter().any(Value::is_null) {
                    continue;
                }
                let samples = json!(samples);
                let rule = &rules["metrics"][metric];
                measurements.push(json!({"metric":metric,"definition":rule[2],"unit":rule[1],
                    "case":case_id,"prompt_identity":identity,"reference_identity":null,
                    "session_state":if index==0{"fresh"}else{"reused"},
                    "warm_state":"resident engine; no hidden warmup","output_bound":case["maximum_output"],
                    "statistics":statistics(&samples)?,"samples":samples,
                    "scope":"local client and server terminal-summary measurements; no quality qualification",
                    "evidence":dir.join("events.jsonl").display().to_string()}));
            }
        }
    }
    Ok((measurements, separated_inputs))
}

fn receipt(
    workload: (&Value, &Value, &[String]),
    engine: &raw::yvex_server_engine_summary,
    choice: &Value,
    policy: (&str, &str),
    lineage: &ffi::execution::BindingLineage,
    rows: &[Value],
    dir: &Path,
) -> Result<Value> {
    let (suite, case, prompts) = workload;
    let (reasoning, selection) = policy;
    let sampling = rows
        .first()
        .and_then(|r| r.get("sampling"))
        .filter(|s| s.is_object())
        .ok_or("qualification: resolved sampling facts missing")?;
    require(
        rows.iter().all(|r| &r["sampling"] == sampling),
        "sampling policy changed between samples",
    )?;
    let rules: Value = serde_json::from_str(RULES)?;
    let mut target = serde_json::Map::new();
    for key in rules["fields"].as_object().unwrap().keys() {
        target.insert(key.clone(), Value::Null);
    }
    let mut target = Value::Object(target);
    target["schema"] = rules["target_schema"].clone();
    for (key, value) in [
        ("context", json!(engine.context_capacity)),
        (
            "prefill_geometry",
            json!(format!("chunk={}", engine.prefill_chunk_tokens)),
        ),
        (
            "sequence_geometry",
            json!(format!("width={}", engine.concurrent_sequences)),
        ),
        ("concurrency", json!(1)),
        (
            "specialization",
            json!(ffi::text(&engine.specialization_identity)),
        ),
        ("artifact_set", json!(ffi::text(&engine.artifact_identity))),
        (
            "binding",
            json!(ffi::text(&engine.runtime_binding_identity)),
        ),
        (
            "transformation_ir",
            json!(ffi::text(&lineage.summary.logical_transform_identity)),
        ),
        (
            "physical_policy",
            json!(ffi::text(&lineage.summary.profile_identity)),
        ),
        (
            "tokenizer_conversation",
            json!(lineage.tokenizer_conversation),
        ),
        (
            "runtime_configuration",
            json!(ffi::text(&engine.capacity_plan_identity)),
        ),
        ("representation", choice["variant"].clone()),
        ("backend", choice["backend"].clone()),
        (
            "strategy",
            json!(client::strategy(engine.execution_strategy)),
        ),
        ("reasoning", json!(reasoning)),
        ("sampling", json!(serde_json::to_string(sampling)?)),
        (
            "product_path",
            json!(format!("product-native-v{}", ffi::LOCAL_PROTOCOL_VERSION)),
        ),
        ("suite", json!(ffi::digest(&serde_json::to_vec(suite)?)?)),
    ] {
        target[key] = if value.as_str().is_some_and(str::is_empty) {
            Value::Null
        } else {
            value
        };
    }
    // Client build is NOT host build. Missing producer provenance prevents qualification.
    let mut claims = serde_json::Map::new();
    for plane in rules["planes"].as_array().unwrap() {
        claims.insert(plane.as_str().unwrap().into(),json!({
            "state":if plane=="product-path"{"CHARACTERIZED"}else{"UNQUALIFIED"},
            "scope":"Local native measurement only; independent reference and producer provenance incomplete",
            "required_evidence":[],"evidence":{},"blockers":[]}));
    }
    let (measurements, separated_inputs) = measurements(case, prompts, rows, &rules, dir)?;
    Ok(
        json!({"schema":rules["receipt_schema"],"id":"local-native","title":"Local native qualification receipt",
        "target_identity":target_identity(&target,&rules)?,
        "target":target,"origin":"local","claims":claims,"measurements":measurements,
        "provenance":{"source_stability":"unknown","evidence_class":"local-characterization",
            "resource_observation":null,
            "client_build":env!("YVEX_BUILD_IDENTITY"),
            "sampling_selection":selection,
            "separated_inputs":separated_inputs,
            "raw_evidence_sha256":ffi::digest(&std::fs::read(dir.join("events.jsonl"))?)?,
            "engine":client::engine_json(engine),"observations":rows,
            "cleanup":"owned sessions closed; generation/count checked"},
        "limitations":["Not YVEX-published qualification or a model-quality claim.",
            "Host source/build/checkpoint/hardware provenance unavailable; unknown fields refuse comparison.",
            "First fragment publication and reasoning-to-final transition not measured.",
            concat!("Different generated histories are separate input groups, ",
                "never pooled or treated as deterministic agreement."),
            "Advisory locks do not prove hardware exclusivity; external resource observation is required.",
            "No independent oracle; source stability unknown; first sample is not relabeled warmed by hidden work."]}),
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn different_histories_are_separate_measurements_not_cross_input_averages() {
        let rows = vec![
            json!({"turn_index":0,"input_identity":"same-first","prefill_seconds":1.0}),
            json!({"turn_index":0,"input_identity":"same-first","prefill_seconds":2.0}),
            json!({"turn_index":1,"input_identity":"history-a","prefill_seconds":3.0}),
            json!({"turn_index":1,"input_identity":"history-b","prefill_seconds":10.0}),
            json!({"turn_index":1,"input_identity":"history-b","prefill_seconds":20.0}),
        ];
        let rules: Value = serde_json::from_str(RULES).unwrap();
        let (metrics, groups) = measurements(
            &json!({"id":"fixture","maximum_output":256}),
            &["first".into(), "next".into()],
            &rows,
            &rules,
            Path::new("/fixture"),
        )
        .unwrap();
        assert_eq!(metrics.len(), 3);
        assert_eq!(metrics[0]["statistics"]["median"], 1.5);
        assert_eq!(metrics[0]["case"], "fixture/turn-0");
        assert_eq!(metrics[1]["statistics"]["median"], 3.0);
        assert_eq!(metrics[1]["samples"], json!([3.0]));
        assert_eq!(metrics[2]["statistics"]["median"], 15.0);
        assert_eq!(metrics[2]["samples"], json!([10.0, 20.0]));
        assert_ne!(metrics[1]["prompt_identity"], metrics[2]["prompt_identity"]);
        assert_ne!(metrics[1]["case"], metrics[2]["case"]);
        assert_eq!(groups.len(), 2);
        assert!(input_groups(&[json!({"turn_index":0})], 0).is_err());
        assert!(input_groups(&rows, 2).is_err());
    }

    #[test]
    #[ignore = "make test-rust-shell runs this alone: process signals and owned protocol fixture"]
    fn cancellation_protocol() {
        use std::{
            process::{Command, Stdio},
            time::Duration,
        };
        struct Fixture {
            child: std::process::Child,
            directory: std::path::PathBuf,
        }
        impl Drop for Fixture {
            fn drop(&mut self) {
                let _ = self.child.kill(); // Exact test-owned producer, never the operator host.
                let _ = self.child.wait();
                let _ = std::fs::remove_dir_all(&self.directory);
            }
        }
        let directory =
            std::env::temp_dir().join(format!("yvex-qual-cancel-{}", std::process::id()));
        std::fs::DirBuilder::new()
            .mode(0o700)
            .create(&directory)
            .unwrap();
        let socket = directory.join("host.sock");
        let executable =
            std::env::var_os("YVEX_TEST_PROTOCOL_FIXTURE").expect("protocol fixture required");
        let mut fixture = Fixture {
            child: Command::new(executable)
                .arg(&socket)
                .stdout(Stdio::null())
                .stderr(Stdio::null())
                .spawn()
                .unwrap(),
            directory,
        };
        let deadline = Instant::now() + Duration::from_secs(5);
        while !socket.exists() {
            assert!(fixture.child.try_wait().unwrap().is_none());
            assert!(Instant::now() < deadline);
            std::thread::sleep(Duration::from_millis(5));
        }
        for (index, (prompt, reasoning)) in [
            ("WAIT_EARLY_CANCEL", "none"),
            ("WAIT_PREFILL_CANCEL", "none"),
            ("WAIT_DECODE_CANCEL", "none"),
            ("WAIT_REASONING_CANCEL", "high"),
            ("PARTIAL_FENCE", "none"),
            ("PARTIAL_REASONING", "high"),
        ]
        .into_iter()
        .enumerate()
        {
            let session = Session {
                socket: Some(socket.to_str().unwrap().into()),
                alias: "deepseek4-v4-flash-dspark".into(),
                generation: 7,
                name: format!("replai-qualification-{index}"),
            };
            session
                .operation(
                    raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_NEW,
                    raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_SESSION,
                )
                .unwrap();
            let mut log =
                File::create(fixture.directory.join(format!("events-{index}.jsonl"))).unwrap();
            let cancellation = Cancellation::new().unwrap();
            cancellation.begin(&session).unwrap();
            let signal = prompt.starts_with("WAIT_").then(|| {
                std::thread::spawn(|| {
                    std::thread::sleep(Duration::from_millis(50));
                    signal_hook::low_level::raise(signal_hook::consts::SIGINT).unwrap();
                })
            });
            let result = turn(
                &session,
                prompt,
                reasoning,
                256,
                true,
                &mut log,
                &cancellation,
            );
            if let Some(signal) = signal {
                signal.join().unwrap();
            }
            cancellation.finish().unwrap();
            let result = result.unwrap();
            if prompt.starts_with("WAIT_") {
                assert_eq!(result["kind"], "cancelled");
            } else {
                assert_eq!(result["kind"], "refused");
                assert_eq!(result["partial"]["reset_required"], 1);
                assert!(result["partial"]["committed_tokens"].as_u64().unwrap() > 0);
            }
            assert_eq!(result["measurement"], false);
            session
                .operation(
                    raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_CLOSE,
                    raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK,
                )
                .unwrap();
            assert!(!fixture.directory.join("receipt.json").exists());
        }
    }

    #[test]
    fn reused_prompt_binds_actual_prior_token_ledger() {
        let empty = input_identity("next", &[]).unwrap();
        let history = vec![("prompt".to_owned(), "tokens-a".to_owned())];
        let identity = input_identity("next", &history).unwrap();
        assert_ne!(empty, identity);
        assert_eq!(identity, input_identity("next", &history).unwrap());
        assert_ne!(
            identity,
            input_identity("next", &[("prompt".into(), "tokens-b".into())]).unwrap()
        );
    }
}
