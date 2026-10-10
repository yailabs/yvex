// Native interaction consumes typed server facts. It does not own session semantics.
use crate::{
    client,
    ffi::{self, Client, raw},
    interaction, presentation,
    registry::{Invocation, Registry},
};
use replai::{CompletionCandidate, CompletionSet, Editor, Event, Interaction, Role};
use std::{
    collections::BTreeMap,
    io::{self, Read, Write},
    sync::{
        Arc, Mutex,
        atomic::{AtomicBool, AtomicUsize, Ordering},
    },
};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

#[derive(Clone)]
struct Binding {
    host: String,
    alias: String,
    generation: u64,
    session: String,
    capacity: u64,
}

impl Binding {
    fn request(
        &self,
        connection: &mut Client,
        operation: raw::yvex_client_operation,
    ) -> Result<raw::yvex_client_request> {
        if connection.host_identity()? != self.host {
            return Err("host lifetime changed; session operation not dispatched".into());
        }
        let mut request = connection.request(operation);
        ffi::put_text(&mut request.model_alias, &self.alias)?;
        ffi::put_text(&mut request.session_name, &self.session)?;
        request.engine_generation = self.generation;
        Ok(request)
    }

    fn operation(
        &self,
        operation: raw::yvex_client_operation,
    ) -> Result<Box<raw::yvex_client_message>> {
        let mut connection = Client::connect(None)?;
        let request = self.request(&mut connection, operation)?;
        connection.send(&request)?;
        let reply = client::response(&mut connection, &request)?;
        let expected = if operation == raw::yvex_client_operation_YVEX_CLIENT_OP_CONSOLE_STATUS {
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_CONSOLE_STATUS
        } else if matches!(
            operation,
            raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_NEW
                | raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_SHOW
        ) {
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_SESSION
        } else {
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK
        };
        if reply.kind != expected {
            return Err("unexpected session operation response".into());
        }
        Ok(reply)
    }

    fn sessions(&self) -> Result<Vec<String>> {
        let mut connection = Client::connect(None)?;
        let request = self.request(
            &mut connection,
            raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_LIST,
        )?;
        connection.send(&request)?;
        let mut names = Vec::new();
        loop {
            let reply = client::response(&mut connection, &request)?;
            match reply.kind {
                raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK => return Ok(names),
                raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_SESSION => {
                    if names.len() as u64 >= self.capacity {
                        return Err("session catalog exceeds server bound".into());
                    }
                    names.push(ffi::text(&reply.session_name));
                }
                _ => return Err("unexpected session catalog response".into()),
            }
        }
    }
}

struct Session {
    binding: Binding,
    attached: bool,
}

impl Session {
    fn open(binding: Binding, create: bool) -> Result<Self> {
        if create && !binding.sessions()?.contains(&binding.session) {
            // Create only after an authoritative catalog says absent, never on transport failure.
            binding.operation(raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_NEW)?;
        }
        binding.operation(raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_ATTACH)?;
        Ok(Self {
            binding,
            attached: true,
        })
    }

    fn detach(&mut self) -> Result<()> {
        if self.attached {
            self.binding
                .operation(raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_DETACH)?;
            self.attached = false;
        }
        Ok(())
    }
}

impl Drop for Session {
    fn drop(&mut self) {
        // A failed/lost detach is not silently represented as success.
        if let Err(error) = self.detach() {
            eprintln!("yvex: session detach unconfirmed: {error}");
        }
    }
}

#[derive(Default)]
struct Attachments(Vec<raw::yvex_content_part>);

impl Attachments {
    fn attach(&mut self, path: &str) -> Result<()> {
        if self.0.len() >= raw::YVEX_CONTENT_MAX_PARTS as usize - 1 {
            return Err("next-turn attachment bound reached".into());
        }
        let path = std::fs::canonicalize(path)?;
        let path = path.to_str().ok_or("attachment path is not UTF-8")?;
        let fd = rustix::fs::open(
            path,
            rustix::fs::OFlags::RDONLY | rustix::fs::OFlags::CLOEXEC | rustix::fs::OFlags::NOFOLLOW,
            rustix::fs::Mode::empty(),
        )?;
        let mut file = std::fs::File::from(fd);
        let metadata = file.metadata()?;
        if !metadata.is_file()
            || metadata.len() == 0
            || metadata.len() > raw::YVEX_CONTENT_LOCAL_MAX_BYTES
        {
            return Err("attachment must be a non-empty bounded regular file".into());
        }
        let mut head = [0; 16];
        let count = file.read(&mut head)?;
        let (kind, mime) = classify(&head[..count]);
        let mut part = raw::yvex_content_part {
            schema_version: raw::YVEX_CONTENT_PART_SCHEMA_V1,
            storage: raw::yvex_content_storage_YVEX_CONTENT_LOCAL_FILE,
            kind,
            byte_count: metadata.len(),
            ..Default::default()
        };
        ffi::put_text(&mut part.media_type, mime)?;
        ffi::put_text(&mut part.reference, path)?;
        ffi::seal_content(&mut part)?;
        self.0.push(part);
        Ok(())
    }

    fn render(&self) -> Vec<String> {
        if self.0.is_empty() {
            return vec!["ATTACHMENTS  none staged".into()];
        }
        let mut output = vec![format!(
            "ATTACHMENTS  {} staged for next turn",
            self.0.len()
        )];
        output.extend(
            self.0
                .iter()
                .map(|part| {
                    format!(
                        "{} · {} · {} bytes · next turn",
                        ffi::text(&part.reference),
                        ffi::text(&part.media_type),
                        part.byte_count
                    )
                })
                .collect::<Vec<_>>(),
        );
        output
    }
}

fn classify(head: &[u8]) -> (raw::yvex_content_kind, &'static str) {
    use raw::*;
    if head.starts_with(b"\x89PNG\r\n\x1a\n") {
        (yvex_content_kind_YVEX_CONTENT_IMAGE, "image/png")
    } else if head.starts_with(b"\xff\xd8\xff") {
        (yvex_content_kind_YVEX_CONTENT_IMAGE, "image/jpeg")
    } else if head.starts_with(b"GIF87a") || head.starts_with(b"GIF89a") {
        (yvex_content_kind_YVEX_CONTENT_IMAGE, "image/gif")
    } else if head.starts_with(b"RIFF") && head.get(8..12) == Some(b"WEBP") {
        (yvex_content_kind_YVEX_CONTENT_IMAGE, "image/webp")
    } else if head.starts_with(b"RIFF") && head.get(8..12) == Some(b"WAVE") {
        (yvex_content_kind_YVEX_CONTENT_AUDIO, "audio/wav")
    } else if head.starts_with(b"fLaC") {
        (yvex_content_kind_YVEX_CONTENT_AUDIO, "audio/flac")
    } else if head.starts_with(b"ID3") {
        (yvex_content_kind_YVEX_CONTENT_AUDIO, "audio/mpeg")
    } else if head.get(4..8) == Some(b"ftyp") {
        (yvex_content_kind_YVEX_CONTENT_VIDEO, "video/mp4")
    } else if head.starts_with(b"\x1a\x45\xdf\xa3") {
        (yvex_content_kind_YVEX_CONTENT_VIDEO, "video/webm")
    } else if head.starts_with(b"%PDF-") {
        (yvex_content_kind_YVEX_CONTENT_FILE, "application/pdf")
    } else {
        (
            yvex_content_kind_YVEX_CONTENT_FILE,
            "application/octet-stream",
        )
    }
}

struct Options {
    maximum: u64,
    reasoning: raw::yvex_reasoning_policy,
    media: raw::yvex_client_media_execution,
    conditions: Vec<raw::yvex_client_media_condition>,
}

impl Options {
    fn from_invocation(
        invocation: &Invocation<'_>,
        kind: raw::yvex_server_engine_kind,
    ) -> Result<Self> {
        let mut media = raw::yvex_client_media_execution {
            schema_version: raw::YVEX_CLIENT_MEDIA_EXECUTION_SCHEMA_V1,
            ..Default::default()
        };
        let maximum = invocation
            .value("--max-new-tokens")
            .map(str::parse)
            .transpose()?
            .unwrap_or(0);
        for (name, bit, field) in [
            (
                "--width",
                raw::YVEX_CLIENT_MEDIA_EXECUTION_WIDTH,
                &mut media.width,
            ),
            (
                "--height",
                raw::YVEX_CLIENT_MEDIA_EXECUTION_HEIGHT,
                &mut media.height,
            ),
            (
                "--seed",
                raw::YVEX_CLIENT_MEDIA_EXECUTION_SEED,
                &mut media.seed,
            ),
        ] {
            if let Some(value) = invocation.value(name) {
                *field = value.parse()?;
                media.present |= bit;
            }
        }
        if let Some(value) = invocation.value("--duration") {
            let duration: f64 = value.parse()?;
            if !duration.is_finite() || duration <= 0.0 || duration * 1000.0 > u64::MAX as f64 {
                return Err("invalid media duration".into());
            }
            media.duration_milliseconds = (duration * 1000.0).round() as u64;
            media.present |= raw::YVEX_CLIENT_MEDIA_EXECUTION_DURATION;
        }
        media.trajectory = match invocation.value("--trajectory") {
            Some("preview") => {
                raw::yvex_client_media_trajectory_YVEX_CLIENT_MEDIA_TRAJECTORY_PREVIEW
            }
            Some("released") => {
                raw::yvex_client_media_trajectory_YVEX_CLIENT_MEDIA_TRAJECTORY_RELEASED
            }
            _ => raw::yvex_client_media_trajectory_YVEX_CLIENT_MEDIA_TRAJECTORY_DEFAULT,
        };
        let mut conditions = Vec::new();
        for (name, role) in [
            (
                "--first-image",
                raw::yvex_client_media_condition_role_YVEX_CLIENT_MEDIA_CONDITION_FIRST,
            ),
            (
                "--last-image",
                raw::yvex_client_media_condition_role_YVEX_CLIENT_MEDIA_CONDITION_LAST,
            ),
        ] {
            if let Some(value) = invocation.value(name) {
                let path = std::fs::canonicalize(value)?;
                let mut condition = raw::yvex_client_media_condition {
                    schema_version: raw::YVEX_CLIENT_MEDIA_CONDITION_SCHEMA_V1,
                    kind: raw::yvex_client_media_condition_kind_YVEX_CLIENT_MEDIA_CONDITION_IMAGE,
                    role,
                    ..Default::default()
                };
                ffi::put_text(
                    &mut condition.source_path,
                    path.to_str().ok_or("image path is not UTF-8")?,
                )?;
                conditions.push(condition);
            }
        }
        let has_media = media.present != 0 || media.trajectory != 0 || !conditions.is_empty();
        if (kind == raw::yvex_server_engine_kind_YVEX_SERVER_ENGINE_MEDIA && maximum != 0)
            || (kind != raw::yvex_server_engine_kind_YVEX_SERVER_ENGINE_MEDIA && has_media)
        {
            return Err("selected options do not apply to the attached engine".into());
        }
        Ok(Self {
            maximum,
            reasoning: raw::yvex_reasoning_policy_YVEX_REASONING_DISABLED,
            media,
            conditions,
        })
    }

    fn apply(&self, request: &mut raw::yvex_client_request, kind: raw::yvex_server_engine_kind) {
        request.maximum_new_tokens = self.maximum;
        request.reasoning_policy = self.reasoning;
        if kind == raw::yvex_server_engine_kind_YVEX_SERVER_ENGINE_MEDIA {
            request.media_execution = self.media;
            request.media_condition_count = self.conditions.len() as u64;
            request.media_conditions[..self.conditions.len()].copy_from_slice(&self.conditions);
            request.stochastic = 0;
            request.seed_present = 0;
            request.seed = 0;
            request.temperature = 1.0;
            request.top_k = 0;
            request.top_p = 1.0;
            request.min_p = 0.0;
            request.typical_p = 1.0;
        }
    }
}

fn print(lines: &[String], _width: usize, styled: bool) -> Result<()> {
    let text = presentation::flow_lines(lines, styled)?;
    let mut output = io::stdout().lock();
    output.write_all(text.as_bytes())?;
    output.flush()?;
    Ok(())
}

fn status(binding: &Binding, width: usize, styled: bool) -> Result<Box<raw::yvex_client_message>> {
    let reply = binding.operation(raw::yvex_client_operation_YVEX_CLIENT_OP_CONSOLE_STATUS)?;
    let console = &reply.console;
    if console.schema_version != raw::YVEX_CONSOLE_STATUS_SCHEMA_V1
        || ffi::text(&console.model_alias) != binding.alias
        || console.engine_generation != binding.generation
    {
        return Err("console identity differs from attached engine generation".into());
    }
    print(
        &[
            format!(
                "SESSION  {} · {} · position {} · turns {}",
                binding.session,
                ffi::session_state_name(console.session_state),
                console.position,
                console.turn_count
            ),
            format!(
                "MODEL  {} · {} · context {}/{}",
                binding.alias,
                ffi::backend_name(console.backend),
                console.context_used,
                console.context_capacity
            ),
        ],
        width,
        styled,
    )?;
    Ok(reply)
}

fn help(registry: &Registry, path: Option<&str>, width: usize, styled: bool) -> Result<()> {
    if let Some(path) = path {
        let path = format!("/{}", path.trim_start_matches('/'));
        let selected = registry
            .operations
            .iter()
            .find(|operation| {
                operation.slash_projection == path || operation.slash_aliases.contains(&path)
            })
            .ok_or("unknown slash help path")?;
        let arguments = selected
            .slash_arguments
            .iter()
            .map(|argument| {
                if argument.required {
                    format!("<{}>", argument.name)
                } else {
                    format!("[{}]", argument.name)
                }
            })
            .collect::<Vec<_>>()
            .join(" ");
        return print(
            &[
                format!("{} {}", selected.slash_projection, arguments),
                selected.summary.clone(),
            ],
            width,
            styled,
        );
    }
    let mut groups: BTreeMap<&str, Vec<String>> = BTreeMap::new();
    for operation in registry
        .operations
        .iter()
        .filter(|op| op.slash_projection != "none")
    {
        groups
            .entry(&operation.slash_group)
            .or_default()
            .push(format!(
                "{}  {}",
                operation.slash_projection, operation.summary
            ));
    }
    let mut lines = Vec::new();
    for (group, commands) in groups {
        lines.push(group.to_uppercase());
        lines.extend(commands);
        lines.push(String::new());
    }
    lines.push("Keyboard · Tab completion · Esc dismiss · Ctrl-C cancel · Ctrl-D leave".into());
    print(&lines, width, styled)
}

fn complete(registry: &Registry, binding: &Binding, input: &mut Interaction) -> Result<()> {
    let snapshot = input.analysis_snapshot();
    let prefix = &snapshot.text()[..snapshot.cursor()];
    let mut candidates = Vec::new();
    if prefix.starts_with('/') && !prefix.contains(char::is_whitespace) {
        for operation in registry
            .operations
            .iter()
            .filter(|op| op.slash_projection.starts_with(prefix))
        {
            let insertion = format!(
                "{}{}",
                operation.slash_projection,
                if operation.slash_arguments.is_empty() {
                    ""
                } else {
                    " "
                }
            );
            candidates.push(
                CompletionCandidate::new(
                    0..snapshot.cursor(),
                    &insertion,
                    &operation.slash_projection,
                )?
                .with_annotation(&operation.summary)?,
            );
        }
    } else if let Some((command, tail)) = prefix.split_once(' ') {
        if matches!(command, "/use" | "/session") {
            for session in binding
                .sessions()?
                .into_iter()
                .filter(|name| name.starts_with(tail))
            {
                candidates.push(
                    CompletionCandidate::new(
                        command.len() + 1..snapshot.cursor(),
                        &session,
                        &session,
                    )?
                    .with_annotation("resident session")?,
                );
            }
        } else if command == "/attach" {
            let path = std::path::Path::new(tail);
            let parent = path
                .parent()
                .filter(|p| !p.as_os_str().is_empty())
                .unwrap_or(std::path::Path::new("."));
            if let Ok(entries) = std::fs::read_dir(parent) {
                for entry in entries.take(256).flatten() {
                    let name = entry.path().to_string_lossy().into_owned();
                    let name = name.strip_prefix("./").unwrap_or(&name);
                    if name.starts_with(tail) {
                        let label = entry.file_name().to_string_lossy().into_owned();
                        candidates.push(
                            CompletionCandidate::new(
                                command.len() + 1..snapshot.cursor(),
                                name,
                                &label,
                            )?
                            .with_annotation("path; attachment admission still required")?,
                        );
                    }
                }
            }
        }
    }
    input.present_completions(CompletionSet::new(snapshot.revision(), candidates)?)?;
    Ok(())
}

enum Next {
    Continue,
    Leave,
}

// A cancellation before TURN_STARTED is pending intent, not an idle-session
// cancellation. Both the signal and acknowledgement paths claim one dispatch.
#[derive(Default)]
struct Interrupts {
    active: AtomicBool,
    admitted: AtomicBool,
    dispatched: AtomicBool,
    count: AtomicUsize,
    terminate: AtomicBool,
    worker: Mutex<Option<std::thread::JoinHandle<()>>>,
}

impl Interrupts {
    fn begin(&self) {
        self.admitted.store(false, Ordering::Release);
        self.dispatched.store(false, Ordering::Release);
        self.count.store(0, Ordering::Release);
        self.active.store(true, Ordering::Release);
    }

    fn request(&self, terminating: bool) {
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

    fn cancel(&self, binding: &Binding) {
        let Ok(mut worker) = self.worker.lock() else {
            eprintln!("yvex: cancellation unconfirmed: worker ownership poisoned");
            return;
        };
        if !self.claim() {
            return;
        }
        let binding = binding.clone();
        // A pending interrupt is dispatched after admission, but its control
        // connection must never block the generation response's only reader.
        // Native socket backpressure can otherwise deadlock both connections.
        match std::thread::Builder::new()
            .name("yvex-chat-cancel".into())
            .spawn(move || {
                let outcome = (|| -> Result<()> {
                    let mut connection = Client::connect(None)?;
                    connection.timeout(2000)?;
                    let request = binding.request(
                        &mut connection,
                        raw::yvex_client_operation_YVEX_CLIENT_OP_GENERATION_CANCEL,
                    )?;
                    connection.send(&request)?;
                    client::response(&mut connection, &request)?;
                    Ok(())
                })();
                if let Err(error) = outcome {
                    eprintln!("yvex: cancellation unconfirmed: {error}");
                }
            }) {
            Ok(handle) => *worker = Some(handle),
            Err(error) => eprintln!("yvex: cancellation unconfirmed: {error}"),
        }
    }

    fn finish(&self) -> Result<()> {
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

fn slash(
    registry: &Registry,
    line: &str,
    session: &mut Session,
    options: &mut Options,
    attachments: &mut Attachments,
    generated: &mut u64,
    output: (usize, bool),
) -> Result<Next> {
    let (width, styled) = output;
    let invocation = registry.slash(line)?;
    let argument = invocation.positionals.first().map(String::as_str);
    let binding = &session.binding;
    if matches!(
        invocation.operation.operation_id.as_str(),
        "generation.cancel" | "session.close" | "session.detach" | "session.reset"
    ) && argument.is_some_and(|name| name != binding.session)
    {
        return Err(
            "chat lifecycle operations affect only the attached session; use /use first".into(),
        );
    }
    match invocation.operation.operation_id.as_str() {
        "repl.quit" | "session.detach" => return Ok(Next::Leave),
        "command.discovery" => help(registry, argument, width, styled)?,
        "console.status" | "console.context" => {
            status(binding, width, styled)?;
        }
        "repl.attachment.attach" => {
            attachments.attach(argument.ok_or("attachment path required")?)?;
            print(&attachments.render(), width, styled)?;
        }
        "repl.attachment.list" => print(&attachments.render(), width, styled)?,
        "repl.attachment.clear" => {
            attachments.0.clear();
            print(&["ATTACHMENTS  cleared".into()], width, styled)?;
        }
        "session.list" => print(&binding.sessions()?, width, styled)?,
        "session.show" => {
            let selected = Binding {
                session: argument.unwrap_or(&binding.session).into(),
                ..binding.clone()
            };
            let reply =
                selected.operation(raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_SHOW)?;
            print(
                &[format!(
                    "SESSION  {} · {} · position {} · turns {}",
                    selected.session,
                    ffi::session_state_name(reply.session_state),
                    reply.final_position,
                    reply.turn_count
                )],
                width,
                styled,
            )?;
        }
        "session.new" | "session.attach" => {
            let create = invocation.operation.operation_id == "session.new";
            let name = match argument {
                Some(name) => name.to_string(),
                None => {
                    let name = format!("chat-{}", *generated);
                    *generated = generated
                        .checked_add(1)
                        .ok_or("session name counter exhausted")?;
                    name
                }
            };
            let mut next = Session::open(
                Binding {
                    session: name,
                    ..binding.clone()
                },
                create,
            )?;
            if let Err(error) = session.detach() {
                next.detach()?;
                return Err(error);
            }
            *session = next;
            status(&session.binding, width, styled)?;
        }
        "session.reset" => {
            binding.operation(raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_RESET)?;
            status(binding, width, styled)?;
        }
        "session.close" => {
            binding.operation(raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_CLOSE)?;
            session.attached = false;
            return Ok(Next::Leave);
        }
        "generation.cancel" => {
            binding.operation(raw::yvex_client_operation_YVEX_CLIENT_OP_GENERATION_CANCEL)?;
            print(&["CANCEL  request acknowledged".into()], width, styled)?;
        }
        operation if operation.starts_with("reasoning.policy.") => {
            let policy = match operation {
                "reasoning.policy.disabled" => raw::yvex_reasoning_policy_YVEX_REASONING_DISABLED,
                "reasoning.policy.low" => raw::yvex_reasoning_policy_YVEX_REASONING_LOW,
                "reasoning.policy.enabled" => raw::yvex_reasoning_policy_YVEX_REASONING_ENABLED,
                "reasoning.policy.maximum" => raw::yvex_reasoning_policy_YVEX_REASONING_MAXIMUM,
                _ => return Err("unprojected reasoning policy".into()),
            };
            let reply =
                binding.operation(raw::yvex_client_operation_YVEX_CLIENT_OP_CONSOLE_STATUS)?;
            if policy != raw::yvex_reasoning_policy_YVEX_REASONING_DISABLED
                && reply.console.explicit_reasoning_channel_supported == 0
            {
                return Err(
                    "reasoning unavailable: model has no explicit reasoning channel".into(),
                );
            }
            // This is a next-request choice only. Source/server admission remains authoritative.
            options.reasoning = policy;
            print(
                &[format!("REASONING  {}", invocation.operation.summary)],
                width,
                styled,
            )?;
        }
        _ => return Err("unprojected slash operation".into()),
    }
    Ok(Next::Continue)
}

fn finish_delivery(
    outcome: Result<crate::chat_stream::Delivery>,
    attachments: &mut Attachments,
    resynchronize: &mut bool,
    width: usize,
    styled: bool,
) -> Result<()> {
    match outcome {
        Ok(crate::chat_stream::Delivery::Resolved) => attachments.0.clear(),
        Ok(crate::chat_stream::Delivery::Refused(reason)) => {
            attachments.0.clear();
            print(&[format!("REFUSED  {reason}")], width, styled)?;
        }
        Ok(crate::chat_stream::Delivery::Indeterminate(reason)) => {
            *resynchronize = true;
            print(
                &[
                    format!("DELIVERY INDETERMINATE  {reason}"),
                    concat!(
                        "Not retried. New work requires same-generation resynchronization; ",
                        "staged attachments retained."
                    )
                    .into(),
                ],
                width,
                styled,
            )?;
        }
        Ok(crate::chat_stream::Delivery::NotDispatched(reason)) => {
            *resynchronize = true;
            print(
                &[format!(
                    "DISCONNECTED  {reason} · not dispatched; attachments retained"
                )],
                width,
                styled,
            )?;
        }
        Err(error) => print(&[format!("TURN  {error}")], width, styled)?,
    }
    Ok(())
}

pub(crate) fn run(
    invocation: &Invocation<'_>,
    registry: &Registry,
    width: usize,
    styled: bool,
) -> Result<()> {
    run_selected(invocation, registry, width, styled, None, None)
}

pub(crate) fn run_selected(
    invocation: &Invocation<'_>,
    registry: &Registry,
    width: usize,
    styled: bool,
    expected: Option<(&str, u64)>,
    mut first_line: Option<String>,
) -> Result<()> {
    use signal_hook::consts::signal::{SIGINT, SIGTERM};
    let (host, catalog) = client::engine_catalog(None, None)?;
    if expected.is_some_and(|(identity, _)| identity != host) {
        return Err("selected host lifetime changed; refresh and select again".into());
    }
    let mut engines = catalog
        .into_iter()
        .filter(|engine| {
            engine.state == raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_LOADED
                && engine.execution_ready != 0
                && engine.engine_kind
                    != raw::yvex_server_engine_kind_YVEX_SERVER_ENGINE_FINITE_DECISION
        })
        .collect::<Vec<_>>();
    let requested = invocation.value("--model");
    let exact = requested.and_then(|alias| {
        engines
            .iter()
            .position(|engine| ffi::text(&engine.alias) == alias)
    });
    if expected.is_some() && exact.is_none() {
        return Err("selected engine unavailable; refresh and select again".into());
    }
    let selected = if let Some(index) = exact {
        index
    } else if engines.len() == 1 && requested.is_none() && !invocation.has("--variant") {
        0
    } else {
        let choice =
            crate::catalog::select_runtime(invocation, requested, Some(&engines), width, styled)?;
        engines
            .iter()
            .position(|engine| ffi::text(&engine.alias) == choice.alias)
            .ok_or("selected deployment is no longer loaded")?
    };
    let engine = engines.remove(selected);
    if expected.is_some_and(|(_, generation)| generation != engine.generation) {
        return Err("selected engine generation changed; refresh and select again".into());
    }
    let mut options = Options::from_invocation(invocation, engine.engine_kind)?;
    let mut session = Session::open(
        Binding {
            host,
            alias: ffi::text(&engine.alias),
            generation: engine.generation,
            session: invocation.value("--session").unwrap_or("main").into(),
            capacity: engine.maximum_sessions,
        },
        true,
    )?;
    let initial = status(&session.binding, width, styled)?;
    options.reasoning = initial.console.reasoning_policy;
    print(
        &["/help · Tab completion · Ctrl-C cancel · /quit".into()],
        width,
        styled,
    )?;
    let current = Arc::new(Mutex::new(session.binding.clone()));
    let interrupts = Arc::new(Interrupts::default());
    let signal_binding = Arc::clone(&current);
    let signal_interrupts = Arc::clone(&interrupts);
    let mut notifications = interaction::Notifications::new(move |signal| {
        if signal != SIGINT && signal != SIGTERM {
            return;
        }
        signal_interrupts.request(signal == SIGTERM);
        if let Ok(binding) = signal_binding.lock() {
            signal_interrupts.cancel(&binding);
        }
    })?;
    let mut input = Interaction::new(Editor::new(65536, 64));
    let mut attachments = Attachments::default();
    let mut generated = 1;
    let mut last_history = String::new();
    let mut resynchronize = false;
    loop {
        if interrupts.terminate.load(Ordering::Acquire) {
            break;
        }
        let line = if let Some(line) = first_line.take() {
            Some(line)
        } else {
            read_line(&mut input, &mut notifications, registry, &session.binding)?
        };
        let Some(line) = line else { break };
        if line.is_empty() {
            continue;
        }
        if line.starts_with('/') {
            match slash(
                registry,
                &line,
                &mut session,
                &mut options,
                &mut attachments,
                &mut generated,
                (width, styled),
            ) {
                Ok(Next::Leave) => break,
                Ok(Next::Continue) => {
                    *current.lock().map_err(|_| "signal binding poisoned")? =
                        session.binding.clone();
                }
                Err(error) => print(&[format!("REFUSED  {error}")], width, styled)?,
            }
            continue;
        }
        if line != last_history {
            input.editor_mut()?.admit_history(&line)?;
            last_history.clone_from(&line);
        }
        if resynchronize {
            match status(&session.binding, width, styled) {
                Ok(reply)
                    if reply.console.session_state
                        == raw::yvex_server_session_state_YVEX_SERVER_SESSION_READY
                        || reply.console.session_state
                            == raw::yvex_server_session_state_YVEX_SERVER_SESSION_DETACHED =>
                {
                    resynchronize = false;
                    print(
                        &["RECONNECTED  same engine/session observed; prior delivery remains indeterminate".into()],
                        width, styled)?;
                }
                Ok(_) => {
                    print(
                        &["REFUSED  session not ready after lost delivery; inspect /status or /reset".into()],
                        width, styled)?;
                    continue;
                }
                Err(error) => {
                    print(
                        &[format!("REFUSED  resynchronization unavailable: {error}")],
                        width,
                        styled,
                    )?;
                    continue;
                }
            }
        }
        interrupts.begin();
        let turn = crate::chat_stream::Turn {
            host: &session.binding.host,
            alias: &session.binding.alias,
            generation: session.binding.generation,
            session: &session.binding.session,
            prompt: &line,
            attachments: &attachments.0,
            width,
            styled,
        };
        let outcome = turn.run(
            |request| options.apply(request, engine.engine_kind),
            || {
                interrupts.admitted.store(true, Ordering::Release);
                interrupts.cancel(&session.binding);
            },
        );
        interrupts.finish()?;
        notifications.take()?;
        finish_delivery(outcome, &mut attachments, &mut resynchronize, width, styled)?;
    }
    if input.is_open() {
        input.close()?;
    }
    session.detach()?;
    Ok(())
}

fn read_line(
    input: &mut Interaction,
    notifications: &mut interaction::Notifications,
    registry: &Registry,
    binding: &Binding,
) -> Result<Option<String>> {
    interaction::open(input, "yvex", &binding.session)?;
    loop {
        match notifications.advance(input)? {
            Some(Event::Submitted(line)) => return Ok(Some(line)),
            Some(Event::Interrupted) => return Ok(Some(String::new())),
            Some(Event::EndOfInput) => return Ok(None),
            Some(Event::CompletionRequested) => {
                if let Err(error) = complete(registry, binding, input) {
                    input.output_flow(&replai::Document::new(vec![replai::Block::Paragraph(
                        presentation::safe_text(
                            &format!("completion unavailable: {error}"),
                            Role::Warning,
                        )?,
                    )])?)?;
                }
            }
            Some(Event::Rejected(error)) => {
                input.output_flow(&replai::Document::new(vec![replai::Block::Paragraph(
                    presentation::safe_text(&error.to_string(), Role::Warning)?,
                )])?)?
            }
            Some(Event::SubmissionRequested(_)) => {
                return Err("unexpected opt-in submission event".into());
            }
            None => {}
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
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
    #[test]
    fn slash_inventory_has_no_silent_holes() {
        let registry = Registry::embedded().unwrap();
        assert_eq!(
            registry
                .operations
                .iter()
                .filter(|op| op.slash_projection != "none")
                .count(),
            19
        );
    }
    #[test]
    fn content_classification_preserves_kind_not_filename_guess() {
        assert_eq!(
            classify(b"\x89PNG\r\n\x1a\n"),
            (raw::yvex_content_kind_YVEX_CONTENT_IMAGE, "image/png")
        );
        assert_eq!(
            classify(b"%PDF-1.7"),
            (raw::yvex_content_kind_YVEX_CONTENT_FILE, "application/pdf")
        );
        assert_eq!(
            classify(b""),
            (
                raw::yvex_content_kind_YVEX_CONTENT_FILE,
                "application/octet-stream"
            )
        );
    }
}
