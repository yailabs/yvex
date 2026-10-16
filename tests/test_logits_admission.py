#!/usr/bin/env python3
"""Observe logits acquisition without opening a model or accelerator.

Compile the actual orchestration owner with a test-only acquisition symbol.
An intentional pre-open refusal checks backend/limits and empty publication.
The mutation control restores the missing backend and device-limit propagation.
"""
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = r'''
#include <yvex/internal/logits.h>
#include <yvex/internal/runtime.h>
#include <yvex/internal/runtime_capacity.h>
#include <stdio.h>
#include <string.h>
static yvex_backend_kind expected_backend;
static unsigned mismatches, calls, stage, capacity_calls, profile_calls, context_calls;
int yvex_test_logits_acquire(yvex_runtime_cleanup_lease **lease,
    const yvex_model_engine_open_request *mr,
    const yvex_runtime_session_open_request *sr, yvex_model_engine **model,
    yvex_runtime_execution_session **session,
    yvex_model_engine_failure *failure, yvex_error *error)
{
    ++calls;
    if (!lease || *lease || !model || *model || !session || *session ||
        !failure || !mr || !sr) return YVEX_ERR_INVALID_ARG;
    if (mr->residency_backend != expected_backend || sr->backend != expected_backend ||
        mr->maximum_device_bytes != 4096ull || sr->maximum_device_bytes != 4096ull ||
        mr->maximum_host_bytes != 8192ull || sr->maximum_host_bytes != 8192ull ||
        strcmp(mr->target_id, "fixture-target") ||
        strcmp(mr->artifact_path, "/not-opened-artifact") ||
        strcmp(mr->runtime_binding_path, "/not-opened-binding")) ++mismatches;
    if (!stage) {
        yvex_error_set(error, YVEX_ERR_CANCELLED, "test.logits.acquisition", "pre-open refusal");
        return YVEX_ERR_CANCELLED;
    }
    return YVEX_OK;
}
int yvex_test_logits_capacity(yvex_model_engine *model,
    yvex_runtime_execution_session *session, const yvex_runtime_capacity_options *options,
    yvex_runtime_capacity *out, yvex_graph_attention_capacity_plan **attention,
    yvex_error *error)
{
    (void)model; (void)session; (void)error;
    ++capacity_calls;
    if (options->backend != expected_backend || options->context_capacity != 16ull ||
        options->prefill_chunk_tokens != 8ull || options->concurrent_sequences != 1ull ||
        options->maximum_host_bytes != 8192ull || options->maximum_device_bytes != 4096ull ||
        options->sampling_requirement != YVEX_EXECUTION_SAMPLING_NOT_INVOKED) ++mismatches;
    memset(out, 0, sizeof(*out));
    out->physical_rows = 7ull;
    *attention = NULL;
    return YVEX_OK;
}
int yvex_test_logits_profile(const yvex_runtime_execution_profile_derivation *input,
    yvex_runtime_execution_profile *out, yvex_error *error)
{
    (void)error;
    ++profile_calls;
    if (input->backend != expected_backend || !input->workload ||
        input->sampling_requirement != YVEX_EXECUTION_SAMPLING_NOT_INVOKED ||
        input->evidence != YVEX_EXECUTION_EVIDENCE_PRODUCTION) ++mismatches;
    memset(out, 0, sizeof(*out));
    out->engine_generation = 123ull;
    return YVEX_OK;
}
int yvex_test_logits_input(yvex_transformer_input **out, const char *path,
    const yvex_transformer_input_limits *limits, yvex_error *error)
{
    (void)path; (void)limits; (void)error;
    *out = NULL;
    return YVEX_OK;
}
int yvex_test_logits_transformer(yvex_runtime_transformer_context **out,
    yvex_model_engine *model, yvex_runtime_execution_session *session,
    const yvex_runtime_transformer_options *options,
    unsigned long long *workspace, yvex_error *error)
{
    (void)model; (void)session; (void)workspace;
    ++context_calls;
    *out = NULL;
    if (options->workspace_token_capacity != 7ull || options->context_capacity != 16ull ||
        !options->execution_profile || options->execution_profile->engine_generation != 123ull ||
        options->maximum_host_bytes != 8192ull || options->maximum_device_bytes != 4096ull) ++mismatches;
    yvex_error_set(error, YVEX_ERR_CANCELLED, "test.logits.acquisition", "pre-context refusal");
    return YVEX_ERR_CANCELLED;
}
int main(void)
{
    for (stage = 0u; stage < 2u; ++stage) {
    for (unsigned b = 0; b < 2u; ++b) {
        expected_backend = b ? YVEX_BACKEND_KIND_CUDA : YVEX_BACKEND_KIND_CPU;
        yvex_logits_operator_request request = {.target="fixture-target",
            .artifact_path="/not-opened-artifact", .runtime_binding_path="/not-opened-binding",
            .input_path="/not-opened-input", .backend=expected_backend,
            .prefill_tokens=8ull, .prefill_chunk_tokens=8ull, .context_capacity=16ull,
            .maximum_host_bytes=8192ull, .maximum_device_bytes=4096ull};
        yvex_logits_operator_result result = {0};
        yvex_runtime_cleanup_lease *cleanup = NULL;
        yvex_error error = {0};
        int rc = yvex_runtime_logits_operator_execute(&request, &result, &cleanup, &error);
        if (rc != YVEX_ERR_CANCELLED || result.completed || result.raw_logits || cleanup ||
            strcmp(error.where, "test.logits.acquisition")) return 2;
        yvex_runtime_logits_operator_result_release(&result);
    }
    }
    printf("{\"calls\":%u,\"mismatches\":%u}\n", calls, mismatches);
    return calls == 4u && capacity_calls == 2u && profile_calls == 2u &&
        context_calls == 2u && !mismatches ? 0 : 1;
}
'''


def main():
    subprocess.run(["make", "-j2", "lib", "build/generated/link_build_config"], cwd=ROOT, check=True,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    link = dict(line.split("=", 1) for line in
                (ROOT / "build/generated/link_build_config").read_text().splitlines())
    owner = ROOT / "src/runtime/logits_operator.c"
    original = owner.read_text()
    changed = original.replace(".residency_backend = request->backend,",
                               ".residency_backend = YVEX_BACKEND_KIND_CPU,")
    changed = changed.replace(".maximum_device_bytes = request->maximum_device_bytes,",
                              ".maximum_device_bytes = 0ull,")
    changed = changed.replace("transformer_options.workspace_token_capacity = workspace_tokens;",
                              "transformer_options.workspace_token_capacity = 0ull;")
    assert changed != original, "mutation control no longer targets acquisition"
    with tempfile.TemporaryDirectory(prefix="yvex-logits-admission-") as directory:
        root = Path(directory)
        fixture = root / "fixture.c"
        fixture.write_text(FIXTURE)
        for label, source, expected in (("corrected", original, 0), ("mutation", changed, 1)):
            translation = root / (label + ".c")
            translation.write_text(source)
            binary = root / label
            argv = [*shlex.split(link["cc"]), "-std=c11",
                    "-D_POSIX_C_SOURCE=200809L", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    "-Dyvex_runtime_cleanup_lease_acquire=yvex_test_logits_acquire",
                    "-Dyvex_runtime_capacity_derive=yvex_test_logits_capacity",
                    "-Dyvex_runtime_execution_profile_derive=yvex_test_logits_profile",
                    "-Dyvex_transformer_input_open_file=yvex_test_logits_input",
                    "-Dyvex_runtime_transformer_context_open=yvex_test_logits_transformer",
                    "-I.", "-Iinclude", "-Ibuild/generated", str(translation), str(fixture),
                    "build/lib/libyvex.a", *shlex.split(link["ldflags"]),
                    *shlex.split(link["ldlibs"]), "-o", str(binary)]
            compiled = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True)
            assert compiled.returncode == 0, compiled.stderr
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
            assert result.returncode == expected, (label, result.stdout, result.stderr)
            facts = json.loads(result.stdout)
            assert facts["calls"] == 4 and ((facts["mismatches"] == 0) == (expected == 0)), facts
    print("PASS: CPU/CUDA acquisition, both limits, derived nonsampling profile/workspace and mutation control; no model/GPU")


if __name__ == "__main__":
    main()
