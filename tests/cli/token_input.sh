#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/token-input}
PARTIAL_MODEL="$OUT_DIR/token-input-partial-F16.gguf"
SEGMENT_MODEL="$OUT_DIR/token-input-segment-F16.gguf"
TOKENIZER_FIXTURE=tests/fixtures/gguf/valid-tokenizer-simple.gguf

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    file=$1
    text=$2
    grep -F "$text" "$file" >/dev/null || fail "$file missing: $text"
}

not_contains() {
    file=$1
    text=$2
    ! grep -F "$text" "$file" >/dev/null || fail "$file unexpectedly contained: $text"
}

expect_fail() {
    name=$1
    shift
    "$@" >"$OUT_DIR/$name.out" 2>"$OUT_DIR/$name.err" && fail "$name unexpectedly passed" || true
}

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

"$YVEX_BIN" gguf-emit controlled \
  --out "$PARTIAL_MODEL" \
  --model-name token-input-partial \
  --arch deepseek \
  --target-qtype F16 \
  --overwrite >"$OUT_DIR/emit-partial.out" 2>"$OUT_DIR/emit-partial.err"

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
    kv_string("general.name", "token-input-segment"),
    kv_u32("general.alignment", 32),
    kv_u32("general.file_type", 1),
    kv_f32("deepseek2.attention.layer_norm_rms_epsilon", 0.000001),
]
tensors = [
    tensor("token_embd.weight", [4, 8], ggml_f16, 0),
    tensor("blk.0.attn_norm.weight", [4], ggml_f16, 64),
]
payload = b"".join(f16(i) for i in range(32)) + b"".join(f16(1) for _ in range(4))
data = u32(magic) + u32(version) + u64(len(tensors)) + u64(len(metadata))
data += b"".join(metadata) + b"".join(tensors)
data += b"\0" * ((-len(data)) % 32)
path.write_bytes(data + payload)
PY

"$YVEX_BIN" input tokens --model "$PARTIAL_MODEL" --tokens 0,1 \
  >"$OUT_DIR/input-tokens.out" 2>"$OUT_DIR/input-tokens.err"
contains "$OUT_DIR/input-tokens.out" "token_input: tokens"
contains "$OUT_DIR/input-tokens.out" "token_input_status: pass"
contains "$OUT_DIR/input-tokens.out" "token_input_kind: explicit"
contains "$OUT_DIR/input-tokens.out" "token_count: 2"
contains "$OUT_DIR/input-tokens.out" "token_bounds_status: pass"
contains "$OUT_DIR/input-tokens.out" "vocab_size: 8"
contains "$OUT_DIR/input-tokens.out" "prefill_ready: false"
contains "$OUT_DIR/input-tokens.out" "generation: unsupported"
contains "$OUT_DIR/input-tokens.out" "status: token-input-pass"

expect_fail malformed-empty "$YVEX_BIN" input tokens --model "$PARTIAL_MODEL" --tokens ""
contains "$OUT_DIR/malformed-empty.out" "status: token-input-fail"
contains "$OUT_DIR/malformed-empty.err" "token-list-empty"

expect_fail malformed-double-comma "$YVEX_BIN" input tokens --model "$PARTIAL_MODEL" --tokens 1,,2
contains "$OUT_DIR/malformed-double-comma.err" "token-parse-invalid"

expect_fail malformed-alpha "$YVEX_BIN" input tokens --model "$PARTIAL_MODEL" --tokens abc
contains "$OUT_DIR/malformed-alpha.err" "token-parse-invalid"

expect_fail malformed-negative "$YVEX_BIN" input tokens --model "$PARTIAL_MODEL" --tokens -1
contains "$OUT_DIR/malformed-negative.err" "token-parse-invalid"

expect_fail malformed-overflow "$YVEX_BIN" input tokens --model "$PARTIAL_MODEL" --tokens 184467440737095516160
contains "$OUT_DIR/malformed-overflow.err" "token-id-overflow"

expect_fail token-out-of-vocab "$YVEX_BIN" input tokens --model "$PARTIAL_MODEL" --tokens 8
contains "$OUT_DIR/token-out-of-vocab.out" "token_bounds_status: fail"
contains "$OUT_DIR/token-out-of-vocab.err" "token-out-of-vocab"

"$YVEX_BIN" input prompt --model "$TOKENIZER_FIXTURE" --text "hello world" \
  >"$OUT_DIR/prompt-fixture.out" 2>"$OUT_DIR/prompt-fixture.err"
contains "$OUT_DIR/prompt-fixture.out" "token_input: prompt"
contains "$OUT_DIR/prompt-fixture.out" "tokenizer_status: present"
contains "$OUT_DIR/prompt-fixture.out" "tokenizer_support: fixture-encode-decode"
contains "$OUT_DIR/prompt-fixture.out" "token_input_kind: prompt-text"
contains "$OUT_DIR/prompt-fixture.out" "token_count: 3"
contains "$OUT_DIR/prompt-fixture.out" "status: token-input-pass"

expect_fail prompt-tokenizer-missing "$YVEX_BIN" input prompt --model "$SEGMENT_MODEL" --text hello
contains "$OUT_DIR/prompt-tokenizer-missing.out" "tokenizer_status: missing"
contains "$OUT_DIR/prompt-tokenizer-missing.out" "reason: tokenizer-metadata-missing"
contains "$OUT_DIR/prompt-tokenizer-missing.err" "tokenizer-metadata-missing"

"$YVEX_BIN" graph --model "$PARTIAL_MODEL" --backend cpu --execute-partial \
  --tokens 0,1 --token-index 0 \
  >"$OUT_DIR/partial-token-index-0.out" 2>"$OUT_DIR/partial-token-index-0.err"
contains "$OUT_DIR/partial-token-index-0.out" "token_input_status: pass"
contains "$OUT_DIR/partial-token-index-0.out" "selected_token_index: 0"
contains "$OUT_DIR/partial-token-index-0.out" "selected_token_id: 0"
contains "$OUT_DIR/partial-token-index-0.out" "partial_token: 0"
contains "$OUT_DIR/partial-token-index-0.out" "status: real-partial-graph-executed"

"$YVEX_BIN" graph --model "$PARTIAL_MODEL" --backend cpu --execute-partial \
  --tokens 0,1 --token-index 1 \
  >"$OUT_DIR/partial-token-index-1.out" 2>"$OUT_DIR/partial-token-index-1.err"
contains "$OUT_DIR/partial-token-index-1.out" "selected_token_index: 1"
contains "$OUT_DIR/partial-token-index-1.out" "selected_token_id: 1"
contains "$OUT_DIR/partial-token-index-1.out" "partial_token: 1"
contains "$OUT_DIR/partial-token-index-1.out" "partial_output_sample_values: 16,20,24,28"

"$YVEX_BIN" graph --model "$SEGMENT_MODEL" --backend cpu \
  --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 1 \
  >"$OUT_DIR/segment-token-index-1.out" 2>"$OUT_DIR/segment-token-index-1.err"
contains "$OUT_DIR/segment-token-index-1.out" "token_input_status: pass"
contains "$OUT_DIR/segment-token-index-1.out" "graph_kind: selected-embedding-rmsnorm"
contains "$OUT_DIR/segment-token-index-1.out" "selected_token_id: 1"
contains "$OUT_DIR/segment-token-index-1.out" "partial_token: 1"
contains "$OUT_DIR/segment-token-index-1.out" "status: real-segment-graph-executed"

expect_fail token-index-out-of-range "$YVEX_BIN" graph --model "$SEGMENT_MODEL" --backend cpu \
  --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 2
contains "$OUT_DIR/token-index-out-of-range.out" "token_input_status: fail"
contains "$OUT_DIR/token-index-out-of-range.out" "selected_token_index: 2"
contains "$OUT_DIR/token-index-out-of-range.out" "dispatch_attempted: false"
contains "$OUT_DIR/token-index-out-of-range.out" "status: graph-integrity-fail"
contains "$OUT_DIR/token-index-out-of-range.err" "token-index-out-of-range"

"$YVEX_BIN" graph --model "$SEGMENT_MODEL" --backend cpu \
  --execute-segment --segment embedding-rmsnorm --partial-token 0 \
  >"$OUT_DIR/segment-partial-token-unchanged.out" 2>"$OUT_DIR/segment-partial-token-unchanged.err"
contains "$OUT_DIR/segment-partial-token-unchanged.out" "status: real-segment-graph-executed"
contains "$OUT_DIR/segment-partial-token-unchanged.out" "partial_token: 0"
not_contains "$OUT_DIR/segment-partial-token-unchanged.out" "token_input_status:"

"$YVEX_BIN" graph --model "$PARTIAL_MODEL" --backend cpu --execute-partial --partial-token 0 \
  >"$OUT_DIR/m5-partial-unchanged.out" 2>"$OUT_DIR/m5-partial-unchanged.err"
contains "$OUT_DIR/m5-partial-unchanged.out" "status: real-partial-graph-executed"
contains "$OUT_DIR/m5-partial-unchanged.out" "partial_token: 0"
not_contains "$OUT_DIR/m5-partial-unchanged.out" "token_input_status:"

echo "cli token input: ok"
