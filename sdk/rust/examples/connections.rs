//! Native shared SDK connection client. Never accepts or prints credentials.
//! Explicit trust uses a reviewed candidate file; invocation uses a retained file.
#[cfg(feature = "native-credentials")]
use std::path::PathBuf;
use std::time::Duration;
use yvex_sdk::management::{
    connections::{ConnectionManager, OwnerActionRequest},
    network, Invocation,
};
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let command = args.first().map(String::as_str).unwrap_or("list");
    if command == "probe" {
        let candidate = network::probe(
            args.get(1).ok_or("probe ENDPOINT")?,
            Duration::from_secs(10),
        )?;
        println!("{}", serde_json::to_string(&candidate)?);
        return Ok(());
    }
    #[cfg(feature = "native-credentials")]
    let manager = if let Some(root) = std::env::var_os("YVEX_SDK_CONNECTION_PROFILE_ROOT") {
        ConnectionManager::native_at(PathBuf::from(root))?
    } else {
        ConnectionManager::native()?
    };
    #[cfg(not(feature = "native-credentials"))]
    let manager = ConnectionManager::native()?;
    let result: Result<serde_json::Value, yvex_sdk::management::Error> = (|| {
        Ok(match command {
            "owner-inspect" => serde_json::to_value(ConnectionManager::inspect_owner_invitation(
                std::path::Path::new(args.get(1).ok_or_else(usage)?),
            )?)
            .unwrap(),
            "owner-prepare" => {
                if args.get(3).map(String::as_str) != Some("--invitation-source-verified") {
                    return Err(usage());
                }
                serde_json::to_value(manager.prepare_owner(
                    std::path::Path::new(args.get(1).ok_or_else(usage)?),
                    args.get(2).ok_or_else(usage)?,
                )?)
                .unwrap()
            }
            "owner-list" => serde_json::to_value(manager.owner_list()?).unwrap(),
            "owner-get" => {
                serde_json::to_value(manager.owner_get(args.get(1).ok_or_else(usage)?)?).unwrap()
            }
            "owner-claim" => {
                serde_json::to_value(manager.claim_owner(args.get(1).ok_or_else(usage)?)?).unwrap()
            }
            "owner-status" => {
                serde_json::to_value(manager.owner_status(args.get(1).ok_or_else(usage)?)?).unwrap()
            }
            "owner-connections" => {
                serde_json::to_value(manager.owner_connections(args.get(1).ok_or_else(usage)?)?)
                    .unwrap()
            }
            "owner-action" => {
                let request: OwnerActionRequest = serde_json::from_slice(
                    &std::fs::read(args.get(2).ok_or_else(usage)?).map_err(|_| usage())?,
                )
                .map_err(|_| usage())?;
                serde_json::to_value(
                    manager.owner_action(args.get(1).ok_or_else(usage)?, &request)?,
                )
                .unwrap()
            }
            "owner-action-status" => serde_json::to_value(manager.owner_action_status(
                args.get(1).ok_or_else(usage)?,
                args.get(2).ok_or_else(usage)?,
            )?)
            .unwrap(),
            "owner-forget" => {
                manager.forget_owner(args.get(1).ok_or_else(usage)?)?;
                serde_json::json!({"forgotten":true,"remote_revocation":false})
            }
            "list" => serde_json::to_value(manager.list()?).unwrap(),
            "local" => serde_json::to_value(manager.detect_local()?).unwrap(),
            "get" => {
                serde_json::to_value(manager.get(args.get(1).ok_or_else(|| usage())?)?).unwrap()
            }
            "trust" => {
                if args.get(3).map(String::as_str) != Some("--fingerprint-verified") {
                    return Err(usage());
                }
                let candidate: network::HostCandidate = serde_json::from_slice(
                    &std::fs::read(args.get(1).ok_or_else(|| usage())?).map_err(|_| usage())?,
                )
                .map_err(|_| usage())?;
                serde_json::to_value(
                    manager.trust(&candidate, args.get(2).ok_or_else(|| usage())?)?,
                )
                .unwrap()
            }
            "new-request" => {
                serde_json::to_value(manager.new_pairing(args.get(1).ok_or_else(usage)?)?).unwrap()
            }
            "request" => {
                serde_json::to_value(manager.request_pairing(args.get(1).ok_or_else(|| usage())?)?)
                    .unwrap()
            }
            "status" => {
                serde_json::to_value(manager.pairing_status(args.get(1).ok_or_else(|| usage())?)?)
                    .unwrap()
            }
            "forget" => {
                manager.forget(args.get(1).ok_or_else(|| usage())?)?;
                serde_json::json!({"forgotten":true,"remote_revocation":false})
            }
            "invoke" => {
                let request: Invocation = serde_json::from_slice(
                    &std::fs::read(args.get(2).ok_or_else(|| usage())?).map_err(|_| usage())?,
                )
                .map_err(|_| usage())?;
                serde_json::to_value(
                    manager
                        .client(args.get(1).ok_or_else(|| usage())?)?
                        .invoke(&request)?,
                )
                .unwrap()
            }
            _ => return Err(usage()),
        })
    })();
    match result {
        Ok(value) => println!("{value}"),
        Err(error) => {
            println!("{}", serde_json::to_string(&error)?);
            std::process::exit(2);
        }
    }
    Ok(())
}
fn usage() -> yvex_sdk::management::Error {
    yvex_sdk::management::Error{code:"usage: list|local|get REF|probe HTTPS|trust CANDIDATE_FILE CLIENT_NAME --fingerprint-verified|request REF|status REF|forget REF|invoke REF REQUEST_FILE".into(),dispatch_state:yvex_sdk::management::DispatchState::NotDispatched,reason:None,request_id:None}
}
