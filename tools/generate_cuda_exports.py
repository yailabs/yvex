#!/usr/bin/env python3
"""Derive CUDA symbol/module ownership from the actual compiled PTX images."""

import re
import sys
from pathlib import Path


def exports(images):
    records = {}
    if not images:
        raise ValueError("CUDA export manifest requires at least one module")
    for ordinal, image in enumerate(images):
        text = re.sub(r"/\*.*?\*/|//[^\n]*", "", image, flags=re.S)
        entries = re.findall(r"\.entry\s+([^\s(]+)\s*\(", text)
        if not entries or len(entries) != len(re.findall(r"\.entry\b", text)):
            raise ValueError("CUDA module has missing or malformed entry declarations")
        for name in entries:
            if not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", name):
                raise ValueError("CUDA entry has an unsupported symbol spelling")
            if name in records:
                raise ValueError("CUDA symbol has more than one module owner: " + name)
            records[name] = ordinal
    lines = ["/* Generated from compiled module exports; no runtime probing. */",
             "static const struct { const char *symbol; unsigned int module; }",
             "cuda_kernel_exports[] = {"]
    lines.extend(f'    {{"{name}", {ordinal}u}},' for name, ordinal in sorted(records.items()))
    lines.extend(["};", f"#define CUDA_KERNEL_EXPORT_MODULE_COUNT {len(images)}u", ""])
    return "\n".join(lines)


if __name__ == "__main__":
    try:
        print(exports([Path(path).read_text(encoding="utf-8") for path in sys.argv[1:]]), end="")
    except (OSError, ValueError) as error:
        sys.exit(str(error))
