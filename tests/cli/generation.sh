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

not_contains() {
    file=$1
    text=$2
    if grep -F -- "$text" "$file" >/dev/null; then
        fail "$file unexpectedly contained: $text"
    fi
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
contains "$OUT_DIR/max_one.out" "lifecycle_status: cleaned"
contains "$OUT_DIR/max_one.out" "generation_state: completed"
contains "$OUT_DIR/max_one.out" "state_dirty: true"
contains "$OUT_DIR/max_one.out" "active_step: 0"
contains "$OUT_DIR/max_one.out" "last_completed_step: 0"
contains "$OUT_DIR/max_one.out" "cancel_supported: true"
contains "$OUT_DIR/max_one.out" "cancel_requested: false"
contains "$OUT_DIR/max_one.out" "cancel_reason: none"
contains "$OUT_DIR/max_one.out" "cancel_step: none"
contains "$OUT_DIR/max_one.out" "cancel_timing: none"
contains "$OUT_DIR/max_one.out" "cancel_safe_point: none"
contains "$OUT_DIR/max_one.out" "partial_output_available: true"
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
contains "$OUT_DIR/max_one.out" "trace_level: none"
contains "$OUT_DIR/max_one.out" "trace_enabled: false"
contains "$OUT_DIR/max_one.out" "trace_records: 0"
contains "$OUT_DIR/max_one.out" "trace_tokens: 0"
contains "$OUT_DIR/max_one.out" "trace_steps: 0"
contains "$OUT_DIR/max_one.out" "trace_kv: 0"
contains "$OUT_DIR/max_one.out" "trace_logits: 0"
contains "$OUT_DIR/max_one.out" "trace_sampling: 0"
contains "$OUT_DIR/max_one.out" "trace_append: 0"
contains "$OUT_DIR/max_one.out" "trace_stop: 0"
contains "$OUT_DIR/max_one.out" "trace_cancel: 0"
contains "$OUT_DIR/max_one.out" "trace_cleanup: 0"
contains "$OUT_DIR/max_one.out" "trace_failures: 0"
contains "$OUT_DIR/max_one.out" "trace_status: disabled"
contains "$OUT_DIR/max_one.out" "cleanup_idempotent: true"
contains "$OUT_DIR/max_one.out" "cleanup_repeated: false"
contains "$OUT_DIR/max_one.out" "cleanup_owned_state_released: true"
contains "$OUT_DIR/max_one.out" "failure_preserved: true"
contains "$OUT_DIR/max_one.out" "partial_output_preserved: true"
not_contains "$OUT_DIR/max_one.out" "trace.step."
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

run_ok cancel_before_first "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --cancel-after-steps 0
contains "$OUT_DIR/cancel_before_first.out" "status: generation-loop-cancelled"
contains "$OUT_DIR/cancel_before_first.out" "generation_state: cancelled"
contains "$OUT_DIR/cancel_before_first.out" "state_dirty: false"
contains "$OUT_DIR/cancel_before_first.out" "active_step: 0"
contains "$OUT_DIR/cancel_before_first.out" "last_completed_step: none"
contains "$OUT_DIR/cancel_before_first.out" "cancel_requested: true"
contains "$OUT_DIR/cancel_before_first.out" "cancel_reason: interrupted"
contains "$OUT_DIR/cancel_before_first.out" "cancel_step: 0"
contains "$OUT_DIR/cancel_before_first.out" "cancel_timing: before-step"
contains "$OUT_DIR/cancel_before_first.out" "cancel_safe_point: before-decode"
contains "$OUT_DIR/cancel_before_first.out" "partial_output_available: false"
contains "$OUT_DIR/cancel_before_first.out" "generated_token_count: 0"
contains "$OUT_DIR/cancel_before_first.out" "decode_steps: 0"
contains "$OUT_DIR/cancel_before_first.out" "logits_steps: 0"
contains "$OUT_DIR/cancel_before_first.out" "sample_steps: 0"
contains "$OUT_DIR/cancel_before_first.out" "append_steps: 0"
contains "$OUT_DIR/cancel_before_first.out" "runtime_token_sequence: 0,1,2,3"
contains "$OUT_DIR/cancel_before_first.out" "stop_reason: interrupted"
contains "$OUT_DIR/cancel_before_first.out" "stop_timing: cancel-safe-point"
contains "$OUT_DIR/cancel_before_first.out" "stop_before_append: true"
contains "$OUT_DIR/cancel_before_first.out" "failure_stop: false"

run_ok cancel_after_one "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --cancel-after-steps 1
contains "$OUT_DIR/cancel_after_one.out" "status: generation-loop-cancelled"
contains "$OUT_DIR/cancel_after_one.out" "generation_state: cancelled"
contains "$OUT_DIR/cancel_after_one.out" "state_dirty: true"
contains "$OUT_DIR/cancel_after_one.out" "last_completed_step: 0"
contains "$OUT_DIR/cancel_after_one.out" "cancel_step: 1"
contains "$OUT_DIR/cancel_after_one.out" "cancel_timing: after-step"
contains "$OUT_DIR/cancel_after_one.out" "cancel_safe_point: after-append"
contains "$OUT_DIR/cancel_after_one.out" "partial_output_available: true"
contains "$OUT_DIR/cancel_after_one.out" "generated_token_count: 1"
contains "$OUT_DIR/cancel_after_one.out" "append_steps: 1"
contains "$OUT_DIR/cancel_after_one.out" "stop_after_append: true"
contains "$OUT_DIR/cancel_after_one.out" "partial_output_preserved: true"

run_ok cancel_after_two_trace "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --cancel-after-steps 2 --trace-level full
contains "$OUT_DIR/cancel_after_two_trace.out" "status: generation-loop-cancelled"
contains "$OUT_DIR/cancel_after_two_trace.out" "generated_token_count: 2"
contains "$OUT_DIR/cancel_after_two_trace.out" "cancel_step: 2"
contains "$OUT_DIR/cancel_after_two_trace.out" "trace.cancel.requested: true"
contains "$OUT_DIR/cancel_after_two_trace.out" "trace.cancel.reason: interrupted"
contains "$OUT_DIR/cancel_after_two_trace.out" "trace.cancel.step: 2"
contains "$OUT_DIR/cancel_after_two_trace.out" "trace.cancel.timing: after-step"
contains "$OUT_DIR/cancel_after_two_trace.out" "trace.cancel.safe_point: after-append"
contains "$OUT_DIR/cancel_after_two_trace.out" "trace.cancel.partial_generated_token_count: 2"
contains "$OUT_DIR/cancel_after_two_trace.out" "trace_cancel:"
contains "$OUT_DIR/cancel_after_two_trace.out" "trace.cleanup.status: pass"

run_ok cancel_beyond_max "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2 --cancel-after-steps 999
contains "$OUT_DIR/cancel_beyond_max.out" "status: generation-loop-complete"
contains "$OUT_DIR/cancel_beyond_max.out" "generation_state: completed"
contains "$OUT_DIR/cancel_beyond_max.out" "cancel_requested: false"
contains "$OUT_DIR/cancel_beyond_max.out" "cancel_step: none"
contains "$OUT_DIR/cancel_beyond_max.out" "stop_reason: max-new-tokens"
contains "$OUT_DIR/cancel_beyond_max.out" "generated_token_count: 2"

run_ok cleanup_repeated env YVEX_TEST_REPEAT_GENERATE_CLEANUP=1 "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 1
contains "$OUT_DIR/cleanup_repeated.out" "cleanup_attempted: true"
contains "$OUT_DIR/cleanup_repeated.out" "cleanup_status: already-cleaned"
contains "$OUT_DIR/cleanup_repeated.out" "cleanup_idempotent: true"
contains "$OUT_DIR/cleanup_repeated.out" "cleanup_repeated: true"
contains "$OUT_DIR/cleanup_repeated.out" "cleanup_owned_state_released: true"

run_fail invalid_trace "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 1 --trace-level nonsense
contains "$OUT_DIR/invalid_trace.err" "--trace-level requires none|tokens|steps|kv|logits|sampling|full"

run_fail invalid_cancel "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 1 --cancel-after-steps nope
contains "$OUT_DIR/invalid_cancel.err" "--cancel-after-steps requires a non-negative integer"

run_ok trace_none "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 1 --trace-level none
contains "$OUT_DIR/trace_none.out" "trace_level: none"
contains "$OUT_DIR/trace_none.out" "trace_enabled: false"
contains "$OUT_DIR/trace_none.out" "trace_records: 0"
not_contains "$OUT_DIR/trace_none.out" "trace.tokens."

run_ok trace_tokens "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2 --trace-level tokens
contains "$OUT_DIR/trace_tokens.out" "trace.tokens.prompt: 0,1,2,3"
contains "$OUT_DIR/trace_tokens.out" "trace.tokens.generated:"
contains "$OUT_DIR/trace_tokens.out" "trace.tokens.runtime_sequence:"
contains "$OUT_DIR/trace_tokens.out" "trace.tokens.prompt_count: 4"
contains "$OUT_DIR/trace_tokens.out" "trace.tokens.generated_count: 2"
contains "$OUT_DIR/trace_tokens.out" "trace.tokens.total_count: 6"
contains "$OUT_DIR/trace_tokens.out" "trace.tokens.stop_reason: max-new-tokens"
contains "$OUT_DIR/trace_tokens.out" "trace_level: tokens"
contains "$OUT_DIR/trace_tokens.out" "trace_enabled: true"
contains "$OUT_DIR/trace_tokens.out" "trace_status: emitted"
not_contains "$OUT_DIR/trace_tokens.out" "trace.step.0.logits_checksum"

run_ok trace_steps "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --trace-level steps
contains "$OUT_DIR/trace_steps.out" "trace.step.0.index: 0"
contains "$OUT_DIR/trace_steps.out" "trace.step.2.index: 2"
contains "$OUT_DIR/trace_steps.out" "trace.step.2.stop_reason: max-new-tokens"
contains "$OUT_DIR/trace_steps.out" "trace.step.2.stop_timing: post-append"
contains "$OUT_DIR/trace_steps.out" "trace_level: steps"
contains "$OUT_DIR/trace_steps.out" "trace_status: emitted"

run_ok trace_kv "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 1 --trace-level kv
contains "$OUT_DIR/trace_kv.out" "trace.kv.status: unavailable"
contains "$OUT_DIR/trace_kv.out" "trace.kv.mode: diagnostic"
contains "$OUT_DIR/trace_kv.out" "trace.kv.real_attention_kv: false"
contains "$OUT_DIR/trace_kv.out" "trace.kv.full_model_kv: false"
contains "$OUT_DIR/trace_kv.out" "trace_level: kv"

run_ok trace_kv_requested "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 1 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8 --trace-level kv
contains "$OUT_DIR/trace_kv_requested.out" "trace.kv.status: requested"
contains "$OUT_DIR/trace_kv_requested.out" "trace.kv.layers: 1"
contains "$OUT_DIR/trace_kv_requested.out" "trace.kv.heads: 2"
contains "$OUT_DIR/trace_kv_requested.out" "trace.kv.head_dim: 4"
contains "$OUT_DIR/trace_kv_requested.out" "trace.kv.capacity: 8"
contains "$OUT_DIR/trace_kv_requested.out" "trace.kv.binding_source: generate-decode-options"

run_ok trace_logits "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 1 --trace-level logits
contains "$OUT_DIR/trace_logits.out" "trace.step.0.logits_mode: bounded-diagnostic"
contains "$OUT_DIR/trace_logits.out" "trace.step.0.logits_checksum:"
contains "$OUT_DIR/trace_logits.out" "trace.step.0.logits_min:"
contains "$OUT_DIR/trace_logits.out" "trace.step.0.logits_max:"
contains "$OUT_DIR/trace_logits.out" "trace.step.0.real_output_head_logits: false"
contains "$OUT_DIR/trace_logits.out" "trace_level: logits"

run_ok trace_sampling "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 1 --trace-level sampling
contains "$OUT_DIR/trace_sampling.out" "trace.step.0.sampling_strategy: greedy"
contains "$OUT_DIR/trace_sampling.out" "trace.step.0.candidate_token_id:"
contains "$OUT_DIR/trace_sampling.out" "trace.step.0.candidate_logit:"
contains "$OUT_DIR/trace_sampling.out" "trace.step.0.sample_checksum:"
contains "$OUT_DIR/trace_sampling.out" "trace.step.0.real_vocab_sampling: false"
contains "$OUT_DIR/trace_sampling.out" "trace.step.0.stochastic_sampling: false"
contains "$OUT_DIR/trace_sampling.out" "trace_level: sampling"

run_ok trace_full "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2 --trace-level full
contains "$OUT_DIR/trace_full.out" "trace.tokens.prompt: 0,1,2,3"
contains "$OUT_DIR/trace_full.out" "trace.step.0.index: 0"
contains "$OUT_DIR/trace_full.out" "trace.kv.status: unavailable"
contains "$OUT_DIR/trace_full.out" "trace.step.0.logits_mode: bounded-diagnostic"
contains "$OUT_DIR/trace_full.out" "trace.step.0.sampling_strategy: greedy"
contains "$OUT_DIR/trace_full.out" "trace.append.0.append_status: appended"
contains "$OUT_DIR/trace_full.out" "trace.stop.reason: max-new-tokens"
contains "$OUT_DIR/trace_full.out" "trace.cleanup.attempted: true"
contains "$OUT_DIR/trace_full.out" "trace_level: full"
contains "$OUT_DIR/trace_full.out" "trace_status: emitted"

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

run_ok context_before_trace "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --context-length 4 --trace-level full
contains "$OUT_DIR/context_before_trace.out" "trace.step.0.decode_status: skipped"
contains "$OUT_DIR/context_before_trace.out" "trace.step.0.logits_status: skipped"
contains "$OUT_DIR/context_before_trace.out" "trace.step.0.sample_status: skipped"
contains "$OUT_DIR/context_before_trace.out" "trace.step.0.append_status: context-limit"
contains "$OUT_DIR/context_before_trace.out" "trace.stop.reason: context-limit"
contains "$OUT_DIR/context_before_trace.out" "trace.stop.timing: pre-append"
contains "$OUT_DIR/context_before_trace.out" "trace.stop.before_append: true"

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
contains "$OUT_DIR/decode_failure.out" "status: generation-loop-failed"
contains "$OUT_DIR/decode_failure.out" "generation_state: failed"
contains "$OUT_DIR/decode_failure.out" "partial_output_available: false"
contains "$OUT_DIR/decode_failure.out" "stop_reason: decode-failure"
contains "$OUT_DIR/decode_failure.out" "stop_phase: decode"
contains "$OUT_DIR/decode_failure.out" "failure_stop: true"
contains "$OUT_DIR/decode_failure.out" "failed_phase: decode"
contains "$OUT_DIR/decode_failure.out" "cleanup_attempted: true"
contains "$OUT_DIR/decode_failure.out" "failure_preserved: true"
contains "$OUT_DIR/decode_failure.out" "partial_output_preserved: true"

run_fail logits_failure env YVEX_TEST_FAIL_LOGITS_AFTER_DECODE=1 "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2
contains "$OUT_DIR/logits_failure.out" "status: generation-loop-failed"
contains "$OUT_DIR/logits_failure.out" "generation_state: failed"
contains "$OUT_DIR/logits_failure.out" "stop_reason: logits-failure"
contains "$OUT_DIR/logits_failure.out" "stop_phase: logits"
contains "$OUT_DIR/logits_failure.out" "failure_stop: true"
contains "$OUT_DIR/logits_failure.out" "failed_phase: logits"
contains "$OUT_DIR/logits_failure.out" "cleanup_attempted: true"
contains "$OUT_DIR/logits_failure.out" "failure_preserved: true"

run_fail sample_failure env YVEX_TEST_FAIL_SAMPLE_AFTER_LOGITS=1 "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2
contains "$OUT_DIR/sample_failure.out" "status: generation-loop-failed"
contains "$OUT_DIR/sample_failure.out" "generation_state: failed"
contains "$OUT_DIR/sample_failure.out" "stop_reason: sampler-failure"
contains "$OUT_DIR/sample_failure.out" "stop_phase: sample"
contains "$OUT_DIR/sample_failure.out" "failure_stop: true"
contains "$OUT_DIR/sample_failure.out" "failed_phase: sample"
contains "$OUT_DIR/sample_failure.out" "cleanup_attempted: true"
contains "$OUT_DIR/sample_failure.out" "failure_preserved: true"

run_fail sample_failure_trace env YVEX_TEST_FAIL_SAMPLE_AFTER_LOGITS=1 "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2 --trace-level full
contains "$OUT_DIR/sample_failure_trace.out" "trace.failure.phase: sample"
contains "$OUT_DIR/sample_failure_trace.out" "trace.failure.stop_reason: sampler-failure"
contains "$OUT_DIR/sample_failure_trace.out" "trace.failure.partial_generated_token_count: 0"
contains "$OUT_DIR/sample_failure_trace.out" "trace.cleanup.attempted: true"
contains "$OUT_DIR/sample_failure_trace.out" "trace_failures:"
contains "$OUT_DIR/sample_failure_trace.out" "trace_status: emitted"

run_fail append_failure env YVEX_TEST_FAIL_GENERATE_APPEND=1 "$YVEX_BIN" generate --model "$SEGMENT_MODEL" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2
contains "$OUT_DIR/append_failure.out" "status: generation-loop-failed"
contains "$OUT_DIR/append_failure.out" "generation_state: failed"
contains "$OUT_DIR/append_failure.out" "append_status: append-failed"
contains "$OUT_DIR/append_failure.out" "stop_reason: append-failure"
contains "$OUT_DIR/append_failure.out" "stop_phase: append"
contains "$OUT_DIR/append_failure.out" "failure_stop: true"
contains "$OUT_DIR/append_failure.out" "failed_phase: append"
contains "$OUT_DIR/append_failure.out" "partial_generated_token_count: 0"
contains "$OUT_DIR/append_failure.out" "cleanup_attempted: true"
contains "$OUT_DIR/append_failure.out" "failure_preserved: true"

echo "cli generation: ok"
