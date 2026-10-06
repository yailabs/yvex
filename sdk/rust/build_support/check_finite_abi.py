#!/usr/bin/env python3
"""Check the finite SDK projection against installed public YVEX declarations.

No producer source, protocol implementation or live inference is consumed.
Declaration hashing follows YVEX's public ABI guard; comments are irrelevant.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def compare(include, projection):
    text = (include / projection["header"]).read_text()
    text = re.sub(r"/\*.*?\*/|//[^\n]*", " ", text, flags=re.DOTALL)
    macros = dict(re.findall(r"#define\s+(YVEX_FINITE_PRODUCER_\w+)\s+(\d+)u?", text))
    assert int(macros["YVEX_FINITE_PRODUCER_SCHEMA_V1"]) == projection["producer_schema"]
    for key, value in projection["limits"].items():
        assert int(macros["YVEX_FINITE_PRODUCER_" + key]) == value, f"finite limit drift: {key}"
    for group, kind in (("records", "struct"), ("enums", "enum")):
        for name, record in projection[group].items():
            source = (include / record.get("header", projection["header"])).read_text()
            source = re.sub(r"/\*.*?\*/|//[^\n]*", " ", source, flags=re.DOTALL)
            records = dict((name, body) for body, name in re.findall(
                r"typedef\s+" + kind + r"\s*\{(.*?)\}\s*([A-Za-z_]\w*)\s*;", source, flags=re.DOTALL))
            tokens = re.findall(r"[A-Za-z_]\w*|\d+[A-Za-z0-9_]*|[^\s]", records[name])
            actual = hashlib.sha256(" ".join(tokens).encode()).hexdigest()
            assert actual == record["declaration_sha256"], f"finite declaration drift: {name}"
    core = (include / "yvex/core.h").read_text()
    for name, value in projection["error_limits"].items():
        assert int(re.search(r"#define\s+" + name + r"\s+(\d+)", core).group(1)) == value, f"finite error ABI drift: {name}"
    assert re.search(r"int\s+" + projection["symbol"] + r"\s*\(\s*const\s+char\s*\*\s*socket_path\s*,\s*const\s+yvex_finite_producer_request\s*\*\s*request\s*,\s*yvex_finite_producer_result\s*\*\s*result\s*,\s*yvex_error\s*\*\s*err\s*\)\s*;", text), "finite symbol signature drift"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--include-dir", required=True, type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    projection = json.loads((ROOT / "contract/finite.json").read_text())
    compare(args.include_dir, projection)
    # Controlled client delta proves silent field/limit drift fails closed.
    negative = json.loads(json.dumps(projection))
    negative["limits"]["MAX_CANDIDATES"] += 1
    try:
        compare(args.include_dir, negative)
    except AssertionError:
        pass
    else:
        raise AssertionError("finite negative delta was accepted")
    print("PASS finite public ABI projection; negative limit delta refused; no runtime claim")


if __name__ == "__main__":
    main()
