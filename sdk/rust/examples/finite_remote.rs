//! One independently pinned finite invocation. It does not enroll a key,
//! select a different generation, retry, or qualify YAI Fast Search.
use std::{env, path::PathBuf, time::Duration};
use yvex_sdk::{
    finite::{
        remote::{Invocation, ProducerIdentity, RemoteClient},
        Request,
    },
    SshConnection,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<_> = env::args().collect();
    if args.len() != 11 {
        eprintln!("usage: finite_remote KNOWN_HOSTS CLIENT_KEY ADDRESS PORT USER DEVICE_ID PEER_ID PRODUCER_IDENTITY_JSON REQUEST_JSON TIMEOUT_MS");
        std::process::exit(2);
    }
    let identity: ProducerIdentity = serde_json::from_slice(&std::fs::read(&args[8])?)?;
    let input: Request = serde_json::from_slice(&std::fs::read(&args[9])?)?;
    let client = RemoteClient::new(
        SshConnection {
            pinned_known_hosts: PathBuf::from(&args[1]),
            enrolled_client_key: PathBuf::from(&args[2]),
            address: args[3].clone(),
            port: args[4].parse()?,
            user: args[5].clone(),
            expected_device_identity: args[6].clone(),
            expected_peer_identity: args[7].clone(),
        },
        identity,
    )?
    .with_timeout(Duration::from_millis(args[10].parse()?))?;
    let invocation = Invocation::new(input)?;
    match client.execute(&invocation) {
        Ok(observation) => println!("{}", serde_json::to_string(&observation)?),
        Err(error) => {
            // Correlation and dispatch posture only; never question/context or
            // untrusted producer error text in retained ordinary diagnostics.
            eprintln!("request_id={} {error}", invocation.request_id);
            std::process::exit(1);
        }
    }
    Ok(())
}
