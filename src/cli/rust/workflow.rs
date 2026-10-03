// Product workflows compose typed source/catalog owners; never dispatch a C CLI.
use crate::{
    Output, catalog,
    ffi::{self, raw},
    presentation,
    registry::Invocation,
};
use serde_json::{Value, json};
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn invalid(message: &str) -> ffi::Error {
    ffi::Error {
        code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
        owner: "model.pull".into(),
        message: message.into(),
    }
}

fn unavailable(reason: &str, width: usize, styled: bool) -> Result<Output> {
    Ok(Output {
        exit: 3,
        diagnostic: true,
        text: presentation::record(
            "ACQUISITION  unavailable",
            &[("reason", reason)],
            width,
            styled,
        )?,
    })
}

fn local_storage(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<raw::yvex_source_storage_kind> {
    if invocation.has("--reference") {
        return Ok(raw::yvex_source_storage_kind_YVEX_SOURCE_STORAGE_EXTERNAL);
    }
    if invocation.has("--managed") {
        return Ok(raw::yvex_source_storage_kind_YVEX_SOURCE_STORAGE_MANAGED);
    }
    use std::io::IsTerminal;
    if invocation.has("--json")
        || !std::io::stdin().is_terminal()
        || !std::io::stdout().is_terminal()
    {
        return Err(invalid(
            "local model pull requires --managed or --reference when input is not a terminal",
        )
        .into());
    }
    let index = catalog::interactive_choice(
        &["managed copy".into(), "external reference".into()],
        "IMPORT STORAGE",
        width,
        styled,
    )?;
    Ok(if index == 0 {
        raw::yvex_source_storage_kind_YVEX_SOURCE_STORAGE_MANAGED
    } else {
        raw::yvex_source_storage_kind_YVEX_SOURCE_STORAGE_EXTERNAL
    })
}

fn present(data: Value, invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    if invocation.has("--json") {
        return Ok(Output::standard(format!("{data}\n"), 0));
    }
    let fields = data
        .as_object()
        .expect("workflow projection object")
        .iter()
        .filter(|(key, _)| !["schema", "model"].contains(&key.as_str()))
        .map(|(key, value)| {
            (
                key.as_str(),
                value
                    .as_str()
                    .map(str::to_owned)
                    .unwrap_or_else(|| value.to_string()),
            )
        })
        .collect::<Vec<_>>();
    let pairs = fields
        .iter()
        .map(|(key, value)| (*key, value.as_str()))
        .collect::<Vec<_>>();
    let title = format!("MODEL  {}", data["model"].as_str().unwrap_or("unknown"));
    let mut text = presentation::record(&title, &pairs, width, styled)?;
    if invocation.has("--prepare") && invocation.has("--dry-run") {
        text.push_str(&presentation::record(
            "PREPARE",
            &[(
                "prepare",
                "planned after acquisition; no source or build state changed",
            )],
            width,
            styled,
        )?);
    }
    Ok(Output::standard(text, 0))
}

fn local_pull(
    invocation: &Invocation<'_>,
    locator: &raw::yvex_source_locator,
    root: &str,
    width: usize,
    styled: bool,
) -> Result<Output> {
    if invocation.has("--resume") {
        return unavailable(
            "local representations are copied atomically and have no resumable acquisition",
            width,
            styled,
        );
    }
    let inspected = ffi::catalog::local_representation(locator, root)?;
    let format = ffi::text(&inspected.format);
    let digest = ffi::text(&inspected.digest);
    let precision = ffi::text(&inspected.precision);
    if invocation
        .value("--format")
        .is_some_and(|value| !value.eq_ignore_ascii_case(&format))
        || invocation
            .value("--variant")
            .is_some_and(|value| value != digest && !value.eq_ignore_ascii_case(&precision))
    {
        return Err(invalid(&format!(
            "local representation is {format}/{precision}, not the requested selection"
        ))
        .into());
    }
    let storage = local_storage(invocation, width, styled)?;
    if storage == raw::yvex_source_storage_kind_YVEX_SOURCE_STORAGE_MANAGED
        && !invocation.has("--prepare")
    {
        let existing = ffi::catalog::existing_content(root, &digest)?;
        if existing.found != 0 {
            return present(
                json!({"schema": "yvex.model.pull.v1", "changed": false, "verified": true,
                "model": ffi::text(&existing.model), "location": ffi::text(&existing.artifact.path), "digest": digest}),
                invocation,
                width,
                styled,
            );
        }
    }
    let name = invocation
        .value("--name")
        .map(str::to_owned)
        .unwrap_or_else(|| ffi::text(&inspected.name));
    if invocation.has("--dry-run") {
        return present(
            json!({"schema": "yvex.model.pull.v1", "dry_run": true, "model": name,
            "origin": ffi::text(&locator.canonical), "format": format, "digest": digest,
            "size_bytes": inspected.size_bytes}),
            invocation,
            width,
            styled,
        );
    }
    let receipt = ffi::catalog::import(&ffi::catalog::ImportRequest {
        locator,
        inspected: &inspected,
        root,
        name: invocation.value("--name"),
        family: invocation.value("--family"),
        storage,
    })?;
    let mut output = present(
        json!({"schema": "yvex.model.pull.v1", "model": name,
        "origin": ffi::text(&receipt.origin_uri), "location": ffi::text(&receipt.source_path),
        "storage": ffi::text(&receipt.storage), "format": ffi::text(&receipt.representation.format),
        "digest": ffi::text(&receipt.representation.digest), "size_bytes": receipt.representation.size_bytes}),
        invocation,
        width,
        styled,
    )?;
    if invocation.has("--prepare") {
        // Compose the admitted Rust operation from its generated command path,
        // not a hidden C argv adapter. Acquisition remains a separate effect:
        // preparation refusal never deletes already authenticated source bytes.
        let registry = crate::registry::Registry::embedded()?;
        let operation = registry
            .operations
            .iter()
            .find(|operation| operation.operation_id == "model.prepare")
            .ok_or_else(|| invalid("canonical registry has no model preparation operation"))?;
        let mut words = operation.command_path.clone();
        words.push(name);
        for flag in ["--models-root", "--registry", "--quant"] {
            if let Some(value) = invocation.value(flag) {
                words.extend([flag.into(), value.into()]);
            }
        }
        let prepared = crate::execute(&words, width, styled);
        output.text.push_str(&prepared.text);
        output.exit = prepared.exit;
        output.diagnostic = prepared.diagnostic;
    }
    Ok(output)
}

fn compose(operation: &str, arguments: Vec<String>, width: usize, styled: bool) -> Result<Output> {
    let registry = crate::registry::Registry::embedded()?;
    let owner = registry
        .operations
        .iter()
        .find(|owner| owner.operation_id == operation)
        .ok_or_else(|| invalid("generated registry lacks the composed operation"))?;
    let mut words = owner.command_path.clone();
    words.extend(arguments);
    Ok(crate::execute(&words, width, styled))
}

fn prepare(
    invocation: &Invocation<'_>,
    selector: &str,
    mut output: Output,
    width: usize,
    styled: bool,
) -> Result<Output> {
    if invocation.has("--prepare") && !invocation.has("--dry-run") && output.exit == 0 {
        let mut arguments = vec![selector.into()];
        for flag in ["--models-root", "--quant"] {
            if let Some(value) = invocation.value(flag) {
                arguments.extend([flag.into(), value.into()]);
            }
        }
        let prepared = compose("model.prepare", arguments, width, styled)?;
        output.text.push_str(&prepared.text);
        output.exit = prepared.exit;
        output.diagnostic = prepared.diagnostic;
    }
    Ok(output)
}

fn retained_remote(
    invocation: &Invocation<'_>,
    locator: &raw::yvex_source_locator,
    root: &str,
    width: usize,
    styled: bool,
) -> Result<Option<Output>> {
    use ffi::distribution as native;
    if invocation.has("--refresh") || invocation.has("--include") || invocation.has("--exclude") {
        return Ok(None);
    }
    let revision = invocation
        .value("--revision")
        .map(str::to_owned)
        .or_else(|| (locator.revision_present != 0).then(|| ffi::text(&locator.revision)));
    let repository = ffi::text(&locator.repository);
    let mut selected = native::retained(
        root,
        &repository,
        revision.as_deref(),
        invocation.value("--variant"),
        invocation.value("--format"),
    )?;
    let data = if selected.found != 0 {
        let materialized = if selected.local == 0
            && !invocation.has("--dry-run")
            && !invocation.has("--reference")
        {
            if invocation.value("--auth") == Some("required") {
                let observation = ffi::Account::open("huggingface", None, None)?.observe()?;
                if !["logged-in", "env-token-present"]
                    .contains(&ffi::text(&observation.auth_state).as_str())
                {
                    return Err(crate::acquire::state_error(
                        "authenticated acquisition requires hf auth login",
                    )
                    .into());
                }
            }
            native::materialize(
                &mut selected,
                root,
                invocation.value("--auth") == Some("never"),
            )?
        } else {
            raw::yvex_model_storage_result::default()
        };
        let selector = ffi::text(&selected.logical_identity);
        let data = json!({"schema": "yvex.model.pull.v1", "changed": materialized.changed != 0,
            "local": selected.local != 0, "verified": selected.verified != 0, "model": selector,
            "location": ffi::text(&selected.artifact.path), "revision": ffi::text(&selected.remote.revision),
            "digest": ffi::text(&selected.artifact.identity)});
        return prepare(
            invocation,
            &selector,
            present(data, invocation, width, styled)?,
            width,
            styled,
        )
        .map(Some);
    } else if !invocation.has("--reference") {
        native::retained_source(
            root,
            &repository,
            revision.as_deref(),
            invocation.value("--variant"),
            invocation.value("--format"),
        )?
    } else {
        None
    };
    let Some(source) = data else {
        return Ok(None);
    };
    let selector = ffi::text(&source.name);
    if ffi::text(&source.acquisition_state) == "source-missing" {
        if ffi::text(&source.format) == "gguf" && ffi::text(&source.representation) != "gguf" {
            let receipt = if invocation.has("--dry-run") {
                raw::yvex_model_storage_result::default()
            } else {
                native::materialize_source(
                    &source,
                    root,
                    invocation.value("--auth") == Some("never"),
                )?
            };
            let output = present(
                json!({"schema": "yvex.model.pull.v1", "changed": receipt.changed != 0,
                "location": ffi::text(&source.path), "digest": ffi::text(&source.digest)}),
                invocation,
                width,
                styled,
            )?;
            return prepare(invocation, &selector, output, width, styled).map(Some);
        }
        let mut arguments = vec![
            selector.clone(),
            "--revision".into(),
            ffi::text(&source.revision),
            "--models-root".into(),
            root.into(),
            "--no-native-inventory".into(),
        ];
        for flag in ["--auth", "--progress"] {
            if let Some(value) = invocation.value(flag) {
                arguments.extend([flag.into(), value.into()]);
            }
        }
        for flag in ["--json", "--dry-run"] {
            if invocation.has(flag) {
                arguments.push(flag.into());
            }
        }
        let output = compose("source.acquire", arguments, width, styled)?;
        return prepare(invocation, &selector, output, width, styled).map(Some);
    }
    let output = present(
        json!({"schema": "yvex.model.pull.v1", "changed": false, "model": selector,
        "revision": ffi::text(&source.revision), "digest": ffi::text(&source.digest),
        "location": ffi::text(&source.path)}),
        invocation,
        width,
        styled,
    )?;
    prepare(invocation, &selector, output, width, styled).map(Some)
}

fn remote_choice(
    invocation: &Invocation<'_>,
    model: &ffi::catalog::RemoteModel,
    width: usize,
    styled: bool,
) -> Result<usize> {
    let choices = model
        .representations
        .iter()
        .enumerate()
        .filter(|(_, representation)| {
            invocation
                .value("--format")
                .is_none_or(|value| value.eq_ignore_ascii_case(&ffi::text(&representation.format)))
                && invocation.value("--variant").is_none_or(|value| {
                    value == ffi::text(&representation.identity)
                        || value.eq_ignore_ascii_case(&ffi::text(&representation.precision))
                        || value == ffi::text(&representation.file_pattern)
                })
        })
        .collect::<Vec<_>>();
    if choices.is_empty() {
        return Err(invalid("no remote representation matches --format/--variant").into());
    }
    if choices.len() == 1 {
        return Ok(choices[0].0);
    }
    use std::io::IsTerminal;
    if !std::io::stdin().is_terminal() || !std::io::stdout().is_terminal() {
        return Err(invalid(
            "multiple representations available; non-TTY use requires --format/--variant",
        )
        .into());
    }
    let labels = choices
        .iter()
        .map(|(_, representation)| {
            format!(
                "{} · {} · {} files · {} bytes",
                ffi::text(&representation.format),
                ffi::text(&representation.precision),
                representation.file_count,
                representation.size_bytes
            )
        })
        .collect::<Vec<_>>();
    Ok(choices[catalog::interactive_choice(&labels, "REPRESENTATION", width, styled)?].0)
}

fn product_name(repository: &str) -> String {
    let mut name = String::new();
    for character in repository.rsplit('/').next().unwrap_or("").bytes() {
        if name.len() >= raw::YVEX_REMOTE_NAME_CAP as usize - 1 {
            break;
        }
        if character.is_ascii_alphanumeric() {
            name.push(char::from(character.to_ascii_lowercase()));
        } else if b"-_.".contains(&character) && !name.is_empty() && !name.ends_with('-') {
            name.push(if character == b'_' {
                '-'
            } else {
                char::from(character)
            });
        } else if !name.is_empty() && !name.ends_with('-') {
            name.push('-');
        }
    }
    let name = name.trim_end_matches(['-', '.']);
    if name.is_empty() {
        "model".into()
    } else {
        name.into()
    }
}

fn remote_download(
    invocation: &Invocation<'_>,
    inspection: &ffi::distribution::Inspection,
    index: usize,
    locator: &raw::yvex_source_locator,
    root: &str,
    width: usize,
    styled: bool,
) -> Result<Output> {
    let model = &inspection.model;
    let representation = &model.representations[index];
    let repository = ffi::text(&locator.repository);
    let revision = ffi::text(&model.facts.resolved_revision);
    let name = invocation
        .value("--name")
        .map(str::to_owned)
        .unwrap_or_else(|| product_name(&repository));
    let family = invocation
        .value("--family")
        .map(str::to_owned)
        .unwrap_or_else(|| {
            let family = ffi::text(&model.facts.family);
            if family.is_empty() {
                "unknown".into()
            } else {
                family
            }
        });
    let mut arguments = vec![
        "--repo".into(),
        repository.clone(),
        "--family".into(),
        family,
        "--name".into(),
        name.clone(),
        "--revision".into(),
        revision.clone(),
        "--models-root".into(),
        root.into(),
    ];
    let pattern = ffi::text(&representation.file_pattern);
    if pattern.is_empty() {
        return unavailable(
            "provider representation has no deterministic payload selection",
            width,
            styled,
        );
    }
    arguments.extend(["--include".into(), pattern]);
    if !ffi::text(&representation.format).eq_ignore_ascii_case("gguf") {
        arguments.extend([
            "--include".into(),
            "*.json".into(),
            "--include".into(),
            "tokenizer*".into(),
        ]);
    } else {
        arguments.push("--no-native-inventory".into());
    }
    for file in &model.files {
        if (file.kind == raw::yvex_remote_file_kind_YVEX_REMOTE_FILE_SAFETENSORS
            && file.representation != representation.identity)
            || (ffi::text(&file.path).ends_with(".safetensors.index.json")
                && ffi::text(&representation.identity).starts_with("safetensors-file-"))
        {
            arguments.extend(["--exclude".into(), ffi::text(&file.path)]);
        }
    }
    for flag in ["--include", "--exclude"] {
        if let Some(values) = invocation.flags.get(flag) {
            for value in values {
                arguments.extend([flag.into(), value.clone()]);
            }
        }
    }
    for flag in ["--auth", "--progress"] {
        if let Some(value) = invocation.value(flag) {
            arguments.extend([flag.into(), value.into()]);
        }
    }
    if representation.file_count != 0 {
        arguments.extend([
            "--selected-shards".into(),
            representation.file_count.to_string(),
        ]);
    }
    for flag in ["--json", "--dry-run", "--clear-stale-locks"] {
        if invocation.has(flag) {
            arguments.push(flag.into());
        }
    }
    if invocation.has("--verbose") && !invocation.has("--json") {
        // Verbose exposes typed acquisition diagnostics, not untrusted provider
        // terminal output or an independent human-shaped receipt.
        arguments.push("--audit".into());
    }
    let mut output = compose(
        if invocation.has("--resume") {
            "source.resume"
        } else {
            "source.acquire"
        },
        arguments,
        width,
        styled,
    )?;
    if !invocation.has("--json") {
        output.text = format!(
            "{}{}",
            presentation::record(
                "SELECTION",
                &[
                    ("representation", &ffi::text(&representation.identity)),
                    ("format", &ffi::text(&representation.format)),
                ],
                width,
                styled,
            )?,
            output.text,
        );
    }
    let selector = ffi::distribution::prepare_selector(&repository, &revision, &name)?;
    prepare(invocation, &selector, output, width, styled)
}

fn remote_pull(
    invocation: &Invocation<'_>,
    locator: &raw::yvex_source_locator,
    root: &str,
    width: usize,
    styled: bool,
) -> Result<Output> {
    use ffi::distribution as native;
    if invocation.has("--reference") && invocation.has("--prepare") {
        return unavailable(
            "remote reference cannot be prepared without an admitted streamed transport",
            width,
            styled,
        );
    }
    if let Some(output) = retained_remote(invocation, locator, root, width, styled)? {
        return Ok(output);
    }
    let repository = ffi::text(&locator.repository);
    let mut revision = invocation
        .value("--revision")
        .map(str::to_owned)
        .or_else(|| (locator.revision_present != 0).then(|| ffi::text(&locator.revision)));
    if revision.is_none() && !invocation.has("--refresh") {
        revision = native::retained_revision(root, &repository)?;
    }
    let inspection = native::Inspection::open(
        &repository,
        revision.as_deref(),
        invocation.value("--auth") == Some("never"),
    )?;
    let revision = ffi::text(&inspection.model.facts.resolved_revision);
    if ![40, 64].contains(&revision.len()) || !revision.bytes().all(|byte| byte.is_ascii_hexdigit())
    {
        return Err(crate::acquire::state_error(
            "provider did not resolve an immutable model revision",
        )
        .into());
    }
    let index = remote_choice(invocation, &inspection.model, width, styled)?;
    let representation = &inspection.model.representations[index];
    if invocation.has("--reference") {
        let name = invocation
            .value("--name")
            .map(str::to_owned)
            .unwrap_or_else(|| product_name(&repository));
        let family = invocation
            .value("--family")
            .map(str::to_owned)
            .unwrap_or_else(|| {
                let family = ffi::text(&inspection.model.facts.family);
                if family.is_empty() {
                    "unknown".into()
                } else {
                    family
                }
            });
        let data = if invocation.has("--dry-run") {
            json!({"schema": "yvex.model.pull.v1", "dry_run": true, "state": "REMOTE", "model": name,
                "origin": ffi::text(&locator.canonical), "resolved_revision": revision,
                "format": ffi::text(&representation.format)})
        } else {
            let receipt =
                native::reference(locator, root, &name, &family, &revision, representation)?;
            json!({"schema": "yvex.model.pull.v1", "state": "REMOTE", "model": name,
                "origin": ffi::text(&receipt.immutable_uri), "revision": revision,
                "format": ffi::text(&representation.format),
                "precision": ffi::text(&representation.precision), "record": ffi::text(&receipt.record_path)})
        };
        return present(data, invocation, width, styled);
    }
    if !invocation.has("--include") && !invocation.has("--exclude") {
        let selected = inspection.adopt(index, root, invocation.has("--dry-run"))?;
        if selected.found != 0 {
            let name = ffi::text(&selected.logical_identity);
            let data = json!({"schema": "yvex.model.pull.v1", "changed": false, "model": name,
                "location": ffi::text(&selected.artifact.path), "digest": ffi::text(&selected.artifact.identity),
                "revision": ffi::text(&selected.remote.revision)});
            return prepare(
                invocation,
                &name,
                present(data, invocation, width, styled)?,
                width,
                styled,
            );
        }
    }
    remote_download(invocation, &inspection, index, locator, root, width, styled)
}

pub(crate) fn pull(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    if invocation.has("--stream") {
        return unavailable(
            concat!(
                "streamed preparation is unavailable: ",
                "no admitted transport currently provides authenticated range/retry semantics"
            ),
            width,
            styled,
        );
    }
    let paths = ffi::Paths::with_models_root(invocation.value("--models-root"))?;
    let root = ffi::text(&paths.operator.models_root);
    let locator = ffi::catalog::locator(&invocation.positionals[0])?;
    match locator.kind {
        raw::yvex_source_locator_kind_YVEX_SOURCE_LOCATOR_LOCAL_PATH => {
            local_pull(invocation, &locator, &root, width, styled)
        }
        raw::yvex_source_locator_kind_YVEX_SOURCE_LOCATOR_HUGGINGFACE => {
            remote_pull(invocation, &locator, &root, width, styled)
        }
        _ => unavailable(
            &format!(
                "{} transport unavailable",
                ffi::catalog::locator_kind(locator.kind)?
            ),
            width,
            styled,
        ),
    }
}
