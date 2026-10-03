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

def report_fields(path):
    """Decode the test presentation oracle, never an operator/machine API."""
    fields = {}
    name = None
    column = 0
    for line in pathlib.Path(path).read_text(encoding="utf-8").splitlines():
        if name is not None and column and line.startswith(' ' * column) and line.strip():
            fields[name] += line[column:]
            continue
        match = re.fullmatch(r"[ \t]*([a-z][a-z0-9_.-]*)(?:[ \t]*:[ \t]*|[ \t]{2,})(.*)", line)
        name = match[1] if match else None
        if name is not None:
            assert name not in fields, (path, 'duplicate field', name)
            fields[name] = match[2]
            column = match.start(2)
    return fields

if sys.argv[1] == '--same-fields':
    expected = report_fields(sys.argv[2])
    observed = report_fields(sys.argv[3])
    assert expected, ('empty presentation oracle', sys.argv[2])
    assert expected == observed, (sys.argv[2:], expected, observed)
    raise SystemExit(0)

if sys.argv[1] == '--mapping':
    fields = report_fields(sys.argv[2])
    native, canonical = sys.argv[3:5]
    prefixes = [key.removesuffix('.native_name') for key, value in fields.items()
                if key.endswith('.native_name') and value == native]
    assert len(prefixes) == 1, (sys.argv[2], native, 'mapping absent or ambiguous')
    assert fields.get(prefixes[0] + '.canonical_name') == canonical, (native, canonical, fields)
    raise SystemExit(0)

arguments = sys.argv[1:]
record = None
if arguments[0] == '--record':
    record, arguments = arguments[1], arguments[2:]
regex = arguments[0] == "--regex"
args = arguments[1:] if regex else arguments
content = pathlib.Path(args[0]).read_text(encoding="utf-8")
if record is not None:
    # Scoped record assertions preserve identity/value association after a
    # deliberate row-to-record presentation migration.
    blocks = re.split(r'(?m)^(?=ARTIFACT  )', content)
    matches = [block for block in blocks if block and block.splitlines()[0] == 'ARTIFACT  ' + record]
    assert len(matches) == 1, (args[0], record, 'record absent or ambiguous')
    content = matches[0]
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
