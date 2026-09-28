<!-- docs:metadata
title: Use the Interactive Client
id: yvex.guides.interactive-client
document: guide
status: current
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Use the Interactive Client

**Operate retained sessions through the terminal and typed requests.**

[Up](README.md)

## Interactive console

`./yvex chat` is the interactive entrypoint. Its attachment view derives model,
engine, session, context, and resource facts from the host; example memory
numbers or a screenshot are not admission evidence. `/help` gives the current
registry-authored command catalog.

The [external REPLAI editor](../decisions/0007-external-terminal-editor.md) owns
input editing, history navigation, paste framing, and redraw. YVEX owns
commands, attachment conversion, session/engine binding, and typed generation
output and cancellation. The editor closes before generation starts; retained
presentation helpers are not a second native editor.

The prompt label is a product-catalog projection, while the
session remains bound to the exact engine generation. On transport loss the
same prompt adds `[disconnected]`; it never silently switches models. Model
output distinguishes explicit reasoning from final text; final text has no extra
section label. The terminal renderer supports bounded headings, lists, emphasis,
inline/fenced code and quotes. Prose wraps progressively at display-cell boundaries,
to at most 96 cells or the narrower terminal measure, independently of wire fragment
boundaries. CJK, combining marks and single-codepoint emoji use Linux UTF-8 cell
widths; complex emoji sequences remain terminal-dependent. Inline styling and
normalized prose spacing do not change canonical response bytes. During a turn, the
console updates one server-authored prefill line in place. The terminal result
then reports prefill, generation, TTFT, speculation, initial/final context,
adaptive or explicit output envelope, truthful stop reason, and session in a
compact summary. Candidate token text is never displayed.

On a TTY, cyan marks the prompt and active work, green marks readiness and
completion, orange marks cancellation or warning, red marks refusal, and dim
text carries secondary facts. Redirected and machine-readable output never
contains terminal controls. Set `NO_COLOR=1` to disable color explicitly; if
that variable is already exported in the shell, unset it to see the semantic
colors.

Slash commands are discovered from the canonical registry and their complete
current catalog is visible at startup. `/help` adds one-line descriptions;
`/status`, `/runtime`, `/model`, `/memory`, and `/context` inspect state;
`/session`, `/sessions`, `/new`, `/use`, `/detach`, `/reset`, and `/close`
manage the session; `/attach PATH` stages one local media object for the next
turn, `/attachments` lists the bounded ordered stage, and
`/attachments-clear` discards it. Repeated attachments belong to the same next
turn; after accepted submission the stage clears while the exact session stays
attached. `/cancel` cancels active generation; and `/quit` exits locally.
`/exit` is a registry-owned alias for `/quit`; bare `exit` remains
ordinary model input. Tab completes an unambiguous slash command. Commands for an unsupported
explicit reasoning channel refuse rather than simulate support. The current
DSpark profile admits `/think`, `/think-max`, and `/nothink`; a family with an
authenticated low-effort policy additionally admits `/think-low`. They select the
source-authored model-emitted channel and never expose hidden reasoning. A
policy change that alters the encoded prefix safely rebuilds only physical
sequence state and re-prefills the authoritative semantic history; reset is
not required merely to change reasoning mode.

Ctrl-D deletes at the cursor on nonempty input and exits on empty input.
Ctrl-C during a turn requests server-owned cancellation and returns to the
prompt; a second Ctrl-C requests exit. With no active turn, the first Ctrl-C clears the line and
a second consecutive Ctrl-C exits. EOF, cancellation, resize, and failure all
restore bracketed-paste and terminal modes before returning control to the
shell. Cancellation or failure requires `/reset` only when the server reports
committed partial progress. The console names that state and refuses a new turn
instead of silently appending to it. Reset creates a fresh execution session
while keeping the process-resident model open.

Left/Right, Home/End, Delete and Backspace edit the current UTF-8 line without
submitting it; Up/Down navigate local history. If the server connection closes,
an active progress row is terminated cleanly and the prompt changes to
`yvex [disconnected]>`. Local help and exit remain available. The next remote
operation attempts one foreground reconnect; when that fails, the unsent line
is preserved for another attempt rather than discarded.

Ctrl-L clears the visible terminal while the REPL prompt is active, then redraws
the prompt and any input already typed. It does not detach, reset, cancel, or
otherwise mutate the server-owned session.

While a generation owns the stream, chat suppresses terminal echo and does not
accept a draft for the next turn. Arrow/editing/UTF-8 bytes entered during that
interval are discarded before the line editor returns; they cannot enter the
assistant transcript or the following prompt. Ctrl-C remains an admitted
server-side cancellation signal. Terminal attributes are restored on every
success, refusal, disconnect and cancellation path.


## Interactive and programmatic requests

Human generation remains inside `yvex chat`. `/nothink`, `/think-low`, `/think`,
and `/think-max` select an admitted source-authored reasoning policy for the attached
session and that policy remains active until changed. Reuse an existing named
session by starting chat with `--session NAME`. Omitting
`--max-new-tokens` sends no client cap: the host resolves an adaptive envelope
from the loaded engine and remaining context. Supplying
`--max-new-tokens N` is an explicit upper bound. Neither mode is infinite;
EOS, source-authored stops, cancellation, context exhaustion and the resolved
output envelope remain distinct terminal reasons.

Programmatic inference uses the admitted private protocol or the loopback
OpenAI compatibility API described below; it is not projected as a second
one-shot CLI command. Fork an idle committed session with an explicit upper
bound for shared prefix backing:

```sh
./yvex session fork main experiment 1073741824
./yvex chat --session experiment
```

The child starts at the exact committed position and receives independent RNG,
token ledger, decoder, transcript and conversation state. Shared state pages
remain immutable and become private on write. The command refuses an active or
partial source, a duplicate child name, incompatible state geometry, or a
prefix exceeding the supplied byte bound.
