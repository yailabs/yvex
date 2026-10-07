// One restricted SSH request; enrollment is local and never grants runtime mutation.
use crate::{Output, client, ffi, registry::Invocation};
use rustix::fs::{FlockOperation, Mode, OFlags, flock, fstat, open};
use serde::Deserialize;
use serde_json::{Value, json};
use std::{
    fs,
    io::{self, BufRead, Read},
    os::unix::fs::MetadataExt,
    path::Path,
};

const KEY_CAP: usize = 2048;
const TRUST_CAP: usize = 65536;
const REQUEST_CAP: usize = 4096;
const HEADER: &str = "# yvex.management.authorized-keys.v1\n";
type Result<T> = std::result::Result<T, &'static str>;

fn valid_identity(text: &str) -> bool {
    text.len() == 64 && text.bytes().all(|byte| byte.is_ascii_hexdigit())
}

fn safe_path(path: &str) -> bool {
    path.starts_with('/')
        && path
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || b"/_-.".contains(&byte))
}

fn decode_key(encoded: &str) -> Result<Vec<u8>> {
    if encoded.is_empty() || !encoded.len().is_multiple_of(4) || encoded.len() >= KEY_CAP {
        return Err("malformed_public_key");
    }
    let mut bytes = Vec::new();
    let mut buffer = 0u32;
    let mut bits = 0;
    for (index, byte) in encoded.bytes().enumerate() {
        if byte == b'=' {
            let padding = encoded.len() - index;
            if !((padding == 1 && bits == 2) || (padding == 2 && bits == 4))
                || !encoded[index..].bytes().all(|byte| byte == b'=')
            {
                return Err("malformed_public_key");
            }
            break;
        }
        let digit = match byte {
            b'A'..=b'Z' => byte - b'A',
            b'a'..=b'z' => byte - b'a' + 26,
            b'0'..=b'9' => byte - b'0' + 52,
            b'+' => 62,
            b'/' => 63,
            _ => return Err("malformed_public_key"),
        };
        buffer = (buffer << 6) | u32::from(digit);
        bits += 6;
        if bits >= 8 {
            bits -= 8;
            bytes.push((buffer >> bits) as u8);
            buffer &= (1 << bits) - 1;
        }
    }
    if buffer != 0
        || bytes.len() != 51
        || bytes[..4] != 11u32.to_be_bytes()
        || &bytes[4..15] != b"ssh-ed25519"
        || bytes[15..19] != 32u32.to_be_bytes()
    {
        return Err("malformed_public_key");
    }
    Ok(bytes)
}

fn encoded_identity(encoded: &str) -> Result<String> {
    ffi::digest(&decode_key(encoded)?).map_err(|_| "public_key_identity_unavailable")
}

fn key_identity(path: &str) -> Result<(String, String)> {
    let bytes = ffi::snapshot(path, KEY_CAP).map_err(|_| "public_key_unavailable")?;
    if bytes.len() <= 12 || bytes.contains(&0) || bytes[..bytes.len() - 1].contains(&b'\n') {
        return Err("malformed_public_key");
    }
    let text = std::str::from_utf8(&bytes).map_err(|_| "malformed_public_key")?;
    let encoded = text
        .strip_prefix("ssh-ed25519 ")
        .ok_or("malformed_public_key")?
        .split_ascii_whitespace()
        .next()
        .ok_or("malformed_public_key")?;
    Ok((encoded_identity(encoded)?, encoded.into()))
}

fn private_parent(path: &str) -> Result<()> {
    let path = Path::new(path);
    if !path.is_absolute() || path.as_os_str().len() >= 4096 || path.file_name().is_none() {
        return Err("unsafe_trust_parent");
    }
    let metadata = fs::symlink_metadata(path.parent().ok_or("unsafe_trust_parent")?)
        .map_err(|_| "unsafe_trust_parent")?;
    if !metadata.is_dir()
        || metadata.uid() != rustix::process::geteuid().as_raw()
        || metadata.mode() & 0o022 != 0
    {
        return Err("unsafe_trust_parent");
    }
    Ok(())
}

fn trust_snapshot(path: &str) -> Result<String> {
    let metadata = fs::symlink_metadata(path).map_err(|_| "trust_file_unavailable_or_malformed")?;
    if !metadata.is_file()
        || metadata.uid() != rustix::process::geteuid().as_raw()
        || metadata.mode() & 0o077 != 0
    {
        return Err("trust_file_unavailable_or_malformed");
    }
    let bytes =
        ffi::snapshot(path, TRUST_CAP).map_err(|_| "trust_file_unavailable_or_malformed")?;
    let text = String::from_utf8(bytes).map_err(|_| "trust_file_malformed")?;
    if !text.starts_with(HEADER) || text.contains('\0') {
        return Err("trust_file_malformed");
    }
    Ok(text)
}

fn trust_line_identity(line: &str) -> Result<&str> {
    if line.len() >= REQUEST_CAP || !line.ends_with('\n') {
        return Err("trust_file_malformed");
    }
    let (entry, identity) = line
        .trim_end_matches('\n')
        .rsplit_once(" yvex-management:")
        .ok_or("trust_file_malformed")?;
    if !valid_identity(identity) {
        return Err("trust_file_malformed");
    }
    let (command, encoded) = entry
        .strip_prefix("restrict,command=\"")
        .and_then(|entry| entry.split_once("\" ssh-ed25519 "))
        .ok_or("trust_file_malformed")?;
    if encoded_identity(encoded).map_err(|_| "trust_file_malformed")? != identity {
        return Err("trust_file_malformed");
    }
    let words: Vec<_> = command.split(' ').collect();
    if words.len() != 6
        || words[1] != "management"
        || !matches!(
            words[2],
            "protocol" | "finite-protocol" | "product-protocol"
        )
        || words[4] != identity
        || ![words[0], words[3], words[5]].into_iter().all(safe_path)
    {
        return Err("trust_file_malformed");
    }
    Ok(identity)
}

fn trust_find(text: &str, peer: &str) -> Result<Option<std::ops::Range<usize>>> {
    let mut offset = HEADER.len();
    let mut found = None;
    // Validate all lines, not just the requested peer. A malformed authority fails closed.
    for line in text[HEADER.len()..].split_inclusive('\n') {
        if trust_line_identity(line)? == peer {
            if found.is_some() {
                return Err("trust_file_malformed");
            }
            found = Some(offset..offset + line.len());
        }
        offset += line.len();
    }
    Ok(found)
}

fn trust_lock(path: &str) -> Result<rustix::fd::OwnedFd> {
    private_parent(path)?;
    let path = format!("{path}.lock");
    if path.len() >= 4096 {
        return Err("trust_file_unavailable_or_malformed");
    }
    let file = open(
        &path,
        OFlags::RDWR | OFlags::CREATE | OFlags::CLOEXEC | OFlags::NOFOLLOW,
        Mode::RUSR | Mode::WUSR,
    )
    .map_err(|_| "trust_file_unavailable_or_malformed")?;
    let metadata = fstat(&file).map_err(|_| "trust_file_unavailable_or_malformed")?;
    if metadata.st_mode & 0o170000 != 0o100000
        || metadata.st_uid != rustix::process::geteuid().as_raw()
        || metadata.st_mode & 0o077 != 0
    {
        return Err("trust_file_unavailable_or_malformed");
    }
    flock(&file, FlockOperation::LockExclusive)
        .map_err(|_| "trust_file_unavailable_or_malformed")?;
    Ok(file)
}

fn change(invocation: &Invocation<'_>, enroll: bool) -> Result<Output> {
    let words = &invocation.positionals;
    let path = &words[1];
    let (peer, line) = if enroll {
        let (peer, encoded) =
            key_identity(&words[0]).map_err(|_| "peer_or_host_identity_mismatch")?;
        if peer != words[3]
            || key_identity(&words[2]).is_err()
            || !safe_path(&words[2])
            || !safe_path(path)
        {
            return Err("peer_or_host_identity_mismatch");
        }
        let executable = std::env::current_exe().map_err(|_| "executable_identity_unavailable")?;
        let executable = executable.to_str().ok_or("unsafe_executable_path")?;
        if !safe_path(executable) {
            return Err("unsafe_executable_path");
        }
        let scope = invocation.value("--scope").unwrap_or("management");
        let protocol = match scope {
            "management" => "protocol",
            "finite-decision" => "finite-protocol",
            "product-management" => "product-protocol",
            _ => return Err("unsupported_peer_scope"),
        };
        let line = format!(
            concat!(
                "restrict,command=\"{} management {} {} {} {}\" ",
                "ssh-ed25519 {} yvex-management:{}\n"
            ),
            executable, protocol, words[2], peer, path, encoded, peer
        );
        if line.len() >= REQUEST_CAP {
            return Err("entry_too_large");
        }
        (peer, line)
    } else {
        if !valid_identity(&words[0]) {
            return Err("malformed_peer_identity");
        }
        (words[0].clone(), String::new())
    };
    let _lock = trust_lock(path)?;
    let mut text = trust_snapshot(path)?;
    let found = trust_find(&text, &peer)?;
    if enroll {
        if found.is_some() {
            return Err("peer_already_enrolled");
        }
        if text.len() + line.len() > TRUST_CAP {
            return Err("trust_file_full");
        }
        text.push_str(&line);
    } else {
        text.replace_range(found.ok_or("peer_not_enrolled")?, "");
    }
    ffi::publish(path, text.as_bytes(), true).map_err(|_| "trust_publication_failed")?;
    Ok(Output::standard(
        format!(
            "{} ssh-ed25519:sha256:{peer}\n",
            if enroll { "enrolled" } else { "revoked" }
        ),
        0,
    ))
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Request {
    schema: String,
    request_id: String,
    operation: String,
}

fn parse_request(bytes: &[u8]) -> Result<Request> {
    if bytes.len() >= REQUEST_CAP || !bytes.ends_with(b"\n") {
        return Err("malformed_request");
    }
    let request: Request = serde_json::from_slice(bytes).map_err(|_| "malformed_request")?;
    if request.schema != "yvex.management.request.v1"
        || !valid_identity(&request.request_id)
        || request.operation.len() >= 48
        || request.operation.contains('\0')
    {
        return Err("malformed_request");
    }
    Ok(request)
}

fn protocol_response(request: &Request, device: &str, peer: &str) -> Value {
    let mut result = json!({"schema":"yvex.management.response.v1", "request_id":request.request_id,
        "status":"ok", "device_identity":format!("ssh-ed25519:sha256:{device}"),
        "authenticated_peer":format!("ssh-ed25519:sha256:{peer}")});
    match request.operation.as_str() {
        "device.describe" => {}
        "host.status" => {
            if let Ok(status) = client::management_status() {
                result["host_state"] = json!("running");
                result["engine_count"] = json!(status.engine_count);
                result["loaded_engine_count"] = json!(status.loaded_engine_count);
                result["session_count"] = json!(status.session_count);
            } else {
                result["host_state"] = json!(if ffi::default_socket()
                    .ok()
                    .is_some_and(|path| fs::symlink_metadata(path).is_err())
                {
                    "stopped"
                } else {
                    "unavailable"
                });
            }
        }
        _ => {
            result["status"] = json!("refused");
            result["reason"] = json!("unsupported_operation");
        }
    }
    result
}

pub(crate) fn authenticate(invocation: &Invocation<'_>, scope: &str) -> Result<(String, String)> {
    let nonempty = |key| std::env::var_os(key).is_some_and(|value| !value.is_empty());
    let words = &invocation.positionals;
    if !nonempty("SSH_CONNECTION")
        || nonempty("SSH_ORIGINAL_COMMAND")
        || nonempty("SSH_TTY")
        || !valid_identity(&words[1])
    {
        return Err("restricted_ssh_required");
    }
    let (device, _) = key_identity(&words[0])?;
    let peer = &words[1];
    if !safe_path(&words[2]) {
        return Err("peer_revoked_or_authority_unavailable");
    }
    let trust = trust_snapshot(&words[2])?;
    let range = trust_find(&trust, peer)?.ok_or("peer_revoked_or_authority_unavailable")?;
    let command = trust[range]
        .split_once("\" ssh-ed25519 ")
        .ok_or("trust_file_malformed")?
        .0;
    if command.split(' ').nth(2) != Some(scope) {
        return Err("peer_revoked_or_authority_unavailable");
    }
    Ok((device, peer.clone()))
}

fn protocol(invocation: &Invocation<'_>) -> Result<Output> {
    let (device, peer) = match authenticate(invocation, "protocol") {
        Ok(identity) => identity,
        Err("restricted_ssh_required") => return Err("restricted_ssh_required"),
        Err(_) => {
            return Ok(Output::standard(
                format!(
                    "{}\n",
                    json!({
            "schema":"yvex.management.response.v1", "request_id":null,
            "status":"refused", "reason":"peer_revoked_or_authority_unavailable"})
                ),
                0,
            ));
        }
    };
    let response = {
        let mut input = io::stdin().lock().take(REQUEST_CAP as u64);
        let mut bytes = Vec::new();
        if input
            .read_until(b'\n', &mut bytes)
            .map_err(|_| "request_read_failed")?
            == 0
        {
            return Err("request_unavailable");
        }
        match parse_request(&bytes) {
            Ok(request) => protocol_response(&request, &device, &peer),
            Err(_) => json!({"schema":"yvex.management.response.v1", "request_id":null,
                "status":"refused", "reason":"malformed_request"}),
        }
    };
    Ok(Output::standard(format!("{response}\n"), 0))
}

pub(crate) fn dispatch(invocation: &Invocation<'_>) -> Result<Output> {
    match invocation.operation.operation_id.as_str() {
        "management.identity" => {
            let (identity, _) = key_identity(&invocation.positionals[0])?;
            Ok(Output::standard(
                format!("ssh-ed25519:sha256:{identity}\n"),
                0,
            ))
        }
        "management.trust.init" => {
            let path = &invocation.positionals[0];
            private_parent(path)?;
            ffi::publish(path, HEADER.as_bytes(), false)
                .map_err(|_| "trust_file_exists_or_unavailable")?;
            Ok(Output::standard(String::new(), 0))
        }
        "management.peer.enroll" => change(invocation, true),
        "management.peer.revoke" => change(invocation, false),
        "management.protocol" => protocol(invocation),
        _ => Err("unsupported_management_operation"),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn request_is_exact_bounded_and_duplicate_safe() {
        let good = format!(
            "{{\"schema\":\"yvex.management.request.v1\",\"request_id\":\"{}\",
            \"operation\":\"device.describe\"}}\n",
            "1".repeat(64)
        );
        assert!(parse_request(good.as_bytes()).is_ok());
        for bad in [
            good.replace("device.describe", &"a".repeat(48)),
            good.replace("\"operation\":", "\"extra\":true,\"operation\":"),
            good.replace(
                "\"operation\":",
                "\"operation\":\"host.status\",\"operation\":",
            ),
            good.trim_end().into(),
            " ".repeat(REQUEST_CAP),
        ] {
            assert!(parse_request(bad.as_bytes()).is_err());
        }
    }
    #[test]
    fn command_paths_do_not_admit_shell_syntax() {
        assert!(safe_path("/usr/local/bin/yvex"));
        for bad in ["relative", "/tmp/a b", "/tmp/$(id)", "/tmp/é", "/tmp/a\nb"] {
            assert!(!safe_path(bad));
        }
    }
}
