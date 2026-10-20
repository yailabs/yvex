#!/usr/bin/env python3
"""Pinned original Liquid tokenizer oracle; no model execution or private text."""
import hashlib
import json
import pathlib
import random
import sys

import tokenizers

TOKENIZER_SHA = "695be7802a0e4b8a81048f0ff5ebb7fc811a0ba5a6be63dbb24deb5a81096f41"
REVISION = "1769516ac588c4c558089c94f27d96b0dc67b183"


def main():
    if len(sys.argv) != 3:
        raise ValueError("exact source tokenizer.json and new fixture path required")
    source, destination = map(pathlib.Path, sys.argv[1:])
    raw = source.read_bytes()
    if hashlib.sha256(raw).hexdigest() != TOKENIZER_SHA:
        raise ValueError("original checkpoint tokenizer identity differs")
    oracle = tokenizers.Tokenizer.from_file(str(source))
    corpus = [
        "", "Hello, Liquid!", " a  b\tc\n", "one\r\ntwo\n\nthree", "1234567890 1.25 -3e8",
        "def answer(x):\n    return x + 1\n", '{"value":true,"name":"LFM"}',
        "Città, perché? 日本語 中文 العربية", "🙂🚀 e\u0301 é", "\x00A\x01B",
        "<|startoftext|>", "<|im_start|>user\nHello<|im_end|>",
        "end   ", "\n" * 17, "ab" * 128,
    ]
    corpus += [
        "I'm I'M we'd WE'RE he's she's doesn't 're 'VE", "漢字abcかなカナAZ",
        "a\u0301b\u0308c", "\x1c\x1d\x1e\x1fAZ", "\n \tword", "  \n  ",
        "x\u200dy \ufeffz", "ΑΒΓ\u0301δεж中123٤٥٦７８９", "🙂abc!é",
    ]
    corpus += ["\n" * n for n in range(1, 41)]
    rng = random.Random(20261020)
    alphabet = list("abCD '19!?\n\r\t") + list("é漢かな🙂") + ["\u0301", "\x1c"]
    corpus += ["".join(rng.choices(alphabet, k=80)) for _ in range(64)]
    arrays = []
    for text in corpus:
        ids = oracle.encode(text, add_special_tokens=False).ids
        decoded = oracle.decode(ids, skip_special_tokens=False).encode()
        arrays.append((text.encode(), ids, decoded))
    with destination.open("x") as out:
        out.write(f"yvex.lfm2.tokenizer.reference.v1 {TOKENIZER_SHA} {len(arrays)}\n")
        for text, ids, decoded in arrays:
            out.write(f"{len(text)} {len(ids)} {len(decoded)}\n")
            out.write(text.hex() + "\n")
            out.write(" ".join(map(str, ids)) + "\n")
            out.write(decoded.hex() + "\n")
    print(json.dumps({
        "checkpoint": "LiquidAI/LFM2.5-24B-A2B", "revision": REVISION,
        "tokenizer_sha256": TOKENIZER_SHA, "tokenizers_version": tokenizers.__version__,
        "fixture_sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
        "cases": len(arrays), "encode_special_insertion": False, "decode_skip_special": False,
        "whole_model": False, "gpu_executed": False,
    }, sort_keys=True))


if __name__ == "__main__":
    main()
