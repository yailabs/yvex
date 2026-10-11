//! Bounded Program P through the public local companion, not private runtime IPC.
//! Prepare and retain an invocation before dispatch; observe a lost outcome by ID.
use std::{fs, io::Write, time::Duration};
use yvex_sdk::management::{
    local::LocalConnection, operations, Client, EmptyInput, Invocation, JobInput,
    OptimizationPlanInput, QualificationReadInput,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    if args.first().map(String::as_str) == Some("prepare") && args.len() == 3 {
        let input: OptimizationPlanInput = serde_json::from_slice(&fs::read(&args[1])?)?;
        let prepared = Invocation::prepare::<operations::OptimizationPlan>(&input)?;
        let mut file = fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&args[2])?;
        file.write_all(&serde_json::to_vec_pretty(prepared.invocation())?)?;
        file.sync_all()?;
        println!("{}", prepared.request_id());
        return Ok(());
    }
    let (connection, _) = LocalConnection::detect(Duration::from_secs(10))?
        .ok_or("public local companion unavailable; no private socket fallback")?;
    let client = Client::local(connection);
    let capabilities = client.management().capabilities(&EmptyInput {})?.value;
    if !capabilities
        .operations
        .iter()
        .any(|op| op.operation == "optimization.plan")
    {
        return Err("producer does not advertise optimization.plan".into());
    }
    let observation = match args.first().map(String::as_str) {
        Some("invoke") if args.len() == 2 => {
            let request: Invocation = serde_json::from_slice(&fs::read(&args[1])?)?;
            if request.operation != "optimization.plan" {
                return Err("this example only dispatches the retained planning request".into());
            }
            // Exactly one dispatch. On failure retain the file and use get;
            // do not create a new request identity or retry in a loop.
            client.invoke(&request)?
        }
        Some("get") if args.len() == 2 => {
            let request = Invocation::prepare::<operations::JobGet>(&JobInput { job_id: args[1].clone() })?;
            client.invoke(request.invocation())?
        }
        Some("evidence") if args.len() == 2 => {
            if !capabilities.operations.iter().any(|op| op.operation == "qualification.get") {
                return Err("producer does not advertise qualification.get".into());
            }
            let request = Invocation::prepare::<operations::QualificationGet>(&QualificationReadInput { id: args[1].clone() })?;
            client.invoke(request.invocation())?
        }
        _ => return Err("usage: optimization prepare INPUT_JSON INVOCATION_JSON | invoke INVOCATION_JSON | get JOB_ID | evidence RECORD_ID".into()),
    };
    println!("{}", serde_json::to_string(&observation)?);
    Ok(())
}
