#!/usr/bin/env python3
"""Project Unicode 15.0.0 General_Category=L into the existing tokenizer owner."""
import argparse
import hashlib
from pathlib import Path

SHA256 = "806e9aed65037197f1ec85e12be6e8cd870fc5608b4de0fffd990f689f376a73"
BEGIN = "/* Generated Unicode 15.0.0 letter ranges: begin. */"
END = "/* Generated Unicode 15.0.0 letter ranges: end. */"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("unicode_data", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    data = args.unicode_data.read_bytes()
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise ValueError("expected exact Unicode 15.0.0 UnicodeData.txt")
    ranges, first = [], None
    for line in data.decode().splitlines():
        fields = line.split(";")
        point = int(fields[0], 16)
        if fields[1].endswith(", First>"):
            first = point
            continue
        start = first if fields[1].endswith(", Last>") else point
        first = None
        if fields[2].startswith("L"):
            if ranges and ranges[-1][1] + 1 == start:
                ranges[-1] = (ranges[-1][0], point)
            else:
                ranges.append((start, point))
    rows = [BEGIN, "/* Source digest: " + SHA256 + "; notice in NOTICE.md. */",
            "static const unicode_range letter_ranges[] = {"]
    for offset in range(0, len(ranges), 4):
        rows.append("    " + " ".join(
            f"{{0x{a:x}u, 0x{b:x}u}}," for a, b in ranges[offset:offset + 4]))
    rows += ["};", "#define LETTER_RANGE_COUNT (sizeof(letter_ranges) / sizeof(letter_ranges[0]))", END]
    target = Path(__file__).resolve().parents[1] / "src/tokenizer/unicode.c"
    text = target.read_text()
    start, end = text.index(BEGIN), text.index(END) + len(END)
    generated = text[:start] + "\n".join(rows) + text[end:]
    if args.check:
        if generated != text:
            raise ValueError("tokenizer letter table is stale")
    else:
        target.write_text(generated)
    print(f"Unicode 15.0.0 letters: {len(ranges)} ranges, source {SHA256}")


if __name__ == "__main__":
    main()
