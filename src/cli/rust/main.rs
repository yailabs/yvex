// The one product entrypoint. Native engine ownership stays below the typed FFI.
#![deny(unsafe_code)]
use std::io::{self, IsTerminal, Write};

fn main() -> std::process::ExitCode {
    let arguments = match std::env::args_os()
        .skip(1)
        .map(|argument| argument.into_string())
        .collect::<Result<Vec<_>, _>>()
    {
        Ok(arguments) => arguments,
        Err(_) => {
            eprintln!("yvex: command arguments must be UTF-8");
            return std::process::ExitCode::from(2);
        }
    };
    let stdout = io::stdout();
    let stderr = io::stderr();
    let fallback = std::env::var("COLUMNS")
        .ok()
        .and_then(|value| value.parse::<usize>().ok())
        .filter(|value| (8..=4096).contains(value))
        .unwrap_or(120);
    let width = yvex::presentation::destination_width(fallback);
    let output = yvex::execute(&arguments, width, stdout.is_terminal());
    let write = if output.diagnostic {
        stderr.lock().write_all(output.text.as_bytes())
    } else {
        stdout.lock().write_all(output.text.as_bytes())
    };
    if let Err(error) = write {
        // A downstream pipe closing is ordinary CLI delivery, never engine success/failure.
        if error.kind() == io::ErrorKind::BrokenPipe
            && arguments.get(..2) != Some(&["management".into(), "protocol".into()])
        {
            return std::process::ExitCode::SUCCESS;
        }
        eprintln!("yvex: output: {error}");
        return std::process::ExitCode::FAILURE;
    }
    std::process::ExitCode::from(output.exit)
}
