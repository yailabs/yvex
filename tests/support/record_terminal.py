#!/usr/bin/env python3
"""Record a real PTY from pipe/FIFO input on Darwin, preserving the child's exit status."""
import errno
import fcntl
import os
from pathlib import Path
import pty
import select
import subprocess
import sys
import termios


def record(transcript, command):
    master, slave = pty.openpty()
    process = subprocess.Popen(['/bin/sh', '-c', command], stdin=slave, stdout=slave,
        stderr=slave, start_new_session=True,
        preexec_fn=lambda: fcntl.ioctl(0, termios.TIOCSCTTY, 0))
    os.close(slave)
    input_open = True
    try:
        with transcript.open('wb', buffering=0) as output:
            while True:
                readable, _, _ = select.select([master, *([0] if input_open else [])], [], [], .05)
                if master in readable:
                    try:
                        data = os.read(master, 65536)
                    except OSError as error:
                        if error.errno != errno.EIO:
                            raise
                        data = b''
                    if not data:
                        break
                    output.write(data)
                    sys.stdout.buffer.write(data)
                    sys.stdout.buffer.flush()
                if input_open and 0 in readable:
                    data = os.read(0, 4096)
                    if not data:
                        input_open = False
                        data = b'\x04'
                    while data:
                        written = os.write(master, data)
                        data = data[written:]
                if process.poll() is not None and not readable:
                    break
        return process.wait(timeout=10)
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        os.close(master)


if __name__ == '__main__':
    raise SystemExit(record(Path(sys.argv[1]), sys.argv[2]))
