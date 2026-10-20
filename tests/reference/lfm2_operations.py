#!/usr/bin/env python3
"""Independent, bounded CPU Liquid MoE oracle from a pinned upstream class.

Synthetic public operands exercise the complete routing/expert algorithm, not
checkpoint quality. The upstream file and all generated arrays stay outside Git.
No accelerator, model load, random parameters or installed services are used.
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
import torch.nn.functional as F

IMPLEMENTATION = "ddb849abe009d1089e6c691bfc897f27211c663c"
IMPLEMENTATION_SHA = "535002e934c65ce7295b8e9046be0dc551a4dd13f82876f706f905a6ddb84b21"


def main():
    if len(sys.argv) != 3:
        raise ValueError("pinned upstream modeling file and new fixture path required")
    upstream, target = map(pathlib.Path, sys.argv[1:])
    code = upstream.read_bytes()
    if hashlib.sha256(code).hexdigest() != IMPLEMENTATION_SHA:
        raise ValueError("upstream implementation digest mismatch")
    names = {"Lfm2MoeExperts", "Lfm2MoeSparseMoeBlock"}
    selected = [n for n in ast.parse(code).body if isinstance(n, ast.ClassDef) and n.name in names]
    if {n.name for n in selected} != names:
        raise ValueError("missing source algorithm")
    for node in selected:
        node.decorator_list = []  # Disable optional kernel replacement, retain source methods.
    namespace = {"torch": torch, "nn": torch.nn, "F": F, "Lfm2MoeConfig": object}
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(upstream), "exec"), namespace)
    torch.set_num_threads(2)
    torch.use_deterministic_algorithms(True)
    rows, hidden, intermediate, experts, selected_count = 7, 16, 32, 8, 3
    config = types.SimpleNamespace(num_experts=experts, hidden_size=hidden,
        moe_intermediate_size=intermediate, num_experts_per_tok=selected_count,
        routed_scaling_factor=1.0, norm_topk_prob=True, use_expert_bias=True)

    def dyadic(shape, factor, modulus, denominator):
        count = 1
        for size in shape:
            count *= size
        return (((torch.arange(count, dtype=torch.int64) * factor) % modulus -
                 modulus // 2).float() / denominator).reshape(shape).to(torch.bfloat16)

    inputs = dyadic((rows, hidden), 13, 31, 16)
    logits = dyadic((rows, experts), 7, 23, 4)
    # Bias changes selection but must not enter mixture normalization.
    bias = torch.arange(experts, dtype=torch.float32) * 0.017 - 0.05
    gate = dyadic((experts, intermediate, hidden), 17, 29, 64)
    up = dyadic((experts, intermediate, hidden), 11, 23, 64)
    down = dyadic((experts, hidden, intermediate), 19, 37, 64)
    with torch.device("meta"):
        block = namespace["Lfm2MoeSparseMoeBlock"](config)
    block.expert_bias = bias
    block.experts.gate_up_proj = torch.nn.Parameter(torch.cat((gate, up), 1), requires_grad=False)
    block.experts.down_proj = torch.nn.Parameter(down, requires_grad=False)
    with torch.no_grad():
        indices, weights = block.route_tokens_to_experts(logits)
        scores = logits.sigmoid().float() + bias
        ordered = scores.sort(-1, descending=True).values
        if (ordered[:, selected_count - 1] == ordered[:, selected_count]).any():
            raise ValueError("fixture has ambiguous boundary")
        output = block.experts(inputs, indices, weights)
        single = torch.cat([block.experts(inputs[r:r + 1], indices[r:r + 1], weights[r:r + 1])
                            for r in range(rows)])
        if not torch.equal(output, single):
            raise ValueError("fixture batch/single arithmetic differs")
    arrays = [inputs, logits, bias, gate, up, down, output]
    with target.open("x") as handle:
        handle.write(f"{rows} {hidden} {intermediate} {experts} {selected_count}\n")
        for array in arrays:
            for value in array.float().flatten().tolist():
                handle.write(f"{value:.9g}\n")
    print(json.dumps({
        "oracle": "unmodified Lfm2MoeSparseMoeBlock.route_tokens_to_experts and Lfm2MoeExperts.forward",
        "implementation_revision": IMPLEMENTATION, "implementation_sha256": IMPLEMENTATION_SHA,
        "torch": torch.__version__, "device": "cpu", "threads": 2,
        "rows": rows, "hidden": hidden, "intermediate": intermediate, "experts": experts,
        "selected": selected_count, "fixture_sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
        "routing_indices": indices.tolist(), "routing_weights": weights.float().tolist(),
        "checkpoint_quality": False, "whole_model": False, "cuda_executed": False,
    }, sort_keys=True))


if __name__ == "__main__":
    main()
