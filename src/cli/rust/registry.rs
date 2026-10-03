// Typed grammar is projected from the validated operator registry, never handwritten paths.
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};

#[derive(Debug, Deserialize)]
pub struct Registry {
    pub schema: String,
    pub schema_version: u32,
    pub registry_identity: String,
    pub operations: Vec<Operation>,
    pub removed_paths: Vec<RemovedPath>,
}

#[derive(Debug, Deserialize)]
pub struct RemovedPath {
    pub path: Vec<String>,
    pub hint: String,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Deserialize, Serialize)]
pub enum Lane {
    #[serde(rename = "runtime-client")]
    Client,
    #[serde(rename = "offline-engine")]
    Offline,
    #[serde(rename = "daemon-entrypoint")]
    Server,
    #[serde(rename = "REPL-local")]
    Chat,
    #[serde(rename = "API-only")]
    Api,
    #[serde(rename = "test-only")]
    Test,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Deserialize, Serialize)]
pub enum Visibility {
    #[serde(rename = "product-default")]
    Default,
    #[serde(rename = "product-advanced")]
    Advanced,
    #[serde(rename = "engineering")]
    Engineering,
    #[serde(rename = "automation")]
    Automation,
    #[serde(rename = "API-only")]
    Api,
    #[serde(rename = "test-only")]
    Test,
    #[serde(rename = "removed")]
    Removed,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct Alias {
    pub path: Vec<String>,
    pub deprecation: String,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct Operation {
    pub schema_version: u32,
    pub operation_id: String,
    pub command_path: Vec<String>,
    pub aliases: Vec<Alias>,
    pub lane: Lane,
    pub visibility: Visibility,
    #[serde(rename = "CLI_projection")]
    pub cli: bool,
    pub summary: String,
    pub help_group: String,
    pub slash_group: String,
    pub arguments: Vec<Argument>,
    pub slash_arguments: Vec<Argument>,
    pub flags: Vec<Flag>,
    pub slash_projection: String,
    pub slash_aliases: Vec<String>,
    pub adapter_id: String,
    pub protocol_operation: String,
    pub result_schema: String,
    #[serde(flatten)]
    pub metadata: BTreeMap<String, serde_json::Value>,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct Argument {
    pub name: String,
    pub value_type: String,
    pub required: bool,
    pub multiplicity: String,
    pub range: String,
    pub enum_values: Vec<String>,
    #[serde(flatten)]
    pub metadata: BTreeMap<String, serde_json::Value>,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct Flag {
    pub name: String,
    pub description: String,
    pub aliases: Vec<String>,
    pub value_type: String,
    pub takes_value: bool,
    pub multiplicity: String,
    pub required: bool,
    pub range: String,
    pub enum_values: Vec<String>,
    pub conflicts: Vec<String>,
    pub dependencies: Vec<String>,
    #[serde(flatten)]
    pub metadata: BTreeMap<String, serde_json::Value>,
}

#[derive(Debug, PartialEq, Eq)]
pub struct Refusal {
    pub reason: String,
    pub hint: Option<String>,
}

impl std::fmt::Display for Refusal {
    fn fmt(&self, out: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        out.write_str(&self.reason)
    }
}
impl std::error::Error for Refusal {}

fn refuse(reason: impl Into<String>) -> Refusal {
    Refusal {
        reason: reason.into(),
        hint: None,
    }
}

#[derive(Debug)]
pub struct Invocation<'a> {
    pub operation: &'a Operation,
    pub positionals: Vec<String>,
    pub flags: BTreeMap<String, Vec<String>>,
    pub ordered_flags: Vec<(String, String)>,
}

impl Invocation<'_> {
    pub fn has(&self, name: &str) -> bool {
        self.flags.contains_key(name)
    }
    pub fn value(&self, name: &str) -> Option<&str> {
        self.flags.get(name)?.first().map(String::as_str)
    }
}

impl Registry {
    pub fn embedded() -> Result<Self, Refusal> {
        let registry: Self = serde_json::from_str(include_str!(env!("YVEX_OPERATOR_REGISTRY")))
            .map_err(|error| refuse(format!("invalid generated registry: {error}")))?;
        if registry.schema != "yvex.operator.registry.v1" || registry.schema_version != 1 {
            return Err(refuse("incompatible generated registry"));
        }
        Ok(registry)
    }

    pub fn discovery(&self) -> serde_json::Value {
        serde_json::json!({
            "schema": "yvex.command.discovery.v1",
            "registry_identity": self.registry_identity,
            "build_commit": env!("YVEX_BUILD_COMMIT"),
            "operations": self.operations.iter().map(Operation::discovery).collect::<Vec<_>>(),
        })
    }

    pub fn completion(&self, shell: &str) -> Result<String, Refusal> {
        let mut cases: BTreeMap<String, BTreeSet<String>> = BTreeMap::new();
        for operation in self.operations.iter().filter(|operation| {
            operation.cli
                && !matches!(
                    operation.visibility,
                    Visibility::Removed | Visibility::Api | Visibility::Test
                )
        }) {
            for path in std::iter::once(&operation.command_path)
                .chain(operation.aliases.iter().map(|alias| &alias.path))
            {
                if path.is_empty() {
                    continue;
                }
                for count in 0..path.len() {
                    // Default discovery stays porcelain-sized; explicitly typed
                    // advanced prefixes still expose their complete descendants.
                    if count == 0
                        && (operation.visibility != Visibility::Default || path[0].starts_with('-'))
                    {
                        continue;
                    }
                    cases
                        .entry(path[..count].join(" "))
                        .or_default()
                        .insert(path[count].clone());
                }
                let leaf = cases.entry(path.join(" ")).or_default();
                for flag in &operation.flags {
                    leaf.insert(flag.name.clone());
                    leaf.extend(flag.aliases.iter().cloned());
                }
                for argument in &operation.arguments {
                    leaf.extend(argument.enum_values.iter().cloned());
                }
                for flag in &operation.flags {
                    if !flag.enum_values.is_empty() {
                        for name in std::iter::once(&flag.name).chain(flag.aliases.iter()) {
                            cases
                                .entry(format!("{} {name}", path.join(" ")))
                                .or_default()
                                .extend(flag.enum_values.iter().cloned());
                        }
                    }
                }
            }
        }
        // These are executable shell source, not terminal text. Only the bounded
        // registry token alphabet may enter quoted command/candidate positions.
        let safe = |value: &str| {
            value
                .bytes()
                .all(|byte| byte.is_ascii_alphanumeric() || b" ._:/@+-".contains(&byte))
        };
        if cases
            .iter()
            .any(|(path, values)| !safe(path) || values.iter().any(|value| !safe(value)))
        {
            return Err(refuse(
                "completion metadata cannot be represented safely in a shell script",
            ));
        }
        let (header, footer) = match shell {
            "bash" => (
                "_yvex_complete() {\n  local cur=${COMP_WORDS[COMP_CWORD]} path='' candidates=''\n\
                 \u{20}\u{20}if (( COMP_CWORD > 1 )); then path=${COMP_WORDS[*]:1:$((COMP_CWORD-1))}; fi\n\
                 \u{20}\u{20}case \"$path\" in\n",
                concat!(
                    "  esac\n  COMPREPLY=( $(compgen -W \"$candidates\" -- \"$cur\") )\n}\n",
                    "complete -F _yvex_complete yvex\n"
                ),
            ),
            "zsh" => (
                "#compdef yvex\n_yvex_complete() {\n  local path='' candidates=''\n\
                 \u{20}\u{20}if (( CURRENT > 2 )); then path=${(j: :)words[2,$((CURRENT-1))]}; fi\n\
                 \u{20}\u{20}case \"$path\" in\n",
                "  esac\n  compadd -- ${(z)candidates}\n}\ncompdef _yvex_complete yvex\n",
            ),
            "fish" => (
                "function __yvex_candidates\n  set -l tokens (commandline -opc)\n  set -e tokens[1]\n\
                 \u{20}\u{20}set -l path (string join ' ' $tokens)\n  set -l candidates\n  switch $path\n",
                "  end\n  printf '%s\\n' $candidates\nend\ncomplete -c yvex -f -a '(__yvex_candidates)'\n",
            ),
            _ => return Err(refuse("completion shell must be bash, zsh, or fish")),
        };
        let mut result = header.to_string();
        for (path, values) in cases {
            let values = values.into_iter().collect::<Vec<_>>().join(" ");
            if shell == "fish" {
                result.push_str(&format!(
                    "    case '{path}'\n      set candidates {values}\n"
                ));
            } else {
                result.push_str(&format!("    '{path}') candidates='{values}' ;;\n"));
            }
        }
        result.push_str(footer);
        Ok(result)
    }

    pub fn parse<'a>(&'a self, words: &[String]) -> Result<Invocation<'a>, Refusal> {
        for removed in &self.removed_paths {
            if words.starts_with(&removed.path) {
                return Err(Refusal {
                    reason: format!("removed command: {}", removed.path.join(" ")),
                    hint: Some(removed.hint.clone()),
                });
            }
        }
        let mut found: Option<(&Operation, usize)> = None;
        for operation in self.operations.iter().filter(|operation| operation.cli) {
            for path in std::iter::once(&operation.command_path)
                .chain(operation.aliases.iter().map(|alias| &alias.path))
            {
                if path.is_empty() && !words.is_empty() && !words[0].starts_with('-') {
                    continue;
                }
                if words.starts_with(path) && found.is_none_or(|(_, count)| path.len() > count) {
                    found = Some((operation, path.len()));
                }
            }
        }
        let Some((operation, consumed)) = found else {
            let input = words
                .iter()
                .take_while(|word| !word.starts_with('-'))
                .cloned()
                .collect::<Vec<_>>()
                .join(" ");
            return Err(Refusal {
                reason: format!("unknown command: {input}"),
                hint: self.nearest(&input),
            });
        };
        operation.parse(&words[consumed..]).map_err(|mut error| {
            if error.hint.is_none() {
                error.hint = Some(
                    format!("yvex help {}", operation.command_path.join(" "))
                        .trim_end()
                        .into(),
                );
            }
            error
        })
    }

    pub fn slash<'a>(&'a self, line: &str) -> Result<Invocation<'a>, Refusal> {
        let mut words = line.split([' ', '\t']).filter(|word| !word.is_empty());
        let name = words.next().unwrap_or("");
        let operation = self
            .operations
            .iter()
            .find(|operation| {
                operation.slash_projection == name
                    || operation.slash_aliases.iter().any(|alias| alias == name)
            })
            .ok_or_else(|| refuse(format!("unknown slash command: {name}")))?;
        let positionals = words.map(String::from).collect::<Vec<_>>();
        validate_arguments(&operation.slash_arguments, &positionals)?;
        Ok(Invocation {
            operation,
            positionals,
            flags: BTreeMap::new(),
            ordered_flags: Vec::new(),
        })
    }

    fn nearest(&self, input: &str) -> Option<String> {
        let mut candidates: Vec<_> = self
            .operations
            .iter()
            .filter(|operation| operation.cli && !operation.command_path.is_empty())
            .map(|operation| operation.command_path.join(" "))
            .map(|path| (distance(input, &path), path))
            .collect();
        candidates.sort();
        let (score, path) = candidates.first()?;
        if *score > 3 || candidates.get(1).is_some_and(|other| other.0 == *score) {
            return None;
        }
        Some(format!("try yvex {path}"))
    }
}

impl Operation {
    fn discovery(&self) -> serde_json::Value {
        let mut result = serde_json::to_value(self).expect("validated registry data");
        let output = result.as_object_mut().unwrap();
        for key in [
            "CLI_projection",
            "TTY_policy",
            "adapter_argv",
            "flag_sets",
            "architectural_plane",
            "protocol_operation",
            "slash_projection",
            "slash_aliases",
            "daemon_requirement",
            "model_requirement",
            "artifact_requirement",
            "backend_requirement",
            "validator_ids",
            "deprecation_state",
        ] {
            output.remove(key);
        }
        output.insert("command_path".into(), self.command_path.join(" ").into());
        // Discovery's established host-entrypoint spelling is independent of the
        // authored registry's daemon-entrypoint lane tag.
        if self.lane == Lane::Server {
            output.insert("lane".into(), "host-entrypoint".into());
        }
        output.insert(
            "aliases".into(),
            self.aliases
                .iter()
                .map(|alias| alias.path.join(" "))
                .collect::<Vec<_>>()
                .into(),
        );
        output.insert("plane".into(), self.metadata["architectural_plane"].clone());
        output.insert("tty_policy".into(), self.metadata["TTY_policy"].clone());
        output.insert("validators".into(), self.metadata["validator_ids"].clone());
        output.insert(
            "deprecation".into(),
            self.metadata["deprecation_state"].clone(),
        );
        output.insert(
            "output_schemas".into(),
            vec![self.result_schema.clone()].into(),
        );
        output.insert(
            "projections".into(),
            serde_json::json!({
                "cli": self.cli, "slash": self.slash_projection,
                "slash_aliases": self.slash_aliases, "protocol": self.protocol_operation,
            }),
        );
        for collection in ["arguments", "slash_arguments", "flags"] {
            for item in output[collection].as_array_mut().unwrap() {
                let item = item.as_object_mut().unwrap();
                let kind = item.remove("value_type").unwrap();
                item.insert("type".into(), kind);
            }
        }
        result
    }

    pub fn parse<'a>(&'a self, words: &[String]) -> Result<Invocation<'a>, Refusal> {
        let mut flags: BTreeMap<String, Vec<String>> = BTreeMap::new();
        let mut ordered_flags = Vec::new();
        let mut positionals = Vec::new();
        let mut words = words.iter();
        while let Some(word) = words.next() {
            if !word.starts_with('-') {
                positionals.push(word.clone());
                continue;
            }
            let flag = self
                .flags
                .iter()
                .find(|flag| flag.name == *word || flag.aliases.contains(word))
                .ok_or_else(|| refuse(format!("unknown flag: {word}")))?;
            if flags.contains_key(&flag.name) && flag.multiplicity != "repeatable" {
                return Err(refuse(format!("duplicate flag: {}", flag.name)));
            }
            let value = if flag.takes_value {
                let value = words
                    .next()
                    .ok_or_else(|| refuse(format!("{} requires a value", flag.name)))?;
                if !valid_value(value, &flag.value_type, &flag.range, &flag.enum_values) {
                    return Err(refuse(format!("invalid value for {}: {value}", flag.name)));
                }
                value.clone()
            } else {
                String::new()
            };
            ordered_flags.push((flag.name.clone(), value.clone()));
            flags.entry(flag.name.clone()).or_default().push(value);
        }
        if !flags.contains_key("--help") {
            validate_arguments(&self.arguments, &positionals)?;
            for flag in &self.flags {
                if flag.required && !flags.contains_key(&flag.name) {
                    return Err(refuse(format!("required flag missing: {}", flag.name)));
                }
                if !flags.contains_key(&flag.name) {
                    continue;
                }
                for conflict in &flag.conflicts {
                    if flags.contains_key(conflict) {
                        return Err(refuse(format!("{} conflicts with {conflict}", flag.name)));
                    }
                }
                for dependency in &flag.dependencies {
                    if !flags.contains_key(dependency) {
                        return Err(refuse(format!("{} requires {dependency}", flag.name)));
                    }
                }
            }
        }
        Ok(Invocation {
            operation: self,
            positionals,
            flags,
            ordered_flags,
        })
    }
}

fn validate_arguments(arguments: &[Argument], values: &[String]) -> Result<(), Refusal> {
    let minimum = arguments
        .iter()
        .filter(|argument| argument.required)
        .count();
    let unlimited = arguments
        .last()
        .is_some_and(|argument| argument.multiplicity == "many");
    if values.len() < minimum || (!unlimited && values.len() > arguments.len()) {
        return Err(refuse(format!(
            "expected {minimum}..{} positional arguments, received {}",
            if unlimited {
                "many".into()
            } else {
                arguments.len().to_string()
            },
            values.len()
        )));
    }
    for (index, value) in values.iter().enumerate() {
        let Some(argument) = arguments.get(index).or_else(|| arguments.last()) else {
            break;
        };
        if !valid_value(
            value,
            &argument.value_type,
            &argument.range,
            &argument.enum_values,
        ) {
            return Err(refuse(format!("invalid {}: {value}", argument.name)));
        }
    }
    Ok(())
}

fn valid_value(value: &str, kind: &str, range: &str, values: &[String]) -> bool {
    match kind {
        "enum" => values.iter().any(|candidate| candidate == value),
        "u64" => {
            let Ok(number) = value.trim_start().parse::<u64>() else {
                return false;
            };
            match range.split_once("..") {
                None => true,
                Some((lower, upper)) => matches!((lower.parse::<u64>(), upper.parse::<u64>()),
                    (Ok(lower), Ok(upper)) if lower <= number && number <= upper),
            }
        }
        "number" => {
            let Ok(number) = value.trim_start().parse::<f64>() else {
                return false;
            };
            if !number.is_finite() {
                return false;
            }
            match range.split_once("..") {
                None => true,
                Some((lower, upper)) => matches!((lower.parse::<f64>(), upper.parse::<f64>()),
                    (Ok(lower), Ok(upper)) if lower.is_finite() && upper.is_finite()
                        && lower <= number && number <= upper),
            }
        }
        _ => true,
    }
}

fn distance(left: &str, right: &str) -> usize {
    let mut prior: Vec<usize> = (0..=right.len()).collect();
    for (row, left) in left.bytes().enumerate() {
        let mut next = vec![row + 1; right.len() + 1];
        for (column, right) in right.bytes().enumerate() {
            next[column + 1] = (next[column] + 1)
                .min(prior[column + 1] + 1)
                .min(prior[column] + usize::from(left != right));
        }
        prior = next;
    }
    prior[right.len()]
}

#[cfg(test)]
mod tests {
    use super::*;
    fn words(input: &[&str]) -> Vec<String> {
        input.iter().map(|word| (*word).into()).collect()
    }
    #[test]
    fn every_command_and_alias_has_one_canonical_owner() {
        let registry = Registry::embedded().unwrap();
        for operation in registry.operations.iter().filter(|operation| operation.cli) {
            for path in std::iter::once(&operation.command_path)
                .chain(operation.aliases.iter().map(|alias| &alias.path))
            {
                let mut input = path.clone();
                input.push("--help".into());
                let parsed = registry.parse(&input).unwrap();
                assert_eq!(parsed.operation.operation_id, operation.operation_id);
            }
        }
    }
    #[test]
    fn longest_path_and_flag_aliases() {
        let registry = Registry::embedded().unwrap();
        let parsed = registry
            .parse(&words(&["model", "list", "--json"]))
            .unwrap();
        assert_eq!(parsed.operation.command_path, words(&["model", "list"]));
        assert!(parsed.has("--json"));
        assert!(
            registry
                .parse(&words(&["version", "-h"]))
                .unwrap()
                .has("--help")
        );
    }
    #[test]
    fn negative_controls_do_not_dispatch() {
        let registry = Registry::embedded().unwrap();
        for input in [
            &["version", "--bad"][..],
            &["version", "--json", "--json"],
            &["model", "list", "--output", "nonsense"],
            &["model", "load", "x", "--ctx", "NaN"],
        ] {
            assert!(registry.parse(&words(input)).is_err(), "accepted {input:?}");
        }
        assert!(registry.slash("/reset extra extra").is_err());
        for name in ["/reset", "/cancel", "/close", "/detach"] {
            assert!(registry.slash(name).is_ok());
            assert!(registry.slash(&format!("{name} main")).is_ok());
        }
        assert!(registry.parse(&words(&["session", "reset"])).is_err());
        assert!(registry.slash("/unknown").is_err());
        assert!(!valid_value("NaN", "number", "delegated", &[]));
        assert!(!valid_value("-1", "u64", "delegated", &[]));
        assert!(!valid_value(
            "18446744073709551616",
            "u64",
            "delegated",
            &[]
        ));
    }

    #[test]
    fn discovery_preserves_the_published_host_lane() {
        let registry = Registry::embedded().unwrap();
        let discovery = registry.discovery();
        let host = discovery["operations"]
            .as_array()
            .unwrap()
            .iter()
            .find(|operation| operation["operation_id"] == "host.serve")
            .unwrap();
        assert_eq!(host["lane"], "host-entrypoint");
        assert_eq!(host["command_path"], "serve");
    }

    #[test]
    fn completion_is_derived_from_the_same_flags_and_values() {
        let registry = Registry::embedded().unwrap();
        for shell in ["bash", "zsh", "fish"] {
            let source = registry.completion(shell).unwrap();
            assert!(source.contains("model list"));
            assert!(source.contains("serve --logs"));
            assert!(source.contains("human") && source.contains("--json"));
            assert!(!source.contains("finite.decision.producer"));
        }
        assert!(registry.completion("unadmitted").is_err());
    }
}
