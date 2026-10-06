//! Explicitly pinned read-only YVEX management. This example does not enroll
//! peers, discover untrusted devices or start/stop a YVEX host.

use std::{env, path::PathBuf};
use yvex_sdk::{ManagementClient, SshConnection};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<_> = env::args().collect();
    if args.len() != 9 || !matches!(args[8].as_str(), "device.describe" | "host.status") {
        eprintln!("usage: management KNOWN_HOSTS CLIENT_KEY ADDRESS PORT USER DEVICE_ID PEER_ID device.describe|host.status");
        std::process::exit(2);
    }
    let connection = SshConnection {
        pinned_known_hosts: PathBuf::from(&args[1]),
        enrolled_client_key: PathBuf::from(&args[2]),
        address: args[3].clone(),
        port: args[4].parse()?,
        user: args[5].clone(),
        expected_device_identity: args[6].clone(),
        expected_peer_identity: args[7].clone(),
    };
    let client = ManagementClient::new(connection)?;
    let value = match args[8].as_str() {
        "device.describe" => serde_json::to_value(client.device_describe()?)?,
        "host.status" => serde_json::to_value(client.host_status()?)?,
        _ => unreachable!(),
    };
    println!("{}", serde_json::to_string(&value)?);
    Ok(())
}
