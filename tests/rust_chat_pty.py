#!/usr/bin/env python3
"""Native Rust chat over the existing typed producer fixture, never the public host."""
import argparse
import fcntl
import json
import os
import re
from pathlib import Path
import signal
import struct
import subprocess
import tempfile
import termios
import time

from replai_consumer import Chat, ENABLE


def transcript(data):
    return re.sub(rb"\x1b\[[0-?]*[ -/]*[@-~]", b"", bytes(data)).replace(b"\r", b"").replace(b"\n", b"")


def run(binary, fixture, output):
    with tempfile.TemporaryDirectory(prefix="yvex-rust-chat-") as directory:
        runtime = Path(directory) / "runtime"
        socket = runtime / "yvex/yvexd.sock"
        socket.parent.mkdir(parents=True, mode=0o700)
        log_path = output / "host.log"
        # A terminal-unsafe completion label exercises coordinated notices
        # without injecting controls into the editor or touching operator files.
        (Path(directory) / "bad\ncandidate").write_text("fixture")
        (Path(directory) / "badXTAIL").write_text("attachment fixture")
        previous = os.environ.get("XDG_RUNTIME_DIR")
        os.environ["XDG_RUNTIME_DIR"] = str(runtime)
        with log_path.open("wb") as log:
            host = subprocess.Popen([str(fixture), str(socket)], stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 10
                while not socket.exists():
                    assert host.poll() is None, "isolated fixture failed"
                    assert time.monotonic() < deadline, "isolated fixture readiness"
                    time.sleep(0.01)
                result = subprocess.run([str(binary), "chat"], input="", capture_output=True, text=True)
                assert result.returncode == 2 and "requires a terminal" in result.stderr
                assert "session.new" not in log_path.read_text(), "non-TTY chat mutated the producer"
                for columns, plain in ((40, True), (80, False), (180, True)):
                    chat = Chat(binary, f"replai-rust-{columns}", output, plain=plain, columns=columns)
                    try:
                        assert b"/attachments-clear" not in chat.data, "startup dumps command catalog"
                        draft = f"/attach {directory}/badTAIL".encode()
                        start = chat.send(draft + b'\x1b[D' * 4 + b'\t')
                        chat.wait(b'completion unavailable:', start)
                        chat.quiet()
                        # Insertion at the original cursor proves both suffix
                        # and cursor survived output_flow, not merely the prompt.
                        start = chat.send(b'X\r')
                        chat.wait(b'1 staged for next turn', start)
                        chat.wait(ENABLE, start)
                        start = chat.send(b'/attachments-clear\r')
                        chat.wait(b'cleared', start)
                        chat.wait(ENABLE, start)
                        # Real native fragments, including byte-split UTF-8, keep
                        # their authored lines at every width and after resize.
                        flow = []
                        for request, resized in ((b'FORMAT_WHOLE', columns), (b'FORMAT_BYTES', 24), (b'FORMAT_WHOLE', 200)):
                            fcntl.ioctl(chat.slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, resized, 0, 0))
                            os.kill(chat.process.pid, signal.SIGWINCH)
                            chat.quiet()
                            start = chat.send(request + b'\r')
                            chat.wait(ENABLE, start)
                            raw = bytes(chat.data[start:])
                            text = re.sub(rb'\x1b\[[0-9;]*m', b'', raw).replace(b'\r\n', b'\n')
                            text = text[text.index(b'FORMAT BEGIN'):text.index(b'FORMAT END')]
                            identifier = b'identifier_' + b'abcdefghijklmnopqrstuvwxyz_' * 5 + b'abcdefghijklmnopqrstuvwxyz'
                            assert identifier in text, 'identifier physically hard-wrapped'
                            assert b'Spacing:    alpha   bold words   omega.' in text
                            flow.append(text)
                            (output / f'flow-{columns}-{resized}-{request.decode()}.txt').write_bytes(text)
                        assert flow[0] == flow[1] == flow[2], 'resize/fragmentation changed logical content'
                        start = chat.send(b'PROGRESSIVE_STREAM\r')
                        chat.wait(b'arrive now', start)
                        first_visible_ns = time.monotonic_ns()
                        first_visible = time.monotonic()
                        assert b'finish later' not in transcript(chat.data[start:]), 'client batched separately published fragments'
                        chat.wait(b'finish later', start)
                        print(f'flow first-to-last fragment gap={time.monotonic() - first_visible:.6f}s (fixture separation 0.7s)', flush=True)
                        chat.wait(ENABLE, start)
                        publications = re.findall(r'flow-publication request=(\d+) fragment=0 monotonic_ns=(\d+)', log_path.read_text())
                        request_id, published_ns = publications[-1]
                        delivery_ns = first_visible_ns - int(published_ns)
                        assert 0 <= delivery_ns < 700_000_000, 'first publication waited for the next fragment'
                        (output / f'flow-delivery-{columns}.json').write_text(json.dumps({
                            'scope': 'isolated native protocol fixture to client PTY read; not GPU/model TTFT',
                            'request': request_id, 'fragment': 0,
                            'server_before_send_monotonic_ns': int(published_ns),
                            'client_pty_visible_monotonic_ns': first_visible_ns,
                            'delivery_observation_ns': delivery_ns,
                            'next_fragment_fixture_delay_ns': 700_000_000,
                        }, indent=2) + '\n')
                        start = chat.send(b"/help\r")
                        chat.wait(b"/quit", start)
                        chat.wait(ENABLE, start)
                        for name in (b"/status", b"/sessions", b"/attach", b"/think", b"/cancel"):
                            assert name in chat.data[start:]
                        start = chat.send(b"/help reset\r")
                        chat.wait(b"[name]", start)
                        chat.wait(ENABLE, start)
                        for text in (b"hello", "Ae\u0301界🌍\x1b[D\x7fX".encode()):
                            start = chat.send(text + b"\r")
                            chat.wait(b"hello from yvex", start)
                            chat.wait(ENABLE, start)
                        assert "Ae\u0301X🌍".encode().hex() in log_path.read_text()
                        start = chat.send(b"/s\t")
                        chat.wait(b"/sessions", start)
                        chat.quiet()
                        if columns == 80:
                            # Delay consumption, not terminal protocol expiry: a
                            # sender-side sleep cannot prove the host read Esc.
                            os.kill(chat.process.pid, signal.SIGSTOP)
                        dismiss = chat.send(b"\x1b")
                        if columns == 80:
                            chat.quiet()
                            chat.quiet()
                            os.kill(chat.process.pid, signal.SIGCONT)
                        # REPLAI hides the menu when it consumes the ambiguous
                        # Escape prefix. Start the idle interval from that
                        # observed transition, not from the PTY write above.
                        chat.wait(b"\r\x1b[2K\x1b[1B", dismiss)
                        # This is a semantic control, not a deadline-latency
                        # benchmark. Hosted Darwin observed a 344 ms wake for
                        # a 250 ms deadline; allow scheduler delivery margin
                        # after consumption without changing the producer timer.
                        for _ in range(4):
                            chat.quiet()
                        chat.send(b"\x01\x0b")
                        start = chat.send(b"/think-max\r")
                        chat.wait(b"REASONING", start)
                        chat.wait(ENABLE, start)
                        assert b"unknown or incomplete terminal sequence" not in chat.data[dismiss:]
                        fcntl.ioctl(chat.slave, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 55, 0, 0))
                        started = time.monotonic()
                        start = len(chat.data)
                        os.kill(chat.process.pid, signal.SIGWINCH)
                        chat.wait(b"yvex", start)
                        elapsed = time.monotonic() - started
                        assert elapsed < 0.2, "driven resize redraw stalled"
                        print(f"resize width={columns}->55 observed_redraw={elapsed:.6f}s", flush=True)
                        for request, marker in ((b"WAIT_PREFILL_CANCEL", b"prefill"),
                                                (b"WAIT_DECODE_CANCEL", b"prefill"),
                                                (b"WAIT_REASONING_CANCEL", b"reasoning before cancellation")):
                            start = chat.send(request + b"\r")
                            chat.wait(marker, start)
                            chat.send(b"\x03")
                            chat.wait(ENABLE, start)
                            assert b"cancellation admitted" in transcript(chat.data[start:])
                            assert b"DELIVERY INDETERMINATE" not in chat.data[start:], "cancel response became lost delivery"
                        early_identity = b"WAIT_EARLY_CANCEL".hex()
                        before_early = log_path.read_text().count(early_identity)
                        start = chat.send(b"WAIT_EARLY_CANCEL\r")
                        deadline = time.monotonic() + 5
                        while log_path.read_text().count(early_identity) == before_early:
                            assert time.monotonic() < deadline, "request not dispatched"
                            chat.pump()
                        chat.send(b"\x03")
                        chat.wait(ENABLE, start)
                        assert b"cancellation admitted" in transcript(chat.data[start:])
                        assert log_path.read_text().count(
                            f"generation.cancel {chat.name}\n") == 4, "cancel dispatch was missing or repeated"
                        start = chat.send(b"NATIVE_LOST_REPLY\r")
                        chat.wait(ENABLE, start)
                        assert b"DELIVERY INDETERMINATE" in transcript(chat.data[start:])
                        assert log_path.read_text().count(b"NATIVE_LOST_REPLY".hex()) == (1 if columns == 40 else 2 if columns == 80 else 3), "lost request was retried"
                        start = chat.send(b"hello\r")
                        chat.wait(b"hello from yvex", start)
                        chat.wait(ENABLE, start)
                        assert b"RECONNECTED" in transcript(chat.data[start:])
                        start = chat.send(b"REASONING_STREAM\r")
                        chat.wait(ENABLE, start)
                        channels = transcript(chat.data[start:])
                        assert b"REASONING" in channels and b"ANSWER" in channels
                        assert b"<think>" not in channels and b"</think>" not in channels
                        start = chat.send(b"PARTIAL_REASONING\r")
                        chat.wait(b"REASONING", start)
                        chat.wait(ENABLE, start)
                        assert b"reset required" in transcript(chat.data[start:])
                        start = chat.send(b"/reset\r")
                        chat.wait(b"SESSION", start)
                        chat.wait(ENABLE, start)
                        start = chat.send(b"hello\r")
                        chat.wait(b"hello from yvex", start)
                        chat.wait(ENABLE, start)
                        start = chat.send(b"/status\r")
                        chat.wait(b"SESSION", start)
                        chat.wait(ENABLE, start)
                        chat.finish(b"/quit\r")
                    finally:
                        chat.dispose()
                print("PASS native Rust chat: three widths, plain/styled, Unicode, grouped/detail slash help, rich completion, driven resize, cancellation before admission and in three phases, lost-response/no-retry/resynchronization, exact reasoning/final channels, partial-state recovery, restoration")
            finally:
                if host.poll() is None:
                    host.terminate()
                    assert host.wait(timeout=10) == 0
                if previous is None:
                    os.environ.pop("XDG_RUNTIME_DIR", None)
                else:
                    os.environ["XDG_RUNTIME_DIR"] = previous


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    run(args.binary.resolve(), args.fixture.resolve(), args.output.resolve())
