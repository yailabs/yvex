// Operator input UX consumes source-authored tokenizer/prompt semantics through C.
use crate::{
    ffi::{self, raw},
    presentation,
    registry::Invocation,
};
use replai::{Block, Document, Role, Theme};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

pub(crate) fn input(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<String> {
    let action = &invocation.positionals[0];
    let model = crate::pipeline::required(invocation, "--model")?;
    let value = match action.as_str() {
        "tokens" if !invocation.has("--text") => crate::pipeline::required(invocation, "--tokens")?,
        "prompt" if !invocation.has("--tokens") => crate::pipeline::required(invocation, "--text")?,
        _ => {
            return Err(crate::registry::Refusal {
                reason: "input tokens requires --tokens; input prompt requires --text".into(),
                hint: None,
            }
            .into());
        }
    };
    let reference = ffi::Reference::resolve(model)?;
    if reference.is_alias() {
        let (integrity, status) = reference.integrity(None, false, 0)?;
        let verification = reference.integrity_verification(&integrity)?;
        if status != 0 || !verification.passed {
            return Err(ffi::Error {
                code: raw::yvex_status_YVEX_ERR_STATE,
                owner: "input.identity".into(),
                message: "registered artifact identity or metadata no longer agrees".into(),
            }
            .into());
        }
    }
    let path = reference.path()?;
    let ids = if action == "prompt" {
        let view = ffi::ModelView::tokenizer(&path)?;
        if view.info().support != "fixture-encode-decode" {
            return Err(ffi::Error {
                code: raw::yvex_status_YVEX_ERR_UNSUPPORTED,
                owner: "input.prompt".into(),
                message:
                    "bounded legacy token input does not admit this tokenizer; use tokenizer encode"
                        .into(),
            }
            .into());
        }
        view.encode(value.as_bytes(), false, false, false)?.ids
    } else {
        Vec::new()
    };
    let input = ffi::token_input(&path, (action == "tokens").then_some(value), &ids)?;
    let tokens = input.tokens[..input.token_count as usize]
        .iter()
        .map(u32::to_string)
        .collect::<Vec<_>>()
        .join(" ");
    presentation::record(
        &format!("INPUT  {action} · validated"),
        &[
            ("model", model),
            ("token_input_status", "pass"),
            (
                "token_input_kind",
                match input.kind {
                    raw::yvex_token_input_kind_YVEX_TOKEN_INPUT_EXPLICIT => "explicit",
                    raw::yvex_token_input_kind_YVEX_TOKEN_INPUT_PROMPT_TEXT => "prompt-text",
                    _ => return Err("native input returned an unknown kind".into()),
                },
            ),
            ("token_count", &input.token_count.to_string()),
            ("vocab_size", &input.vocab_size.to_string()),
            ("token_bounds_status", "pass"),
            ("ids", &tokens),
            ("generation_ready", "false"),
            ("prefill_ready", "false"),
            ("generation", "unsupported"),
            ("status", "token-input-pass"),
        ],
        width,
        styled,
    )
    .map_err(Into::into)
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<String> {
    let view = ffi::ModelView::tokenizer(&invocation.positionals[0])?;
    match invocation.operation.operation_id.as_str() {
        "tokenizer.show" => describe(&view.info(), width, styled),
        "tokenizer.encode" => {
            let input = invocation.value("--text").ok_or("encode requires --text")?;
            // Explicit insertion is CLI policy; the native tokenizer owns the actual special IDs.
            let bos = invocation
                .ordered_flags
                .iter()
                .rev()
                .find_map(|(name, _)| match name.as_str() {
                    "--bos" => Some(true),
                    "--no-bos" => Some(false),
                    _ => None,
                })
                .unwrap_or(false);
            let encoded = view.encode(
                input.as_bytes(),
                bos,
                invocation.has("--eos"),
                invocation.has("--pieces"),
            )?;
            encoding(&encoded, width, styled)
        }
        "tokenizer.decode" => {
            let ids = invocation.value("--ids").ok_or("decode requires --ids")?;
            let ids = parse_ids(ids)?;
            let decoded = view.decode(&ids)?;
            presentation::record(
                "TOKENIZER  decoded",
                &[
                    ("text", &quoted(&decoded.bytes)),
                    ("decoder_identity", &decoded.identity),
                    (
                        "detokenization_ready",
                        if decoded.runtime { "true" } else { "false" },
                    ),
                    ("generation_ready", "false"),
                ],
                width,
                styled,
            )
            .map_err(Into::into)
        }
        "tokenizer.prompt" => prompt(invocation, &view, width, styled),
        _ => Err("unprojected tokenizer operation".into()),
    }
}

fn parse_ids(input: &str) -> Result<Vec<u32>> {
    let refuse = || {
        Box::<dyn std::error::Error>::from(crate::registry::Refusal {
            reason: "token IDs must be comma-separated unsigned 32-bit integers".into(),
            hint: None,
        })
    };
    if input.is_empty() {
        return Err(refuse());
    }
    input
        .split(',')
        .map(|word| {
            let word = word.trim();
            if word.is_empty() || !word.bytes().all(|byte| byte.is_ascii_digit()) {
                return Err(refuse());
            }
            word.parse::<u32>().map_err(|_| refuse())
        })
        .collect()
}

fn quoted(bytes: &[u8]) -> String {
    if let Ok(text) = std::str::from_utf8(bytes) {
        return serde_json::to_string(text).expect("string projection");
    }
    // Non-UTF8 data remains byte-preserving and cannot execute terminal controls.
    format!(
        "\"{}\"",
        bytes
            .iter()
            .flat_map(|byte| std::ascii::escape_default(*byte))
            .map(char::from)
            .collect::<String>()
    )
}

fn describe(info: &ffi::TokenizerInfo, width: usize, styled: bool) -> Result<String> {
    let mut facts = vec![
        ("architecture", info.architecture.clone()),
        ("model_name", info.name.clone()),
        ("tokenizer_model", info.kind.clone()),
        ("support", info.support.clone()),
        ("vocab_size", info.vocabulary.to_string()),
        (
            "runtime_support",
            if info.plan.is_some() {
                "exact-artifact-bpe"
            } else {
                "unavailable"
            }
            .into(),
        ),
        (
            "chat_template",
            if info.chat_template {
                "present"
            } else {
                "absent"
            }
            .into(),
        ),
    ];
    if let Some(plan) = &info.plan {
        facts.extend([
            ("base_vocab_size", plan.base_vocabulary_size.to_string()),
            ("merge_count", plan.merge_count.to_string()),
            ("added_token_count", plan.added_token_count.to_string()),
            ("special_token_count", plan.special_token_count.to_string()),
            (
                "tokenizer_json_identity",
                ffi::text(&plan.tokenizer_json_identity),
            ),
            (
                "tokenizer_config_identity",
                ffi::text(&plan.tokenizer_config_identity),
            ),
            (
                "tokenizer_plan_identity",
                ffi::text(&plan.tokenizer_plan_identity),
            ),
            (
                "prompt_policy",
                if plan.prompt_policy
                    == raw::yvex_tokenizer_prompt_policy_YVEX_TOKENIZER_PROMPT_CONVERSATION
                {
                    "conversation-family-policy"
                } else {
                    "verbatim-no-special"
                }
                .into(),
            ),
        ]);
    } else {
        facts.push(("prompt_policy", "unavailable".into()));
    }
    for (name, id) in [
        "bos_token_id",
        "eos_token_id",
        "pad_token_id",
        "unk_token_id",
    ]
    .into_iter()
    .zip(info.specials)
    {
        facts.push((
            name,
            id.map(|value| value.to_string())
                .unwrap_or_else(|| "absent".into()),
        ));
    }
    facts.extend([
        ("tokenizer_runtime_ready", info.plan.is_some().to_string()),
        ("generation_ready", "false".into()),
        (
            "status",
            if info.plan.is_some() {
                "tokenizer-ready"
            } else {
                "tokenizer-descriptor"
            }
            .into(),
        ),
    ]);
    let borrowed = facts
        .iter()
        .map(|(name, value)| (*name, value.as_str()))
        .collect::<Vec<_>>();
    Ok(presentation::record("TOKENIZER", &borrowed, width, styled)?)
}

fn encoding(encoded: &ffi::Encoding, width: usize, styled: bool) -> Result<String> {
    let ids = encoded
        .ids
        .iter()
        .map(u32::to_string)
        .collect::<Vec<_>>()
        .join(" ");
    let mut output = presentation::record(
        "TOKENIZER  encoded",
        &[
            ("tokens", &encoded.ids.len().to_string()),
            ("ids", &ids),
            ("encoding_identity", &encoded.identity),
            (
                "tokenizer_runtime_ready",
                if encoded.runtime { "true" } else { "false" },
            ),
            ("generation_ready", "false"),
        ],
        width,
        styled,
    )?;
    if !encoded.pieces.is_empty() {
        let lines = encoded
            .ids
            .iter()
            .zip(&encoded.pieces)
            .map(|(id, piece)| format!("{id}  {}", quoted(piece)))
            .collect::<Vec<_>>();
        output.push_str(&presentation::lines(&lines, width, styled)?);
    }
    Ok(output)
}

fn prompt(
    invocation: &Invocation<'_>,
    view: &ffi::ModelView,
    width: usize,
    styled: bool,
) -> Result<String> {
    // Preserve message order across interleaved repeatable roles, not sorted flag order.
    let messages = invocation
        .ordered_flags
        .iter()
        .filter_map(|(flag, text)| {
            let role = match flag.as_str() {
                "--system" => raw::yvex_prompt_role_YVEX_PROMPT_ROLE_SYSTEM,
                "--user" => raw::yvex_prompt_role_YVEX_PROMPT_ROLE_USER,
                "--assistant" => raw::yvex_prompt_role_YVEX_PROMPT_ROLE_ASSISTANT,
                "--tool" => raw::yvex_prompt_role_YVEX_PROMPT_ROLE_TOOL,
                _ => return None,
            };
            Some((role, text.clone()))
        })
        .collect::<Vec<_>>();
    let (bytes, identity) = view.prompt(
        &messages,
        invocation.has("--thinking"),
        !invocation.has("--no-generation-prompt"),
    )?;
    let text = std::str::from_utf8(&bytes)?;
    let mut output = presentation::record(
        "PROMPT  source-authored",
        &[
            ("rendered_bytes", &bytes.len().to_string()),
            ("prompt_identity", &identity),
            ("generation_ready", "false"),
        ],
        width,
        styled,
    )?;
    output.push_str(
        &Document::new(vec![Block::Literal(presentation::safe_text(
            text,
            Role::Default,
        )?)])?
        .render(width, Theme::from_environment(styled))?,
    );
    if invocation.has("--tokens") {
        output.push_str(&encoding(
            &view.encode(&bytes, false, false, false)?,
            width,
            styled,
        )?);
    }
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn explicit_ids_and_binary_text_remain_bounded_data() {
        assert_eq!(parse_ids("0, 12,4294967295").unwrap(), [0, 12, u32::MAX]);
        for invalid in ["", "1,", "-1", "1,,2", "4294967296", "1x"] {
            assert!(parse_ids(invalid).is_err(), "{invalid}");
        }
        assert_eq!(quoted(&[0xff, 27, b'\n']), "\"\\xff\\x1b\\n\"");
        assert!(!quoted(b"\x1b[2J").contains('\x1b'));
    }
}
