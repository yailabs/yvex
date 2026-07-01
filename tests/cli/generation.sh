#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/generation}
SEGMENT_MODEL="$OUT_DIR/generation-segment-F16.gguf"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    file=$1
    text=$2
    grep -F -- "$text" "$file" >/dev/null || fail "$file missing: $text"
}

line_count() {
    file=$1
    text=$2
    expected=$3
    actual=$(grep -F -- "$text" "$file" | wc -l | tr -d ' ')
    if [ "$actual" -ne "$expected" ]; then
        fail "$file contained '$text' $actual times, expected $expected"
    fi
}

run_ok() {
    name=$1
    shift
    "$@" >"$OUT_DIR/$name.out" 2>"$OUT_DIR/$name.err" || fail "$name exited non-zero"
}

run_fail() {
    name=$1
    shift
    set +e
    "$@" >"$OUT_DIR/$name.out" 2>"$OUT_DIR/$name.err"
    rc=$?
    set -e
    if [ "$rc" -eq 0 ]; then
        fail "$name unexpectedly succeeded"
    fi
}

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

python3 - "$SEGMENT_MODEL" <<'PY'
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
magic = 0x46554747
version = 3
string = 8
uint32 = 4
float32 = 6
ggml_f16 = 1

def u32(value):
    return struct.pack("<I", value)

def u64(value):
    return struct.pack("<Q", value)

def gguf_string(value):
    data = value.encode("utf-8")
    return u64(len(data)) + data

def kv_string(key, value):
    return gguf_string(key) + u32(string) + gguf_string(value)

def kv_u32(key, value):
    return gguf_string(key) + u32(uint32) + u32(value)

def kv_f32(key, value):
    return gguf_string(key) + u32(float32) + struct.pack("<f", value)

def tensor(name, dims, ggml_type, offset):
    data = gguf_string(name) + u32(len(dims))
    for dim in dims:
        data += u64(dim)
    return data + u32(ggml_type) + u64(offset)

def f16(value):
    return struct.pack("<e", float(value))

metadata = [
    kv_string("general.architecture", "deepseek"),
    kv_string("general.name", "generation-segment-fixture"),
    kv_u32("general.alignment", 32),
    kv_u32("general.file_type", 1),
    kv_f32("deepseek2.attention.layer_norm_rms_epsilon", 0.000001),
]
embed_payload = b"".join(f16((i % 17) - 8) for i in range(4 * 32))
rms_payload = b"".join(f16(1) for _ in range(4))
header = u32(magic) + u32(version) + u64(2) + u64(len(metadata))
header += b"".join(metadata)
header += tensor("token_embd.weight", [4, 32], ggml_f16, 0)
header += tensor("blk.0.attn_norm.weight", [4], ggml_f16, len(embed_payload))
header += b"\0" * ((-len(header)) % 32)
path.write_bytes(header + embed_payload + rms_payload)
PY

run_ok max_one "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 1
contains "$OUT_DIR/max_one.out" "status: generation-loop-complete"
contains "$OUT_DIR/max_one.out" "context_length: 5"
contains "$OUT_DIR/max_one.out" "generated_token_count: 1"
contains "$OUT_DIR/max_one.out" "accepted_token_count: 1"
contains "$OUT_DIR/max_one.out" "append_steps: 1"
contains "$OUT_DIR/max_one.out" "total_token_count: 5"
contains "$OUT_DIR/max_one.out" "stop_policy: bounded-diagnostic"
contains "$OUT_DIR/max_one.out" "stop_requested: true"
contains "$OUT_DIR/max_one.out" "stop_reason: max-new-tokens"
contains "$OUT_DIR/max_one.out" "stop_phase: stop-check"
contains "$OUT_DIR/max_one.out" "stop_step: 0"
contains "$OUT_DIR/max_one.out" "stop_timing: post-append"
contains "$OUT_DIR/max_one.out" "stop_after_append: true"
contains "$OUT_DIR/max_one.out" "stop_before_append: false"
contains "$OUT_DIR/max_one.out" "failure_stop: false"
contains "$OUT_DIR/max_one.out" "unsupported_stop_feature: false"
contains "$OUT_DIR/max_one.out" "eos_policy: unsupported"
contains "$OUT_DIR/max_one.out" "stop_token_policy: unsupported"
contains "$OUT_DIR/max_one.out" "full_model_generation: false"
contains "$OUT_DIR/max_one.out" "real_deepseek_generation: false"
contains "$OUT_DIR/max_one.out" "generation: unsupported-full-model"
contains "$OUT_DIR/max_one.out" "benchmark_status: not-measured"
line_count "$OUT_DIR/max_one.out" "stop_reason:" 1

run_ok max_three "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3
contains "$OUT_DIR/max_three.out" "context_length: 7"
contains "$OUT_DIR/max_three.out" "generated_token_count: 3"
contains "$OUT_DIR/max_three.out" "accepted_token_count: 3"
contains "$OUT_DIR/max_three.out" "append_steps: 3"
contains "$OUT_DIR/max_three.out" "total_token_count: 7"
contains "$OUT_DIR/max_three.out" "stop_reason: max-new-tokens"
contains "$OUT_DIR/max_three.out" "stop_step: 2"
contains "$OUT_DIR/max_three.out" "stop_timing: post-append"
contains "$OUT_DIR/max_three.out" "failure_stop: false"

run_ok context_before "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --context-length 4
contains "$OUT_DIR/context_before.out" "context_length: 4"
contains "$OUT_DIR/context_before.out" "generated_token_count: 0"
contains "$OUT_DIR/context_before.out" "accepted_token_count: 0"
contains "$OUT_DIR/context_before.out" "partial_generated_token_count: 0"
contains "$OUT_DIR/context_before.out" "append_steps: 0"
contains "$OUT_DIR/context_before.out" "total_token_count: 4"
contains "$OUT_DIR/context_before.out" "runtime_token_sequence: 0,1,2,3"
contains "$OUT_DIR/context_before.out" "stop_reason: context-limit"
contains "$OUT_DIR/context_before.out" "stop_phase: stop-check"
contains "$OUT_DIR/context_before.out" "stop_step: 0"
contains "$OUT_DIR/context_before.out" "stop_timing: pre-append"
contains "$OUT_DIR/context_before.out" "stop_after_append: false"
contains "$OUT_DIR/context_before.out" "stop_before_append: true"
contains "$OUT_DIR/context_before.out" "failure_stop: false"
contains "$OUT_DIR/context_before.out" "cleanup_attempted: true"

run_ok context_after "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --context-length 5
contains "$OUT_DIR/context_after.out" "context_length: 5"
contains "$OUT_DIR/context_after.out" "generated_token_count: 1"
contains "$OUT_DIR/context_after.out" "accepted_token_count: 1"
contains "$OUT_DIR/context_after.out" "partial_generated_token_count: 1"
contains "$OUT_DIR/context_after.out" "append_steps: 1"
contains "$OUT_DIR/context_after.out" "total_token_count: 5"
contains "$OUT_DIR/context_after.out" "stop_reason: context-limit"
contains "$OUT_DIR/context_after.out" "stop_step: 1"
contains "$OUT_DIR/context_after.out" "stop_timing: pre-append"
contains "$OUT_DIR/context_after.out" "stop_before_append: true"
contains "$OUT_DIR/context_after.out" "failure_stop: false"

run_fail decode_failure env YVEX_TEST_FAIL_DECODE_AFTER_PREFILL=1 "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2
contains "$OUT_DIR/decode_failure.out" "stop_reason: decode-failure"
contains "$OUT_DIR/decode_failure.out" "stop_phase: decode"
contains "$OUT_DIR/decode_failure.out" "failure_stop: true"
contains "$OUT_DIR/decode_failure.out" "failed_phase: decode"
contains "$OUT_DIR/decode_failure.out" "cleanup_attempted: true"

run_fail logits_failure env YVEX_TEST_FAIL_LOGITS_AFTER_DECODE=1 "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2
contains "$OUT_DIR/logits_failure.out" "stop_reason: logits-failure"
contains "$OUT_DIR/logits_failure.out" "stop_phase: logits"
contains "$OUT_DIR/logits_failure.out" "failure_stop: true"
contains "$OUT_DIR/logits_failure.out" "failed_phase: logits"
contains "$OUT_DIR/logits_failure.out" "cleanup_attempted: true"

run_fail sample_failure env YVEX_TEST_FAIL_SAMPLE_AFTER_LOGITS=1 "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2
contains "$OUT_DIR/sample_failure.out" "stop_reason: sampler-failure"
contains "$OUT_DIR/sample_failure.out" "stop_phase: sample"
contains "$OUT_DIR/sample_failure.out" "failure_stop: true"
contains "$OUT_DIR/sample_failure.out" "failed_phase: sample"
contains "$OUT_DIR/sample_failure.out" "cleanup_attempted: true"

run_fail append_failure env YVEX_TEST_FAIL_GENERATE_APPEND=1 "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2
contains "$OUT_DIR/append_failure.out" "append_status: append-failed"
contains "$OUT_DIR/append_failure.out" "stop_reason: append-failure"
contains "$OUT_DIR/append_failure.out" "stop_phase: append"
contains "$OUT_DIR/append_failure.out" "failure_stop: true"
contains "$OUT_DIR/append_failure.out" "failed_phase: append"
contains "$OUT_DIR/append_failure.out" "partial_generated_token_count: 0"
contains "$OUT_DIR/append_failure.out" "cleanup_attempted: true"

echo "cli generation: ok"
