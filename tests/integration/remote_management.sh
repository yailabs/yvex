#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "$0")/../.." && pwd)
. "$repo_dir/tests/support/cleanup.sh"
cache_dir=${XDG_CACHE_HOME:-$HOME/.cache}
mkdir -p "$cache_dir"
fixture_dir=$(mktemp -d "$cache_dir/yvex-management-test.XXXXXX")
sshd_pid=
host_pid=
remote_dir=
cleanup() {
    if [[ -S $fixture_dir/control ]]; then
        ssh -O exit -S "$fixture_dir/control" -p "$port" \
            "$(id -un)@127.0.0.1" >/dev/null 2>&1 || true
    fi
    if [[ -n $host_pid ]]; then
        kill "$host_pid" 2>/dev/null || true
        wait "$host_pid" 2>/dev/null || true
    fi
    if [[ -n $sshd_pid ]]; then
        kill "$sshd_pid" 2>/dev/null || true
        wait "$sshd_pid" 2>/dev/null || true
    fi
    if [[ -n $remote_dir && $remote_dir =~ ^/tmp/yvex-mgmt-client\.[A-Za-z0-9]{6}$ &&
          -n ${YVEX_MANAGEMENT_TEST_REMOTE:-} ]]; then
        ssh -o BatchMode=yes "$YVEX_MANAGEMENT_TEST_REMOTE" \
            "rm -f -- '$remote_dir/peer-b' '$remote_dir/remote_known_hosts'; rmdir -- '$remote_dir'" \
            2>/dev/null || true
    fi
    if [[ -d $fixture_dir ]]; then
        (cd "$repo_dir" && TMPDIR="$cache_dir" yvex_test_cleanup "$fixture_dir")
    fi
}
trap cleanup EXIT

ssh-keygen -q -N '' -t ed25519 -f "$fixture_dir/host"
ssh-keygen -q -N '' -t ed25519 -f "$fixture_dir/peer-a"
ssh-keygen -q -N '' -t ed25519 -f "$fixture_dir/peer-b"
ssh-keygen -q -N '' -t ed25519 -f "$fixture_dir/unknown"
mkdir -m 700 "$fixture_dir/runtime"
"$repo_dir/yvex" management trust-init "$fixture_dir/authorized_keys"
if "$repo_dir/yvex" management enroll >/dev/null 2>&1; then
    echo 'incomplete enrollment unexpectedly accepted' >&2
    exit 1
fi
peer_a=$("$repo_dir/yvex" management identity "$fixture_dir/peer-a.pub")
peer_b=$("$repo_dir/yvex" management identity "$fixture_dir/peer-b.pub")
device=$("$repo_dir/yvex" management identity "$fixture_dir/host.pub")
"$repo_dir/yvex" management enroll "$fixture_dir/peer-a.pub" \
    "$fixture_dir/authorized_keys" "$fixture_dir/host.pub" \
    "${peer_a#ssh-ed25519:sha256:}" >/dev/null
"$repo_dir/yvex" management enroll "$fixture_dir/peer-b.pub" \
    "$fixture_dir/authorized_keys" "$fixture_dir/host.pub" \
    "${peer_b#ssh-ed25519:sha256:}" >/dev/null

listen_address=127.0.0.1
if [[ -n ${YVEX_MANAGEMENT_TEST_REMOTE:-} &&
      -n ${YVEX_MANAGEMENT_TEST_ADDRESS:-} ]]; then
    listen_address=0.0.0.0
fi
port=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()')
printf '[127.0.0.1]:%s %s\n' "$port" "$(<"$fixture_dir/host.pub")" >"$fixture_dir/known_hosts"
chmod 600 "$fixture_dir/known_hosts"
printf '%s\n' \
    "Port $port" \
    "ListenAddress $listen_address" \
    "HostKey $fixture_dir/host" \
    "AuthorizedKeysFile $fixture_dir/authorized_keys" \
    "PidFile $fixture_dir/pid" \
    'PubkeyAuthentication yes' \
    'PasswordAuthentication no' \
    'KbdInteractiveAuthentication no' \
    'GSSAPIAuthentication no' \
    'UsePAM no' \
    'StrictModes yes' \
    'PermitTTY no' \
    'DisableForwarding yes' \
    'PermitUserRC no' \
    "SetEnv XDG_RUNTIME_DIR=$fixture_dir/runtime" \
    "AllowUsers $(id -un)" >"$fixture_dir/sshd_config"
/usr/sbin/sshd -t -f "$fixture_dir/sshd_config"
/usr/sbin/sshd -D -f "$fixture_dir/sshd_config" -E "$fixture_dir/sshd.log" &
sshd_pid=$!
for attempt in {1..40}; do
    if ssh-keyscan -T 1 -p "$port" 127.0.0.1 >/dev/null 2>&1; then break; fi
    sleep 0.1
done

request='{"schema":"yvex.management.request.v1","request_id":"1111111111111111111111111111111111111111111111111111111111111111","operation":"device.describe"}'
ssh_request() {
    local private_key=$1
    printf '%s\n' "$request" | ssh -T -F /dev/null \
        -o BatchMode=yes -o IdentitiesOnly=yes \
        -o PreferredAuthentications=publickey -o PasswordAuthentication=no \
        -o KbdInteractiveAuthentication=no -o StrictHostKeyChecking=yes \
        -o UserKnownHostsFile="$fixture_dir/known_hosts" \
        -o ConnectTimeout=4 -i "$private_key" -p "$port" \
        "$(id -un)@127.0.0.1"
}

first=$(ssh_request "$fixture_dir/peer-a")
second=$(ssh_request "$fixture_dir/peer-b")
[[ $first == *'"status":"ok"'* && $first == *"$device"* &&
   $first == *"$peer_a"* ]]
[[ $second == *'"status":"ok"'* && $second == *"$peer_b"* ]]
[[ $(ssh_request "$fixture_dir/peer-a") == "$first" ]]
if "$repo_dir/yvex" management enroll "$fixture_dir/unknown.pub" \
    "$fixture_dir/authorized_keys" "$fixture_dir/host.pub" \
    "${peer_a#ssh-ed25519:sha256:}" >/dev/null 2>&1; then
    echo 'peer identity mismatch unexpectedly enrolled' >&2
    exit 1
fi
if ssh_request "$fixture_dir/unknown" >/dev/null 2>&1; then
    echo 'unknown peer unexpectedly authenticated' >&2
    exit 1
fi
if ssh -T -F /dev/null -o BatchMode=yes -o IdentitiesOnly=yes \
    -o StrictHostKeyChecking=yes -o UserKnownHostsFile="$fixture_dir/known_hosts" \
    -i "$fixture_dir/peer-a" -p "$port" "$(id -un)@127.0.0.1" uname \
    >/dev/null 2>&1; then
    echo 'arbitrary remote command unexpectedly succeeded' >&2
    exit 1
fi
request='{"schema":"invalid","request_id":"1111111111111111111111111111111111111111111111111111111111111111","operation":"device.describe"}'
[[ $(ssh_request "$fixture_dir/peer-a") == *'"reason":"malformed_request"'* ]]
request='{"schema":"yvex.management.request.v1","request_id":"1111111111111111111111111111111111111111111111111111111111111111","operation":"device.describe","extra":true}'
[[ $(ssh_request "$fixture_dir/peer-a") == *'"reason":"malformed_request"'* ]]
request='{"schema":"yvex.management.request.v1","request_id":"1111111111111111111111111111111111111111111111111111111111111111","request_id":"2222222222222222222222222222222222222222222222222222222222222222","operation":"device.describe"}'
[[ $(ssh_request "$fixture_dir/peer-a") == *'"reason":"malformed_request"'* ]]
request='{"schema":"yvex.management.request.v1","request_id":"1111111111111111111111111111111111111111111111111111111111111111","operation":"model.load"}'
[[ $(ssh_request "$fixture_dir/peer-a") == *'"reason":"unsupported_operation"'* ]]

request='{"schema":"yvex.management.request.v1","request_id":"2222222222222222222222222222222222222222222222222222222222222222","operation":"host.status"}'
stopped=$(ssh_request "$fixture_dir/peer-a")
[[ $stopped == *'"host_state":"stopped"'* ]]
ssh -M -N -f -S "$fixture_dir/control" -T -F /dev/null \
    -o BatchMode=yes -o IdentitiesOnly=yes \
    -o StrictHostKeyChecking=yes -o UserKnownHostsFile="$fixture_dir/known_hosts" \
    -i "$fixture_dir/peer-a" -p "$port" "$(id -un)@127.0.0.1"
XDG_RUNTIME_DIR="$fixture_dir/runtime" "$repo_dir/yvex" serve \
    --openai off --logs off >"$fixture_dir/host.log" 2>&1 &
host_pid=$!
for attempt in {1..40}; do
    if [[ -S $fixture_dir/runtime/yvex/yvexd.sock ]]; then break; fi
    sleep 0.1
done
running=$(ssh_request "$fixture_dir/peer-a")
[[ $running == *'"host_state":"running"'* &&
   $running == *'"engine_count":0'* ]]
kill -INT "$host_pid"
wait "$host_pid"
host_pid=
[[ $(ssh_request "$fixture_dir/peer-a") == *'"host_state":"stopped"'* ]]

"$repo_dir/yvex" management revoke "${peer_a#ssh-ed25519:sha256:}" \
    "$fixture_dir/authorized_keys" >/dev/null
revoked_over_existing_transport=$(printf '%s\n' "$request" | \
    ssh -S "$fixture_dir/control" -T -F /dev/null \
        -o BatchMode=yes -o ControlMaster=no -p "$port" \
        "$(id -un)@127.0.0.1")
[[ $revoked_over_existing_transport == *'"reason":"peer_revoked_or_authority_unavailable"'* ]]
ssh -O exit -S "$fixture_dir/control" -p "$port" \
    "$(id -un)@127.0.0.1" >/dev/null 2>&1 || true
if ssh_request "$fixture_dir/peer-a" >/dev/null 2>&1; then
    echo 'revoked peer unexpectedly authenticated' >&2
    exit 1
fi
[[ $(ssh_request "$fixture_dir/peer-b") == *'"host_state":"stopped"'* ]]

if [[ -n ${YVEX_MANAGEMENT_TEST_REMOTE:-} &&
      -n ${YVEX_MANAGEMENT_TEST_ADDRESS:-} ]]; then
    remote_dir=$(ssh -o BatchMode=yes "$YVEX_MANAGEMENT_TEST_REMOTE" \
        'mktemp -d /tmp/yvex-mgmt-client.XXXXXX')
    [[ $remote_dir =~ ^/tmp/yvex-mgmt-client\.[A-Za-z0-9]{6}$ ]]
    printf '[%s]:%s %s\n' "$YVEX_MANAGEMENT_TEST_ADDRESS" "$port" \
        "$(<"$fixture_dir/host.pub")" >"$fixture_dir/remote_known_hosts"
    scp -q -o BatchMode=yes "$fixture_dir/peer-b" \
        "$fixture_dir/remote_known_hosts" \
        "$YVEX_MANAGEMENT_TEST_REMOTE:$remote_dir/"
    remote_request='{"schema":"yvex.management.request.v1","request_id":"3333333333333333333333333333333333333333333333333333333333333333","operation":"device.describe"}'
    remote_result=$(ssh -o BatchMode=yes "$YVEX_MANAGEMENT_TEST_REMOTE" \
        "chmod 600 '$remote_dir/peer-b' '$remote_dir/remote_known_hosts'; printf '%s\\n' '$remote_request' | ssh -T -F /dev/null -o BatchMode=yes -o IdentitiesOnly=yes -o StrictHostKeyChecking=yes -o UserKnownHostsFile='$remote_dir/remote_known_hosts' -o ConnectTimeout=4 -i '$remote_dir/peer-b' -p '$port' '$(id -un)@$YVEX_MANAGEMENT_TEST_ADDRESS'")
    [[ $remote_result == *'"status":"ok"'* &&
       $remote_result == *"$device"* && $remote_result == *"$peer_b"* ]]
fi

kill "$sshd_pid"
wait "$sshd_pid" 2>/dev/null || true
sshd_pid=
/usr/sbin/sshd -D -f "$fixture_dir/sshd_config" -E "$fixture_dir/sshd.log" &
sshd_pid=$!
for attempt in {1..40}; do
    if ssh-keyscan -T 1 -p "$port" 127.0.0.1 >/dev/null 2>&1; then break; fi
    sleep 0.1
done
[[ $(ssh_request "$fixture_dir/peer-b") == *'"host_state":"stopped"'* ]]

printf '[127.0.0.1]:%s %s\n' "$port" "$(<"$fixture_dir/unknown.pub")" \
    >"$fixture_dir/known_hosts"
if ssh_request "$fixture_dir/peer-b" >/dev/null 2>&1; then
    echo 'changed host identity unexpectedly accepted' >&2
    exit 1
fi
printf 'PASS remote_management address=%s:%s device=%s clients=2 host=stopped\n' \
    "${YVEX_MANAGEMENT_TEST_ADDRESS:-127.0.0.1}" "$port" "$device"
