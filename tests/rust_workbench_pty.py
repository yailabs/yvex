#!/usr/bin/env python3
"""Real terminal workbench, isolated native fixture and optional tiny CPU runtime.

No GPU, operator socket, catalog, session or resident engine is changed.
"""
import argparse
import fcntl
import os
from pathlib import Path
import re
import signal
import struct
import subprocess
import tempfile
import termios
import time

from replai_consumer import Chat, ENABLE


def action(chat, command, expected):
    start = chat.send(command.encode() + b'\r')
    chat.wait(expected, start)
    chat.wait(ENABLE, start)
    chat.quiet()
    return start


def exercise(binary, output, columns, real=False):
    chat = Chat(binary, f'workbench-{columns}', output, plain=columns != 80,
                columns=columns, arguments=['workbench'])
    try:
        chat.wait(b'Home')
        action(chat, '/models', b'Models')
        action(chat, '/engine/1', b'generation')
        action(chat, '/unload', b'type confirm')
        action(chat, '', b'Unload cancelled before dispatch')
        action(chat, '/inspect', b'workbench')
        action(chat, '/inspect', b'Inspector')
        action(chat, '/compile', b'Compile')
        technique = action(chat, '/techniques', b'source-retention-allocation-v1')
        assert b'REFUSED' not in chat.data[technique:]
        action(chat, '/request', b'optimization request file')
        action(chat, '/missing/界-request.json', b'REFUSED')
        action(chat, '/activity', b'LOCAL ACTION RESULTS')
        action(chat, '/event/1', b'Monotonic ns')
        action(chat, '/home', b'Home')
        # REPLAI owns completion focus and history; escaping a menu preserves
        # the draft. A resize republishes semantic context, never stale padding.
        start = chat.send(b'/mod\t')
        chat.wait(b'/models', start)
        chat.send(b'\x1b')
        chat.quiet()
        for width in (24, 180):
            fcntl.ioctl(chat.slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, width, 0, 0))
            os.kill(chat.process.pid, signal.SIGWINCH)
            chat.quiet()
        action(chat, 'els', b'Models')
        action(chat, '/home', b'Home')
        start = chat.send(('a' if real else 'PROGRESSIVE_STREAM').encode() + b'\r')
        if real:
            chat.wait(b'ANSWER', start)
            chat.wait(b'ok', start)
        else:
            chat.wait(b'arrive now', start)
            assert b'finish later' not in bytes(chat.data[start:]), 'committed stream batched'
            chat.wait(b'finish later', start)
        chat.wait(ENABLE, start)
        if not real:
            # Unicode and cancellation remain the actual chat owner's behavior.
            action(chat, 'Ae\u0301界🌍', b'hello from yvex')
            start = chat.send(b'WAIT_REASONING_CANCEL\r')
            chat.wait(b'reasoning before cancellation', start)
            chat.send(b'\x03')
            chat.wait(b'cancellation admitted', start)
            chat.wait(ENABLE, start)
        action(chat, '/quit', b'YVEX')
        action(chat, '/activity', b'LOCAL ACTION RESULTS')
        idle = len(chat.data)
        time.sleep(0.3)
        chat.quiet()
        assert len(chat.data) == idle, 'idle workbench floods terminal with refreshes'
        chat.finish(b'/quit\r')
        if columns != 80:
            assert not re.search(rb'\x1b\[[0-9;]*m', chat.data), 'NO_COLOR emitted SGR'
        print(f'workbench width={columns}: navigation, focus, resize, refused request, streaming, restoration PASS')
    finally:
        chat.dispose()


def isolated(binary, fixture, output):
    with tempfile.TemporaryDirectory(prefix='yvex-workbench-') as directory:
        root = Path(directory)
        runtime = root / 'runtime'
        socket = runtime / 'yvex/yvexd.sock'
        socket.parent.mkdir(parents=True, mode=0o700)
        settings = {'XDG_RUNTIME_DIR': str(runtime), 'XDG_DATA_HOME': str(root / 'data'),
                    'XDG_CONFIG_HOME': str(root / 'config'), 'YVEX_MODELS_ROOT': str(root / 'models'),
                    'YVEX_MODELS_REGISTRY': str(root / 'registry.json')}
        (root / 'models').mkdir()
        previous = {key: os.environ.get(key) for key in settings}
        os.environ.update(settings)
        host = None
        try:
            piped = subprocess.run([str(binary), 'workbench'], input='', text=True, capture_output=True)
            assert piped.returncode == 2 and 'requires a terminal' in piped.stderr
            disconnected = Chat(binary, 'workbench-offline', output, arguments=['workbench'])
            try:
                disconnected.wait(b'DISCONNECTED')
                action(disconnected, '/chat', b'REFUSED')
                with (output / 'fixture.log').open('wb') as log:
                    host = subprocess.Popen([str(fixture), str(socket)], stdout=log, stderr=log)
                    deadline = time.monotonic() + 10
                    while not socket.exists():
                        assert host.poll() is None and time.monotonic() < deadline
                        time.sleep(0.02)
                action(disconnected, '/refresh', b'Snapshots refreshed')
                assert b'DISCONNECTED' not in disconnected.data[disconnected.data.rfind(b'YVEX'):]
                disconnected.finish()
            finally:
                disconnected.dispose()
            stale = Chat(binary, 'workbench-host-restart', output, arguments=['workbench'])
            try:
                action(stale, '/engine/1', b'generation')
                host.terminate()
                host.wait(timeout=10)
                action(stale, '/refresh', b'DISCONNECTED')
                with (output / 'fixture.log').open('ab') as log:
                    host = subprocess.Popen([str(fixture), str(socket)], stdout=log, stderr=log)
                deadline = time.monotonic() + 10
                while not socket.exists():
                    assert host.poll() is None and time.monotonic() < deadline
                    time.sleep(0.02)
                action(stale, '/refresh', b'Snapshots refreshed')
                action(stale, '/chat', b'REFUSED')
                assert b'SESSION ' not in stale.data, 'old selection followed a restarted host'
                stale.finish()
            finally:
                stale.dispose()
            for width in (40, 80, 180):
                exercise(binary, output, width)
            term = Chat(binary, 'workbench-sigterm', output, arguments=['workbench'])
            try:
                os.kill(term.process.pid, signal.SIGTERM)
                term.finish(b'')
            finally:
                term.dispose()
            confirm = Chat(binary, 'workbench-confirm-sigterm', output, arguments=['workbench'])
            try:
                action(confirm, '/models', b'Models')
                action(confirm, '/engine/1', b'generation')
                action(confirm, '/unload', b'type confirm')
                os.kill(confirm.process.pid, signal.SIGTERM)
                confirm.finish(b'')
            finally:
                confirm.dispose()
        finally:
            if host is not None and host.poll() is None:
                host.terminate()
                host.wait(timeout=10)
            for key, value in previous.items():
                if value is None: os.environ.pop(key, None)
                else: os.environ[key] = value


def retirement(binary, output, refusal):
    chat = Chat(binary, 'workbench-retire-' + str(refusal), output,
                plain=True, arguments=['workbench'])
    try:
        action(chat, '/models', b'Models')
        action(chat, '/engine/1', b'generation')
        action(chat, '/unload', b'type confirm')
        action(chat, 'confirm', b'live sessions or model leases prevent unload' if refusal
               else b'Selected engine retirement returned successfully')
        chat.finish(b'/quit\r')
    finally:
        chat.dispose()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--fixture', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--tiny-runtime', action='store_true')
    parser.add_argument('--retire-runtime', choices=['refuse', 'allow'])
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if args.retire_runtime:
        retirement(args.binary.resolve(), args.output, args.retire_runtime == 'refuse')
    elif args.tiny_runtime:
        exercise(args.binary.resolve(), args.output, 100, real=True)
    else:
        isolated(args.binary.resolve(), args.fixture.resolve(), args.output)
