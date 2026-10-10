// Bind the shell to compiler-derived C layouts and Make-owned native inputs.
use std::{env, path::PathBuf, process::Command};

fn main() {
    let root = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());
    let build = env::var_os("YVEX_NATIVE_BUILD_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| root.join("build"));
    let build = if build.is_absolute() {
        build
    } else {
        root.join(build)
    };
    let archive = build.join("lib/libyvex.a");
    assert!(
        archive.is_file(),
        "build the C engine with make lib before Cargo"
    );
    let generated = build.join("generated/operator/registry.json");
    assert!(
        generated.is_file(),
        "run make generate-operator-registry before Cargo"
    );
    println!("cargo:rerun-if-env-changed=YVEX_NATIVE_BUILD_DIR");
    println!("cargo:rerun-if-changed={}", archive.display());
    println!("cargo:rerun-if-changed={}", generated.display());
    println!(
        "cargo:rustc-env=YVEX_OPERATOR_REGISTRY={}",
        generated.display()
    );
    let provenance = build.join("generated/build_commit.h");
    println!("cargo:rerun-if-changed={}", provenance.display());
    let provenance = std::fs::read_to_string(provenance).expect("Make-owned native provenance");
    for key in [
        "YVEX_BUILD_COMMIT",
        "YVEX_BUILD_SOURCE_TREE",
        "YVEX_BUILD_SOURCE_STATE",
        "YVEX_BUILD_SOURCE_DELTA_IDENTITY",
        "YVEX_BUILD_IDENTITY",
        "YVEX_BUILD_SOURCE_ROOT",
    ] {
        let prefix = format!("#define {key} \"");
        let value = provenance
            .lines()
            .find_map(|line| line.strip_prefix(&prefix))
            .and_then(|value| value.strip_suffix('"'))
            .expect("complete native provenance");
        println!("cargo:rustc-env={key}={value}");
    }
    println!(
        "cargo:rustc-link-search=native={}",
        build.join("lib").display()
    );
    println!("cargo:rustc-link-lib=static=yvex");
    native_link_inputs(&build);
    gguf_reference_bindings(&build);
    let bindings = native_standard_headers(native_bindings(&root))
        .generate()
        .expect("compile the actual installed C contracts");
    bindings
        .write_to_file(PathBuf::from(env::var_os("OUT_DIR").unwrap()).join("ffi.rs"))
        .expect("write generated private FFI projection");
}

fn gguf_reference_bindings(build: &std::path::Path) {
    let prefix = build.join("external/gguf-reference");
    for name in [
        "receipt.json",
        "include/gguf.h",
        "include/ggml.h",
        "lib/libggml-base.a",
    ] {
        let path = prefix.join(name);
        assert!(path.is_file(), "run make gguf-reference-dependency");
        println!("cargo:rerun-if-changed={}", path.display());
    }
    println!(
        "cargo:rustc-link-search=native={}",
        prefix.join("lib").display()
    );
    println!("cargo:rustc-link-lib=static=ggml-base");
    println!(
        "cargo:rustc-link-lib={}",
        if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("macos") {
            "c++"
        } else {
            "stdc++"
        }
    );
    native_standard_headers(
        bindgen::Builder::default()
            .header(prefix.join("include/gguf.h").to_str().unwrap())
            .allowlist_function("gguf_(init_from_file|free|get_.*|find_key)")
            .allowlist_function("ggml_(free|get_tensor|n_dims|nbytes)")
            .derive_default(true)
            .layout_tests(false),
    )
    .generate()
    .expect("pinned official GGUF header")
    .write_to_file(PathBuf::from(env::var_os("OUT_DIR").unwrap()).join("gguf_reference.rs"))
    .expect("write official-reader FFI");
}

fn native_link_inputs(build: &std::path::Path) {
    let config = build.join("generated/rust_build_config");
    println!("cargo:rerun-if-changed={}", config.display());
    let text = std::fs::read_to_string(config).expect("Make-owned Rust build configuration");
    let identity = text
        .lines()
        .find_map(|line| line.strip_prefix("shell-identity="))
        .expect("shell build identity");
    println!("cargo:rustc-env=YVEX_SHELL_BUILD_IDENTITY={identity}");
    for (name, fallback) in [
        ("YVEX_NATIVE_LDFLAGS", ""),
        ("YVEX_NATIVE_LDLIBS", "-ldl -pthread -lm -lz"),
    ] {
        println!("cargo:rerun-if-env-changed={name}");
        let flags = env::var(name).unwrap_or_else(|_| {
            text.lines()
                .find_map(|line| {
                    line.strip_prefix(if name.ends_with("LDLIBS") {
                        "ldlibs="
                    } else {
                        "ldflags="
                    })
                })
                .unwrap_or(fallback)
                .to_owned()
        });
        let flags =
            shlex::split(&flags).expect("native link flags must have balanced shell quoting");
        darwin_sanitizer_inputs(&flags);
        // rustc passes -nodefaultlibs. GNU's driver consequently omits its
        // automatic sanitizer runtimes even when -fsanitize reaches the link.
        // Keep them explicit and before the ordinary native/system libraries.
        if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("linux")
            && env::var("CARGO_CFG_TARGET_ENV").as_deref() == Ok("gnu")
        {
            let sanitizers = flags
                .iter()
                .filter_map(|flag| flag.strip_prefix("-fsanitize="))
                .flat_map(|value| value.split(','))
                .collect::<std::collections::BTreeSet<_>>();
            for (sanitizer, library) in [
                ("address", "asan"),
                ("undefined", "ubsan"),
                ("thread", "tsan"),
                ("leak", "lsan"),
            ] {
                if sanitizers.contains(sanitizer)
                    && !(sanitizer == "leak" && sanitizers.contains("address"))
                {
                    println!("cargo:rustc-link-lib=dylib={library}");
                }
            }
        }
        let mut flags = flags.into_iter();
        while let Some(flag) = flags.next() {
            if flag == "-framework" {
                let framework = flags.next().expect("native framework flag requires a name");
                println!("cargo:rustc-link-lib=framework={framework}");
            } else if let Some(library) = flag
                .strip_prefix("-l")
                .filter(|library| !library.is_empty())
            {
                println!("cargo:rustc-link-lib={library}");
            } else if let Some(path) = flag.strip_prefix("-L").filter(|path| !path.is_empty()) {
                println!("cargo:rustc-link-search=native={path}");
            } else {
                println!("cargo:rustc-link-arg={flag}");
            }
        }
    }
}

fn darwin_sanitizer_inputs(flags: &[String]) {
    if env::var("CARGO_CFG_TARGET_OS").as_deref() != Ok("macos") {
        return;
    }
    let sanitizers = flags
        .iter()
        .filter_map(|flag| flag.strip_prefix("-fsanitize="))
        .flat_map(|value| value.split(','))
        .collect::<std::collections::BTreeSet<_>>();
    for (sanitizer, runtime) in [
        ("address", "asan"),
        ("undefined", "ubsan"),
        ("thread", "tsan"),
    ] {
        if !sanitizers.contains(sanitizer) {
            continue;
        }
        // rustc's -nodefaultlibs also suppresses Apple's compiler-rt inputs.
        // Ask the selected native compiler for its runtime, not an SDK guess.
        println!("cargo:rerun-if-env-changed=CC");
        let compiler = env::var("CC").unwrap_or_else(|_| "cc".into());
        let words = shlex::split(&compiler).expect("native compiler must have balanced quoting");
        let (program, arguments) = words.split_first().expect("native compiler is required");
        let library = format!("libclang_rt.{runtime}_osx_dynamic.dylib");
        let output = Command::new(program)
            .args(arguments)
            .arg(format!("-print-file-name={library}"))
            .output()
            .expect("query native sanitizer runtime");
        assert!(
            output.status.success(),
            "native sanitizer runtime query failed"
        );
        let path = PathBuf::from(String::from_utf8(output.stdout).unwrap().trim());
        assert!(
            path.is_file(),
            "requested native sanitizer runtime is unavailable"
        );
        println!("cargo:rustc-link-arg={}", path.display());
        println!(
            "cargo:rustc-link-arg=-Wl,-rpath,{}",
            path.parent().unwrap().display()
        );
    }
}

fn native_bindings(root: &std::path::Path) -> bindgen::Builder {
    bindgen::Builder::default()
        .header_contents(
            "yvex_shell.h",
            concat!(
                "#include <yvex/core.h>\n#include <yvex/server.h>\n#include <yvex/catalog.h>\n",
                "#include <yvex/finite_decision_producer.h>\n",
                "#include <yvex/internal/media_target.h>\n",
                "#include <yvex/internal/model_target.h>\n",
                "#include <yvex/internal/server_loader.h>\n",
                "#include <yvex/internal/backend.h>\n",
                "#include <yvex/internal/source_catalog.h>\n",
                "#include <yvex/internal/source_acquisition.h>\n",
                "#include <yvex/internal/source_payload.h>\n",
                "#include <yvex/internal/registry.h>\n",
                "#include <yvex/internal/materialization.h>\n",
                "#include <yvex/internal/graph.h>\n#include <yvex/internal/quant_numeric.h>\n",
                "#include <yvex/internal/runtime_operator.h>\n",
                "#include <yvex/internal/moe.h>\n#include <yvex/internal/transformer.h>\n",
                "#include <yvex/internal/decode.h>\n#include <yvex/internal/logits.h>\n",
                "#include <yvex/internal/sampling.h>\n",
                "#include <yvex/internal/generation.h>\n",
                "#include <yvex/internal/media.h>\n",
                "#include <yvex/internal/gguf_writer.h>\n",
                "#include <yvex/internal/artifact.h>\n",
                "#include <yvex/internal/artifact_catalog.h>\n",
                "#include <yvex/internal/model.h>\n",
                "#include <yvex/internal/model_lifecycle.h>\n",
                "#include <yvex/internal/model_preparation.h>\n",
                "#include <yvex/optimization.h>\n",
                "#include <yvex/internal/runtime_capacity.h>\n",
                "#include <yvex/internal/source_distribution.h>\n",
                "#include <yvex/tokenizer.h>\n#include <yvex/internal/family_catalog.h>\n",
                "#include <yvex/internal/core.h>\n",
                "#include <yvex/internal/platform.h>\n#include <time.h>\n"
            ),
        )
        .clang_arg(format!("-I{}", root.join("include").display()))
        .allowlist_function(concat!(
            "yvex_(version_.*|status_name|client_.*|provider_.*|",
            "backend_kind_name|server_session_state_name)"
        ))
        .allowlist_function("yvex_model_library_.*")
        .allowlist_function("yvex_model_preparation_.*")
        .allowlist_function("yvex_optimization_.*")
        .allowlist_function("yvex_runtime_private_memory_capacity")
        .allowlist_function("yvex_runtime_capacity_preflight")
        .allowlist_function("yvex_artifact_catalog_(open|close|count|at|next)")
        .allowlist_function("yvex_source_acquisition_provenance_(paths|read|resolve)")
        .allowlist_function("yvex_source_acquisition_(target_find|default_patterns)")
        .allowlist_function("yvex_source_(provider_path|selection_identity|representation_verify_local)")
        .allowlist_function("yvex_model_remote_inspect_policy")
        .allowlist_function(concat!(
            "yvex_source_acquisition_(operation_.*|process_.*|reconcile|",
            "terminal|lifecycle_name|health_name)"
        ))
        .allowlist_function("yvex_source_acquisition_lock")
        .allowlist_function("yvex_source_acquisition_try_lock")
        .allowlist_function("yvex_source_acquisition_reopen")
        .allowlist_function("yvex_platform_process_argument_count")
        .allowlist_function("yvex_core_timestamp_utc")
        .allowlist_function("gmtime_r|strftime")
        .allowlist_function("yvex_source_acquisition_(observe|scan|inspect)")
        .allowlist_function("yvex_remote_(model_(search|inspect|kind_name)|catalog_.*|file_kind_name)")
        .allowlist_function("yvex_local_catalog_(open|close|source_(count|at)|package_(count|at))")
        .allowlist_function("yvex_model_support_stage_name")
        .allowlist_function(concat!("yvex_model_target_(find|catalog_(count|at)|class_(count|at)|",
            "summary_get|candidate_(count|at)|report_(build|close))"))
        .allowlist_function("yvex_model_(storage_(inspect|report_free)|source_evict|local_evict)")
        .allowlist_function("yvex_source_export_local")
        .allowlist_function("yvex_source_(locator_(parse|kind_name)|representation_resolve_local|import_local)")
        .allowlist_function("yvex_model_local_content_resolve")
        .allowlist_function(concat!(
            "yvex_model_remote_(selection_resolve|source_resolve|revision_resolve|",
            "materialize|adopt_existing)"
        ))
        .allowlist_function("yvex_model_source_file_materialize")
        .allowlist_function("yvex_source_(register_reference|target_identity_find_repository)")
        .allowlist_function("yvex_source_(manifest_write_json|status_name|target_identity_find)")
        .allowlist_function("yvex_source_(payload_(budget_default|verify_snapshot|trust_class_name))")
        .allowlist_function("yvex_source_report_(request_prepare|build)")
        .allowlist_function("yvex_native_weight_table_(open|close|count|at|find|summary)")
        .allowlist_function("yvex_native_dtype_name")
        .allowlist_function("yvex_model_ref_(resolve|clear|registry_entry_view)")
        .allowlist_function("yvex_model_ref_verify_integrity")
        .allowlist_function("yvex_model_metadata_snapshot_read")
        .allowlist_function("yvex_model_registry_compare_metadata")
        .allowlist_function(concat!("yvex_model_registry_(open|close|remove|remove_exact|save|",
            "scan_root|scan_free|find|create|verify|derive|default_path)"))
        .allowlist_function(
            "yvex_(paths_(default|project)|operator_paths_.*|run_dir_(prepare|create))",
        )
        .allowlist_function("yvex_component_target_profile")
        .allowlist_function("yvex_model_context_(open|close)")
        .allowlist_function("yvex_plan_(create|close)")
        .allowlist_function("yvex_model_context_vocab_size")
        .allowlist_function("yvex_weight_table_(materialize_report|close)")
        .allowlist_function("yvex_weight_status_name")
        .allowlist_function("yvex_backend_get_memory_stats")
        .allowlist_function(concat!("yvex_model_(arch|name|context_length|role_count|",
            "total_storage_bytes|unsupported_tensor_accounting_count)"))
        .allowlist_function("yvex_backend_tensor_(alloc|release)")
        .allowlist_function("yvex_tensor_(table_(count|at)|role_name|range_validate)")
        .allowlist_function("yvex_dtype_name")
        .allowlist_function("yvex_arch_name")
        .allowlist_function("yvex_family_tokenizer_open")
        .allowlist_function("yvex_tokenizer_(from_gguf|kind_of|kind_name|support_of|support_name)")
        .allowlist_function("yvex_tokenizer_(plan_summary_get|vocab_size|token_at|chat_template)")
        .allowlist_function("yvex_tokenizer_(bos_id|eos_id|pad_id|unk_id)")
        .allowlist_function(
            "yvex_tokenizer_(encode|decode|encode_result_clear|decode_result_clear)",
        )
        .allowlist_function("yvex_(tokenize_text|detokenize_ids|tokens_clear)")
        .allowlist_function("yvex_(prompt_render|rendered_prompt_free)")
        .allowlist_function("yvex_content_part_seal")
        .allowlist_function("yvex_token_input_(parse_explicit|from_ids|validate_bounds)")
        .allowlist_function("yvex_(account_.*|accounts_(run|capture)_provider_command)")
        .allowlist_function("yvex_backend_report_build")
        .allowlist_type("yvex_backend_resource_fact")
        .allowlist_function("yvex_backend_(open|close_checked|supports|status_of|kind_parse)")
        .allowlist_function("yvex_backend_(status_name|capability_name|operation_variant_name)")
        .allowlist_function(
            "yvex_backend_(capability_state_name|capability_reason_name|bundle_admission_name)",
        )
        .allowlist_function("yvex_server_(create|start|serve|stop|finish|close|socket_path)")
        .allowlist_function("yvex_server_(event_next|event_kind_name|get_summary)")
        .allowlist_function("yvex_finite_producer_execute_local")
        .allowlist_function("yvex_server_registry_.*")
        .allowlist_function("yvex_artifact_(open|close)")
        .allowlist_function("yvex_artifact_sha256_hex_bytes")
        .allowlist_function("yvex_artifact_integrity_check_path")
        .allowlist_function("yvex_(materialize_gate_.*|materialize_scope_name|materialize_backend_status_name)")
        .allowlist_function("yvex_materialize_failure_class_name")
        .allowlist_function("yvex_(model_gate_.*|model_support_level_name)")
        .allowlist_function("yvex_gguf_(emit_controlled|emit_status_name|template_.*)")
        .allowlist_function("yvex_conversion_(plan_write_json|emit_gguf)")
        .allowlist_function("yvex_qtype_support_.*")
        .allowlist_function("yvex_quant_(policy_.*|selector_kind_name|qtype_name|job_.*)")
        .allowlist_function("yvex_graph_(execution_find|component_variant_find)")
        .allowlist_function("yvex_graph_attention_operator_.*")
        .allowlist_function("yvex_transformer_operator_execute")
        .allowlist_function("yvex_runtime_(moe|decode|logits|sampling)_operator_(execute|result_release)")
        .allowlist_function("yvex_runtime_generation_(operator_(execute|result_release)|stop_reason_name)")
        .allowlist_function("yvex_runtime_profile_(mode|phase|counter)_name")
        .allowlist_function("yvex_runtime_component_api_get")
        .allowlist_function("yvex_runtime_av_generate")
        .allowlist_function("yvex_runtime_media_request_specialize")
        .allowlist_function("yvex_media_avi_publish")
        .allowlist_function("yvex_error_set")
        .allowlist_function("yvex_tensor_table_(from_gguf|close)")
        .allowlist_function("yvex_runtime_(cleanup_lease_close|binding_.*|benchmark_.*)")
        .allowlist_function("yvex_quant_(plan_(summary_get|decision_at|file_write|file_validate)|execute)")
        .allowlist_function("yvex_quant_(digest_sink_(create|release|adapter)|executor_options_default)")
        .allowlist_function("yvex_quant_metrics_rmse")
        .allowlist_function("yvex_gguf_writer_plan_summary_get")
        .allowlist_function("yvex_gguf_(file_sink_.*|roundtrip_(options_default|validate))")
        .allowlist_function("yvex_imatrix_(manifest_.*|format_.*|status_.*)")
        .allowlist_function("yvex_weight_mapping_.*")
        .allowlist_type("yvex_gguf_qtype_id")
        .allowlist_type("yvex_runtime_(quality_status|lifecycle_phase|benchmark_statistic)")
        .allowlist_function("yvex_core_(file_(read_snapshot|publish_noreplace|publish_replace)|allocate)")
        .allowlist_function("yvex_gguf_(open|close|tensor_count|tensor_at|header_view|alignment|tensor_data_offset)")
        .allowlist_function("yvex_gguf_(metadata_(count|key|value)|value_.*)")
        .allowlist_var("YVEX_.*")
        .derive_default(true)
        .layout_tests(true)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
}

fn native_standard_headers(mut builder: bindgen::Builder) -> bindgen::Builder {
    // A system libclang need not have a matching clang executable/resource tree.
    // Resolve builtin standard headers from the selected native C toolchain.
    println!("cargo:rerun-if-env-changed=CC");
    let compiler = env::var("CC").unwrap_or_else(|_| "cc".into());
    let mut words = compiler.split_whitespace();
    if let Some(program) = words.next()
        && let Ok(output) = Command::new(program)
            .args(words)
            .arg("-print-resource-dir")
            .output()
        && output.status.success()
        && let Ok(directory) = String::from_utf8(output.stdout)
        && PathBuf::from(directory.trim()).is_absolute()
        && PathBuf::from(directory.trim()).is_dir()
    {
        // Replace libclang's builtin tree rather than adding a duplicate stdint.h
        // whose include_next guard can hide the SDK's integer typedefs.
        return builder
            .clang_arg("-resource-dir")
            .clang_arg(directory.trim());
    }
    let mut words = compiler.split_whitespace();
    if let Some(program) = words.next()
        && let Ok(output) = Command::new(program)
            .args(words)
            .arg("-print-file-name=include")
            .output()
        && output.status.success()
        && let Ok(directory) = String::from_utf8(output.stdout)
        && PathBuf::from(directory.trim()).is_absolute()
        && PathBuf::from(directory.trim()).is_dir()
    {
        builder = builder.clang_arg("-isystem").clang_arg(directory.trim());
    }
    builder
}
