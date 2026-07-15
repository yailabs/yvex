/*
 * tests/test.c - Baseline YVEX test runner.
 *
 * This runner groups non-CUDA unit coverage into domain sections.
 */

#include "test.h"

int yvex_test_status(void);
int yvex_test_error(void);
int yvex_test_version(void);
int yvex_test_log(void);
int yvex_test_fs(void);
int yvex_test_artifact(void);
int yvex_test_artifact_integrity(void);
int yvex_test_gguf(void);
int yvex_test_gguf_artifact_abi(void);
int yvex_test_gguf_layout_integrity(void);
int yvex_test_gguf_qtype_abi(void);
int yvex_test_artifact_naming(void);
int yvex_test_source_manifest(void);
int yvex_test_source_verify(void);
int yvex_test_source_payload(void);
int yvex_test_native_weights(void);
int yvex_test_safetensors_header(void);
int yvex_test_dtype(void);
int yvex_test_tensor_table(void);
int yvex_test_model_descriptor(void);
int yvex_test_deepseek_arch_ir(void);
int yvex_test_transform_ir(void);
int yvex_test_quant_numeric(void);
int yvex_test_quant_execute(void);
int yvex_test_deepseek_tensor_coverage(void);
int yvex_test_weights(void);
int yvex_test_materialize_cpu(void);
int yvex_test_model_gate(void);
int yvex_test_model_ref(void);
int yvex_test_model_registry(void);
int yvex_test_materialize_gate(void);
int yvex_test_tokenizer(void);
int yvex_test_token_input(void);
int yvex_test_prompt(void);
int yvex_test_shape(void);
int yvex_test_graph(void);
int yvex_test_memory_plan(void);
int yvex_test_planner(void);
int yvex_test_backend_cpu(void);
int yvex_test_backend_ops(void);
int yvex_test_engine(void);
int yvex_test_session(void);
int yvex_test_kv(void);
int yvex_test_logits(void);
int yvex_test_runtime_diagnostics(void);
int yvex_test_chat_runtime(void);
int yvex_test_slash_commands(void);
int yvex_test_run_artifacts(void);
int yvex_test_metrics(void);
int yvex_test_trace(void);
int yvex_test_profile(void);
int yvex_test_http(void);
int yvex_test_server(void);
int yvex_test_weight_mapping(void);
int yvex_test_qtype_support(void);
int yvex_test_qwen_adapter(void);
int yvex_test_deepseek_adapter(void);
int yvex_test_conversion_plan(void);
int yvex_test_conversion_payload(void);
int yvex_test_quant_job(void);
int yvex_test_quant_policy(void);
int yvex_test_imatrix(void);
int yvex_test_gguf_emit(void);
int yvex_test_gguf_template(void);

static int run_test(const char *name, int (*fn)(void))
{
    int rc;

    fprintf(stderr, "test: %s\n", name);
    rc = fn();
    if (rc != 0) {
        fprintf(stderr, "FAIL: %s exited %d\n", name, rc);
    }
    return rc;
}

/* Core */
static int run_core(void)
{
    if (run_test("status", yvex_test_status) != 0) return 1;
    if (run_test("error", yvex_test_error) != 0) return 1;
    if (run_test("version", yvex_test_version) != 0) return 1;
    if (run_test("log", yvex_test_log) != 0) return 1;
    return 0;
}

/* Filesystem, artifacts, and GGUF */
static int run_filesystem_artifacts_gguf(void)
{
    if (run_test("fs", yvex_test_fs) != 0) return 1;
    if (run_test("artifact", yvex_test_artifact) != 0) return 1;
    if (run_test("artifact_integrity", yvex_test_artifact_integrity) != 0) return 1;
    if (run_test("gguf", yvex_test_gguf) != 0) return 1;
    if (run_test("gguf_artifact_abi", yvex_test_gguf_artifact_abi) != 0) return 1;
    if (run_test("gguf_layout_integrity", yvex_test_gguf_layout_integrity) != 0) return 1;
    if (run_test("gguf_qtype_abi", yvex_test_gguf_qtype_abi) != 0) return 1;
    if (run_test("artifact_naming", yvex_test_artifact_naming) != 0) return 1;
    if (run_test("source_manifest", yvex_test_source_manifest) != 0) return 1;
    if (run_test("source_verify", yvex_test_source_verify) != 0) return 1;
    if (run_test("source_payload", yvex_test_source_payload) != 0) return 1;
    if (run_test("native_weights", yvex_test_native_weights) != 0) return 1;
    if (run_test("safetensors_header", yvex_test_safetensors_header) != 0) return 1;
    return 0;
}

/* Model and weights */
static int run_model_weights(void)
{
    if (run_test("dtype", yvex_test_dtype) != 0) return 1;
    if (run_test("tensor_table", yvex_test_tensor_table) != 0) return 1;
    if (run_test("model_descriptor", yvex_test_model_descriptor) != 0) return 1;
    if (run_test("deepseek_arch_ir", yvex_test_deepseek_arch_ir) != 0) return 1;
    if (run_test("transform_ir", yvex_test_transform_ir) != 0) return 1;
    if (run_test("quant_numeric", yvex_test_quant_numeric) != 0) return 1;
    if (run_test("quant_execute", yvex_test_quant_execute) != 0) return 1;
    if (run_test("deepseek_tensor_coverage", yvex_test_deepseek_tensor_coverage) != 0) return 1;
    if (run_test("weights", yvex_test_weights) != 0) return 1;
    if (run_test("materialize_cpu", yvex_test_materialize_cpu) != 0) return 1;
    if (run_test("model_gate", yvex_test_model_gate) != 0) return 1;
    if (run_test("model_ref", yvex_test_model_ref) != 0) return 1;
    if (run_test("model_registry", yvex_test_model_registry) != 0) return 1;
    if (run_test("materialize_gate", yvex_test_materialize_gate) != 0) return 1;
    return 0;
}

/* Tokenizer and prompt */
static int run_tokenizer_prompt(void)
{
    if (run_test("tokenizer", yvex_test_tokenizer) != 0) return 1;
    if (run_test("token_input", yvex_test_token_input) != 0) return 1;
    if (run_test("prompt", yvex_test_prompt) != 0) return 1;
    return 0;
}

/* Graph and planner */
static int run_graph_planner(void)
{
    if (run_test("shape", yvex_test_shape) != 0) return 1;
    if (run_test("graph", yvex_test_graph) != 0) return 1;
    if (run_test("memory_plan", yvex_test_memory_plan) != 0) return 1;
    if (run_test("planner", yvex_test_planner) != 0) return 1;
    return 0;
}

/* CPU backend */
static int run_cpu_backend(void)
{
    if (run_test("backend_cpu", yvex_test_backend_cpu) != 0) return 1;
    if (run_test("backend_ops", yvex_test_backend_ops) != 0) return 1;
    return 0;
}

/* Runtime and console */
static int run_runtime_console(void)
{
    if (run_test("engine", yvex_test_engine) != 0) return 1;
    if (run_test("session", yvex_test_session) != 0) return 1;
    if (run_test("kv", yvex_test_kv) != 0) return 1;
    if (run_test("logits", yvex_test_logits) != 0) return 1;
    if (run_test("runtime_diagnostics", yvex_test_runtime_diagnostics) != 0) return 1;
    if (run_test("chat_runtime", yvex_test_chat_runtime) != 0) return 1;
    if (run_test("slash_commands", yvex_test_slash_commands) != 0) return 1;
    if (run_test("run_artifacts", yvex_test_run_artifacts) != 0) return 1;
    return 0;
}

/* Observability */
static int run_observability(void)
{
    if (run_test("metrics", yvex_test_metrics) != 0) return 1;
    if (run_test("trace", yvex_test_trace) != 0) return 1;
    if (run_test("profile", yvex_test_profile) != 0) return 1;
    return 0;
}

/* Server */
static int run_server(void)
{
    if (run_test("http", yvex_test_http) != 0) return 1;
    if (run_test("server", yvex_test_server) != 0) return 1;
    return 0;
}

/* GGUF and model artifact tools */
static int run_gguf_model_artifact_tools(void)
{
    if (run_test("weight_mapping", yvex_test_weight_mapping) != 0) return 1;
    if (run_test("qtype_support", yvex_test_qtype_support) != 0) return 1;
    if (run_test("qwen_adapter", yvex_test_qwen_adapter) != 0) return 1;
    if (run_test("deepseek_adapter", yvex_test_deepseek_adapter) != 0) return 1;
    if (run_test("conversion_plan", yvex_test_conversion_plan) != 0) return 1;
    if (run_test("conversion_payload", yvex_test_conversion_payload) != 0) return 1;
    if (run_test("quant_job", yvex_test_quant_job) != 0) return 1;
    if (run_test("quant_policy", yvex_test_quant_policy) != 0) return 1;
    if (run_test("imatrix", yvex_test_imatrix) != 0) return 1;
    if (run_test("gguf_emit", yvex_test_gguf_emit) != 0) return 1;
    if (run_test("gguf_template", yvex_test_gguf_template) != 0) return 1;
    return 0;
}

int main(void)
{
    if (run_core() != 0) return 1;
    if (run_filesystem_artifacts_gguf() != 0) return 1;
    if (run_model_weights() != 0) return 1;
    if (run_tokenizer_prompt() != 0) return 1;
    if (run_graph_planner() != 0) return 1;
    if (run_cpu_backend() != 0) return 1;
    if (run_runtime_console() != 0) return 1;
    if (run_observability() != 0) return 1;
    if (run_server() != 0) return 1;
    if (run_gguf_model_artifact_tools() != 0) return 1;
    return 0;
}
