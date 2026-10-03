// Typed channels and bounded UTF-8 fragments; terminal controls never enter as model data.
use crate::{
    ffi::{self, Client, raw},
    presentation,
};
use replai::{OutputSession, Role, Text};
use std::io::{self, Write};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

pub(crate) enum Delivery {
    Resolved,
    Refused(String),
    Indeterminate(String),
    NotDispatched(String),
}

#[derive(Default)]
struct Stream {
    pending: Vec<u8>,
    channel: Option<raw::yvex_client_stream_channel>,
    line_boundary: bool,
    presentation: Option<presentation::StreamText>,
    carriage_return: bool,
}

impl Stream {
    fn finish_fragment(&mut self, bytes: &[u8]) -> Result<String> {
        self.pending.extend_from_slice(bytes);
        let valid = match std::str::from_utf8(&self.pending) {
            Ok(_) => self.pending.len(),
            Err(error) if error.error_len().is_none() => error.valid_up_to(),
            Err(_) => return Err("invalid UTF-8 in admitted model stream".into()),
        };
        let text = std::str::from_utf8(&self.pending[..valid])?;
        let mut normalized = String::new();
        for ch in text.chars() {
            if self.carriage_return && ch != '\n' {
                normalized.push_str("\\x0d");
            }
            self.carriage_return = ch == '\r';
            if ch != '\r' {
                normalized.push(ch);
            }
        }
        let safe = presentation::escaped_text(&normalized);
        if !safe.is_empty() {
            self.line_boundary = safe.ends_with('\n');
        }
        self.pending.drain(..valid);
        Ok(safe)
    }

    fn write(
        &mut self,
        reply: &raw::yvex_client_message,
        width: usize,
        styled: bool,
    ) -> Result<()> {
        if reply.byte_count > reply.bytes.len() as u64 {
            return Err("fragment exceeds decoded extent".into());
        }
        let mut output = io::stdout().lock();
        if self.channel != Some(reply.stream_channel) {
            if !self.pending.is_empty() {
                return Err("channel changed inside UTF-8 scalar".into());
            }
            if let Some(mut prior) = self.presentation.take() {
                output.write_all(prior.finish()?.as_bytes())?;
            }
            let label = match reply.stream_channel {
                raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_EXPLICIT_REASONING => {
                    "REASONING"
                }
                raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_FINAL_TEXT
                | raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_UNSPECIFIED => "ANSWER",
                raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_TOOL_CALL => "TOOL CALL",
                raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_TOOL_RESULT => "TOOL RESULT",
                raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_CONTROL_EVENT => "NOTICE",
                raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_ERROR => "ERROR",
                _ => return Err("unknown admitted stream channel".into()),
            };
            output.write_all(presentation::record(label, &[], width, styled)?.as_bytes())?;
            self.channel = Some(reply.stream_channel);
            self.line_boundary = true;
            self.presentation = Some(presentation::StreamText::new(
                width,
                styled,
                reply.stream_channel
                    == raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_EXPLICIT_REASONING,
            ));
        }
        let text = self.finish_fragment(&reply.bytes[..reply.byte_count as usize])?;
        output.write_all(
            self.presentation
                .as_mut()
                .expect("channel presentation")
                .write(&text)?
                .as_bytes(),
        )?;
        output.flush()?;
        Ok(())
    }

    fn finish(&mut self) -> Result<()> {
        if !self.pending.is_empty() {
            return Err("incomplete UTF-8 in terminated model stream".into());
        }
        if let Some(mut presentation) = self.presentation.take() {
            let mut output = io::stdout().lock();
            if self.carriage_return {
                output.write_all(presentation.write("\\x0d")?.as_bytes())?;
            }
            output.write_all(presentation.finish()?.as_bytes())?;
            output.flush()?;
            self.line_boundary = true;
        }
        Ok(())
    }
}

pub(crate) struct Turn<'a> {
    pub alias: &'a str,
    pub generation: u64,
    pub session: &'a str,
    pub prompt: &'a str,
    pub attachments: &'a [raw::yvex_content_part],
    pub width: usize,
    pub styled: bool,
}

impl Turn<'_> {
    pub(crate) fn run(
        &self,
        configure: impl FnOnce(&mut raw::yvex_client_request),
        admitted: impl Fn(),
    ) -> Result<Delivery> {
        let mut connection = match Client::connect(None) {
            Ok(connection) => connection,
            Err(error) => return Ok(Delivery::NotDispatched(error.to_string())),
        };
        let mut request =
            connection.request(raw::yvex_client_operation_YVEX_CLIENT_OP_GENERATION_TURN);
        ffi::put_text(&mut request.model_alias, self.alias)?;
        ffi::put_text(&mut request.session_name, self.session)?;
        request.engine_generation = self.generation;
        configure(&mut request);
        let mut output = OutputSession::open(&io::stdin(), &io::stdout())?;
        let result =
            if let Err(error) = connection.send_text(&request, self.prompt, self.attachments) {
                Delivery::Indeterminate(error.to_string())
            } else {
                self.receive(&mut connection, &request, &mut output, admitted)
                    .unwrap_or_else(|error| Delivery::Indeterminate(error.to_string()))
            };
        // There is deliberately no editor during a turn. Quiet-output owns
        // echo suppression and the queued-input discard, so bytes typed while
        // generation is active cannot become a later draft or corrupt UTF-8.
        // Signal/cancellation semantics remain terminal-owned and unchanged.
        output.close(true)?;
        Ok(result)
    }

    fn receive(
        &self,
        connection: &mut Client,
        request: &raw::yvex_client_request,
        output: &mut OutputSession,
        admitted: impl Fn(),
    ) -> Result<Delivery> {
        let mut stream = Stream::default();
        let mut turn_identity = None;
        loop {
            let reply = match connection.receive() {
                Ok(reply) => reply,
                Err(error) => {
                    output.feedback(Text::new("")?)?;
                    // Best effort line restoration cannot assert an incomplete stream was valid.
                    let _ = stream.finish();
                    return Ok(Delivery::Indeterminate(error.to_string()));
                }
            };
            if reply.request_number != request.request_number {
                return Ok(Delivery::Indeterminate(
                    "response request identity mismatch".into(),
                ));
            }
            let identity = ffi::text(&reply.turn_identity);
            if !identity.is_empty() {
                if turn_identity
                    .as_ref()
                    .is_some_and(|prior| prior != &identity)
                {
                    return Ok(Delivery::Indeterminate(
                        "response turn identity mismatch".into(),
                    ));
                }
                turn_identity = Some(identity);
            }
            match reply.kind {
                raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_TURN_STARTED => admitted(),
                raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_EVENT
                    if stream.channel.is_none() =>
                {
                    let event = &reply.event;
                    let phase = ffi::text(&event.phase);
                    let text = if event.measurement.available
                        & raw::YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE as u64
                        != 0
                    {
                        format!(
                            "{} · {}/{}",
                            phase, event.measurement.completed_units, event.measurement.total_units
                        )
                    } else {
                        phase
                    };
                    output.feedback(presentation::safe_text(&text, Role::Dim)?)?;
                }
                raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_EVENT => {}
                raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_FRAGMENT => {
                    output.feedback(Text::new("")?)?;
                    stream.write(&reply, self.width, self.styled)?;
                }
                raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_TURN_COMPLETE => {
                    output.feedback(Text::new("")?)?;
                    stream.finish()?;
                    if reply.media_result.available != 0 {
                        let text = format!(
                            "MEDIA  {} · {} bytes",
                            ffi::text(&reply.media_result.output_path),
                            reply.media_result.file_bytes
                        );
                        io::stdout().write_all(
                            presentation::lines(&[text], self.width, self.styled)?.as_bytes(),
                        )?;
                    }
                    return Ok(Delivery::Resolved);
                }
                raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ERROR => {
                    output.feedback(Text::new("")?)?;
                    stream.finish()?;
                    let mut reason = format!(
                        "{} · {}",
                        ffi::status_name(reply.status),
                        ffi::text(&reply.reason)
                    );
                    if reply.partial_turn.available != 0 {
                        reason.push_str(&format!(
                            " · {} committed tokens · position {} · {}",
                            reply.partial_turn.committed_token_count,
                            reply.partial_turn.final_committed_position,
                            if reply.partial_turn.reset_required != 0 {
                                "reset required (/reset)"
                            } else {
                                "recovery unavailable"
                            }
                        ));
                    }
                    return Ok(Delivery::Refused(reason));
                }
                _ => {
                    return Ok(Delivery::Indeterminate(
                        "unexpected generation response".into(),
                    ));
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn fragmented_unicode_is_preserved_without_control_execution() {
        let mut stream = Stream::default();
        assert_eq!(stream.finish_fragment(&[0xe7, 0x95]).unwrap(), "");
        assert_eq!(stream.finish_fragment(&[0x8c, 0x1b]).unwrap(), "界\\x1b");
        assert!(stream.pending.is_empty());
    }
    #[test]
    fn invalid_or_incomplete_unicode_is_not_a_successful_stream() {
        assert!(Stream::default().finish_fragment(&[0xff]).is_err());
        let mut stream = Stream::default();
        stream.finish_fragment(&[0xf0]).unwrap();
        assert!(stream.finish().is_err());
    }
}
