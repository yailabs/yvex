// Storage policy/configuration stays native-owned; Rust supplies bounded operator intent.
use crate::{
    ffi, presentation,
    registry::{Invocation, Refusal},
};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn reject(reason: &str) -> Box<dyn std::error::Error> {
    Box::new(Refusal {
        reason: reason.into(),
        hint: None,
    })
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<String> {
    let action = invocation
        .positionals
        .first()
        .map(String::as_str)
        .unwrap_or("");
    let admitted: &[&str] = match action {
        "configure" => &[
            "--help",
            "--project",
            "--models-root",
            "--create",
            "--reset",
        ],
        "resolve" => &["--help", "--project", "--family", "--kind"],
        "" => &[
            "--help",
            "--project",
            "--run",
            "--create",
            "--audit",
            "--output",
        ],
        _ => return Err(reject("unsupported paths action")),
    };
    if invocation
        .flags
        .keys()
        .any(|flag| !admitted.contains(&flag.as_str()))
    {
        return Err(reject("option does not apply to this paths action"));
    }
    let mut paths = ffi::Paths::open(invocation.value("--project"))?;
    let mut facts = Vec::new();
    let title = match action {
        "configure" if invocation.has("--reset") => {
            if invocation.has("--models-root") || invocation.has("--create") {
                return Err(reject(
                    "paths configure --reset conflicts with --models-root and --create",
                ));
            }
            facts.push(("removed", paths.reset()?.to_string()));
            facts.push(("config_path", ffi::text(&paths.operator.config_path)));
            facts.push((
                "models_root_source",
                ffi::text(&paths.operator.models_root_source),
            ));
            facts.push(("models_root", ffi::text(&paths.operator.models_root)));
            "PATHS  reset"
        }
        "configure" => {
            let root = invocation
                .value("--models-root")
                .ok_or_else(|| reject("configure requires --models-root DIR or --reset"))?;
            paths.configure(root, invocation.has("--create"))?;
            facts.extend(operator_fields(&paths, true));
            facts.push(("created", invocation.has("--create").to_string()));
            "PATHS  configured"
        }
        "resolve" => {
            let family = invocation
                .value("--family")
                .ok_or_else(|| reject("resolve requires --family"))?;
            let kind = invocation
                .value("--kind")
                .ok_or_else(|| reject("resolve requires --kind"))?;
            let (path, exists) = paths.resolve(family, kind)?;
            facts.extend([
                (
                    "models_root_source",
                    ffi::text(&paths.operator.models_root_source),
                ),
                ("family", family.into()),
                ("kind", kind.into()),
                ("path", path),
                ("exists", exists.to_string()),
            ]);
            "PATHS  resolved"
        }
        _ if invocation.has("--run") => {
            let run = paths.run(invocation.has("--create"))?;
            facts.extend([
                ("run_id", ffi::text(&run.run_id)),
                ("root", ffi::text(&run.root)),
                ("command", ffi::text(&run.command_path)),
                ("stdout", ffi::text(&run.stdout_path)),
                ("stderr", ffi::text(&run.stderr_path)),
                ("metrics", ffi::text(&run.metrics_path)),
                ("trace", ffi::text(&run.trace_path)),
                ("receipt", ffi::text(&run.receipt_path)),
            ]);
            "PATHS  isolated run"
        }
        _ => {
            if invocation.has("--create") {
                paths.create()?;
            }
            let audit = invocation.has("--audit") || invocation.value("--output") == Some("audit");
            if audit {
                facts.extend([
                    ("config", ffi::text(&paths.base.config_dir)),
                    ("cache", ffi::text(&paths.base.cache_dir)),
                    ("state", ffi::text(&paths.base.state_dir)),
                    ("data", ffi::text(&paths.base.data_dir)),
                    ("project", ffi::text(&paths.base.project_dir)),
                ]);
            }
            facts.extend(operator_fields(&paths, audit));
            if invocation.has("--create") {
                "PATHS  created"
            } else {
                "PATHS"
            }
        }
    };
    Ok(presentation::record(
        title,
        &facts
            .iter()
            .map(|(key, value)| (*key, value.as_str()))
            .collect::<Vec<_>>(),
        width,
        styled,
    )?)
}

fn operator_fields(paths: &ffi::Paths, audit: bool) -> Vec<(&'static str, String)> {
    let operator = &paths.operator;
    let mut facts = vec![
        (
            "models_root_source",
            ffi::text(&operator.models_root_source),
        ),
        ("models_root", ffi::text(&operator.models_root)),
        ("hf_root", ffi::text(&operator.hf_root)),
        ("gguf_root", ffi::text(&operator.gguf_root)),
        ("reports_root", ffi::text(&operator.reports_root)),
        ("registry_root", ffi::text(&operator.registry_root)),
    ];
    if audit {
        facts.extend([
            ("reference_root", ffi::text(&operator.reference_root)),
            ("operator_config_path", ffi::text(&operator.config_path)),
        ]);
    }
    facts
}
