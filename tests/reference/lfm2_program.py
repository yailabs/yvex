#!/usr/bin/env python3
"""Pinned upstream three-block Liquid program; CPU public fixture, not model quality.

Unmodified source classes from Transformers ddb849abe009d1089e6c691bfc897f27211c663c.
Source and generated operands remain outside Git. No accelerator is used.
"""
from __future__ import annotations
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

UPSTREAM_SHA = "535002e934c65ce7295b8e9046be0dc551a4dd13f82876f706f905a6ddb84b21"


def main():
    upstream, target = map(pathlib.Path, sys.argv[1:])
    code = upstream.read_bytes()
    if hashlib.sha256(code).hexdigest() != UPSTREAM_SHA:
        raise ValueError("pinned upstream implementation mismatch")
    names = {"Lfm2MoeRMSNorm", "Lfm2MoeMLP", "Lfm2MoeExperts", "Lfm2MoeSparseMoeBlock",
             "Lfm2MoeAttention", "Lfm2MoeShortConv", "Lfm2MoeDecoderLayer",
             "rotate_half", "apply_rotary_pos_emb", "repeat_kv",
             "eager_attention_forward", "apply_mask_to_padding_states"}
    nodes = [n for n in ast.parse(code).body if isinstance(n, (ast.ClassDef, ast.FunctionDef))
             and n.name in names]
    if {n.name for n in nodes} != names:
        raise ValueError("missing upstream computation")
    for n in nodes:
        n.decorator_list = []
    future = ast.ImportFrom(module="__future__", names=[ast.alias(name="annotations")], level=0)
    module = ast.fix_missing_locations(ast.Module(body=[future, *nodes], type_ignores=[]))
    namespace = {"torch": torch, "nn": torch.nn, "F": F, "is_fast_path_available": False,
                 "ACT2FN": {"silu": F.silu}, "GradientCheckpointingLayer": torch.nn.Module,
                 "ALL_ATTENTION_FUNCTIONS": types.SimpleNamespace(get_interface=lambda name, fallback: fallback)}
    exec(compile(module, str(upstream), "exec"), namespace)
    torch.set_num_threads(2)
    torch.use_deterministic_algorithms(True)
    cfg = dict(architectures=["Lfm2MoeForCausalLM"], model_type="lfm2_moe", dtype="bfloat16",
               hidden_size=16, num_hidden_layers=3, vocab_size=32, intermediate_size=32,
               moe_intermediate_size=32, num_attention_heads=4, num_key_value_heads=2,
               num_experts=4, num_experts_per_tok=2, num_dense_layers=1,
               max_position_embeddings=128, conv_L_cache=3, bos_token_id=1, eos_token_id=2,
               pad_token_id=0, norm_eps=1e-5, routed_scaling_factor=1.0,
               layer_types=["conv", "full_attention", "conv"],
               rope_parameters=dict(rope_type="default", rope_theta=10000000.0),
               conv_bias=False, norm_topk_prob=True, use_expert_bias=True,
               tie_word_embeddings=True, use_cache=True)
    config = types.SimpleNamespace(**cfg, hidden_act="silu", _attn_implementation="eager")
    with torch.device("meta"):
        layers = torch.nn.ModuleList([namespace["Lfm2MoeDecoderLayer"](config, i) for i in range(3)])
        norm = namespace["Lfm2MoeRMSNorm"](16, eps=1e-5)
    weights = {}

    def operand(name, shape, dtype):
        seed = int(hashlib.sha256(name.encode()).hexdigest()[:8], 16)
        size = 1
        for x in shape:
            size *= x
        values = ((torch.arange(size) * 13 + seed) % 31 - 15).float() / 128
        if "norm" in name:
            values = 1 + values
        if name.endswith("expert_bias"):
            values = torch.arange(size).float() * 0.0625
        return values.reshape(shape).to(dtype)

    for prefix, owner in [("model.layers", layers), ("model.embedding_norm", norm)]:
        for name, param in list(owner.named_parameters()):
            path = f"{prefix}.{name}"
            dtype = torch.float32 if name.endswith("expert_bias") else torch.bfloat16
            value = operand(path, param.shape, dtype)
            parent, _, leaf = name.rpartition(".")
            setattr(owner.get_submodule(parent), leaf, torch.nn.Parameter(value, requires_grad=False))
        for name, buffer in list(owner.named_buffers()):
            parent, _, leaf = name.rpartition(".")
            setattr(owner.get_submodule(parent), leaf, operand(f"{prefix}.{name}", buffer.shape, torch.float32))
        for name, value in owner.state_dict().items():
            path = f"{prefix}.{name}"
            if name.endswith("experts.gate_up_proj"):
                weights[path.replace("gate_up_proj", "0.w1.weight")] = value[:, :32]
                weights[path.replace("gate_up_proj", "0.w3.weight")] = value[:, 32:]
            elif name.endswith("experts.down_proj"):
                weights[path.replace("down_proj", "0.w2.weight")] = value
            else:
                weights[path] = value
    embedding = operand("model.embed_tokens.weight", (32, 16), torch.bfloat16)
    weights["model.embed_tokens.weight"] = embedding
    layers.eval()
    norm.eval()

    def forward(tokens):
        n = len(tokens)
        hidden = embedding[torch.tensor(tokens)].unsqueeze(0)
        frequency = 1 / (torch.tensor(10000000.0) ** (torch.arange(0, 4, 2).float() / 4))
        angles = torch.arange(n).float().unsqueeze(1) * frequency
        angles = torch.cat((angles, angles), -1).unsqueeze(0)
        rotary = (angles.cos().to(torch.bfloat16), angles.sin().to(torch.bfloat16))
        mask = torch.full((n, n), float("-inf")).triu(1).to(torch.bfloat16)[None, None]
        for layer in layers:
            hidden = layer(hidden, position_embeddings=rotary,
                           attention_mask=mask if layer.is_attention_layer else None)
        hidden = norm(hidden)
        return hidden[0], F.linear(hidden[0], embedding)

    with torch.no_grad():
        tokens = [3, 5, 7]
        for _ in range(3):
            _, logits = forward(tokens)
            tokens.append(int(logits[-1].argmax()))
        hidden, logits = forward(tokens)
        singles = [forward(tokens[:i + 1]) for i in range(len(tokens))]
        if not torch.equal(hidden, torch.stack([x[0][-1] for x in singles])):
            raise ValueError("upstream full-prefix reference is not chunk invariant")
    with target.open("x") as f:
        f.write(json.dumps(cfg, separators=(",", ":")) + "\n")
        f.write(f"{len(weights)}\n")
        for name, value in sorted(weights.items()):
            f.write(f"{name} {32 if value.dtype == torch.float32 else 16} {value.numel()}\n")
            f.write(" ".join(f"{x:.9g}" for x in value.float().flatten().tolist()) + "\n")
        f.write(f"{len(tokens)}\n" + " ".join(map(str, tokens)) + "\n")
        for value in (hidden, logits):
            f.write(" ".join(f"{x:.9g}" for x in value.float().flatten().tolist()) + "\n")
    print(json.dumps(dict(upstream_sha256=UPSTREAM_SHA, torch=torch.__version__, device="cpu",
                         attention="unmodified eager BF16 scores and probabilities",
                         convolution="unmodified uncached Conv1d, full-prefix recomputation",
                         fixture_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
                         tokens=tokens, checkpoint_quality=False)))


if __name__ == "__main__":
    main()
