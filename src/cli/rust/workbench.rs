// Inline operational workbench. Local state is selection/presentation only;
// commands, runtime facts, editing and generation retain their existing owners.
use crate::{catalog, chat, client, ffi, interaction, presentation, registry::Registry};
use ffi::raw;
use replai::{
    Alignment, Block, Column, CompletionCandidate, CompletionSet, Document, Editor, Event,
    Interaction, Role, Text, Theme,
};
use std::{
    collections::VecDeque,
    io::{self, IsTerminal, Write},
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
    },
};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;
const SNAPSHOT_LIMIT: usize = 256;

#[derive(Debug)]
struct Terminated;
impl std::fmt::Display for Terminated {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("terminal workbench terminated")
    }
}
impl std::error::Error for Terminated {}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum View {
    Home,
    Models,
    Compile,
    Activity,
}
impl View {
    fn label(self) -> &'static str {
        match self {
            Self::Home => "Home",
            Self::Models => "Models",
            Self::Compile => "Compile",
            Self::Activity => "Activity",
        }
    }
}

#[derive(Clone)]
enum Action {
    View(View),
    Refresh,
    Inspector,
    Quit,
    Chat,
    Engine(usize),
    Model(usize),
    Event(usize),
    Details,
    Load,
    Unload,
    Guided,
    Techniques,
    Request,
}
struct Choice {
    key: String,
    label: String,
    action: Action,
}
impl Choice {
    fn new(key: &str, label: &str, action: Action) -> Self {
        Self {
            key: key.into(),
            label: label.into(),
            action,
        }
    }
}

struct Workbench {
    view: View,
    inspector: bool,
    host: Option<raw::yvex_server_summary>,
    host_identity: String,
    selected_host: String,
    engines: Vec<raw::yvex_server_engine_summary>,
    models: Vec<ffi::ModelSnapshot>,
    events: Vec<raw::yvex_server_event>,
    selected: Option<(String, u64)>,
    model: Option<String>,
    event: Option<u64>,
    notices: VecDeque<String>,
    runtime_error: Option<String>,
    catalog_error: Option<String>,
    trace_error: Option<String>,
}
impl Workbench {
    fn new() -> Self {
        Self {
            view: View::Home,
            inspector: true,
            host: None,
            host_identity: String::new(),
            selected_host: String::new(),
            engines: vec![],
            models: vec![],
            events: vec![],
            selected: None,
            model: None,
            event: None,
            notices: VecDeque::new(),
            runtime_error: None,
            catalog_error: None,
            trace_error: None,
        }
    }
    fn note(&mut self, text: impl Into<String>) {
        if self.notices.len() == 16 {
            self.notices.pop_front();
        }
        self.notices.push_back(text.into());
    }
    fn engine(&self) -> Option<&raw::yvex_server_engine_summary> {
        if self.selected_host != self.host_identity {
            return None;
        }
        let (alias, generation) = self.selected.as_ref()?;
        self.engines
            .iter()
            .find(|e| ffi::text(&e.alias) == *alias && e.generation == *generation)
    }
    fn refresh(&mut self) {
        // No old snapshot is presented as live after a failed read. These reads
        // are independently sampled, not an atomic global runtime transaction.
        self.host = None;
        self.engines.clear();
        self.runtime_error = None;
        match client::management_status().and_then(|host| {
            client::engine_catalog(None, Some(2000))
                .map(|(identity, engines)| (host, identity, engines))
        }) {
            Ok((host, identity, engines)) => {
                if identity != self.host_identity {
                    self.event = None;
                }
                self.host_identity = identity;
                self.host = Some(host);
                self.engines = engines;
            }
            Err(error) => self.runtime_error = Some(error.to_string()),
        }
        self.models.clear();
        self.catalog_error = None;
        match ffi::Library::open(None, None).and_then(|library| {
            if library.count() > SNAPSHOT_LIMIT as u64 {
                return Err(ffi::Error {
                    code: -4,
                    owner: "workbench.catalog".into(),
                    message: "catalog exceeds 256 entries; use yvex model list".into(),
                });
            }
            (0..library.count()).map(|i| library.snapshot(i)).collect()
        }) {
            Ok(models) => self.models = models,
            Err(error) => self.catalog_error = Some(error.to_string()),
        }
        if self.view == View::Activity {
            self.events.clear();
            self.trace_error = None;
            match client::inspection::recent_events() {
                Ok(events) => self.events = events,
                Err(error) => self.trace_error = Some(error.to_string()),
            }
        }
    }
    fn choices(&self) -> Vec<Choice> {
        let mut choices = vec![
            Choice::new(
                "/home",
                "Conversation and runtime",
                Action::View(View::Home),
            ),
            Choice::new(
                "/models",
                "Resident engines and local catalog",
                Action::View(View::Models),
            ),
            Choice::new(
                "/compile",
                "Physical compilation through Program P",
                Action::View(View::Compile),
            ),
            Choice::new(
                "/activity",
                "Bounded server trace and local action results",
                Action::View(View::Activity),
            ),
            Choice::new(
                "/refresh",
                "Read new snapshots; never retry an action",
                Action::Refresh,
            ),
            Choice::new("/inspect", "Toggle contextual inspector", Action::Inspector),
            Choice::new(
                "/quit",
                "Leave; preserve engines and sessions",
                Action::Quit,
            ),
        ];
        if matches!(self.view, View::Home | View::Models) {
            choices.push(Choice::new(
                "/chat",
                "Attach conversation; /quit returns here",
                Action::Chat,
            ));
            for (i, e) in self.engines.iter().enumerate() {
                choices.push(Choice::new(
                    &format!("/engine/{}", i + 1),
                    &format!(
                        "{} · generation {} · {}",
                        ffi::text(&e.alias),
                        e.generation,
                        client::engine_kind(e.engine_kind)
                    ),
                    Action::Engine(i),
                ));
            }
        }
        match self.view {
            View::Models => {
                for (i, model) in self.models.iter().enumerate() {
                    choices.push(Choice::new(
                        &format!("/model/{}", i + 1),
                        &catalog::selector(model),
                        Action::Model(i),
                    ));
                }
                choices.extend([
                    Choice::new(
                        "/details",
                        "Inspect selected catalog model",
                        Action::Details,
                    ),
                    Choice::new(
                        "/load",
                        "Confirm load through model lifecycle",
                        Action::Load,
                    ),
                    Choice::new(
                        "/unload",
                        "Confirm retirement of selected exact generation",
                        Action::Unload,
                    ),
                ]);
            }
            View::Compile => choices.extend([
                Choice::new(
                    "/guided",
                    "Resolve model, hardware and goals with native optimizer",
                    Action::Guided,
                ),
                Choice::new(
                    "/techniques",
                    "Inspect implemented techniques and missing evidence",
                    Action::Techniques,
                ),
                Choice::new(
                    "/request",
                    "Run an existing typed optimization request",
                    Action::Request,
                ),
            ]),
            View::Activity => {
                for (i, event) in self.events.iter().enumerate() {
                    choices.push(Choice::new(
                        &format!("/event/{}", i + 1),
                        &format!("#{} {}", event.sequence, ffi::event_name(event.kind)),
                        Action::Event(i),
                    ));
                }
            }
            View::Home => {}
        }
        choices
    }
    fn inspector(&self) -> Result<Block> {
        let mut rows = vec![
            ("View", self.view.label().into()),
            (
                "Snapshots",
                "On entry/refresh; not continuous telemetry".into(),
            ),
        ];
        match self.view {
            View::Home | View::Models => {
                if let Some(e) = self.engine() {
                    rows.extend([
                        ("Host lifetime", self.selected_host.clone()),
                        (
                            "Engine",
                            format!("{} · generation {}", ffi::text(&e.alias), e.generation),
                        ),
                        (
                            "Execution",
                            format!(
                                "{} · {} · ready={}",
                                client::engine_kind(e.engine_kind),
                                client::strategy(e.execution_strategy),
                                e.execution_ready != 0
                            ),
                        ),
                        (
                            "Geometry",
                            format!(
                                "context {} · prefill {} · concurrent {}",
                                e.context_capacity, e.prefill_chunk_tokens, e.concurrent_sequences
                            ),
                        ),
                        (
                            "Work",
                            format!(
                                "{} active · {} sessions · {} clients · {} leases",
                                e.active_work,
                                e.session_count,
                                e.attached_client_count,
                                e.model_lease_count
                            ),
                        ),
                        ("Model identity", ffi::text(&e.runtime_model_identity)),
                        ("Artifact", ffi::text(&e.artifact_identity)),
                        ("Binding", ffi::text(&e.runtime_binding_identity)),
                        ("Specialization", ffi::text(&e.specialization_identity)),
                    ]);
                } else {
                    rows.push((
                        "Engine",
                        if self.selected.is_some() {
                            "Selected generation unavailable; explicitly reselect"
                        } else {
                            "None selected; choose /engine/N"
                        }
                        .into(),
                    ));
                }
                rows.push((
                    "Catalog selection",
                    self.model
                        .clone()
                        .unwrap_or_else(|| "None; choose /model/N in Models".into()),
                ));
                rows.push((
                    "Conversation",
                    "Dedicated session workbench; /status in chat observes current state".into(),
                ));
            }
            View::Compile => rows.extend([
                (
                    "Authority",
                    "Native optimizer; same compile optimize registry operation".into(),
                ),
                (
                    "Evidence",
                    "Feasibility is not quality or performance qualification".into(),
                ),
                (
                    "Production",
                    "Explicit recipe/artifact output only; no installation or engine replacement"
                        .into(),
                ),
            ]),
            View::Activity => {
                if let Some(e) = self.events.iter().find(|e| Some(e.sequence) == self.event) {
                    rows.extend([
                        (
                            "Event",
                            format!(
                                "#{} {} · process {}",
                                e.sequence,
                                ffi::event_name(e.kind),
                                e.process_id
                            ),
                        ),
                        (
                            "Session / request",
                            format!(
                                "{} / {}",
                                ffi::text(&e.session_id),
                                ffi::text(&e.request_id)
                            ),
                        ),
                        ("Phase", ffi::text(&e.phase)),
                        ("Monotonic ns", e.monotonic_time_ns.to_string()),
                        (
                            "Phase values",
                            format!(
                                "a={} b={} c={} (event-defined units)",
                                e.value_a, e.value_b, e.value_c
                            ),
                        ),
                    ]);
                } else {
                    rows.push((
                        "Event",
                        "Choose /event/N; server retains a bounded trace, not a complete history"
                            .into(),
                    ));
                }
            }
        }
        fields(rows)
    }
    fn document(&self) -> Result<Document> {
        let mut blocks = vec![
            heading(&format!("YVEX · {}", self.view.label()))?,
            paragraph(
                "/home  /models  /compile  /activity  ·  /inspect  /refresh  /quit",
                Role::Dim,
            )?,
        ];
        if let Some(host) = &self.host {
            blocks.push(fields(vec![
                (
                    "Host",
                    format!(
                        "{} · {} loaded engines · {} active requests",
                        if host.host_ready != 0 {
                            "ready"
                        } else {
                            "not ready"
                        },
                        host.loaded_engine_count,
                        host.metrics.active_requests
                    ),
                ),
                (
                    "Memory",
                    format!(
                        "{:.2} GiB process RSS (not total device residency)",
                        host.metrics.current_rss_bytes as f64 / 1073741824.0
                    ),
                ),
            ])?);
        } else {
            blocks.push(paragraph(
                &format!(
                    "DISCONNECTED · {} · /refresh reconnects without replay",
                    self.runtime_error.as_deref().unwrap_or("not observed")
                ),
                Role::Warning,
            )?);
        }
        match self.view {
            View::Home => blocks.push(paragraph("Write a message to the selected engine, or /chat to continue. Tab opens actions; arrows select, Enter accepts, Enter submits. /quit in chat returns to this workbench.", Role::Default)?),
            View::Models => {
                let rows = self.engines.iter().enumerate().map(|(i, e)| vec![
                    format!("/engine/{}", i + 1), ffi::text(&e.alias),
                    format!("generation {} · ready={} · {}", e.generation, e.execution_ready != 0, client::strategy(e.execution_strategy)),
                ]).chain(self.models.iter().enumerate().map(|(i, m)| vec![
                    format!("/model/{}", i + 1), catalog::selector(m),
                    format!("{} artifacts · {} profiles · launchable={} (not residency)", m.entry.artifact_count, m.entry.profile_count, m.entry.profile_launchable != 0),
                ])).collect();
                blocks.push(table(&["Select", "Model / engine", "Observed facts"], rows)?);
                if let Some(error) = &self.catalog_error { blocks.push(paragraph(error, Role::Warning)?); }
                blocks.push(paragraph("/details inspects a catalog model. /load and /unload require explicit confirmation. Nothing is automatically loaded.", Role::Dim)?);
            }
            View::Compile => {
                blocks.push(table(&["Action", "Native operation"], vec![
                    vec!["/guided".into(), "compile optimize --guided (model → hardware → goal → candidates)".into()],
                    vec!["/request".into(), "compile optimize --request FILE (reproducible typed request)".into()],
                    vec!["/techniques".into(), "compile optimize --list-techniques".into()],
                ])?);
                blocks.push(paragraph("Program P remains IN PROGRESS. Estimated, measured and qualified candidates are distinct. The workbench does not create recommendations.", Role::Warning)?);
            }
            View::Activity => {
                blocks.push(paragraph("SERVER TRACE · newest 32 retained events; /refresh samples again", Role::Dim)?);
                blocks.push(table(&["Select", "Sequence / event", "Session / phase"], self.events.iter().enumerate().map(|(i, e)| vec![
                    format!("/event/{}", i + 1), format!("#{} {}", e.sequence, ffi::event_name(e.kind)),
                    format!("{} {}", ffi::text(&e.session_id), ffi::text(&e.phase)),
                ]).collect())?);
                if let Some(error) = &self.trace_error { blocks.push(paragraph(error, Role::Warning)?); }
                blocks.push(paragraph("LOCAL ACTION RESULTS · UI outcomes, not runtime telemetry", Role::Dim)?);
                for notice in &self.notices { blocks.push(paragraph(notice, Role::Default)?); }
            }
        }
        if self.inspector {
            blocks.push(heading("Inspector")?);
            blocks.push(self.inspector()?);
        }
        if self.view != View::Activity
            && let Some(notice) = self.notices.back()
        {
            blocks.push(paragraph(notice, Role::Dim)?);
        }
        Ok(Document::new(blocks)?)
    }
}

fn text(value: &str, role: Role) -> Result<Text> {
    Ok(presentation::safe_text(value, role)?)
}
fn heading(value: &str) -> Result<Block> {
    Ok(Block::Heading {
        level: 1,
        text: text(value, Role::Accent)?,
    })
}
fn paragraph(value: &str, role: Role) -> Result<Block> {
    Ok(Block::Paragraph(text(value, role)?))
}
fn fields(rows: Vec<(&str, String)>) -> Result<Block> {
    Ok(Block::KeyValue(
        rows.into_iter()
            .map(|(k, v)| Ok((text(k, Role::Dim)?, text(&v, Role::Default)?)))
            .collect::<Result<_>>()?,
    ))
}
fn table(headings: &[&str], rows: Vec<Vec<String>>) -> Result<Block> {
    Ok(Block::Table {
        columns: headings
            .iter()
            .map(|h| {
                Ok(Column {
                    heading: text(h, Role::Accent)?,
                    alignment: Alignment::Left,
                })
            })
            .collect::<Result<_>>()?,
        rows: rows
            .iter()
            .map(|row| row.iter().map(|v| text(v, Role::Default)).collect())
            .collect::<Result<_>>()?,
    })
}
fn write(value: &str) -> Result<()> {
    let mut output = io::stdout().lock();
    output.write_all(value.as_bytes())?;
    output.flush()?;
    Ok(())
}

// View choices are opaque menu selections, not another shell/argument grammar.
fn read_input(
    label: &str,
    choices: &[Choice],
    editor: &mut Interaction,
    document: Option<&Document>,
) -> Result<Option<String>> {
    let terminate = Arc::new(AtomicBool::new(false));
    let observed = terminate.clone();
    let mut notifications = interaction::Notifications::new(move |signal| {
        if signal == signal_hook::consts::signal::SIGTERM {
            observed.store(true, Ordering::Release);
        }
    })?;
    interaction::open(editor, "workbench", label)?;
    let mut width = presentation::destination_width(80);
    loop {
        let event = notifications.advance(editor)?;
        let current = presentation::destination_width(width);
        if current != width && editor.is_open() {
            width = current;
            if let Some(document) = document {
                editor.output_document(document)?;
            }
        }
        match event {
            Some(Event::Submitted(line)) => return Ok(Some(line)),
            Some(Event::EndOfInput) => return Ok(None),
            Some(Event::Interrupted) => {
                if terminate.load(Ordering::Acquire) {
                    return Err(Box::new(Terminated));
                }
                return Ok(Some(String::new()));
            }
            Some(Event::CompletionRequested) => {
                let snapshot = editor.analysis_snapshot();
                let prefix = &snapshot.text()[..snapshot.cursor()];
                let candidates = choices
                    .iter()
                    .filter(|c| c.key.starts_with(prefix))
                    .map(|c| {
                        CompletionCandidate::new(0..snapshot.cursor(), &c.key, &c.key)?
                            .with_annotation(&presentation::escaped_text(&c.label))
                    })
                    .collect::<std::result::Result<Vec<_>, _>>()?;
                editor.present_completions(CompletionSet::new(snapshot.revision(), candidates)?)?;
            }
            Some(Event::Rejected(error)) => {
                editor.output_flow(&Document::new(vec![paragraph(
                    &error.to_string(),
                    Role::Warning,
                )?])?)?
            }
            Some(Event::SubmissionRequested(_)) => {
                return Err("unexpected opt-in submission event".into());
            }
            None => {}
        }
    }
}

fn words(registry: &Registry, operation: &str, arguments: &[String]) -> Result<Vec<String>> {
    let op = registry
        .operations
        .iter()
        .find(|op| op.operation_id == operation && op.cli)
        .ok_or("action is absent from the canonical operator registry")?;
    Ok(op.command_path.iter().chain(arguments).cloned().collect())
}
fn execute(
    registry: &Registry,
    operation: &str,
    args: &[String],
    width: usize,
    styled: bool,
) -> Result<()> {
    let argv = words(registry, operation, args)?;
    let invocation = registry.parse(&argv).map_err(|e| e.to_string())?;
    let output = crate::dispatch(&invocation, registry, width, styled);
    write(&output.text)?;
    if output.exit != 0 {
        return Err(format!(
            "{} refused (exit {}); no automatic retry",
            operation, output.exit
        )
        .into());
    }
    Ok(())
}

fn confirm(editor: &mut Interaction, message: &str, styled: bool) -> Result<bool> {
    write(
        &Document::new(vec![paragraph(message, Role::Warning)?])?
            .render_flow(Theme::from_environment(styled))?,
    )?;
    Ok(read_input("type confirm; empty cancels", &[], editor, None)?.as_deref() == Some("confirm"))
}

fn run_chat(
    state: &Workbench,
    registry: &Registry,
    first: Option<String>,
    width: usize,
    styled: bool,
) -> Result<()> {
    let engine = state
        .engine()
        .ok_or("select an available engine in Models first")?;
    let argv = words(
        registry,
        "generation.chat",
        &[
            "--model".into(),
            ffi::text(&engine.alias),
            "--session".into(),
            "workbench".into(),
        ],
    )?;
    let invocation = registry.parse(&argv).map_err(|e| e.to_string())?;
    chat::run_selected(
        &invocation,
        registry,
        width,
        styled,
        Some((&state.selected_host, engine.generation)),
        first,
    )
}

fn act(
    state: &mut Workbench,
    action: Action,
    registry: &Registry,
    editor: &mut Interaction,
    width: usize,
    styled: bool,
) -> Result<bool> {
    match action {
        Action::Quit => return Ok(false),
        Action::View(view) => {
            state.view = view;
            state.refresh();
        }
        Action::Refresh => {
            state.refresh();
            state.note("Snapshots refreshed; no operation replayed.");
        }
        Action::Inspector => state.inspector = !state.inspector,
        Action::Engine(i) => {
            let e = state
                .engines
                .get(i)
                .ok_or("selection is no longer available")?;
            state.selected = Some((ffi::text(&e.alias), e.generation));
            state.selected_host.clone_from(&state.host_identity);
        }
        Action::Model(i) => {
            state.model = Some(catalog::selector(
                state.models.get(i).ok_or("catalog selection unavailable")?,
            ))
        }
        Action::Event(i) => {
            state.event = Some(state.events.get(i).ok_or("event unavailable")?.sequence)
        }
        Action::Chat => {
            run_chat(state, registry, None, width, styled)?;
            state.note("Conversation detached; session preserved.");
            state.refresh();
        }
        Action::Details => execute(
            registry,
            "model.show",
            &[state
                .model
                .clone()
                .ok_or("select a catalog model with /model/N first")?],
            width,
            styled,
        )?,
        Action::Load => {
            let model = state
                .model
                .clone()
                .ok_or("select a catalog model with /model/N first")?;
            if confirm(
                editor,
                &format!(
                    "Load {model} through the native model/profile selector? This allocates runtime resources; no existing engine will be automatically retired."
                ),
                styled,
            )? {
                execute(registry, "model.load", &[model], width, styled)?;
                state.refresh();
                state.note("Load returned successfully; inspect the observed generation.");
            } else {
                state.note("Load cancelled before dispatch.");
            }
        }
        Action::Unload => {
            let e = *state.engine().ok_or("select an exact engine first")?;
            if confirm(
                editor,
                &format!(
                    "Unload {} generation {}? Native lifecycle may refuse active work or sessions. No automatic cancellation.",
                    ffi::text(&e.alias),
                    e.generation
                ),
                styled,
            )? {
                client::inspection::unload_selected(&e, &state.selected_host)?;
                state.refresh();
                state.note("Selected engine retirement returned successfully.");
            } else {
                state.note("Unload cancelled before dispatch.");
            }
        }
        Action::Guided => execute(
            registry,
            "compile.optimize",
            &["--guided".into()],
            width,
            styled,
        )?,
        Action::Techniques => execute(
            registry,
            "compile.optimize",
            &["--list-techniques".into()],
            width,
            styled,
        )?,
        Action::Request => {
            if let Some(path) = read_input(
                "optimization request file; empty cancels",
                &[],
                editor,
                None,
            )? && !path.is_empty()
            {
                execute(
                    registry,
                    "compile.optimize",
                    &["--request".into(), path],
                    width,
                    styled,
                )?;
            }
        }
    }
    Ok(true)
}

pub(crate) fn run(registry: &Registry, width: usize, styled: bool) -> Result<()> {
    if !io::stdin().is_terminal() || !io::stdout().is_terminal() {
        return Err(
            "workbench requires a terminal; use the linear CLI and --json for automation".into(),
        );
    }
    match run_loop(registry, width, styled) {
        Err(error) if error.is::<Terminated>() => Ok(()),
        result => result,
    }
}

fn run_loop(registry: &Registry, width: usize, styled: bool) -> Result<()> {
    let mut state = Workbench::new();
    state.refresh();
    let mut editor = Interaction::new(Editor::new(65536, 64));
    loop {
        let document = state.document()?;
        write(&document.render(
            presentation::destination_width(width),
            Theme::from_environment(styled),
        )?)?;
        let choices = state.choices();
        let Some(line) = read_input(state.view.label(), &choices, &mut editor, Some(&document))?
        else {
            break;
        };
        // Submitted/EOF events normally restore terminal mode. Explicitly close
        // before domain I/O, nested chat/guide or rendering on every path.
        if editor.is_open() {
            editor.close()?;
        }
        if line.is_empty() {
            continue;
        }
        editor.editor_mut()?.admit_history(&line)?;
        let result = if let Some(choice) = choices.iter().find(|c| c.key == line) {
            act(
                &mut state,
                choice.action.clone(),
                registry,
                &mut editor,
                width,
                styled,
            )
        } else if state.view == View::Home && !line.starts_with('/') {
            run_chat(&state, registry, Some(line), width, styled).map(|()| {
                state.refresh();
                true
            })
        } else {
            Err("unknown view action; Tab lists valid selections (not a shell)".into())
        };
        match result {
            Ok(false) => break,
            Ok(true) => {}
            Err(error) if error.is::<Terminated>() => return Err(error),
            Err(error) => state.note(format!("REFUSED · {error}")),
        }
        if editor.is_open() {
            editor.close()?;
        }
    }
    if editor.is_open() {
        editor.close()?;
    }
    write("Workbench closed; runtime and sessions preserved.\n")
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn stale_selection_never_follows_same_alias_to_new_generation() {
        let mut state = Workbench::new();
        let mut e = raw::yvex_server_engine_summary {
            generation: 2,
            ..Default::default()
        };
        ffi::put_text(&mut e.alias, "model").unwrap();
        state.engines.push(e);
        state.selected = Some(("model".into(), 1));
        assert!(state.engine().is_none());
        state.selected = Some(("model".into(), 2));
        assert!(state.engine().is_some());
        state.host_identity = "another-host-lifetime".into();
        assert!(
            state.engine().is_none(),
            "same alias/generation cannot survive host restart"
        );
    }
    #[test]
    fn actions_reuse_registry_and_not_command_strings() {
        let registry = Registry::embedded().unwrap();
        for (op, args) in [
            ("compile.optimize", vec!["--guided".into()]),
            ("generation.chat", vec!["--model".into(), "a b".into()]),
            ("model.show", vec!["a b".into()]),
        ] {
            assert!(
                registry
                    .parse(&words(&registry, op, &args).unwrap())
                    .is_ok()
            );
        }
    }
    #[test]
    fn views_are_responsive_safe_and_do_not_claim_connectivity() {
        let mut state = Workbench::new();
        state.note("bad\x1b[31m value");
        for view in [View::Home, View::Models, View::Compile, View::Activity] {
            state.view = view;
            for width in [24, 40, 80, 160] {
                let rendered = state
                    .document()
                    .unwrap()
                    .render(width, Theme::from_environment(false))
                    .unwrap();
                assert!(rendered.contains("DISCONNECTED"));
                assert!(!rendered.contains('\x1b'));
            }
        }
    }
    #[test]
    fn local_action_history_is_bounded_and_distinct_from_server_trace() {
        let mut state = Workbench::new();
        for i in 0..100 {
            state.note(i.to_string());
        }
        assert_eq!(state.notices.len(), 16);
        assert!(state.events.is_empty());
    }
}
