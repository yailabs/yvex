//! One public C-client call. Not a YAI Case admission or model-quality test.
use yvex_sdk::finite;

fn main() {
    let args = std::env::args().skip(1).collect::<Vec<_>>();
    if args.len() != 2 {
        eprintln!("usage: finite ABSOLUTE_SOCKET REQUEST_JSON_FILE");
        std::process::exit(2);
    }
    let bytes = std::fs::read(&args[1]).expect("request file unavailable");
    let request: finite::Request = serde_json::from_slice(&bytes).expect("invalid finite request");
    match finite::execute_local(std::path::Path::new(&args[0]), &request) {
        Ok(result) => println!("{}", serde_json::to_string(&result).unwrap()),
        Err(error) => {
            eprintln!("{error}");
            std::process::exit(1);
        }
    }
}
