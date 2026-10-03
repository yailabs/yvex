// Parse actual Rust, including methods and traits; retain physical-function limits.
use std::{fs, path::Path};
use syn::visit::{self, Visit};

#[derive(Default)]
struct NativeReferences(std::collections::BTreeSet<String>);
impl<'ast> Visit<'ast> for NativeReferences {
    fn visit_macro(&mut self, mac: &'ast syn::Macro) {
        // Declarative macro bodies are token trees rather than ExprPaths.
        // Rust's lexer excludes comments and keeps strings opaque.
        self.macro_tokens(mac.tokens.clone());
        visit::visit_macro(self, mac);
    }
    fn visit_expr_path(&mut self, expression: &'ast syn::ExprPath) {
        let segments = &expression.path.segments;
        if segments.len() == 2 && segments[0].ident == "raw" {
            let symbol = segments[1].ident.to_string();
            if symbol.starts_with("yvex_") {
                self.0.insert(symbol);
            }
        }
        visit::visit_expr_path(self, expression);
    }
}

impl NativeReferences {
    fn macro_tokens(&mut self, tokens: proc_macro2::TokenStream) {
        let tokens = tokens.into_iter().collect::<Vec<_>>();
        for token in &tokens {
            if let proc_macro2::TokenTree::Group(group) = token {
                self.macro_tokens(group.stream());
            }
        }
        for window in tokens.windows(4) {
            if let [
                proc_macro2::TokenTree::Ident(owner),
                proc_macro2::TokenTree::Punct(a),
                proc_macro2::TokenTree::Punct(b),
                proc_macro2::TokenTree::Ident(symbol),
            ] = window
                && owner == "raw"
                && a.as_char() == ':'
                && b.as_char() == ':'
                && symbol.to_string().starts_with("yvex_")
            {
                self.0.insert(symbol.to_string());
            }
        }
    }
}

fn native_references(source: &str) -> std::collections::BTreeSet<String> {
    let tree = syn::parse_file(source).expect("production Rust must parse");
    let mut references = NativeReferences::default();
    references.visit_file(&tree);
    references.0
}

#[test]
fn native_consumer_index_uses_rust_syntax_not_text_or_comments() {
    let refs = native_references(concat!(
        "fn example() { raw::yvex_real(); ",
        "let _ = raw::yvex_function_pointer; ",
        "let _ = r#\"raw::yvex_fake()\"#; /* raw::yvex_comment() */ }"
    ));
    assert_eq!(
        refs.into_iter().collect::<Vec<_>>(),
        ["yvex_function_pointer", "yvex_real"]
    );
    let root = Path::new(env!("CARGO_MANIFEST_DIR"));
    let owners = fs::read_to_string(root.join("config/source_owners.tsv")).unwrap();
    let mut sources = serde_json::Map::new();
    for row in owners.lines() {
        let path = row.split('\t').next().unwrap();
        if path.starts_with("src/") && path.ends_with(".rs") {
            let text = fs::read_to_string(root.join(path)).unwrap();
            sources.insert(
                path.into(),
                serde_json::json!({
                "symbols": native_references(&text), "source": text }),
            );
        }
    }
    // A derived, exact-source AST receipt; never another authored ABI allowlist.
    let build = std::env::var_os("YVEX_NATIVE_BUILD_DIR")
        .map(std::path::PathBuf::from)
        .unwrap_or_else(|| root.join("build"));
    let output = build.join("generated/rust_ffi_consumers.json");
    fs::create_dir_all(output.parent().unwrap()).unwrap();
    fs::write(
        output,
        serde_json::to_vec(&serde_json::json!({
        "schema": "yvex.rust.ffi-consumers.v1", "sources": sources }))
        .unwrap(),
    )
    .unwrap();
}

#[test]
fn native_macro_references_preserve_paths_but_exclude_literals() {
    let refs = native_references(
        r#"macro_rules! x { () => { raw::yvex_actual() }; }
        fn f() { vec![raw::yvex_nested(), "raw::yvex_not_reference"]; }"#,
    );
    assert_eq!(
        refs.into_iter().collect::<Vec<_>>(),
        ["yvex_actual", "yvex_nested"]
    );
}

struct Functions {
    maximum: usize,
    violations: Vec<(String, usize)>,
}

impl Functions {
    fn check(&mut self, signature: &syn::Signature, block: &syn::Block) {
        let start = signature.fn_token.span.start().line;
        let end = block.brace_token.span.close().end().line;
        if end - start + 1 > self.maximum {
            self.violations
                .push((signature.ident.to_string(), end - start + 1));
        }
    }
}

impl<'ast> Visit<'ast> for Functions {
    fn visit_item_fn(&mut self, item: &'ast syn::ItemFn) {
        self.check(&item.sig, &item.block);
        visit::visit_item_fn(self, item);
    }
    fn visit_impl_item_fn(&mut self, item: &'ast syn::ImplItemFn) {
        self.check(&item.sig, &item.block);
        visit::visit_impl_item_fn(self, item);
    }
    fn visit_trait_item_fn(&mut self, item: &'ast syn::TraitItemFn) {
        if let Some(block) = &item.default {
            self.check(&item.sig, block);
        }
        visit::visit_trait_item_fn(self, item);
    }
}

fn violations(source: &str, maximum: usize) -> Vec<(String, usize)> {
    let tree = syn::parse_file(source).expect("production Rust must parse independently");
    let mut visitor = Functions {
        maximum,
        violations: Vec::new(),
    };
    visitor.visit_file(&tree);
    visitor.violations
}

#[test]
fn all_authored_production_rust_preserves_physical_function_limits() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"));
    let policy: serde_json::Value =
        serde_json::from_str(&fs::read_to_string(root.join("config/c_policy.json")).unwrap())
            .unwrap();
    let maximum = policy["limits"]["function_lines"].as_u64().unwrap() as usize;
    let owners = fs::read_to_string(root.join("config/source_owners.tsv")).unwrap();
    let mut checked = 0;
    let mut errors = Vec::new();
    for row in owners.lines() {
        let path = row.split('\t').next().unwrap();
        if path.starts_with("src/") && path.ends_with(".rs") {
            checked += 1;
            let source = fs::read_to_string(root.join(path)).unwrap();
            for (function, lines) in violations(&source, maximum) {
                errors.push(format!(
                    "{path}:{function}: {lines} > {maximum} physical lines"
                ));
            }
        }
    }
    assert!(
        checked > 0,
        "Rust production disappeared from membership authority"
    );
    assert!(errors.is_empty(), "{}", errors.join("\n"));
}

#[test]
fn methods_traits_strings_and_comment_lines_do_not_evade_the_guard() {
    let comments = "// retained physical source line\n".repeat(201);
    for source in [
        format!("fn example() {{\n{comments}}}"),
        format!("impl Example {{ fn example() {{\n{comments}}} }}"),
        format!("trait Example {{ fn example() {{\n{comments}}} }}"),
    ] {
        assert_eq!(violations(&source, 200).len(), 1);
    }
    assert!(violations("fn short() { let _ = r#\"} fake fn {{\"#; }", 200).is_empty());
}

#[derive(Default)]
struct ProductBranches {
    exact: std::collections::BTreeSet<String>,
    prefixes: std::collections::BTreeSet<String>,
}
impl<'ast> Visit<'ast> for ProductBranches {
    fn visit_item_mod(&mut self, item: &'ast syn::ItemMod) {
        // Test expectations and example strings are not dispatch evidence.
        if item.attrs.iter().any(|attr| {
            attr.path().is_ident("cfg")
                && attr
                    .meta
                    .require_list()
                    .is_ok_and(|list| list.tokens.to_string() == "test")
        }) {
            return;
        }
        visit::visit_item_mod(self, item);
    }
    fn visit_pat(&mut self, pattern: &'ast syn::Pat) {
        if let syn::Pat::Lit(literal) = pattern
            && let syn::Lit::Str(value) = &literal.lit
        {
            self.exact.insert(value.value());
        }
        visit::visit_pat(self, pattern);
    }
    fn visit_expr_binary(&mut self, expression: &'ast syn::ExprBinary) {
        if matches!(expression.op, syn::BinOp::Eq(_)) {
            for side in [&expression.left, &expression.right] {
                if let syn::Expr::Lit(literal) = side.as_ref()
                    && let syn::Lit::Str(value) = &literal.lit
                {
                    self.exact.insert(value.value());
                }
            }
        }
        visit::visit_expr_binary(self, expression);
    }
    fn visit_expr_method_call(&mut self, expression: &'ast syn::ExprMethodCall) {
        if expression.method == "starts_with"
            && matches!(expression.receiver.as_ref(), syn::Expr::Path(path) if path.path.is_ident("operation"))
            && let Some(syn::Expr::Lit(literal)) = expression.args.first()
            && let syn::Lit::Str(value) = &literal.lit
        {
            self.prefixes.insert(value.value());
        }
        visit::visit_expr_method_call(self, expression);
    }
    fn visit_macro(&mut self, mac: &'ast syn::Macro) {
        if mac.path.is_ident("matches") {
            for token in mac.tokens.clone() {
                if let proc_macro2::TokenTree::Literal(literal) = token
                    && let Ok(value) = syn::parse_str::<syn::LitStr>(&literal.to_string())
                {
                    self.exact.insert(value.value());
                }
            }
        }
        visit::visit_macro(self, mac);
    }
}

#[test]
fn every_active_cli_and_slash_operation_has_a_rust_product_branch() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"));
    let owners = fs::read_to_string(root.join("config/source_owners.tsv")).unwrap();
    let mut branches = ProductBranches::default();
    let mut slash = ProductBranches::default();
    for row in owners.lines() {
        let path = row.split('\t').next().unwrap();
        if path.starts_with("src/cli/") && path.ends_with(".rs") {
            let tree = syn::parse_file(&fs::read_to_string(root.join(path)).unwrap()).unwrap();
            branches.visit_file(&tree);
            if path == "src/cli/rust/chat.rs" {
                for item in &tree.items {
                    if let syn::Item::Fn(function) = item
                        && function.sig.ident == "slash"
                    {
                        slash.visit_item_fn(function);
                    }
                }
            }
        }
    }
    let registry: serde_json::Value =
        serde_json::from_str(include_str!(env!("YVEX_OPERATOR_REGISTRY"))).unwrap();
    let operations = registry["operations"].as_array().unwrap();
    let mut cli_count = 0;
    let mut slash_count = 0;
    for operation in operations {
        let id = operation["operation_id"].as_str().unwrap();
        if operation["CLI_projection"] == true {
            cli_count += 1;
            assert!(
                branches.exact.contains(id)
                    || branches
                        .prefixes
                        .iter()
                        .any(|prefix| id.starts_with(prefix)),
                "active CLI operation has no Rust branch: {id}"
            );
        }
        if operation["slash_projection"] != "none" {
            slash_count += 1;
            assert!(
                slash.exact.contains(id),
                "active slash operation has no Rust branch: {id}"
            );
        }
    }
    assert!(cli_count > 100 && slash_count > 0);
    println!("Rust branch inventory: {cli_count} CLI operations; {slash_count} slash operations");
    // This structural proof complements runtime/contract tests, not replaces them.
}
