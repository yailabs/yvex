#!/usr/bin/env python3
"""Match a human field independent of alignment, not machine JSON semantics.

Literal expectations still match literally. Reassemble only value-column
continuations, removing the renderer's hanging gutter, not content bytes.
Do not normalize values, identities, JSON, case or ordering. --regex retains
explicit regular-expression field assertions used by engineering fixtures.
"""
import pathlib
import re
import sys

regex = sys.argv[1] == "--regex"
args = sys.argv[2:] if regex else sys.argv[1:]
content = pathlib.Path(args[0]).read_text(encoding="utf-8")
wanted = args[1]
if (re.search(wanted, content) if regex else wanted in content):
    raise SystemExit(0)
field = re.fullmatch(r"(.+?): ?(.*)", wanted)
if not field:
    lines = content.splitlines()
    parent = None
    if wanted.startswith('yvex '):
        for line in lines:
            heading = re.fullmatch(r"  (yvex(?: .*)?)", line)
            if heading:
                parent = heading[1]
            elif parent and re.match(r"^    [a-z]", line):
                combined = parent + ' ' + line.strip()
                if combined.startswith(wanted) and (
                    len(combined) == len(wanted) or combined[len(wanted)].isspace()
                ):
                    raise SystemExit(0)
    for index, line in enumerate(lines):
        match = re.fullmatch(r"[ \t]*[A-Za-z_][A-Za-z0-9_. -]*?(?:[ \t]*:[ \t]*|[ \t]{2,})(.*)", line)
        if not match:
            continue
        actual, column = match[1], match.start(1)
        for continuation in lines[index + 1:]:
            if column and continuation.startswith(' ' * column) and continuation.strip():
                actual += continuation[column:]
            else:
                break
        if (re.search(wanted, actual) if regex else wanted in actual):
            raise SystemExit(0)
    raise SystemExit(1)
label, value = field.groups()
label_pattern = label if regex else re.escape(label)
value_pattern = value if regex else re.escape(value)
lines = content.splitlines()
for index, line in enumerate(lines):
    match = re.fullmatch(r"[ \t]*" + label_pattern + r"(?:[ \t]*:[ \t]*|[ \t]{2,})(.*)", line)
    if not match:
        continue
    actual = match[1]
    column = match.start(1)
    for continuation in lines[index + 1:]:
        if column and continuation.startswith(' ' * column) and continuation.strip():
            actual += continuation[column:]
        else:
            break
    if not value or re.fullmatch(value_pattern, actual):
        raise SystemExit(0)
raise SystemExit(1)
