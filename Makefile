# YVEX - Build and validation entrypoint
#
# File: Makefile
# Layer: build system
#
# Purpose:
#   Builds the YVEX C core library, filesystem/artifact/GGUF/model/tokenizer/
#   graph planning/backend/session layers, CLI bootstrap, and tests. Also runs
#   documentation and guardrail validation.
#
# Primary commands:
#   make info
#   make lib
#   make cli
#   make server
#   make cuda-info
#   make cuda
#   make test-cuda
#   make check-cuda
#   make test
#   make test-core
#   make test-cli
#   make smoke
#   make check
#   make clean
#
# Interface policy:
#   - YVEX is CLI-only.
#   - ./yvex and ./yvexd are repository-local launchers.
#   - build/bin/yvex and build/bin/yvexd remain compiled products.

.PHONY: info lib cli server cuda-info cuda test-cuda smoke-cuda check-cuda test test-core test-cli check-launchers smoke check check-docs check-guardrails clean

CC ?= cc
AR ?= ar
NVCC ?= nvcc
CUDA_HOME ?= /usr/local/cuda
CUDA_CFLAGS ?=
CUDA_LDFLAGS ?=
YVEX_CUDA_ARCH ?= auto

CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic
LDFLAGS ?=
LDLIBS ?= -ldl

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
LIB_DIR := $(BUILD_DIR)/lib
BIN_DIR := $(BUILD_DIR)/bin
TEST_DIR := $(BUILD_DIR)/tests

LIBYVEX := $(LIB_DIR)/libyvex.a
YVEX_BIN := $(BIN_DIR)/yvex
YVEXD_BIN := $(BIN_DIR)/yvexd

CORE_SRCS := \
	src/core/version.c \
	src/core/status.c \
	src/core/error.c \
	src/core/log.c \
	src/fs/paths.c \
	src/fs/run_dir.c \
	src/artifact/artifact.c \
	src/artifact/range.c \
	src/formats/gguf.c \
	src/model/dtype.c \
	src/model/role.c \
	src/model/tensor_table.c \
	src/model/descriptor.c \
	src/model/weights.c \
	src/model/materialize.c \
	src/model/materialize_report.c \
	src/tokenizer/tokenizer.c \
	src/tokenizer/vocab.c \
	src/tokenizer/special.c \
	src/tokenizer/encode.c \
	src/tokenizer/decode.c \
	src/tokenizer/prompt.c \
	src/graph/graph.c \
	src/graph/value.c \
	src/graph/op.c \
	src/graph/builder.c \
	src/graph/shape.c \
	src/graph/dump.c \
	src/graph/planner.c \
	src/graph/memory_plan.c \
	src/backend/backend.c \
	src/backend/cpu_backend.c \
	src/backend/cpu_tensor.c \
	src/backend/cpu_ops.c \
	src/session/engine.c \
	src/session/session.c \
	src/session/state.c \
	src/session/kv.c \
	src/session/logits.c \
	src/session/runtime_diagnostics.c \
	src/chat/chat.c \
	src/chat/repl.c \
	src/chat/slash.c \
	src/chat/run_command.c \
	src/chat/status_line.c \
	src/metrics/metrics.c \
	src/metrics/trace.c \
	src/metrics/profile.c \
	src/metrics/run_artifacts.c \
	src/metrics/time.c \
	src/metrics/json_writer.c \
	src/server/server.c \
	src/server/http.c \
	src/server/router.c \
	src/server/handlers.c \
	src/server/server_metrics.c \
	src/tools/artifact_naming.c \
	src/tools/artifact_naming_report.c \
	src/tools/conversion.c \
	src/tools/conversion_plan.c \
	src/tools/conversion_emit.c \
	src/tools/conversion_payload.c \
	src/tools/conversion_report.c \
	src/tools/gguf_emit.c \
	src/tools/gguf_emit_metadata.c \
	src/tools/gguf_emit_tensor.c \
	src/tools/gguf_emit_report.c \
	src/tools/gguf_template.c \
	src/tools/gguf_template_compare.c \
	src/tools/gguf_template_report.c \
	src/tools/gguf_template_validate.c \
	src/tools/imatrix.c \
	src/tools/imatrix_json.c \
	src/tools/imatrix_report.c \
	src/tools/imatrix_validate.c \
	src/tools/materialize_gate.c \
	src/tools/materialize_gate_json.c \
	src/tools/materialize_gate_report.c \
	src/tools/model_gate.c \
	src/tools/model_gate_json.c \
	src/tools/model_gate_report.c \
	src/tools/model_registry.c \
	src/tools/model_registry_json.c \
	src/tools/model_registry_scan.c \
	src/tools/model_registry_report.c \
	src/tools/native_weights.c \
	src/tools/native_weight_report.c \
	src/tools/quant_job.c \
	src/tools/quant_job_json.c \
	src/tools/quant_job_report.c \
	src/tools/quant_policy.c \
	src/tools/quant_policy_from_template.c \
	src/tools/quant_policy_json.c \
	src/tools/quant_policy_report.c \
	src/tools/quant_policy_validate.c \
	src/tools/safetensors.c \
	src/tools/safetensors_json.c \
	src/tools/source_manifest.c \
	src/tools/source_manifest_json.c \
	src/tools/source_manifest_scan.c \
	src/tools/weight_mapping.c \
	src/tools/weight_mapping_report.c \
	src/tools/qtype_support.c \
	src/tools/adapters/deepseek_adapter.c \
	src/tools/adapters/qwen_adapter.c \
	src/tools/quantizers/q8_0_quant.c

CUDA_SRCS := \
	backends/cuda/cuda_backend.c \
	backends/cuda/cuda_tensor.c \
	backends/cuda/cuda_ops.c \
	backends/cuda/cuda_info.c \
	backends/cuda/cuda_errors.c

CORE_OBJS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
CUDA_OBJS := $(patsubst backends/%.c,$(OBJ_DIR)/backends/%.o,$(CUDA_SRCS))
CORE_OBJS += $(CUDA_OBJS)

TEST_SRCS := \
	tests/test_status.c \
	tests/test_error.c \
	tests/test_version.c \
	tests/test_log.c \
	tests/test_fs.c \
	tests/test_artifact.c \
	tests/test_gguf.c \
	tests/test_dtype.c \
	tests/test_tensor_table.c \
	tests/test_model_descriptor.c \
	tests/test_weights.c \
	tests/test_materialize_cpu.c \
	tests/test_tokenizer.c \
	tests/test_prompt.c \
	tests/test_shape.c \
	tests/test_graph.c \
	tests/test_memory_plan.c \
	tests/test_planner.c \
	tests/test_backend_cpu.c \
	tests/test_backend_ops.c \
	tests/test_engine.c \
	tests/test_session.c \
	tests/test_kv.c \
	tests/test_logits.c \
	tests/test_runtime_diagnostics.c \
	tests/test_chat_runtime.c \
	tests/test_slash_commands.c \
	tests/test_metrics.c \
	tests/test_trace.c \
	tests/test_profile.c \
	tests/test_run_artifacts.c \
	tests/test_http.c \
	tests/test_server.c \
	tests/test_artifact_naming.c \
	tests/test_weight_mapping.c \
	tests/test_qtype_support.c \
	tests/test_qwen_adapter.c \
	tests/test_conversion_plan.c \
	tests/test_conversion_payload.c \
	tests/test_quant_job.c \
	tests/test_quant_policy.c \
	tests/test_imatrix.c \
	tests/test_gguf_emit.c \
	tests/test_gguf_template.c \
	tests/test_materialize_gate.c \
	tests/test_model_gate.c \
	tests/test_model_registry.c \
	tests/test_deepseek_adapter.c \
	tests/test_safetensors_header.c \
	tests/test_native_weights.c \
	tests/test_source_manifest.c

TEST_BINS := $(patsubst tests/%.c,$(TEST_DIR)/%,$(TEST_SRCS))

CUDA_TEST_SRCS := \
	tests/test_cuda_info.c \
	tests/test_cuda_tensor.c \
	tests/test_cuda_ops.c \
	tests/test_cuda_parity.c \
	tests/test_materialize_cuda.c

CUDA_TEST_BINS := $(patsubst tests/%.c,$(TEST_DIR)/%,$(CUDA_TEST_SRCS))

CURRENT_DOCS := README.md NOTICE.md docs/README.md docs/spine.md \
	docs/api.md docs/backend-contract.md docs/runtime-filesystem.md \
	docs/cli-runtime.md docs/cli-commands.md

info:
	@echo "yvex: C local inference engine"
	@echo "status: M0 fixture weight materialization"
	@echo "interface: CLI-only"
	@echo "library: libyvex.a"
	@echo "filesystem: implemented"
	@echo "artifact: open/read implemented"
	@echo "gguf: metadata/tensor directory parsing implemented"
	@echo "model: descriptor-only implemented"
	@echo "tokenizer: fixture encode/decode implemented"
	@echo "prompt: default renderer implemented"
	@echo "graph: partial planning implemented"
	@echo "planner: estimate-only implemented"
	@echo "backend: CPU reference implemented"
	@echo "backend_cuda: L0 dynamic driver attachment implemented"
	@echo "weights: fixture materialization implemented"
	@echo "engine: runtime object skeleton implemented"
	@echo "session: lifecycle skeleton implemented"
	@echo "run: accepted-only runtime shell implemented"
	@echo "chat: accepted-only REPL shell implemented"
	@echo "metrics: runtime collector implemented"
	@echo "trace: JSONL writer implemented"
	@echo "profile: JSON writer implemented"
	@echo "run_artifacts: metrics/trace/profile files implemented"
	@echo "source_manifest: provenance JSON writer implemented"
	@echo "native_weights: safetensors header inventory implemented"
	@echo "gguf_template: contract validator implemented"
	@echo "gguf_emit: controlled GGUF writer implemented"
	@echo "conversion: open-weight selected tensor bridge implemented"
	@echo "model_registry: local model alias registry implemented"
	@echo "quant_job: external quantization job manifest implemented"
	@echo "qtype_support: conversion support matrix implemented"
	@echo "weight_mapping: tensor adapter contract implemented"
	@echo "quant_policy: manifest validator implemented"
	@echo "imatrix: calibration artifact manifest implemented"
	@echo "server_binary: yvexd shell implemented"
	@echo "server_endpoints: health/metrics/models status implemented"
	@echo "server_generation: not implemented"
	@echo "kv: unavailable skeleton implemented"
	@echo "logits: unavailable skeleton implemented"
	@echo "generation: unsupported"
	@echo "inference: not implemented"
	@echo "cuda: tensor movement and F32 embed parity implemented when driver/device are available"
	@echo "server: yvexd status shell implemented"

lib: $(LIBYVEX)

cli: $(YVEX_BIN)

server: $(YVEXD_BIN)

cuda-info: $(YVEX_BIN)
	@echo "nvcc: $$(command -v $(NVCC) >/dev/null 2>&1 && command -v $(NVCC) || echo unavailable)"
	@echo "CUDA_HOME: $(CUDA_HOME)"
	@echo "YVEX_CUDA_ARCH: $(YVEX_CUDA_ARCH)"
	$(YVEX_BIN) cuda-info

cuda: lib cli server $(CUDA_TEST_BINS)
	@echo "yvex cuda build: dynamic CUDA Driver API path"

test-cuda: cuda
	$(YVEX_BIN) cuda-info >/dev/null
	@set -e; for test_bin in $(CUDA_TEST_BINS); do \
		echo "$$test_bin"; \
		"$$test_bin"; \
	done

smoke-cuda: cuda
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_cuda.sh

check-cuda: cuda-info test-cuda smoke-cuda
	@echo "yvex check-cuda: ok"

test-core: $(TEST_BINS)
	@set -e; for test_bin in $(TEST_BINS); do \
		echo "$$test_bin"; \
		"$$test_bin"; \
	done

test-cli: $(YVEX_BIN) $(YVEXD_BIN) tests/test_cli.sh tests/test_cli_run.sh tests/test_cli_chat.sh tests/test_cli_metrics.sh tests/test_cli_server.sh tests/test_cli_materialize.sh tests/test_cli_materialize_gate.sh tests/test_cli_source_manifest.sh tests/test_cli_native_weights.sh tests/test_cli_gguf_template.sh tests/test_cli_gguf_emit.sh tests/test_cli_tensor_map.sh tests/test_cli_convert.sh tests/test_cli_model_gate.sh tests/test_cli_models.sh tests/test_cli_quant_job.sh tests/test_cli_quant_policy.sh tests/test_cli_imatrix.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_run.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_chat.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_metrics.sh
	YVEXD_BIN=$(YVEXD_BIN) sh tests/test_cli_server.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_materialize.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_materialize_gate.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_source_manifest.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_native_weights.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_gguf_template.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_gguf_emit.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_tensor_map.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_convert.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_model_gate.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_models.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_quant_job.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_quant_policy.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_imatrix.sh

test: test-core test-cli

check-launchers: $(YVEX_BIN) $(YVEXD_BIN) tests/test_cli_root_launchers.sh
	@test -x yvex
	@test -x yvexd
	YVEX_BIN=$(YVEX_BIN) YVEXD_BIN=$(YVEXD_BIN) sh tests/test_cli_root_launchers.sh

smoke: test-cli check-launchers

check: check-docs check-guardrails lib cli server test smoke
	@echo "yvex check: ok"

$(LIBYVEX): $(CORE_OBJS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/backends/%.o: backends/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(YVEX_BIN): cli/yvex_cli.c $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(YVEXD_BIN): server/yvexd.c $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_DIR)/%: tests/%.c $(LIBYVEX) tests/test.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

check-docs:
	@test -f README.md
	@test -f NOTICE.md
	@test -f docs/README.md
	@test -f docs/spine.md
	@test -f docs/api.md
	@test -f docs/backend-contract.md
	@test -f docs/runtime-filesystem.md
	@test -f docs/cli-interface-spine.md
	@test -f docs/cli-runtime.md
	@test -f docs/cli-commands.md
	@! find docs -maxdepth 1 -type f -name '*.md' \
		! -name README.md \
		! -name spine.md \
		! -name api.md \
		! -name backend-contract.md \
		! -name runtime-filesystem.md \
		! -name cli-interface-spine.md \
		! -name cli-runtime.md \
		! -name cli-commands.md \
		-print | grep .
	@grep -F "YVEX Implementation Spine" docs/spine.md >/dev/null
	@grep -F "YVEX is CLI-only" docs/spine.md >/dev/null
	@grep -F "YVEX is a C local inference engine" README.md >/dev/null
	@grep -F "Completed Milestones" docs/spine.md >/dev/null
	@grep -F "C1 - GGUF metadata and tensor directory" docs/spine.md >/dev/null
	@grep -F "### C1 - GGUF metadata and tensor directory" docs/spine.md >/dev/null
	@grep -F "D0 - Tensor and model layer" docs/spine.md >/dev/null
	@grep -F "E0 - Tokenizer and prompt rendering" docs/spine.md >/dev/null
	@grep -F "F0 - Graph and planner" docs/spine.md >/dev/null
	@grep -F "G0 - CPU reference backend" docs/spine.md >/dev/null
	@grep -F "H0 - Engine and session runtime" docs/spine.md >/dev/null
	@grep -F "I0 - CLI run/chat runtime" docs/spine.md >/dev/null
	@grep -F "J0 - Metrics and tracing" docs/spine.md >/dev/null
	@grep -F "K0 - yvexd server/provider" docs/spine.md >/dev/null
	@grep -F "L0 - CUDA/DGX Spark backend" docs/spine.md >/dev/null
	@grep -F "Implemented by:" docs/spine.md >/dev/null
	@grep -F "YVEX API" docs/api.md >/dev/null
	@grep -F "YVEX Backend Contract" docs/backend-contract.md >/dev/null
	@grep -F "YVEX Runtime Filesystem" docs/runtime-filesystem.md >/dev/null
	@grep -F "YVEX CLI Runtime" docs/cli-runtime.md >/dev/null
	@grep -F "YVEX CLI Command Index" docs/cli-commands.md >/dev/null
	@grep -F "CUDA / DGX Spark Track" docs/backend-contract.md >/dev/null

check-guardrails:
	@test ! -d docs/spines
	@test ! -d docs/integration
	@test ! -d docs/benchmark
	@test ! -d benches
	@test ! -d examples
	@test ! -d protocols
	@test ! -e src/README.md
	@test ! -e tests/README.md
	@test ! -e include/yvex/sampler.h
	@test -d backends/cuda
	@test -f include/yvex/server.h
	@test -d src/server
	@test ! -d fixtures
	@test -d cli
	@test -f cli/yvex_cli.c
	@test ! -d ui
	@test ! -d app
	@test ! -d desktop
	@! grep -RIn -E "N[E]T\\.SPINE|N[E]T moves streams|C[L]ORI|c[l]ori-codename|docs/arc[h]ive|c[l]ori_|libc[l]ori|c[l]orid|include/c[l]ori|~/\\.config/c[l]ori|github\\.com/yailabs/c[l]ori|yailabs/c[l]ori" --exclude-dir=.git --exclude-dir=build . >/dev/null
	@! grep -Ei "production-read[y]|implemented infer[e]nce|implemented ser[v]er|supports C[U]DA|supports M[e]tal|supports M[L]X|supports llama\\.cpp|O[p]enAI-compatible ser[v]er" README.md >/dev/null
	@! grep -Ei "benchmark results" README.md | grep -vi "benchmark results: none" >/dev/null

clean:
	@rm -rf $(BUILD_DIR)
