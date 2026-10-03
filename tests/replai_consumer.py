#!/usr/bin/env python3
"""Observe the production chat process across the installed REPLAI boundary."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import pty
import select
import struct
import subprocess
import termios
import time
import tempfile
import sys
import platform
import re

ENABLE = b'\x1b[?2004h'
DISABLE = b'\x1b[?2004l'
LABEL = b'deepseek4-v4-flash-dspark'


class Chat:
    def __init__(self, binary, name, output, *, plain=False, dumb=False, memcheck=None, model=None, columns=100):
        self.master, self.slave = pty.openpty()
        self.name, self.output = name, output
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, columns, 0, 0))
        original = termios.tcgetattr(self.slave)
        original[0] ^= termios.IXOFF
        original[6][termios.VMIN] = 3
        original[6][termios.VTIME] = 7
        termios.tcsetattr(self.slave, termios.TCSANOW, original)
        self.before = termios.tcgetattr(self.slave)
        env = os.environ.copy()
        env.pop('NO_COLOR', None)
        env['TERM'] = 'dumb' if dumb else 'xterm-256color'
        self.termios_receipt = output / (name + '.termios.json')
        if sys.platform == 'darwin':
            probe = Path(env['YVEX_TEST_TERMIOS_PROBE'])
            assert probe.is_file(), 'Darwin terminal observation probe is required'
            env['DYLD_INSERT_LIBRARIES'] = str(probe)
            env['YVEX_TEST_TERMIOS_RECEIPT'] = str(self.termios_receipt)
        if plain: env['NO_COLOR'] = ''
        command = [str(binary), 'chat', '--session', name]
        if model: command += ['--model', model, '--max-new-tokens', '3']
        self.memlog = output / (name + '.memcheck')
        if memcheck:
            command = [memcheck, '--leak-check=full', '--show-leak-kinds=definite,indirect',
                       '--errors-for-leak-kinds=definite,indirect', '--error-exitcode=99',
                       '--log-file=' + str(self.memlog), *command]
        self.process = subprocess.Popen(command, stdin=self.slave, stdout=self.slave, stderr=self.slave,
            env=env, start_new_session=True, preexec_fn=lambda: fcntl.ioctl(0, termios.TIOCSCTTY, 0))
        self.data = bytearray()
        self.wait(ENABLE)
        self.quiet()
        assert self.before != termios.tcgetattr(self.slave)
        self.fd_baseline = self.tty_fds()
        assert self.fd_baseline == 5, self.fd_baseline  # caller 0/1/2 and two library duplicates

    def pump(self, timeout=0.02):
        if select.select([self.master], [], [], timeout)[0]:
            self.data.extend(os.read(self.master, 65536))

    def quiet(self):
        until = time.monotonic() + 0.15
        while time.monotonic() < until: self.pump()

    def wait(self, needle, start=0):
        deadline = time.monotonic() + 20
        # SGR span boundaries are not a transcript contract. Terminal lifecycle
        # assertions still observe exact escape bytes; text observes content.
        def contains():
            view = bytes(self.data[start:])
            if b'\x1b' not in needle:
                view = re.sub(rb'\x1b\[[0-9;]*m', b'', view)
            return needle in view
        while not contains():
            if time.monotonic() > deadline or self.process.poll() is not None:
                raise AssertionError((needle, bytes(self.data[-6000:])))
            self.pump()
        return bytes(self.data[start:])

    def send(self, data):
        if isinstance(data, str): data = data.encode()
        start = len(self.data)
        assert os.write(self.master, data) == len(data)
        return start

    def tty_fds(self):
        target = os.ttyname(self.slave)
        if sys.platform == 'darwin':
            descriptors = subprocess.check_output(
                ['lsof', '-a', '-p', str(self.process.pid), '-Ffn'], text=True)
            return sum(line == 'n' + target for line in descriptors.splitlines())
        count = 0
        for descriptor in Path(f'/proc/{self.process.pid}/fd').iterdir():
            try: count += os.readlink(descriptor) == target
            except FileNotFoundError: pass
        return count

    def submit(self, data, expected, host_log):
        start = self.send(data + b'\r')
        self.wait(ENABLE, start)
        self.quiet()
        wanted = f'replai-input {self.name} {expected.hex()}\n'
        assert wanted in host_log.read_text(), (wanted, host_log.read_text()[-2000:])
        self.wait(b'hello from yvex', start)
        assert self.tty_fds() == self.fd_baseline
        print(f'chat input={expected.hex()} streamed=hello from yvex next_prompt=1 tty_fds=5', flush=True)

    def finish(self, exit_input=b'\x04'):
        self.send(exit_input)
        deadline = time.monotonic() + 20
        while self.process.poll() is None and time.monotonic() < deadline: self.pump()
        assert self.process.wait(timeout=1) == 0
        self.quiet()
        if sys.platform == 'darwin':
            expected = [*self.before[:6],
                        [v if isinstance(v, int) else v[0] for v in self.before[6]]]
            assert json.loads(self.termios_receipt.read_text()) == expected, 'captured termios not restored'
        else:
            assert termios.tcgetattr(self.slave) == self.before, 'captured termios not restored'
        assert self.data.count(ENABLE) == self.data.count(DISABLE)
        for forbidden in [b'\x1b[?1049h', b'\x1b[48;', b'\x1b[40m']:
            assert forbidden not in self.data
        if self.memlog.exists():
            memory = self.memlog.read_text()
            assert 'ERROR SUMMARY: 0 errors' in memory, memory
            print('\n'.join(line for line in memory.splitlines() if any(tag in line for tag in
                  ['in use at exit', 'definitely lost', 'indirectly lost', 'ERROR SUMMARY'])), flush=True)
        print(f'{self.name}: termios before==after; paste balanced; exit=0', flush=True)

    def dispose(self):
        (self.output / (self.name + '.typescript')).write_bytes(self.data)
        if self.process.poll() is None:
            self.process.kill(); self.process.wait()
        os.close(self.master); os.close(self.slave)


def dependency_rejections(root, pin):
    # Rejected preparations must neither download/build nor replace an existing prefix.
    with tempfile.TemporaryDirectory(prefix='yvex-terminal-pin-') as temp:
        prefix = Path(temp) / 'prefix'
        prefix.mkdir()
        sentinel = prefix / 'keep'
        sentinel.write_text('caller-owned')
        command = ['python3', str(root / 'tools/prepare_replai.py'), '--prefix', str(prefix)]
        def rejected(expected, arguments=()):
            result = subprocess.run(command + list(arguments), text=True, capture_output=True)
            assert result.returncode != 0 and expected in result.stderr, result
            assert sentinel.read_text() == 'caller-owned'
        rejected('without a verified build receipt')
        rejected('exact pinned revision', ['--source', str(root)])
        receipt = prefix / 'replai-build.json'
        receipt.write_text(json.dumps({'pin': dict(pin, abi=999)}))
        rejected('incompatible REPLAI prefix')
        receipt.write_text(json.dumps({'pin': pin,
            'target': {'system': platform.system(), 'machine': platform.machine()},
            'sha256': {'include/replai.h': '0' * 64}}))
        rejected('No such file')
        (prefix / 'include').mkdir()
        (prefix / 'include/replai.h').write_text('incompatible header')
        rejected('artifact integrity failure')
    print('dependency rejection: wrong revision, stale ABI receipt, missing/tampered artifacts; prefix preserved', flush=True)


def audit(binary):
    root = Path(__file__).resolve().parents[1]
    source_root = root / 'build/external/replai-source'
    pin = json.loads((root / 'config/replai.json').read_text())
    receipt = json.loads(source_root.with_suffix('.json').read_text())
    assert receipt['pin'] == pin
    dependency_rejections(root, pin)
    symbols = subprocess.check_output(['nm', str(binary)], text=True)
    if sys.platform == 'darwin':
        symbols = symbols.replace(' _', ' ').replace('\n_', '\n').lstrip('_')
    for symbol in ['replai_create', 'replai_prompt_composed', 'replai_open',
                   'replai_abi_version', 'yvex_cli_terminal_editor_open']:
        assert not any(line.split()[-1] == symbol for line in symbols.splitlines()), symbol
    assert 'replai' in symbols, 'native Rust terminal producer absent'
    loader = subprocess.check_output(
        ['otool', '-L', str(binary)] if sys.platform == 'darwin' else ['ldd', str(binary)], text=True)
    assert 'libreplai' not in loader
    source = (root / 'src/cli/rust/chat.rs').read_text()
    for retired in ['repl_read_line(', 'repl_redraw(', 'repl_insert_byte(',
                    'repl_escape_read(', 'repl_columns(', 'repl_erase(']:
        assert retired not in source
    assert '\\033[?2004' not in source
    print(f'product linkage: native Rust REPLAI revision={pin["revision"]}; no C adapter/editor', flush=True)


def reply_format(binary, output, memcheck):
    import re
    import unicodedata
    for plain, columns in ((False, 100), (True, 100), (True, 40)):
        c = Chat(binary, 'replai-format', output, plain=plain, memcheck=memcheck, columns=columns)
        try:
            rendered = []
            for request in (b'FORMAT_WHOLE', b'FORMAT_BYTES'):
                start = c.send(request + b'\r')
                c.wait(ENABLE, start); c.quiet()
                raw = bytes(c.data[start:])
                plain_reply = re.sub(rb'\x1b\[[0-9;]*m', b'', raw)
                reply = raw
                if not plain:
                    assert b'\x1b[1;38;5;250m' in reply
                    # REPLAI styles semantic spans; escape run coalescing is not
                    # an API. Every strongly emphasized scalar retains its role.
                    assert b'\x1b[1;38;5;250mC' in reply, reply
                text = plain_reply[plain_reply.index(b'FORMAT BEGIN'):plain_reply.index(b'FORMAT END')].decode().replace('\r\n', '\n')
                assert '**' not in text, text
                assert '你指的是C.I.A.A.吗？' in text, text
                assert 'Spacing: alpha bold words omega.' in text, text
                assert '👩‍💻' in text and '👍🏽' in text and '🇮🇹' in text, text
                assert '  • CIA' in text, text
                assert any(line.startswith('    ') for line in text.splitlines()), text
                assert '\n\n\n' not in text, text
                for line in text.splitlines():
                    cells = sum(0 if unicodedata.combining(ch) else
                                2 if unicodedata.east_asian_width(ch) in ('W', 'F') else 1
                                for ch in line)
                    assert cells <= min(96, columns - 2), (cells, line)
                rendered.append(text)
            assert rendered[0] == rendered[1], rendered
            c.finish()
            print(f'reply: whole == byte fragments; cells <= {min(96, columns - 2)}; inline bold/bullets/spacing preserved', flush=True)
        finally: c.dispose()


def run(binary, host_log, output, memcheck):
    reply_format(binary, output, memcheck)
    for columns, plain in ((40, True), (80, False), (180, True)):
        c = Chat(binary, f'replai-help-{columns}', output, plain=plain,
                 memcheck=memcheck, columns=columns)
        try:
            start = c.send(b'/help\r'); c.wait(b'Keyboard', start); c.wait(ENABLE, start); c.quiet()
            raw = bytes(c.data[start:])
            for group in (b'OBSERVATION', b'REASONING', b'CONTENT', b'SESSIONS', b'LIFECYCLE'):
                assert group in raw, (columns, group, raw)
            for operation in (b'/status', b'/think', b'/attach', b'/use', b'/cancel', b'/quit'):
                assert operation in raw, (columns, operation)
            start = c.send(b'hello\r'); c.wait(b'hello from yvex', start); c.wait(ENABLE, start)
            c.finish()
        finally: c.dispose()
    for name, plain, dumb in [('styled', False, False), ('plain', True, False), ('dumb', False, True)]:
        c = Chat(binary, 'replai-' + name, output, plain=plain, dumb=dumb, memcheck=memcheck)
        try:
            import re
            plain_prompt = re.sub(rb'\x1b\[[0-9;]*m', b'', bytes(c.data))
            assert ('yvex · replai-' + name + '> ').encode() in plain_prompt
            assert b'/attachments-clear' not in c.data  # catalog is demand-driven
            if plain or dumb:
                import re
                assert not re.search(rb'\x1b\[[0-9;]*m', c.data)
            c.finish()
        finally: c.dispose()
    c = Chat(binary, 'replai-edit', output, plain=True, memcheck=memcheck)
    try:
        # Combining mark deletion is a grapheme operation; protocol observes exact bytes.
        c.submit('Aé界🌍\x1b[D\x7fX'.encode(), 'AéX🌍'.encode(), host_log)
        c.submit(b'draft\x1b[D\x1b[A\x1b[BX', b'drafXt', host_log)
        c.submit(b'abc\x04\x01\x04', b'bc', host_log)
        c.submit('\x1b[200~é\r\n界\x1b[201~'.encode(), 'é\n界'.encode(), host_log)
        assert b'... ' in c.data
        start = c.send(b'/sta\t')
        c.wait(b'/status', start)
        c.send(b'\r')  # menu acceptance is not submission
        start = c.send(b'\r'); c.wait(ENABLE, start)
        c.quiet()
        assert b'context' in c.data[start:] and b'hello from yvex' not in c.data[start:]
        # Ambiguous candidates are visible, Esc dismisses without submitting.
        start = c.send(b'/s\t'); c.wait(b'/sessions', start)
        c.send(b'\x1b'); c.quiet(); c.quiet()  # retain the semantic ESC ambiguity deadline
        c.send(b'\x7f\x7f')  # discard the two draft characters, not a product operation
        asset = output / 'completion-local.txt'
        asset.write_text('local qualification fixture')
        path_command = '/attach ' + str(output / 'completion-lo')
        start = c.send(path_command + '\t')
        # A long path label may be ellipsized; accepting must retain the full value.
        c.wait(b'path; attachment admission still required', start)
        c.send(b'\r')  # accept the path, do not admit an attachment yet
        c.wait(b'completion-local.txt', start)
        c.quiet()
        assert b'attached\r\n' not in c.data[start:]
        c.send(b'\x7f' * len(('/attach ' + str(asset)).encode()))
        start = c.send(b'/use replai-\t'); c.wait(b'resident session', start)
        c.send(b'\r\r'); c.wait(ENABLE, start); c.quiet()
        start = c.send('wide 界🌍'.encode()); c.wait('界🌍'.encode(), start)
        fcntl.ioctl(c.slave, termios.TIOCSWINSZ, struct.pack('HHHH', 20, 32, 0, 0))
        import signal
        os.kill(c.process.pid, signal.SIGWINCH)
        c.quiet()
        c.send(b'\x0c'); c.wait(b'\x1b[2J\x1b[H', start)
        c.submit(b'\x1b[D!', 'wide 界!🌍'.encode(), host_log)
        # Editing interrupt never sends a generation cancellation request.
        cancel_before = host_log.read_text().count('generation.cancel replai-edit')
        start = c.send(b'unsubmitted\x03'); c.wait(ENABLE, start); c.quiet()
        assert b'^C' in c.data[start:]
        assert host_log.read_text().count('generation.cancel replai-edit') == cancel_before
        # While generation owns output, only the caller's three TTY FDs remain.
        start = c.send(b'WAIT_PREFILL_CANCEL\r')
        c.wait(b'prefill', start)
        assert c.tty_fds() == 5  # quiet-output producer owns two terminal duplicates
        flags = termios.tcgetattr(c.slave)[3]
        assert flags & termios.ICANON and flags & termios.ISIG and not flags & termios.ECHO
        c.send(b'\x03'); c.wait(b'YVEX_ERR_CANCELLED', start); c.wait(ENABLE, start); c.quiet()
        assert 'generation.cancel replai-edit' in host_log.read_text()
        assert c.tty_fds() == 5
        print('generation transition: editor/quiet scopes each own 2 duplicates; ICANON/ISIG restored; Ctrl-C routed to cancellation', flush=True)
        for index in range(20):
            text = f'repeat-{index}'.encode()
            c.submit(text, text, host_log)
        c.finish()
    finally: c.dispose()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--host-log', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--memcheck')
    args = parser.parse_args()
    audit(args.binary.resolve())
    run(args.binary.resolve(), args.host_log, args.output, args.memcheck)
