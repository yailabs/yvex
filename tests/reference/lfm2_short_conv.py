#!/usr/bin/env python3
"""Pinned independent LFM2 component oracle; no model inference or CUDA.

Read original source weights and emit a private, bounded convolution fixture.
Uses the unmodified upstream class from Transformers v5.12.1, loaded from a
hash-checked local source file. No upstream code or weights are copied into Git.
This validates the projected-input convolution boundary, not full-model quality.
"""
import ast
import hashlib
import json
import os
import pathlib
import sys
import types

os.environ["CUDA_VISIBLE_DEVICES"] = ""
import torch
from safetensors import safe_open

REVISION = "1769516ac588c4c558089c94f27d96b0dc67b183"
IMPLEMENTATION = "ddb849abe009d1089e6c691bfc897f27211c663c"
IMPLEMENTATION_SHA = "535002e934c65ce7295b8e9046be0dc551a4dd13f82876f706f905a6ddb84b21"


def main():
    root, upstream, target = map(pathlib.Path, sys.argv[1:4])
    if len(sys.argv) != 4 or REVISION not in root.name:
        raise ValueError("exact acquired source, pinned modeling file and output fixture required")
    code = upstream.read_bytes()
    if hashlib.sha256(code).hexdigest() != IMPLEMENTATION_SHA:
        raise ValueError("upstream implementation digest mismatch")
    tree = ast.parse(code)
    selected = [node for node in tree.body if isinstance(node, ast.ClassDef)
                and node.name == "Lfm2MoeShortConv"]
    if len(selected) != 1:
        raise ValueError("exact upstream convolution class required")
    namespace = {"torch": torch, "nn": torch.nn, "Lfm2MoeConfig": object, "Cache": object,
                 "apply_mask_to_padding_states": lambda value, mask: value if mask is None else None}
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(upstream), "exec"), namespace)
    config_data = json.loads((root / "config.json").read_text())
    if config_data["model_type"] != "lfm2_moe" or config_data["conv_bias"]:
        raise ValueError("unsupported source convolution")
    config = types.SimpleNamespace(**config_data)
    index = json.loads((root / "model.safetensors.index.json").read_text())["weight_map"]

    def tensor(name):
        with safe_open(root / index[name], framework="pt", device="cpu") as source:
            return source.get_tensor(name)

    torch.set_num_threads(2)
    torch.use_deterministic_algorithms(True)
    # No random/full-model weight allocation. Projection is independently
    # computed from nine public token IDs and the real first-layer weights.
    with torch.device("meta"):
        mixer = namespace["Lfm2MoeShortConv"](config, 0)
    mixer.in_proj = torch.nn.Identity()
    mixer.out_proj = torch.nn.Identity()
    weight = tensor("model.layers.0.conv.conv.weight")
    mixer.conv.weight = torch.nn.Parameter(weight, requires_grad=False)
    tokens = [1, 42, 100, 1000, 5000, 13, 23, 7, 91]
    with torch.no_grad():
        # get_slice avoids materializing the 512 MB embedding.
        name = "model.embed_tokens.weight"
        with safe_open(root / index[name], framework="pt", device="cpu") as source:
            embedding = source.get_slice(name)
            hidden = torch.cat([embedding[token:token + 1] for token in tokens])
        norm = tensor("model.layers.0.operator_norm.weight")
        normalized = hidden.float() * torch.rsqrt(hidden.float().square().mean(-1, keepdim=True) + config.norm_eps)
        normalized = norm * normalized.to(hidden.dtype)
        projection = torch.nn.functional.linear(normalized, tensor("model.layers.0.conv.in_proj.weight"))
        output = mixer.slow_forward(projection.unsqueeze(0), past_key_values=None)[0]
        b, c, x = projection.chunk(3, dim=-1)
        bx = b * x
        state = bx[-config.conv_L_cache:].T.contiguous()
        # The upstream cached slow path rounds each product before reducing.
        # Measure its distinction; never silently transfer exact conformance.
        slow = []
        history = torch.zeros_like(state)
        for row in range(len(tokens)):
            history = torch.roll(history, -1, -1)
            history[:, -1] = bx[row]
            slow.append(c[row] * torch.sum(history * weight[:, 0, :], dim=-1))
        slow = torch.stack(slow)
    arrays = [projection, weight, output, state]
    with target.open("x") as handle:
        handle.write(f"{len(tokens)} {config.hidden_size} {config.conv_L_cache}\n")
        for array in arrays:
            for value in array.float().flatten().tolist():
                handle.write(f"{value:.9g}\n")
    print(json.dumps({
        "oracle": "Transformers Lfm2MoeShortConv.slow_forward uncached Conv1d",
        "source_repository": "LiquidAI/LFM2.5-24B-A2B", "revision": REVISION,
        "implementation_revision": IMPLEMENTATION, "implementation_sha256": IMPLEMENTATION_SHA,
        "torch": torch.__version__, "device": "cpu", "threads": 2,
        "tokens": tokens, "rows": len(tokens), "width": config.hidden_size,
        "kernel": config.conv_L_cache, "dtype": "bfloat16",
        "fixture_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
        "cached_slow_vs_uncached_differing_values": int((slow != output).sum()),
        "cached_slow_vs_uncached_max_abs": float((slow.float() - output.float()).abs().max()),
        "whole_model": False, "cuda_executed": False,
    }, sort_keys=True))


if __name__ == "__main__":
    main()
