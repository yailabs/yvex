// Human help and machine discovery are sibling projections of the generated grammar.
use crate::{
    presentation,
    registry::{Operation, Refusal, Registry, Visibility},
};
use replai::{Alignment, Block, Column, Document, Role, Text, Theme};
use std::collections::BTreeMap;

fn text(value: &str) -> Text {
    presentation::safe_text(value, Role::Default).expect("bounded validated help metadata")
}

fn prose(value: &str, width: usize, styled: bool) -> Result<String, Refusal> {
    presentation::lines(
        &value.lines().map(str::to_owned).collect::<Vec<_>>(),
        width,
        styled,
    )
    .map_err(|error| Refusal {
        reason: error.to_string(),
        hint: None,
    })
}

fn table(
    rows: Vec<Vec<Text>>,
    headings: [&str; 2],
    width: usize,
    styled: bool,
) -> Result<String, Refusal> {
    let columns = vec![
        Column {
            heading: presentation::safe_text(headings[0], Role::Accent)
                .expect("bounded help heading"),
            alignment: Alignment::Left,
        },
        Column {
            heading: presentation::safe_text(headings[1], Role::Accent)
                .expect("bounded help heading"),
            alignment: Alignment::Left,
        },
    ];
    Document::new(vec![Block::Table { columns, rows }])
        .and_then(|document| document.render(width, Theme::from_environment(styled)))
        .map_err(|error| Refusal {
            reason: error.to_string(),
            hint: None,
        })
}

pub fn render(
    registry: &Registry,
    path: &[String],
    advanced: bool,
    width: usize,
    styled: bool,
) -> Result<String, Refusal> {
    let operations: Vec<_> = registry
        .operations
        .iter()
        .filter(|operation| {
            operation.cli
                && operation.visibility != Visibility::Removed
                && operation.command_path.starts_with(path)
        })
        .collect();
    if operations.is_empty() {
        return Err(Refusal {
            reason: format!("unknown help path: {}", path.join(" ")),
            hint: None,
        });
    }
    if path.is_empty() {
        let mut roots: BTreeMap<&str, Vec<&Operation>> = BTreeMap::new();
        for operation in operations
            .iter()
            .filter(|operation| operation.visibility == Visibility::Default)
        {
            if let Some(root) = operation.command_path.first() {
                roots.entry(root).or_default().push(operation);
            }
        }
        let rows = roots
            .iter()
            .map(|(root, entries)| {
                let purpose = entries
                    .iter()
                    .find(|operation| operation.command_path.len() == 1)
                    .map_or_else(
                        || {
                            entries
                                .iter()
                                .map(|operation| operation.command_path[1..].join(" "))
                                .collect::<Vec<_>>()
                                .join(" · ")
                        },
                        |operation| operation.summary.clone(),
                    );
                vec![text(&format!("yvex {root}")), text(&purpose)]
            })
            .collect();
        let mut output = "YVEX native model execution\n\n".to_string();
        output.push_str(&table(rows, ["COMMAND", "PURPOSE"], width, styled)?);
        output.push_str(&prose(
            "\nLIFECYCLE\n  model search → model pull → model prepare\n  serve → model load → chat\n\n\
             READ\n  yvex model list / show / active\n  yvex host status / memory / logs\n",
            width, styled,
        )?);
        if advanced {
            output.push_str("\nADVANCED AND ENGINEERING\n");
            let mut groups: BTreeMap<&str, Vec<&Operation>> = BTreeMap::new();
            for operation in &operations {
                if matches!(
                    operation.visibility,
                    Visibility::Advanced | Visibility::Engineering
                ) {
                    groups
                        .entry(&operation.help_group)
                        .or_default()
                        .push(operation);
                }
            }
            for (group, entries) in groups {
                output.push_str(&format!("\n{}\n", group.to_uppercase()));
                let rows = entries
                    .iter()
                    .map(|operation| {
                        vec![
                            text(&format!("yvex {}", operation.command_path.join(" "))),
                            text(&operation.summary),
                        ]
                    })
                    .collect();
                output.push_str(&table(rows, ["COMMAND", "PURPOSE"], width, styled)?);
            }
        } else {
            output.push_str(&prose(
                "\nUse yvex help COMMAND for details; yvex help --advanced for engineering.\n",
                width,
                styled,
            )?);
        }
        return Ok(output);
    }
    let mut output = String::new();
    if let Some(exact) = operations
        .iter()
        .find(|operation| operation.command_path.len() == path.len())
    {
        output.push_str(&leaf(exact, width, styled)?);
    } else {
        output.push_str(&format!("{}\n", path.join(" ").to_uppercase()));
    }
    let rows: Vec<_> = operations
        .iter()
        .filter(|operation| operation.command_path.len() > path.len())
        .map(|operation| {
            vec![
                text(&format!("yvex {}", operation.command_path.join(" "))),
                text(&operation.summary),
            ]
        })
        .collect();
    if !rows.is_empty() {
        output.push_str(&table(rows, ["COMMAND", "PURPOSE"], width, styled)?);
    }
    Ok(output)
}

pub fn leaf(operation: &Operation, width: usize, styled: bool) -> Result<String, Refusal> {
    let mut usage = format!("yvex {}", operation.command_path.join(" "));
    for argument in &operation.arguments {
        usage.push_str(&format!(
            " {}{}{}",
            if argument.required { "<" } else { "[" },
            argument.name,
            if argument.required { ">" } else { "]" }
        ));
        if argument.multiplicity == "many" {
            usage.push_str("...");
        }
    }
    if !operation.flags.is_empty() {
        usage.push_str(" [options]");
    }
    let mut output = prose(
        &format!(
            "{usage}\n{}\noperation: {}\n\n",
            operation.summary, operation.operation_id
        ),
        width,
        styled,
    )?;
    if !operation.arguments.is_empty() {
        output.push_str("ARGUMENTS\n");
        let rows = operation
            .arguments
            .iter()
            .map(|argument| {
                let mut detail = format!(
                    "{}; {}",
                    if argument.value_type == "delegated" {
                        "value"
                    } else {
                        &argument.value_type
                    },
                    if argument.required {
                        "required"
                    } else {
                        "optional"
                    }
                );
                if argument.multiplicity == "many" {
                    detail.push_str("; repeatable");
                }
                if !argument.enum_values.is_empty() {
                    detail.push_str(&format!(" [{}]", argument.enum_values.join(" | ")));
                }
                if !argument.range.is_empty() && argument.range != "delegated" {
                    detail.push_str(&format!("; range {}", argument.range));
                }
                vec![text(&argument.name), text(&detail)]
            })
            .collect();
        output.push_str(&table(rows, ["ARGUMENT", "CONSTRAINT"], width, styled)?);
        output.push('\n');
    }
    if !operation.aliases.is_empty() {
        output.push_str("COMPATIBILITY SPELLINGS\n");
        let rows = operation
            .aliases
            .iter()
            .map(|alias| {
                vec![
                    text(&format!("yvex {}", alias.path.join(" "))),
                    text(&alias.deprecation),
                ]
            })
            .collect();
        output.push_str(&table(rows, ["COMMAND", "STATUS"], width, styled)?);
        output.push('\n');
    }
    let rows = operation
        .flags
        .iter()
        .map(|flag| {
            let names = std::iter::once(flag.name.as_str())
                .chain(flag.aliases.iter().map(String::as_str))
                .collect::<Vec<_>>()
                .join(", ");
            let names = if flag.takes_value {
                let kind = if flag.value_type == "delegated" {
                    "value"
                } else {
                    &flag.value_type
                };
                format!("{names} <{kind}>")
            } else {
                names
            };
            let mut detail = flag.description.clone();
            if !flag.enum_values.is_empty() {
                detail.push_str(&format!(" [{}]", flag.enum_values.join(" | ")));
            }
            if flag.required {
                detail.push_str(" Required.");
            }
            if flag.multiplicity == "repeatable" {
                detail.push_str("; repeatable");
            }
            if !flag.range.is_empty() && flag.range != "delegated" {
                detail.push_str(&format!("; range {}", flag.range));
            }
            if !flag.conflicts.is_empty() {
                detail.push_str(&format!("; conflicts {}", flag.conflicts.join(" | ")));
            }
            if !flag.dependencies.is_empty() {
                detail.push_str(&format!("; requires {}", flag.dependencies.join(" | ")));
            }
            vec![text(&names), text(&detail)]
        })
        .collect();
    if !operation.flags.is_empty() {
        output.push_str("OPTIONS\n");
        output.push_str(&table(rows, ["OPTION", "MEANING"], width, styled)?);
    }
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn porcelain_help_retains_canonical_option_constraints() {
        let registry = Registry::embedded().unwrap();
        let find = |id: &str| {
            registry
                .operations
                .iter()
                .find(|op| op.operation_id == id)
                .unwrap()
        };
        let search = leaf(find("model.search"), 300, false).unwrap();
        assert!(search.contains("range 1..20"));
        assert!(search.contains("conflicts --page | --limit"));
        let pull = leaf(find("model.pull"), 300, false).unwrap();
        assert!(pull.contains("requires --prepare"));
        assert!(pull.contains("conflicts --managed | --resume"));
        assert!(pull.contains("repeatable"));
    }
    #[test]
    fn help_is_registry_derived_and_plain() {
        let registry = Registry::embedded().unwrap();
        for width in [32, 80, 160] {
            let help = render(&registry, &[], false, width, false).unwrap();
            let leaf = render(
                &registry,
                &["inspect".into(), "target".into()],
                true,
                width,
                false,
            )
            .unwrap();
            assert!(!leaf.contains("delegated"));
            assert!(help.contains("YVEX native model execution"));
            assert!(!help.contains('\u{1b}'));
            assert!(!help.contains("bench transformer"));
            let advanced = render(&registry, &[], true, width, false).unwrap();
            assert!(advanced.contains("ENGINEERING"));
            for result in [&help, &advanced] {
                assert!(result.lines().all(|line| {
                    replai::WidthPolicy::UnicodeNarrow.measure(line).unwrap() <= width
                }));
            }
            assert!(advanced.contains("tokenizer"));
        }
    }
}
