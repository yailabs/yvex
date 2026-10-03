// Source/target discovery projects the immutable native catalog. A source
// target is not a launchable model, and these reports never run computation.
use crate::{
    Output, ffi, presentation,
    registry::{Invocation, Refusal},
};
use serde_json::{Value, json};
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn render(value: &Value, title: &str, width: usize, styled: bool) -> Result<String> {
    let fields = value
        .as_object()
        .expect("target projection object")
        .iter()
        .filter(|(_, value)| !value.is_null())
        .map(|(name, value)| {
            (
                name.as_str(),
                value
                    .as_str()
                    .map(str::to_owned)
                    .unwrap_or_else(|| value.to_string()),
            )
        })
        .collect::<Vec<_>>();
    let pairs = fields
        .iter()
        .map(|(name, value)| (*name, value.as_str()))
        .collect::<Vec<_>>();
    Ok(presentation::record(title, &pairs, width, styled)?)
}

fn catalog(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
    machine: bool,
) -> Result<Output> {
    let targets = ffi::catalog::target_catalog()?;
    let mut rows = Vec::new();
    let mut text = String::new();
    for target in targets {
        let summary = ffi::catalog::target_summary(&target.target_id)?;
        rows.push(
            json!({"target_id": target.target_id, "family": target.family,
            "class": target.target_class, "release_selected": summary.release_selected,
            "runtime": target.runtime_execution, "generation": target.generation}),
        );
        if !machine {
            let fields =
                if invocation.has("--audit") || invocation.value("--output") == Some("audit") {
                    serde_json::to_value(&target)?
                } else {
                    json!({"family": target.family, "class": target.target_class,
                    "runtime": target.runtime_execution, "generation": target.generation})
                };
            text.push_str(&render(
                &fields,
                &format!("TARGET  {}", target.target_id),
                width,
                styled,
            )?);
        }
    }
    Ok(Output::standard(
        if machine {
            format!(
                "{}\n",
                json!({"status": "model-target-list", "targets": rows})
            )
        } else {
            text
        },
        0,
    ))
}

fn inspect(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
    machine: bool,
) -> Result<Output> {
    let selector = invocation
        .value("--target")
        .or_else(|| invocation.positionals.get(1).map(String::as_str))
        .ok_or_else(|| Refusal {
            reason: "inspect target inspect requires TARGET".into(),
            hint: None,
        })?;
    let target = ffi::catalog::target(selector)?.ok_or_else(|| Refusal {
        reason: format!("unknown model target: {selector}"),
        hint: None,
    })?;
    let summary = ffi::catalog::target_summary(&target.target_id)?;
    let mut value = json!({"status": "model-target", "target_id": target.target_id,
        "family": target.family, "class": target.target_class,
        "release_selected": summary.release_selected, "upstream_repository": summary.upstream_repository,
        "source_status": summary.source_status, "artifact_status": summary.artifact_status,
        "runtime": summary.runtime, "generation": target.generation, "next": summary.next});
    if machine {
        return Ok(Output::standard(format!("{value}\n"), 0));
    }
    if invocation.has("--audit") || invocation.value("--output") == Some("audit") {
        value
            .as_object_mut()
            .unwrap()
            .extend(serde_json::to_value(&target)?.as_object().unwrap().clone());
        value
            .as_object_mut()
            .unwrap()
            .extend(serde_json::to_value(&summary)?.as_object().unwrap().clone());
    }
    value["boundary"] = json!(summary.boundary);
    if invocation.has("--paths") {
        let report = ffi::target::report(invocation)?;
        value
            .as_object_mut()
            .unwrap()
            .extend(report.facts.as_object().unwrap().clone());
    }
    Ok(Output::standard(
        render(
            &value,
            &format!("TARGET  {}", target.target_id),
            width,
            styled,
        )?,
        0,
    ))
}

fn engineering(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
    machine: bool,
) -> Result<Output> {
    let action = &invocation.positionals[0];
    if ["candidate", "dense-candidate", "qwen-metal", "decision"].contains(&action.as_str())
        && invocation.value("--release").is_none()
    {
        return Err(Box::new(Refusal {
            reason: "target selection report requires --release VERSION".into(),
            hint: None,
        }));
    }
    if invocation.has("--gate") && invocation.has("--role") {
        return Err(Box::new(Refusal {
            reason: "target gate and role selectors conflict".into(),
            hint: None,
        }));
    }
    if machine && ["candidate", "dense-candidate", "qwen-metal"].contains(&action.as_str()) {
        return Err(Box::new(Refusal {
            reason: "this engineering report has no admitted JSON contract".into(),
            hint: None,
        }));
    }
    let mut report = ffi::target::report(invocation)?;
    if !report.errors.is_empty() {
        let reason = report.errors.join("; ");
        return Ok(Output {
            text: render(
                &json!({"status": report.facts["status"], "operation": action,
                    "target": invocation.positionals.get(1), "reason": reason}),
                "TARGET request refused",
                width,
                styled,
            )?,
            exit: report.exit,
            diagnostic: true,
        });
    }
    if ["candidate", "dense-candidate"].contains(&action.as_str()) {
        let candidates = ffi::target::candidates(action == "dense-candidate")?;
        report.facts["candidates"] = json!(
            candidates
                .into_iter()
                .filter(|row| {
                    invocation
                        .value("--target")
                        .is_none_or(|id| row["id"] == id)
                })
                .collect::<Vec<_>>()
        );
    }
    if machine {
        if let Some(value) = report.machine {
            return Ok(Output::standard(format!("{value}\n"), report.exit));
        }
        if report.exit != 0 {
            return Ok(Output::standard(
                format!(
                    "{}\n",
                    json!({"status": "target-report-refused",
                "target_id": invocation.value("--target").or_else(|| invocation.positionals.get(1).map(String::as_str)),
                "reason": report.facts.get("reason").cloned().unwrap_or_else(|| json!("native target report refused"))})
                ),
                report.exit,
            ));
        }
        return Err(Box::new(Refusal {
            reason: "this report has no admitted JSON contract".into(),
            hint: None,
        }));
    }
    let audit = invocation.has("--audit") || invocation.value("--output") == Some("audit");
    let mut summary = report.facts.clone();
    // Candidate object collections have their own records; a scalar native
    // qtype candidate list is an ordinary fact and must not disappear.
    if summary["candidates"].is_array() {
        summary.as_object_mut().unwrap().remove("candidates");
    }
    let table = invocation.value("--output") == Some("table");
    if !audit && !table {
        summary.as_object_mut().unwrap().retain(|name, value| {
            !value.is_array()
                && !value.is_object()
                && !name.ends_with("_identity")
                && !name.ends_with("_revision")
        });
    }
    let title = format!("TARGET REPORT  {action}");
    let mut text = if table {
        let rows = summary
            .as_object()
            .unwrap()
            .iter()
            .filter(|(key, value)| *key != "candidates" && !value.is_null())
            .map(|(key, value)| {
                vec![
                    key.clone(),
                    value
                        .as_str()
                        .map(str::to_owned)
                        .unwrap_or_else(|| value.to_string()),
                ]
            })
            .collect::<Vec<_>>();
        let mut text = presentation::lines(&[title], width, styled)?;
        text.push_str(&presentation::table(
            &[
                ("FACT", replai::Alignment::Left),
                ("VALUE", replai::Alignment::Left),
            ],
            &rows,
            width,
            styled,
        )?);
        text
    } else {
        render(&summary, &title, width, styled)?
    };
    if let Some(candidates) = report.facts["candidates"].as_array() {
        for candidate in candidates {
            text.push_str(&render(
                candidate,
                &format!(
                    "CANDIDATE  {}",
                    candidate["id"].as_str().unwrap_or("unknown")
                ),
                width,
                styled,
            )?);
        }
    }
    text.push_str(
        "\nSource/target evidence only; not an engine admission or a generation qualification.\n",
    );
    Ok(Output::standard(text, report.exit))
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let mode = invocation.value("--output").unwrap_or("normal");
    if !["normal", "table", "audit", "json"].contains(&mode) {
        return Err(Box::new(Refusal {
            reason: format!("unsupported output mode: {mode}"),
            hint: None,
        }));
    }
    let machine = invocation.has("--json") || mode == "json";
    match invocation
        .positionals
        .first()
        .map(String::as_str)
        .unwrap_or("help")
    {
        "help" => Ok(Output::standard(
            crate::help::leaf(invocation.operation, width, styled)?,
            0,
        )),
        "list" => catalog(invocation, width, styled, machine),
        "classes" => {
            let classes = ffi::catalog::target_classes()?;
            let mut text = String::new();
            for class in classes {
                text.push_str(&render(
                    &serde_json::to_value(&class)?,
                    &format!("CLASS  {}", class.class_id),
                    width,
                    styled,
                )?);
            }
            // The existing classes surface has no JSON contract.
            if machine {
                return Err(Box::new(Refusal {
                    reason: "target classes has no admitted JSON contract".into(),
                    hint: None,
                }));
            }
            Ok(Output::standard(text, 0))
        }
        "inspect" => {
            if invocation.has("--models-root") && !invocation.has("--paths") {
                return Err(Box::new(Refusal {
                    reason: "target inspect --models-root requires --paths".into(),
                    hint: None,
                }));
            }
            inspect(invocation, width, styled, machine)
        }
        _ => engineering(invocation, width, styled, machine),
    }
}
