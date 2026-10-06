//! One prepared public management invocation. No automatic retry or shell commands.
use std::{path::PathBuf, time::Duration};
use yvex_sdk::{
    management::{Client, Invocation},
    SshConnection,
};
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 9 && args.len() != 10 {
        return Err("usage: management_product PINS KEY ADDRESS PORT USER DEVICE PEER REQUEST_JSON [TIMEOUT_MS]".into());
    }
    let client = Client::new(SshConnection {
        pinned_known_hosts: PathBuf::from(&args[1]),
        enrolled_client_key: PathBuf::from(&args[2]),
        address: args[3].clone(),
        port: args[4].parse()?,
        user: args[5].clone(),
        expected_device_identity: args[6].clone(),
        expected_peer_identity: args[7].clone(),
    })?;
    // A caller must retain the complete invocation/request ID before dispatch.
    let request: Invocation = serde_json::from_slice(&std::fs::read(&args[8])?)?;
    let timeout = args
        .get(9)
        .map(|s| s.parse::<u64>())
        .transpose()?
        .unwrap_or(15000);
    match client
        .with_timeout(Duration::from_millis(timeout))?
        .invoke(&request)
    {
        Ok(observation) => println!("{}", serde_json::to_string(&observation)?),
        Err(error) => {
            println!("{}", serde_json::to_string(&error)?);
            std::process::exit(2);
        }
    }
    Ok(())
}
