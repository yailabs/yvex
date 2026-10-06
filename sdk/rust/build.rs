use std::{env, path::PathBuf};

fn main() {
    let root = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());
    let contract = root.join("contract/finite.json");
    println!("cargo:rerun-if-changed={}", contract.display());
    let value: serde_json::Value =
        serde_json::from_slice(&std::fs::read(contract).unwrap()).unwrap();
    let mut constants = String::new();
    for (name, value) in value["limits"].as_object().unwrap() {
        constants.push_str(&format!(
            "pub const {name}: usize = {};\n",
            value.as_u64().unwrap()
        ));
    }
    constants.push_str(&format!(
        "pub const PRODUCER_SCHEMA: u32 = {};\n",
        value["producer_schema"].as_u64().unwrap()
    ));
    std::fs::write(
        PathBuf::from(env::var_os("OUT_DIR").unwrap()).join("finite_limits.rs"),
        constants,
    )
    .unwrap();
    #[cfg(feature = "finite-decision-native")]
    native(&root, &value);
}

#[cfg(feature = "finite-decision-native")]
fn native(root: &std::path::Path, contract: &serde_json::Value) {
    for key in [
        "YVEX_CLIENT_PREFIX",
        "YVEX_CLIENT_LINK_LIBS",
        "YVEX_SDK_PYTHON",
    ] {
        println!("cargo:rerun-if-env-changed={key}");
    }
    let prefix = PathBuf::from(env::var_os("YVEX_CLIENT_PREFIX")
        .expect("finite-decision-native requires a reviewed public YVEX client installation: YVEX_CLIENT_PREFIX"));
    assert!(prefix.is_absolute(), "YVEX_CLIENT_PREFIX must be absolute");
    let include = prefix.join("include");
    let header = include.join("yvex/finite_decision_producer.h");
    let library = prefix.join("lib/libyvex.a");
    assert!(
        header.is_file() && library.is_file(),
        "public YVEX headers and lib/libyvex.a are required; no source/wire fallback"
    );
    println!("cargo:rerun-if-changed={}", header.display());
    println!(
        "cargo:rerun-if-changed={}",
        include.join("yvex/core.h").display()
    );
    println!("cargo:rerun-if-changed={}", library.display());
    let status = std::process::Command::new(
        env::var_os("YVEX_SDK_PYTHON").unwrap_or_else(|| "python3".into()),
    )
    .arg(root.join("build_support/check_finite_abi.py"))
    .arg("--include-dir")
    .arg(&include)
    .arg("--check")
    .status()
    .expect("public ABI projection guard could not run");
    assert!(
        status.success(),
        "YVEX installed finite producer ABI differs from the reviewed client projection"
    );
    let out = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    let guard = out.join("finite_abi_guard.c");
    let mut assertions = String::from("#include <yvex/finite_decision_producer.h>\n");
    for (record, value) in contract["records"].as_object().unwrap() {
        assertions.push_str(&format!(
            "_Static_assert(sizeof({record}) == {}, \"finite ABI layout mismatch\");\n",
            value["size_64bit"].as_u64().unwrap()
        ));
    }
    assertions.push_str(
        "_Static_assert(sizeof(void *) == 8, \"only qualified 64-bit native ABI is supported\");\n",
    );
    std::fs::write(&guard, assertions).unwrap();
    cc::Build::new()
        .include(&include)
        .file(&guard)
        .flag_if_supported("-std=c11")
        .warnings_into_errors(true)
        .compile("yvex_sdk_finite_abi_guard");
    bindgen::Builder::default()
        .header(header.to_string_lossy())
        .clang_arg(format!("-I{}", include.display()))
        .allowlist_type("yvex_finite_producer_.*|yvex_error|yvex_status")
        .allowlist_function("yvex_finite_producer_execute_local")
        .allowlist_var("YVEX_FINITE_PRODUCER_.*")
        .derive_default(true)
        .derive_debug(false)
        .generate_comments(false)
        .formatter(bindgen::Formatter::None)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("installed public YVEX bindings could not be generated")
        .write_to_file(out.join("finite_native.rs"))
        .unwrap();
    println!(
        "cargo:rustc-link-search=native={}",
        prefix.join("lib").display()
    );
    println!("cargo:rustc-link-lib=static=yvex");
    // Explicit deployment linker dependencies; never discover private source.
    if let Ok(libraries) = env::var("YVEX_CLIENT_LINK_LIBS") {
        for library in libraries.split(',').filter(|_| !libraries.is_empty()) {
            assert!(
                !library.is_empty()
                    && library
                        .bytes()
                        .all(|b| b.is_ascii_alphanumeric() || b == b'_'),
                "invalid native link library"
            );
            println!("cargo:rustc-link-lib={library}");
        }
    }
}
