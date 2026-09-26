"""Independent upstream Laya raw-score oracle; never a production dependency.

Run with the pinned upstream source on PYTHONPATH and a local copy of the
immutable convaiinnovations/laya-typed-decisions revision. The output is
computational evidence, not calibration, action-head or model-quality proof.
"""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import torch
from safetensors.torch import load_file
from transformers import AutoTokenizer

from laya.common import QTYPES, build_model, build_sequence
import laya


REVISION = "1a793eb568e6718f15941d08f85432581df534e3"
UPSTREAM_COMMIT = "4066d5d5fbf08b66c6757ddeedbd797bd7655bc0"
WEIGHT_SHA256 = "4fa56de72383a9d3efa9cfa78955733c81b9fc8067a587ca4beb82c78107a24e"
MODEL_CONFIG_SHA256 = "ebf0cd524d92342a6be5e48e9fca3d7c2babfb5a56ccd79d2171ef5d8c7f7be8"
ENCODER_CONFIG_SHA256 = "5268d24ad3b77c8151de5dcb0762ba4391619aad9ab0bda33e36fb083cfeae6d"
TOKENIZER_SHA256 = "6c8aaa9a542084f2457eab775d4eeb51f92a70c0fd9de28d5edb0ddec3c08d30"
TOKENIZER_CONFIG_SHA256 = "08d4cf3ac4dca381759441b85b91a6d40e688471dcd33d15d6649eb0a9a854d1"


def digest(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(8 * 1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("model_dir", type=Path)
    args = parser.parse_args()
    root = args.model_dir
    upstream_root = Path(laya.__file__).resolve().parent.parent
    upstream_commit = subprocess.check_output(
        ["git", "-C", str(upstream_root), "rev-parse", "HEAD"], text=True
    ).strip()
    if upstream_commit != UPSTREAM_COMMIT:
        raise SystemExit(f"upstream implementation mismatch: {upstream_commit}")
    expected = {
        "model.safetensors": WEIGHT_SHA256,
        "rl_agent_config.json": MODEL_CONFIG_SHA256,
        "encoder/config.json": ENCODER_CONFIG_SHA256,
        "tokenizer/tokenizer.json": TOKENIZER_SHA256,
        "tokenizer/tokenizer_config.json": TOKENIZER_CONFIG_SHA256,
    }
    observed = {name: digest(root / name) for name in expected}
    if observed != expected:
        raise SystemExit(f"checkpoint identity mismatch: {observed}")

    config = json.loads((root / "rl_agent_config.json").read_text())
    model = build_model(config, encoder_dir=str(root / "encoder"), pretrained=False)
    weights = load_file(root / "model.safetensors")
    model.load_state_dict(weights, strict=True)
    model.eval()
    tokenizer = AutoTokenizer.from_pretrained(root / "tokenizer")
    question = {
        "t": "choice",
        "ins": "Select the best option.",
        "crit": {"A": "continue", "B": "stop", "C": "escalate"},
    }
    tokens, markers = build_sequence(
        tokenizer, "A short state.", question, max_len=64, head_max_len=48
    )
    with torch.no_grad():
        logits, _unused_action_head = model(
            torch.tensor([tokens]),
            torch.ones(1, len(tokens), dtype=torch.long),
            torch.tensor([markers]),
            torch.ones(1, len(markers), dtype=torch.bool),
            torch.tensor([QTYPES["choice"]]),
        )
        relative = torch.softmax(logits.double(), dim=-1)
    print(json.dumps({
        "repository": "convaiinnovations/laya-typed-decisions",
        "revision": REVISION,
        "upstream_implementation": "NandhaKishorM/laya",
        "upstream_commit": upstream_commit,
        "weight_sha256": WEIGHT_SHA256,
        "tensor_count": len(weights),
        "token_ids": tokens,
        "marker_positions": markers,
        "question_type": "choice",
        "raw_candidate_logits": logits[0].tolist(),
        "uncalibrated_relative_distribution": relative[0].tolist(),
        "calibration_claim": False,
        "action_head_claim": False,
    }, sort_keys=True))


if __name__ == "__main__":
    main()
