// Shared terminal intent uses REPLAI's native layout, geometry and semantic safety.
use replai::{Alignment, Block, Column, Document, Role, Text, Theme};

/// Bounded progressive projection of the existing chat markup subset. This is
/// presentation, never a channel classifier or a Markdown/domain interpreter.
/// REPLAI alone supplies styling and Unicode cell geometry.
pub(crate) struct StreamText {
    pending: String,
    geometry: String,
    prefix: String,
    width: usize,
    styled: bool,
    reasoning: bool,
    started: bool,
    fence: bool,
    suppressed: bool,
    strong: bool,
    emphasis: bool,
    inline_code: bool,
    space: bool,
    previous: Option<char>,
    base: Role,
}

#[derive(Default)]
struct StreamOutput(Vec<(Role, String)>);
impl StreamOutput {
    fn append(&mut self, text: &str, role: Role) {
        if let Some((previous, content)) = self.0.last_mut()
            && *previous == role
        {
            content.push_str(text);
        } else {
            self.0.push((role, text.into()));
        }
    }
    fn push(&mut self, ch: char) {
        self.append(&ch.to_string(), Role::Default);
    }
    fn render(self, styled: bool) -> Result<String, replai::EditError> {
        if self.0.is_empty() {
            return Ok(String::new());
        }
        let spans = self
            .0
            .iter()
            .map(|(role, text)| replai::Span::new(*role, text))
            .collect::<Result<Vec<_>, _>>()?;
        let value = Document::new(vec![Block::Paragraph(Text::from_spans(spans)?)])?
            .render(4096, Theme::from_environment(styled))?;
        Ok(value.strip_suffix('\n').unwrap_or(&value).into())
    }
}

impl StreamText {
    pub(crate) fn new(width: usize, styled: bool, reasoning: bool) -> Self {
        Self {
            pending: String::new(),
            geometry: String::new(),
            prefix: String::new(),
            width: width.saturating_sub(2).clamp(4, 96),
            styled,
            reasoning,
            started: false,
            fence: false,
            suppressed: false,
            strong: false,
            emphasis: false,
            inline_code: false,
            space: false,
            previous: None,
            base: Role::Default,
        }
    }

    fn paint(&self, text: &str, role: Role, out: &mut StreamOutput) {
        let role = if self.reasoning { Role::Dim } else { role };
        out.append(text, role);
    }

    fn cells(text: &str) -> Result<usize, replai::EditError> {
        replai::WidthPolicy::UnicodeNarrow.measure(text)
    }

    fn start(&mut self, finish: bool, out: &mut StreamOutput) -> Result<bool, replai::EditError> {
        let value = self.pending.as_str();
        let trimmed = value.trim_start_matches(' ');
        if !finish
            && (trimmed.is_empty()
                || (trimmed.starts_with('`') && (trimmed.len() < 3 || trimmed.starts_with("```")))
                || (trimmed.starts_with('#') && trimmed.trim_start_matches('#').trim().is_empty())
                || ((trimmed.starts_with(['-', '*', '+', '>'])
                    || trimmed.starts_with(|c: char| c.is_ascii_digit()))
                    && !trimmed.contains(|c: char| !c.is_ascii_digit() && !"-*+>. ".contains(c))))
        {
            return Ok(false);
        }
        self.started = true;
        self.suppressed = false;
        self.base = Role::Default;
        self.prefix = if self.reasoning {
            "  │ ".into()
        } else {
            "  ".into()
        };
        if let Some(language) = trimmed.strip_prefix("```") {
            self.fence = !self.fence;
            self.suppressed = true;
            if self.fence {
                let label = if language.trim().is_empty() {
                    "code".into()
                } else {
                    format!("code · {}", language.trim())
                };
                self.paint(&format!("{}{label}", self.prefix), Role::Dim, out);
            }
            self.pending.clear();
            return Ok(true);
        }
        if self.fence {
            self.prefix.push_str("  ");
            self.base = Role::Accent;
        } else {
            let heading = value.bytes().take_while(|b| *b == b'#').count();
            if (1..=6).contains(&heading) && value.as_bytes().get(heading) == Some(&b' ') {
                self.base = Role::Strong;
                self.pending = value[heading..].trim_start_matches(' ').into();
            } else if ["- ", "* ", "+ "]
                .iter()
                .any(|prefix| value.starts_with(prefix))
            {
                self.prefix.push_str("• ");
                self.pending.drain(..2);
            } else if value.starts_with("> ") {
                self.prefix.push_str("│ ");
                self.base = Role::Dim;
                self.pending.drain(..2);
            }
        }
        self.geometry = self.prefix.clone();
        self.paint(&self.prefix, self.base, out);
        Ok(true)
    }

    fn emit(
        &mut self,
        text: &str,
        role: Role,
        out: &mut StreamOutput,
    ) -> Result<(), replai::EditError> {
        let before = Self::cells(&self.geometry)?;
        let pending = if self.space { " " } else { "" };
        let combined = format!("{}{pending}{text}", self.geometry);
        if combined.len() > 16 * 1024 {
            return Err(replai::EditError::Capacity);
        }
        if Self::cells(&combined)? > self.width && before > Self::cells(&self.prefix)? {
            out.push('\n');
            let indent = Self::cells(&self.prefix)?;
            let continuation = if self.reasoning {
                format!("  │ {}", " ".repeat(indent.saturating_sub(4)))
            } else {
                " ".repeat(indent)
            };
            self.paint(&continuation, self.base, out);
            self.geometry = continuation;
            self.space = false;
        }
        if self.space {
            self.paint(" ", role, out);
            self.geometry.push(' ');
            self.space = false;
        }
        self.paint(text, role, out);
        self.geometry.push_str(text);
        Ok(())
    }

    fn flush(&mut self, finish: bool, out: &mut StreamOutput) -> Result<(), replai::EditError> {
        if !self.started && !self.start(finish, out)? {
            return Ok(());
        }
        while !self.pending.is_empty() {
            let ch = self.pending.chars().next().unwrap();
            if !self.fence && !finish && self.pending.chars().all(|c| "*_`".contains(c)) {
                break;
            }
            let may_open = self
                .previous
                .is_none_or(|c| !c.is_ascii() || c.is_whitespace() || c.is_ascii_punctuation());
            if !self.fence
                && ch == '*'
                && self.pending.starts_with("**")
                && (self.strong || may_open)
            {
                self.strong = !self.strong;
                self.pending.drain(..2);
                continue;
            }
            if !self.fence
                && ((ch == '`' && (self.inline_code || may_open))
                    || ("*_".contains(ch) && (self.emphasis || may_open)))
            {
                if ch == '`' {
                    self.inline_code = !self.inline_code;
                } else {
                    self.emphasis = !self.emphasis;
                }
                self.pending.drain(..ch.len_utf8());
                continue;
            }
            self.pending.drain(..ch.len_utf8());
            if !self.fence && matches!(ch, ' ' | '\t') {
                self.space = true;
                self.previous = Some(ch);
                continue;
            }
            let role = if self.strong {
                Role::Strong
            } else if self.inline_code || self.emphasis {
                Role::Accent
            } else {
                self.base
            };
            let value = if ch == '\t' {
                "    ".into()
            } else {
                ch.to_string()
            };
            self.emit(&value, role, out)?;
            self.previous = Some(ch);
        }
        Ok(())
    }

    pub(crate) fn write(&mut self, text: &str) -> Result<String, replai::EditError> {
        let mut output = StreamOutput::default();
        // Process identical scalar boundaries regardless of transport chunk size.
        for ch in text.chars() {
            if ch == '\n' {
                self.flush(true, &mut output)?;
                if !self.suppressed || self.fence {
                    output.push('\n');
                }
                self.started = false;
                self.strong = false;
                self.emphasis = false;
                self.inline_code = false;
                self.space = false;
                self.previous = None;
            } else {
                if self.pending.len() + ch.len_utf8() > 16 * 1024 {
                    return Err(replai::EditError::Capacity);
                }
                self.pending.push(ch);
                self.flush(false, &mut output)?;
            }
        }
        output.render(self.styled)
    }

    pub(crate) fn finish(&mut self) -> Result<String, replai::EditError> {
        let mut output = StreamOutput::default();
        if !self.started && self.pending.is_empty() {
            return Ok(String::new());
        }
        self.flush(true, &mut output)?;
        if self.started && !self.suppressed {
            output.push('\n');
        }
        output.render(self.styled)
    }
}

pub fn table(
    headings: &[(&str, Alignment)],
    rows: &[Vec<String>],
    width: usize,
    styled: bool,
) -> Result<String, replai::EditError> {
    let columns = headings
        .iter()
        .map(|(heading, alignment)| {
            Ok(Column {
                heading: safe_text(heading, Role::Accent)?,
                alignment: *alignment,
            })
        })
        .collect::<Result<Vec<_>, replai::EditError>>()?;
    let rows = rows
        .iter()
        .map(|row| {
            row.iter()
                .map(|value| safe_text(value, Role::Default))
                .collect::<Result<Vec<_>, _>>()
        })
        .collect::<Result<Vec<_>, _>>()?;
    Document::new(vec![Block::Table { columns, rows }])?
        .render(width, Theme::from_environment(styled))
}

pub fn lines(lines: &[String], width: usize, styled: bool) -> Result<String, replai::EditError> {
    let blocks = lines
        .iter()
        .map(|line| Ok(Block::Paragraph(safe_text(line, Role::Default)?)))
        .collect::<Result<Vec<_>, replai::EditError>>()?;
    Document::new(blocks)?.render(width, Theme::from_environment(styled))
}

/// Inline emphasis supplied by the fact owner; REPLAI owns wrapping and color.
pub fn spans(
    parts: &[(Role, &str)],
    width: usize,
    styled: bool,
) -> Result<String, replai::EditError> {
    let spans = parts
        .iter()
        .map(|(role, value)| replai::Span::new(*role, &escaped_text(value)))
        .collect::<Result<Vec<_>, _>>()?;
    Document::new(vec![Block::Paragraph(Text::from_spans(spans)?)])?
        .render(width, Theme::from_environment(styled))
}

/// One terminal-native log line, not a responsive object/detail record.
/// Whole words get a small continuation indent; REPLAI owns cells and styling.
pub(crate) fn log_record(
    header: &[(Role, &str)],
    message: &str,
    role: Role,
    width: usize,
    styled: bool,
) -> Result<String, replai::EditError> {
    let policy = replai::WidthPolicy::UnicodeNarrow;
    let mut spans = Vec::new();
    let mut used = 0;
    for (role, value) in header.iter().copied().chain([(role, message)]) {
        let mut grouped = String::new();
        // Only authored log facts are packed here, never model-output prose.
        // Long indivisible identities still use REPLAI's grapheme-safe overflow.
        for fact in escaped_text(value).split_whitespace() {
            let cells = policy.measure(fact)?;
            if used != 0 {
                if used + 1 + cells > width {
                    grouped.push_str("\n  ");
                    used = 2;
                } else {
                    grouped.push(' ');
                    used += 1;
                }
            }
            grouped.push_str(fact);
            used += cells;
        }
        spans.push(replai::Span::new(role, &grouped)?);
    }
    Document::new(vec![Block::Paragraph(Text::from_spans(spans)?)])?
        .render(width, Theme::from_environment(styled))
}

pub fn safe_text(value: &str, role: Role) -> Result<Text, replai::EditError> {
    Text::styled(role, &escaped_text(value))
}

pub(crate) fn escaped_text(value: &str) -> String {
    // Treat external text as data. Control bytes are visible, never terminal commands.
    value
        .chars()
        .flat_map(|character| {
            if character.is_control() && character != '\n' && character != '\t' {
                if u32::from(character) < 256 {
                    format!("\\x{:02x}", u32::from(character))
                        .chars()
                        .collect::<Vec<_>>()
                } else {
                    character.escape_default().collect::<Vec<_>>()
                }
            } else {
                vec![character]
            }
        })
        .collect()
}

pub fn record(
    title: &str,
    fields: &[(&str, &str)],
    width: usize,
    styled: bool,
) -> Result<String, replai::EditError> {
    let header = Block::Paragraph(safe_text(title, Role::Accent)?);
    let fields = fields
        .iter()
        .map(|(key, value)| Ok((safe_text(key, Role::Dim)?, safe_text(value, Role::Default)?)))
        .collect::<Result<Vec<_>, replai::EditError>>()?;
    let theme = Theme::from_environment(styled);
    let mut output = Document::new(vec![header])?.render(width, theme)?;
    output
        .push_str(&Document::new(vec![Block::KeyValue(fields)])?.render_indented(width, theme, 2)?);
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn streaming_markup_is_bounded_and_transport_chunk_independent() {
        let fixture = "你好 **bold**\n- one\n```c\n  int x;\n```\n👩‍💻 👍🏽 🇮🇹\n";
        for width in [40, 80, 180] {
            let mut whole = StreamText::new(width, false, false);
            let mut expected = whole.write(fixture).unwrap();
            expected.push_str(&whole.finish().unwrap());
            let mut pieces = StreamText::new(width, false, false);
            let mut observed = String::new();
            for ch in fixture.chars() {
                observed.push_str(&pieces.write(&ch.to_string()).unwrap());
            }
            observed.push_str(&pieces.finish().unwrap());
            assert_eq!(expected, observed);
            assert!(observed.contains("你好 bold"));
            assert!(observed.contains("  • one"));
            assert!(observed.contains("code · c"));
            assert!(observed.contains("    int x;"));
        }
    }
    #[test]
    fn narrow_and_plain_preserve_facts() {
        let fields = [("state", "READY"), ("name", "café 界")];
        for width in [12, 80, 160] {
            let rendered = record("MODEL", &fields, width, false).unwrap();
            assert!(rendered.contains("READY"));
            assert!(rendered.contains("café 界"));
            assert!(!rendered.contains('\u{1b}'));
        }
    }
    #[test]
    fn untrusted_escapes_are_not_executed() {
        let output = record("NOTICE", &[("text", "x\u{1b}[2Jy")], 80, false).unwrap();
        assert!(!output.contains('\u{1b}'));
        assert!(output.contains("[2Jy"));
    }
}
