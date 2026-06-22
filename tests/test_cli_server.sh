#!/bin/sh
#
# YVEX - yvexd smoke tests
#
# File: tests/test_cli_server.sh
# Layer: test
#
# Purpose:
#   Proves the K0 yvexd binary serves status endpoints on localhost in
#   one-request mode without generation claims.

set -eu

YVEXD_BIN=${YVEXD_BIN:-build/bin/yvexd}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli-server}
FIXTURE=tests/fixtures/gguf/valid-tokenizer-simple.gguf

mkdir -p "$OUT_DIR"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    file=$1
    text=$2
    grep -F "$text" "$file" >/dev/null || fail "$file missing: $text"
}

http_request() {
    port=$1
    method=$2
    path=$3
    out=$4
    PORT=$port METHOD=$method PATH_INFO=$path OUT_FILE=$out python3 - <<'PY'
import os
import socket
import time

port = int(os.environ["PORT"])
method = os.environ["METHOD"]
path = os.environ["PATH_INFO"]
out_file = os.environ["OUT_FILE"]

last = None
for _ in range(50):
    try:
        s = socket.create_connection(("127.0.0.1", port), timeout=1.0)
        break
    except OSError as exc:
        last = exc
        time.sleep(0.05)
else:
    raise SystemExit(f"connect failed: {last}")

req = f"{method} {path} HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"
s.sendall(req.encode("ascii"))
chunks = []
while True:
    data = s.recv(4096)
    if not data:
        break
    chunks.append(data)
s.close()
open(out_file, "wb").write(b"".join(chunks))
PY
}

serve_one() {
    port=$1
    name=$2
    shift 2
    "$YVEXD_BIN" --host 127.0.0.1 --port "$port" --one-request "$@" \
        >"$OUT_DIR/$name.stdout" 2>"$OUT_DIR/$name.stderr" &
    server_pid=$!
}

"$YVEXD_BIN" --help >"$OUT_DIR/help.out" 2>"$OUT_DIR/help.err"
contains "$OUT_DIR/help.out" "usage: yvexd"
"$YVEXD_BIN" --version >"$OUT_DIR/version.out" 2>"$OUT_DIR/version.err"
contains "$OUT_DIR/version.out" "0.1.0"

serve_one 18180 health
http_request 18180 GET /health "$OUT_DIR/health.http"
wait "$server_pid" || fail "health server failed"
contains "$OUT_DIR/health.http" "HTTP/1.1 200 OK"
contains "$OUT_DIR/health.http" "\"schema\": \"yvex.health.v1\""
contains "$OUT_DIR/health.http" "\"generation_available\": false"

serve_one 18181 metrics
http_request 18181 GET /metrics "$OUT_DIR/metrics.http"
wait "$server_pid" || fail "metrics server failed"
contains "$OUT_DIR/metrics.http" "\"schema\": \"yvex.server_metrics.v1\""
contains "$OUT_DIR/metrics.http" "\"request_count\": 1"

serve_one 18182 models --model "$FIXTURE" --backend cpu
http_request 18182 GET /v1/models "$OUT_DIR/models.http"
wait "$server_pid" || fail "models server failed"
contains "$OUT_DIR/models.http" "\"schema\": \"yvex.models.v1\""
contains "$OUT_DIR/models.http" "\"id\": \"yvex-tokenizer-test\""
contains "$OUT_DIR/models.http" "\"generation_available\": false"
contains "$OUT_DIR/models.http" "\"inference\": \"not_implemented\""

serve_one 18183 unsupported
http_request 18183 POST /v1/completions "$OUT_DIR/unsupported.http"
wait "$server_pid" || fail "unsupported server failed"
contains "$OUT_DIR/unsupported.http" "HTTP/1.1 501 Not Implemented"
contains "$OUT_DIR/unsupported.http" "generation endpoints are not implemented in K0"

printf 'cli server smoke: ok\n'
