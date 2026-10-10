//! Product projection of the canonical qualification records and generated rules.
//! No runtime admission, family interpretation or Python subprocess lives here.
use crate::{Output, ffi, presentation, registry::Invocation};
use serde_json::{Value, json};
use std::collections::BTreeSet;
use std::io::Read;
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;
mod run;

const CATALOG: &str =
    include_str!("../../../docs/evaluation/benchmarks/generated/qualification.json");
const WORKLOADS: &str =
    include_str!("../../../docs/evaluation/benchmarks/generated/qualification-workloads.json");
const RULES: &str =
    include_str!("../../../docs/evaluation/benchmarks/schema/qualification-rules.json");

fn require(ok: bool, reason: &str) -> Result<()> {
    if ok {
        Ok(())
    } else {
        Err(format!("qualification: {reason}").into())
    }
}

fn read_receipt(path: &str) -> Result<Value> {
    let mut bytes = Vec::new();
    std::fs::File::open(path)?
        .take(4 * 1024 * 1024 + 1)
        .read_to_end(&mut bytes)?;
    require(bytes.len() <= 4 * 1024 * 1024, "receipt exceeds 4 MiB")?;
    let value = serde_json::from_slice(&bytes)?;
    validate(&value)?;
    Ok(value)
}

/// Inspect qualification envelopes related by policy and source transformation.
/// These keys do not establish exact recipe identity: calibration, physical
/// variant and produced artifact must be bound independently before selection.
/// This is inspection, not admission: differing builds, hardware, workloads and
/// quality references remain visible and are never merged into a winner.
pub(crate) fn optimization_evidence(
    path: Option<&str>,
    candidates: &[ffi::raw::yvex_optimization_candidate],
    deployment: Option<&ffi::variant::DeploymentAssessment>,
) -> Result<Value> {
    let Some(path) = path else {
        return Ok(json!([]));
    };
    let mut bytes = Vec::new();
    std::fs::File::open(path)?
        .take(4 * 1024 * 1024 + 1)
        .read_to_end(&mut bytes)?;
    require(
        bytes.len() <= 4 * 1024 * 1024,
        "optimization evidence exceeds 4 MiB",
    )?;
    let input: Value = serde_json::from_slice(&bytes)?;
    let records = if let Some(records) = input.as_array() {
        records.clone()
    } else {
        vec![input]
    };
    let mut projected = project_optimization_evidence(&records, candidates)?;
    if let Some(deployment) = deployment {
        bind_optimization_evidence(&mut projected, deployment)?;
    }
    compare_optimization_evidence(&mut projected)?;
    Ok(projected)
}

/// Only published records admitted by the embedded authority enter automatic
/// comparison. Changed representation is an explicit experimental axis, never
/// permission to change checkpoint, workload, hardware, build or sampling.
/// Comparison is characterization; this does not grant recipe/quality selection.
fn compare_optimization_evidence(projected: &mut Value) -> Result<()> {
    let snapshot = projected
        .as_array()
        .ok_or("invalid evidence projection")?
        .clone();
    let rows = projected
        .as_array_mut()
        .ok_or("invalid evidence projection")?;
    for (index, row) in rows.iter_mut().enumerate() {
        let mut comparisons = Vec::new();
        if row["authentication"] == "embedded-publication-match" {
            let record = &snapshot[index]["receipt"];
            for other in snapshot
                .iter()
                .take(index)
                .filter(|p| p["authentication"] == "embedded-publication-match")
            {
                let peer = &other["receipt"];
                for metric in record["measurements"]
                    .as_array()
                    .ok_or("missing measurements")?
                {
                    if !peer["measurements"].as_array().is_some_and(|ms| {
                        ms.iter()
                            .any(|m| m["metric"] == metric["metric"] && m["case"] == metric["case"])
                    }) {
                        continue;
                    }
                    let result = compare(
                        peer,
                        record,
                        metric["metric"].as_str().ok_or("missing metric")?,
                        metric["case"].as_str().ok_or("missing case")?,
                        "physical_policy,representation,artifact_set,binding,specialization",
                    );
                    comparisons.push(match result {
                        Ok(value) => {
                            json!({"status":"comparable-characterization", "comparison":value})
                        }
                        Err(error) => {
                            json!({"status":"incompatible", "left":peer["target_identity"],
                            "right":record["target_identity"], "metric":metric["metric"],
                            "case":metric["case"], "reason":error.to_string()})
                        }
                    });
                }
            }
        }
        row["authenticated_comparisons"] = json!(comparisons);
    }
    Ok(())
}

/// The native binding reader already matched its sealed physical variant and
/// transformation to this compiler candidate. Join only that exact artifact
/// and binding; a matching policy alone cannot transfer calibration evidence.
/// Identity correspondence is not receipt authentication or earned selection.
fn bind_optimization_evidence(
    projected: &mut Value,
    deployment: &ffi::variant::DeploymentAssessment,
) -> Result<()> {
    for row in projected
        .as_array_mut()
        .ok_or("invalid evidence projection")?
    {
        let target = &row["receipt"]["target"];
        let related = row["related_candidate_identities"]
            .as_array()
            .is_some_and(|ids| ids.iter().any(|id| id == &deployment.candidate));
        if !related
            || target["artifact_set"] != deployment.artifact
            || target["binding"] != deployment.binding
        {
            continue;
        }
        row["association"] = json!("sealed-binding-physical-variant");
        row["exact_recipe_match"] = json!(deployment.candidate);
        row["reason"] = json!(
            "sealed binding links the compiled physical variant to this artifact identity; receipt claims, configuration comparability and independent quality still require qualification; capacity refusal is not erased"
        );
    }
    Ok(())
}

fn project_optimization_evidence(
    records: &[Value],
    candidates: &[ffi::raw::yvex_optimization_candidate],
) -> Result<Value> {
    require(
        !records.is_empty() && records.len() <= 32,
        "1..32 qualification receipts required",
    )?;
    let mut seen = BTreeSet::new();
    // This catalog is embedded in the exact producer build from the canonical
    // publication authority. An input hash alone is not a trust root. Equality
    // covers every target, metric, sample, claim, provenance and limitation.
    let catalog: Vec<Value> = serde_json::from_str(CATALOG)?;
    let mut projected = Vec::new();
    for record in records {
        validate(record)?;
        let digest = ffi::digest(&serde_json::to_vec(record)?)?;
        require(
            seen.insert(digest.clone()),
            "duplicate qualification receipt",
        )?;
        let matches = candidates
            .iter()
            .filter(|candidate| {
                candidate.failure_status == 0
                    && record["target"]["physical_policy"] == ffi::text(&candidate.policy_identity)
                    && record["target"]["transformation_ir"]
                        == ffi::text(&candidate.transform_identity)
            })
            .map(|candidate| ffi::text(&candidate.candidate_identity))
            .collect::<Vec<_>>();
        require(
            !matches.is_empty(),
            "receipt does not match any compiled policy/transformation",
        )?;
        projected.push(json!({
            "receipt_identity": digest,
            "authentication": if catalog.iter().any(|trusted| trusted == record) {
                "embedded-publication-match"
            } else { "untrusted-inspection-only" },
            "related_candidate_identities": matches,
            "receipt": record,
            "association": "policy-and-transformation-only",
            "exact_recipe_match": null,
            "selection_eligible": false,
            "reason": "calibration and physical variant are not authenticated by this association; no transfer of quality, lifecycle, working-set or performance evidence"
        }));
    }
    Ok(Value::Array(projected))
}

fn target_identity(target: &Value, rules: &Value) -> Result<String> {
    let object = target
        .as_object()
        .ok_or("qualification: target must be an object")?;
    let fields = rules["fields"]
        .as_object()
        .ok_or("qualification: invalid compiled rules")?;
    require(
        object.len() == fields.len() + 1 && target["schema"] == rules["target_schema"],
        "unknown/missing target fields or version",
    )?;
    for (name, kind) in fields {
        let value = object
            .get(name)
            .ok_or("qualification: missing target dimension")?;
        let valid = value.is_null()
            || if kind == "integer" {
                value.as_u64().is_some_and(|v| v > 0)
            } else {
                value.as_str().is_some_and(|v| !v.trim().is_empty())
            };
        require(valid, &format!("invalid target field: {name}"))?;
    }
    // serde_json's default map is sorted; compact UTF-8 matches canonical JSON.
    Ok(ffi::digest(&serde_json::to_vec(target)?)?)
}

fn statistics(samples: &Value) -> Result<Value> {
    let mut values = samples
        .as_array()
        .ok_or("qualification: samples must be an array")?
        .iter()
        .map(|v| {
            v.as_f64()
                .filter(|f| f.is_finite())
                .ok_or("qualification: non-finite sample")
        })
        .collect::<std::result::Result<Vec<_>, _>>()?;
    require(!values.is_empty(), "samples unavailable")?;
    values.sort_by(f64::total_cmp);
    let median = |v: &[f64]| {
        if v.len().is_multiple_of(2) {
            (v[v.len() / 2 - 1] + v[v.len() / 2]) / 2.0
        } else {
            v[v.len() / 2]
        }
    };
    let mid = median(&values);
    let mut deviations = values.iter().map(|x| (x - mid).abs()).collect::<Vec<_>>();
    deviations.sort_by(f64::total_cmp);
    Ok(
        json!({"count":values.len(), "median":mid, "minimum":values[0],
        "maximum":values[values.len()-1], "median_absolute_deviation":median(&deviations)}),
    )
}

fn validate_outcomes(value: &Value, rules: &Value) -> Result<()> {
    let Some(outcomes) = value["provenance"].get("case_outcomes") else {
        return Ok(());
    };
    for outcome in outcomes.as_array().ok_or("case outcomes must be a list")? {
        exact_fields(outcome, &rules["outcome_fields"])?;
        require(
            rules["outcome_results"]
                .as_array()
                .ok_or("outcome rules missing")?
                .contains(&outcome["result"]),
            "invalid case result",
        )?;
        for key in ["case", "reason", "evidence"] {
            require(
                outcome[key].as_str().is_some_and(|s| !s.is_empty()),
                "case outcome context missing",
            )?;
        }
    }
    Ok(())
}

fn validate(value: &Value) -> Result<()> {
    let rules: Value = serde_json::from_str(RULES)?;
    validate_outcomes(value, &rules)?;
    validate_diagnostics(value, &rules)?;
    exact_fields(value, &rules["receipt_fields"])?;
    require(
        value["title"].as_str().is_some_and(|s| !s.is_empty()),
        "missing title",
    )?;
    let id = value["id"]
        .as_str()
        .ok_or("qualification: receipt ID missing")?;
    require(
        id.split('-').all(|s| {
            !s.is_empty()
                && s.bytes()
                    .all(|b| b.is_ascii_lowercase() || b.is_ascii_digit())
        }),
        "invalid receipt ID",
    )?;
    require(
        value["limitations"]
            .as_array()
            .is_some_and(|v| !v.is_empty()),
        "non-claims required",
    )?;
    require(
        matches!(
            value["provenance"]["source_stability"].as_str(),
            Some("frozen" | "unknown" | "fixture")
        ),
        "source stability required",
    )?;
    require(
        value["schema"] == rules["receipt_schema"],
        "unsupported receipt schema",
    )?;
    require(
        value["target_identity"] == target_identity(&value["target"], &rules)?,
        "target identity mismatch",
    )?;
    require(
        matches!(value["origin"].as_str(), Some("local" | "yvex-published")),
        "invalid origin",
    )?;
    let claims = value["claims"]
        .as_object()
        .ok_or("qualification: claims unavailable")?;
    let planes = rules["planes"]
        .as_array()
        .ok_or("qualification: invalid plane rules")?;
    require(claims.len() == planes.len(), "independent planes required")?;
    let measurements = value["measurements"]
        .as_array()
        .ok_or("qualification: measurements unavailable")?;
    for plane in planes {
        let name = plane.as_str().ok_or("qualification: invalid plane")?;
        let claim = claims.get(name).ok_or("qualification: missing plane")?;
        exact_fields(claim, &rules["claim_fields"])?;
        require(
            claim["scope"].as_str().is_some_and(|s| !s.is_empty()),
            "missing claim scope",
        )?;
        require(
            claim["required_evidence"].is_array(),
            "missing evidence requirements",
        )?;
        let evidence = claim["evidence"]
            .as_object()
            .ok_or("qualification: evidence map missing")?;
        for item in evidence.values() {
            exact_fields(item, &json!(["sha256", "locator", "result"]))?;
            require(
                item["sha256"].as_str().is_some_and(|s| {
                    s.len() == 64
                        && s.bytes()
                            .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
                }),
                "invalid evidence digest",
            )?;
            require(
                item["locator"].as_str().is_some_and(|s| !s.is_empty()),
                "evidence locator missing",
            )?;
            require(
                matches!(
                    item["result"].as_str(),
                    Some("PASS" | "FAIL" | "BLOCKED" | "SKIP" | "ERROR")
                ),
                "invalid evidence result",
            )?;
        }
        require(
            rules["states"]
                .as_array()
                .is_some_and(|s| s.contains(&claim["state"])),
            "invalid claim state",
        )?;
        let blockers = claim["blockers"]
            .as_array()
            .ok_or("qualification: blockers unavailable")?;
        if claim["state"] == "BLOCKED" {
            require(!blockers.is_empty(), "BLOCKED without prerequisite")?;
        }
        if claim["state"] == "QUALIFIED" {
            require(
                blockers.is_empty()
                    && value["provenance"]["source_stability"] == "frozen"
                    && value["provenance"]["evidence_class"] != "fixture",
                "qualification lacks frozen real evidence",
            )?;
            let required = claim["required_evidence"]
                .as_array()
                .ok_or("qualification: evidence gate missing")?;
            require(!required.is_empty(), "evidence gate empty")?;
            for key in required {
                require(
                    claim["evidence"][key.as_str().ok_or("qualification: invalid evidence ID")?]["result"]
                        == "PASS",
                    "required evidence not PASS",
                )?;
            }
            let keys = if name == "family-conformance" {
                rules["quality_key"]
                    .as_array()
                    .unwrap()
                    .iter()
                    .map(|v| v.as_str().unwrap())
                    .collect::<Vec<_>>()
            } else {
                rules["fields"]
                    .as_object()
                    .unwrap()
                    .keys()
                    .map(String::as_str)
                    .collect()
            };
            require(
                keys.iter().all(|k| !value["target"][*k].is_null()),
                "qualification lacks identity",
            )?;
            if matches!(name, "deployment-performance" | "product-path") {
                require(
                    measurements
                        .iter()
                        .any(|m| rules["metrics"][m["metric"].as_str().unwrap_or("")][0] == name),
                    "qualified performance requires measurements",
                )?;
            }
        }
    }
    validate_metrics(value, &rules)
}

fn validate_diagnostics(value: &Value, rules: &Value) -> Result<()> {
    let Some(diagnostics) = value["provenance"].get("diagnostics") else {
        return Ok(());
    };
    let facts = diagnostics.as_array().ok_or("invalid diagnostic extent")?;
    require(facts.len() <= 256, "invalid diagnostic extent")?;
    let mut identities = BTreeSet::new();
    for fact in facts {
        exact_fields(fact, &rules["diagnostic_fields"])?;
        for key in ["id", "case", "definition", "evidence"] {
            require(
                fact[key].as_str().is_some_and(|s| !s.trim().is_empty()),
                "diagnostic context missing",
            )?;
        }
        require(
            identities.insert(fact["id"].as_str().unwrap()),
            "duplicate diagnostic identity",
        )?;
        require(
            rules["diagnostic_units"]
                .as_array()
                .is_some_and(|units| units.contains(&fact["unit"])),
            "unregistered diagnostic unit",
        )?;
        require(
            fact["value"].is_null()
                || fact["value"]
                    .as_f64()
                    .is_some_and(|v| v.is_finite() && v >= 0.0),
            "invalid diagnostic value",
        )?;
    }
    Ok(())
}

fn validate_metrics(value: &Value, rules: &Value) -> Result<()> {
    let measurements = value["measurements"]
        .as_array()
        .ok_or("measurements unavailable")?;
    let claims = value["claims"].as_object().ok_or("claims unavailable")?;
    for metric in measurements {
        exact_fields(metric, &rules["metric_fields"])?;
        let rule = &rules["metrics"][metric["metric"]
            .as_str()
            .ok_or("qualification: metric identity missing")?];
        require(
            rule.is_array() && metric["unit"] == rule[1] && metric["definition"] == rule[2],
            "metric definition mismatch",
        )?;
        let actual = statistics(&metric["samples"])?;
        for key in [
            "count",
            "median",
            "minimum",
            "maximum",
            "median_absolute_deviation",
        ] {
            require(
                actual[key].as_f64() == metric["statistics"][key].as_f64(),
                &format!(
                    "statistics differ from samples: {key}: expected {} observed {}",
                    actual[key], metric["statistics"][key]
                ),
            )?;
        }
        for key in [
            "case",
            "prompt_identity",
            "session_state",
            "warm_state",
            "scope",
            "evidence",
        ] {
            require(
                metric[key].as_str().is_some_and(|s| !s.is_empty()),
                "missing measurement context",
            )?;
        }
        let plane = rule[0].as_str().unwrap();
        if claims[plane]["state"] == "QUALIFIED"
            && matches!(plane, "deployment-performance" | "product-path")
        {
            require(
                actual["count"].as_u64().unwrap_or(0) >= 3
                    && value["provenance"]["profiled"] == false,
                "qualification requires repeated unprofiled samples",
            )?;
        }
    }
    Ok(())
}

fn exact_fields(value: &Value, fields: &Value) -> Result<()> {
    let object = value
        .as_object()
        .ok_or("qualification: record must be an object")?;
    let names = fields
        .as_array()
        .ok_or("qualification: invalid compiled record rule")?;
    require(
        object.len() == names.len()
            && names
                .iter()
                .all(|v| v.as_str().is_some_and(|k| object.contains_key(k))),
        "unknown/missing record fields",
    )
}

fn compare(left: &Value, right: &Value, metric: &str, case: &str, vary: &str) -> Result<Value> {
    validate(left)?;
    validate(right)?;
    let rules: Value = serde_json::from_str(RULES)?;
    let find = |record: &Value| -> Result<Value> {
        let rows = record["measurements"]
            .as_array()
            .unwrap()
            .iter()
            .filter(|m| m["metric"] == metric && m["case"] == case)
            .collect::<Vec<_>>();
        require(rows.len() == 1, "metric/case missing or ambiguous")?;
        Ok(rows[0].clone())
    };
    let lm = find(left)?;
    let rm = find(right)?;
    for key in [
        "metric",
        "definition",
        "unit",
        "case",
        "prompt_identity",
        "reference_identity",
        "session_state",
        "warm_state",
        "output_bound",
    ] {
        require(
            lm[key] == rm[key],
            &format!("incompatible metric/workload: {key}"),
        )?;
    }
    let fields = rules["fields"].as_object().unwrap();
    let varying = vary
        .split(',')
        .filter(|s| !s.is_empty())
        .collect::<BTreeSet<_>>();
    require(
        varying.iter().all(|k| fields.contains_key(*k)),
        "unknown experimental axis",
    )?;
    let quality = matches!(
        rules["metrics"][metric][0].as_str(),
        Some("representation-quality" | "checkpoint-reference")
    );
    require(
        quality
            || (left["provenance"]["profiled"] == false
                && right["provenance"]["profiled"] == false),
        "performance comparison requires explicitly unprofiled samples",
    )?;
    require(
        !quality
            || lm["reference_identity"]
                .as_str()
                .is_some_and(|v| !v.trim().is_empty()),
        "quality comparison lacks independent reference identity",
    )?;
    let quality_keys = rules["quality_key"].as_array().unwrap();
    let mut differences = Vec::new();
    for key in fields.keys() {
        let reference_key = quality_keys.iter().any(|v| v == key);
        require(
            !(quality && reference_key && varying.contains(key.as_str())),
            "checkpoint/reference may not vary",
        )?;
        if !quality || reference_key {
            require(
                !left["target"][key].is_null() && !right["target"][key].is_null(),
                &format!("comparison lacks {key}"),
            )?;
        }
        if left["target"][key] != right["target"][key] {
            require(
                varying.contains(key.as_str()),
                &format!("incompatible target: {key}"),
            )?;
            differences.push(key);
        }
    }
    Ok(json!({"schema":"yvex.qualification.comparison.v1",
        "kind":if differences.is_empty(){"direct"}else{"explicit-experiment"},
        "varying":differences, "left":left["target_identity"], "right":right["target_identity"],
        "metric":metric,"case":case,"left_statistics":lm["statistics"],
        "right_statistics":rm["statistics"],"automatic_ranking":false}))
}

pub(crate) fn dispatch(inv: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let catalog: Vec<Value> = serde_json::from_str(CATALOG)?;
    let value = match inv.operation.operation_id.as_str() {
        "model.qualification.run" => run::execute(inv, width, styled)?,
        "model.qualification.list" => {
            json!({"schema":"yvex.qualification.catalog.v1", "targets":catalog})
        }
        "model.qualification.suite" => serde_json::from_str(WORKLOADS)?,
        "model.qualification.show" => {
            let selector = &inv.positionals[0];
            let matches = catalog
                .iter()
                .filter(|r| {
                    r["id"] == selector.as_str()
                        || r["target_identity"] == selector.as_str()
                        || r["target"]["representation"] == selector.as_str()
                })
                .collect::<Vec<_>>();
            let receipt = if matches.len() == 1 {
                matches[0].clone()
            } else if matches.len() > 1 {
                return Err("qualification: ambiguous variant; select exact target ID".into());
            } else {
                read_receipt(selector)?
            };
            validate(&receipt)?;
            json!({"schema":"yvex.qualification.catalog.v1","targets":[receipt]})
        }
        "model.qualification.compare" => compare(
            &read_receipt(&inv.positionals[0])?,
            &read_receipt(&inv.positionals[1])?,
            inv.value("--metric").ok_or("metric required")?,
            inv.value("--case").ok_or("case required")?,
            inv.value("--vary").unwrap_or(""),
        )?,
        _ => return Err("qualification: operation not implemented".into()),
    };
    if inv.has("--json") {
        return Ok(Output::standard(
            format!("{}\n", serde_json::to_string(&value)?),
            0,
        ));
    }
    let mut text = String::new();
    if let Some(targets) = value["targets"].as_array() {
        for receipt in targets {
            validate(receipt)?;
            let t = &receipt["target"];
            let facts = [
                ("representation", display(&t["representation"])),
                ("backend", display(&t["backend"])),
                ("path", display(&t["product_path"])),
                ("strategy", display(&t["strategy"])),
                (
                    "quality",
                    display(&receipt["claims"]["representation-quality"]["state"]),
                ),
                (
                    "performance",
                    display(&receipt["claims"]["deployment-performance"]["state"]),
                ),
                ("origin", display(&receipt["origin"])),
            ];
            text.push_str(&presentation::record(
                receipt["id"].as_str().unwrap_or("target"),
                &facts
                    .iter()
                    .map(|(k, v)| (*k, v.as_str()))
                    .collect::<Vec<_>>(),
                width,
                styled,
            )?);
            if inv.operation.operation_id == "model.qualification.show" {
                let facts = t
                    .as_object()
                    .unwrap()
                    .iter()
                    .map(|(k, v)| (k.as_str(), display(v)))
                    .collect::<Vec<_>>();
                text.push_str(&presentation::record(
                    "EXACT TARGET",
                    &facts
                        .iter()
                        .map(|(k, v)| (*k, v.as_str()))
                        .collect::<Vec<_>>(),
                    width,
                    styled,
                )?);
                if let Some(facts) = receipt["provenance"]["diagnostics"].as_array() {
                    text.push_str("DIAGNOSTICS · not timed benchmark samples\n");
                    let mut cases = std::collections::BTreeMap::<&str, Vec<(&str, String)>>::new();
                    for fact in facts {
                        let value = if fact["value"].is_null() {
                            "NOT MEASURED".into()
                        } else {
                            display(&fact["value"])
                        };
                        cases
                            .entry(fact["case"].as_str().unwrap())
                            .or_default()
                            .push((
                                fact["id"].as_str().unwrap(),
                                format!("{value} {}", display(&fact["unit"])),
                            ));
                    }
                    for (case, values) in cases {
                        text.push_str(&presentation::record(
                            case,
                            &values
                                .iter()
                                .map(|(key, value)| (*key, value.as_str()))
                                .collect::<Vec<_>>(),
                            width,
                            styled,
                        )?);
                    }
                }
            }
        }
        if targets.is_empty() {
            text.push_str(
                "No published qualification targets. Execution support is a separate fact.\n",
            );
        }
    } else if let Some(suites) = value["suites"].as_array() {
        for suite in suites {
            text.push_str(&format!(
                "{} · {}\n",
                display(&suite["id"]),
                display(&suite["revision"])
            ));
            for case in suite["cases"].as_array().ok_or("invalid suite")? {
                text.push_str(&format!(
                    "  {} · {} · reasoning {} · strategies {}\n",
                    display(&case["id"]),
                    display(&case["class"]),
                    case["reasoning_modes"],
                    case["execution_strategies"]
                ));
            }
        }
    } else {
        text = format!(
            "COMPARISON {} · {} / {}\n  left {}\n  right {}\n  automatic ranking: no\n",
            display(&value["kind"]),
            display(&value["case"]),
            display(&value["metric"]),
            value["left_statistics"],
            value["right_statistics"]
        );
    }
    Ok(Output::standard(text, 0))
}

fn display(value: &Value) -> String {
    if value.is_null() {
        "NOT RETAINED".into()
    } else {
        value
            .as_str()
            .map(str::to_owned)
            .unwrap_or_else(|| value.to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn imported_decimal_samples_retain_exact_binary64_statistics() {
        // Independent Python-generated JSON must round-trip to the same F64
        // values. Do not paper over parser rounding with a comparison tolerance.
        let samples: Value =
            serde_json::from_str("[1.5711003479999999, 1.7673917399999999, 1.641788641]").unwrap();
        let actual = statistics(&samples).unwrap();
        assert_eq!(
            actual["median_absolute_deviation"],
            json!(0.07068829300000012)
        );
        assert_ne!(
            actual["median_absolute_deviation"],
            json!(0.0706882929999999)
        );
    }
    #[test]
    fn published_records_validate_without_promoting_characterized_evidence() {
        let receipts: Vec<Value> = serde_json::from_str(CATALOG).unwrap();
        for receipt in receipts {
            validate(&receipt).unwrap();
        }
    }

    #[test]
    fn optimizer_receipt_identity_is_not_measurement_authentication() {
        let catalog: Vec<Value> = serde_json::from_str(CATALOG).unwrap();
        let record = catalog
            .iter()
            .find(|r| {
                r["target"]["physical_policy"]
                    .as_str()
                    .is_some_and(|s| s.len() == 64)
                    && r["target"]["transformation_ir"]
                        .as_str()
                        .is_some_and(|s| s.len() == 64)
            })
            .unwrap();
        let mut candidate = ffi::raw::yvex_optimization_candidate::default();
        for (target, bytes) in [
            ("physical_policy", &mut candidate.policy_identity),
            ("transformation_ir", &mut candidate.transform_identity),
        ] {
            for (out, byte) in bytes
                .iter_mut()
                .zip(record["target"][target].as_str().unwrap().bytes())
            {
                *out = byte as _;
            }
        }
        let matched =
            project_optimization_evidence(std::slice::from_ref(record), &[candidate]).unwrap();
        assert_eq!(matched[0]["authentication"], "embedded-publication-match");
        assert_eq!(matched[0]["selection_eligible"], false);
        // A plausible, schema-valid edited record is still not published evidence.
        let mut edited = record.clone();
        edited["title"] = json!("Caller edited evidence");
        let inspected = project_optimization_evidence(&[edited], &[candidate]).unwrap();
        assert_eq!(inspected[0]["authentication"], "untrusted-inspection-only");
        assert_eq!(inspected[0]["selection_eligible"], false);
    }

    #[test]
    fn optimizer_comparison_does_not_admit_untrusted_numbers_or_changed_modes() {
        let records: Vec<Value> = serde_json::from_str(CATALOG).unwrap();
        let left = records
            .iter()
            .find(|r| {
                r["id"] == "deepseek-0731-program-p-native-baseline-coding-hash-table-20261010"
            })
            .unwrap();
        // Identity with itself is a plumbing control, not a second measurement.
        let trusted = json!({"authentication":"embedded-publication-match", "receipt":left});
        let mut pairs = json!([trusted.clone(), trusted.clone()]);
        compare_optimization_evidence(&mut pairs).unwrap();
        assert!(
            pairs[1]["authenticated_comparisons"]
                .as_array()
                .unwrap()
                .iter()
                .all(|c| c["status"] == "comparable-characterization")
        );
        assert!(
            !pairs[1]["authenticated_comparisons"]
                .as_array()
                .unwrap()
                .is_empty()
        );
        let right = records.iter().find(|r| r["id"] ==
            "deepseek-0731-program-p-native-mxfp4-publication-speculative-coding-hash-table-20261010").unwrap();
        let mut modes = json!([trusted.clone(), {"authentication":"embedded-publication-match", "receipt":right}]);
        compare_optimization_evidence(&mut modes).unwrap();
        assert!(
            modes[1]["authenticated_comparisons"]
                .as_array()
                .unwrap()
                .iter()
                .all(|c| c["status"] == "incompatible")
        );
        let incomplete = records
            .iter()
            .find(|r| {
                r["id"]
                    == "deepseek-0731-program-p-native-mxfp4-publication-coding-hash-table-20261010"
            })
            .unwrap();
        let mut missing = json!([{"authentication":"embedded-publication-match", "receipt":incomplete},
            {"authentication":"embedded-publication-match", "receipt":incomplete}]);
        compare_optimization_evidence(&mut missing).unwrap();
        assert!(
            missing[1]["authenticated_comparisons"]
                .as_array()
                .unwrap()
                .iter()
                .all(|c| c["status"] == "incompatible"
                    && c["reason"].as_str().unwrap().contains("kernel_bundle"))
        );
        let mut untrusted =
            json!([trusted, {"authentication":"untrusted-inspection-only", "receipt":left}]);
        compare_optimization_evidence(&mut untrusted).unwrap();
        assert!(
            untrusted[1]["authenticated_comparisons"]
                .as_array()
                .unwrap()
                .is_empty()
        );
    }
    #[test]
    fn optimizer_related_receipts_do_not_establish_exact_recipe_or_qualification() {
        let receipts: Vec<Value> = serde_json::from_str(CATALOG).unwrap();
        let record = receipts
            .iter()
            .find(|r| {
                ["physical_policy", "transformation_ir"]
                    .iter()
                    .all(|k| r["target"][k].as_str().is_some_and(|s| s.len() == 64))
            })
            .unwrap();
        let mut candidate = ffi::raw::yvex_optimization_candidate::default();
        for (dst, key) in [
            (&mut candidate.policy_identity, "physical_policy"),
            (&mut candidate.transform_identity, "transformation_ir"),
        ] {
            for (slot, byte) in dst
                .iter_mut()
                .zip(record["target"][key].as_str().unwrap().bytes())
            {
                *slot = byte as _;
            }
        }
        candidate.candidate_identity[..64].fill(b'a' as _);
        let result =
            project_optimization_evidence(std::slice::from_ref(record), &[candidate]).unwrap();
        assert_eq!(result[0]["receipt"], *record);
        assert_eq!(result[0]["selection_eligible"], false);
        assert_eq!(result[0]["related_candidate_identities"][0], "a".repeat(64));
        assert!(result[0]["exact_recipe_match"].is_null());
        // Changing the calibration-dependent physical variant leaves the policy
        // and source transformation unchanged. Neither association is exact.
        let mut recalibrated = candidate;
        recalibrated.physical_variant_identity[..64].fill(b'b' as _);
        let related =
            project_optimization_evidence(std::slice::from_ref(record), &[recalibrated]).unwrap();
        assert!(related[0]["exact_recipe_match"].is_null());
        assert_eq!(related[0]["selection_eligible"], false);
        assert!(project_optimization_evidence(&[], &[candidate]).is_err());
        assert!(project_optimization_evidence(std::slice::from_ref(record), &[]).is_err());
        assert!(project_optimization_evidence(&vec![record.clone(); 33], &[candidate]).is_err());
        assert!(
            project_optimization_evidence(&[record.clone(), record.clone()], &[candidate]).is_err()
        );
        for policy in [true, false] {
            let mut other = candidate;
            let dst = if policy {
                &mut other.policy_identity
            } else {
                &mut other.transform_identity
            };
            dst[0] = if dst[0] == b'a' as std::ffi::c_char {
                b'b' as _
            } else {
                b'a' as _
            };
            assert!(project_optimization_evidence(std::slice::from_ref(record), &[other]).is_err());
        }
        candidate.failure_status = -1;
        assert!(project_optimization_evidence(std::slice::from_ref(record), &[candidate]).is_err());
        let mut forged = record.clone();
        forged["target_identity"] = json!("0".repeat(64));
        assert!(project_optimization_evidence(&[forged], &[candidate]).is_err());
    }
    #[test]
    fn sealed_binding_association_does_not_promote_receipt_or_capacity() {
        let assessment = ffi::variant::DeploymentAssessment {
            candidate: "a".repeat(64),
            binding: "b".repeat(64),
            artifact: "c".repeat(64),
            status: -1,
            reason: "resource refusal".into(),
            required: 200,
            available: 100,
            plan: None,
        };
        let original = json!([{
            "related_candidate_identities": [assessment.candidate],
            "receipt": {"target": {"artifact_set": assessment.artifact, "binding": assessment.binding},
                "claims": {"representation-quality": {"state": "BLOCKED"}}},
            "association": "policy-and-transformation-only", "exact_recipe_match": null,
            "selection_eligible": false
        }]);
        let mut matched = original.clone();
        bind_optimization_evidence(&mut matched, &assessment).unwrap();
        assert_eq!(matched[0]["exact_recipe_match"], assessment.candidate);
        assert_eq!(matched[0]["association"], "sealed-binding-physical-variant");
        assert_eq!(matched[0]["receipt"], original[0]["receipt"]);
        assert_eq!(matched[0]["selection_eligible"], false);
        assert_eq!(assessment.status, -1);
        for key in ["artifact_set", "binding"] {
            let mut wrong = original.clone();
            wrong[0]["receipt"]["target"][key] = json!("d".repeat(64));
            let before = wrong.clone();
            bind_optimization_evidence(&mut wrong, &assessment).unwrap();
            assert_eq!(wrong, before);
        }
        let mut unrelated = original.clone();
        unrelated[0]["related_candidate_identities"] = json!(["d".repeat(64)]);
        let before = unrelated.clone();
        bind_optimization_evidence(&mut unrelated, &assessment).unwrap();
        assert_eq!(unrelated, before);
    }
    #[test]
    fn complete_performance_context_compares_but_missing_context_refuses() {
        let receipts: Vec<Value> = serde_json::from_str(CATALOG).unwrap();
        let rules: Value = serde_json::from_str(RULES).unwrap();
        let metric = "decode.post-first.committed";
        let reference = receipts
            .iter()
            .find(|r| {
                r["target"]
                    .as_object()
                    .unwrap()
                    .values()
                    .all(|v| !v.is_null())
                    && r["claims"]
                        .as_object()
                        .unwrap()
                        .values()
                        .all(|c| c["state"] != "QUALIFIED")
                    && r["provenance"]["profiled"] == false
                    && r["measurements"]
                        .as_array()
                        .unwrap()
                        .iter()
                        .any(|m| m["metric"] == metric)
            })
            .expect("catalog must retain a fully bound characterized performance control");
        let case = reference["measurements"]
            .as_array()
            .unwrap()
            .iter()
            .find(|m| m["metric"] == metric)
            .unwrap()["case"]
            .as_str()
            .unwrap();
        let direct = compare(reference, reference, metric, case, "").unwrap();
        assert_eq!(direct["kind"], "direct");
        assert_eq!(direct["automatic_ranking"], false);
        for key in rules["fields"].as_object().unwrap().keys() {
            let mut incomplete = reference.clone();
            incomplete["target"][key] = Value::Null;
            incomplete["target_identity"] =
                json!(target_identity(&incomplete["target"], &rules).unwrap());
            // Absence is a valid unqualified record, not comparable evidence.
            validate(&incomplete).unwrap();
            let error = compare(reference, &incomplete, metric, case, "").unwrap_err();
            assert!(
                error
                    .to_string()
                    .contains(&format!("comparison lacks {key}"))
            );
        }
        for profiled in [Value::Null, json!(true), json!("false")] {
            let mut ambiguous = reference.clone();
            ambiguous["provenance"]["profiled"] = profiled;
            assert!(
                compare(reference, &ambiguous, metric, case, "")
                    .unwrap_err()
                    .to_string()
                    .contains("explicitly unprofiled")
            );
        }
    }
    #[test]
    fn forged_identity_and_statistics_refuse() {
        let receipts: Vec<Value> = serde_json::from_str(CATALOG).unwrap();
        let reference = receipts
            .iter()
            .find(|receipt| {
                receipt["measurements"]
                    .as_array()
                    .is_some_and(|measurements| !measurements.is_empty())
            })
            .expect("catalog must contain a measured statistics control");
        let mut receipt = reference.clone();
        receipt["target"]["context"] = json!(1);
        assert!(validate(&receipt).is_err());
        let mut receipt = reference.clone();
        receipt["measurements"][0]["statistics"]["median"] = json!(0);
        assert!(validate(&receipt).is_err());
    }
}
