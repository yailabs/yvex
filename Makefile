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
#   - ./yvex and ./yvexd are repository-local compiled products.

.DEFAULT_GOAL := all

.PHONY: all info lib cli server cuda-info cuda test-cuda smoke-cuda check-cuda test test-core test-cli test-layout test-docs-surface smoke check check-docs check-guardrails clean

CC ?= cc
AR ?= ar
NVCC ?= nvcc
CUDA_HOME ?= /usr/local/cuda
CUDA_CFLAGS ?=
CUDA_LDFLAGS ?=
YVEX_CUDA_ARCH ?= auto

CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L -Iinclude -I.
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic
LDFLAGS ?=
LDLIBS ?= -ldl

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
LIB_DIR := $(BUILD_DIR)/lib
TEST_DIR := $(BUILD_DIR)/tests

LIBYVEX := $(LIB_DIR)/libyvex.a
YVEX_BIN := ./yvex
YVEXD_BIN := ./yvexd

CORE_SRCS := \
	yvex_core.c \
	yvex_artifact.c \
	gguf/naming.c \
	gguf/gguf.c \
	gguf/tools.c \
	yvex_model.c \
	yvex_backend.c \
	yvex_graph.c \
	yvex_metrics.c \
	yvex_runtime.c \
	yvex_tokenizer.c \
	yvex_chat.c \
	yvex_server.c \
	yvex_model_tools.c \
	gguf/conversion.c \
	gguf/quant.c \
	yvex_source.c

CUDA_SRCS := \
	cuda/cuda_backend.c \
	cuda/cuda_tensor.c \
	cuda/cuda_ops.c \
	cuda/cuda_info.c \
	cuda/cuda_errors.c

CORE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
CUDA_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CUDA_SRCS))
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
	tests/test_model_ref.c \
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

CURRENT_DOCS := README.md AGENTS.md MODEL_ARTIFACTS.md NOTICE.md \
	docs/api.md docs/contract.md docs/spine.md

info:
	@echo "yvex: C local inference engine"
	@echo "status: fixture materialization fixture weight materialization"
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
	@echo "backend_cuda: CUDA backend dynamic driver attachment implemented"
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
	@echo "model_ref: alias-or-path resolver implemented"
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

all: lib cli server

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

test-cli: $(YVEX_BIN) $(YVEXD_BIN) tests/test_cli.sh tests/test_cli_run.sh tests/test_cli_chat.sh tests/test_cli_metrics.sh tests/test_cli_server.sh tests/test_cli_materialize.sh tests/test_cli_materialize_gate.sh tests/test_cli_source_manifest.sh tests/test_cli_native_weights.sh tests/test_cli_gguf_template.sh tests/test_cli_gguf_emit.sh tests/test_cli_tensor_map.sh tests/test_cli_convert.sh tests/test_cli_model_gate.sh tests/test_cli_models.sh tests/test_cli_model_aliases.sh tests/test_cli_quant_job.sh tests/test_cli_quant_policy.sh tests/test_cli_imatrix.sh
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
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_model_aliases.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_quant_job.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_quant_policy.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli_imatrix.sh

test: test-core test-cli

test-layout: tests/test_source_layout.sh
	sh tests/test_source_layout.sh

test-docs-surface: tests/test_docs_surface.sh
	sh tests/test_docs_surface.sh

smoke: test-cli

check: check-docs check-guardrails lib cli server test test-layout test-docs-surface smoke
	@echo "yvex check: ok"

$(LIBYVEX): $(CORE_OBJS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(YVEX_BIN): yvex_cli.c $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(YVEXD_BIN): yvexd.c $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_DIR)/%: tests/%.c $(LIBYVEX) tests/test.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

check-docs:
	@test -f README.md
	@test -f NOTICE.md
	@test -f AGENTS.md
	@test -f MODEL_ARTIFACTS.md
	@test -f docs/spine.md
	@test -f docs/api.md
	@test -f docs/contract.md
	@! find docs -maxdepth 1 -type f -name '*.md' \
		! -name spine.md \
		! -name api.md \
		! -name contract.md \
		-print | grep .
	@test "$$(find docs -maxdepth 1 -type f -name '*.md' | wc -l | tr -d ' ')" = "3"
	@grep -F "YVEX Inner Delivery Spine" docs/spine.md >/dev/null
	@grep -F "internal roadmap" docs/spine.md >/dev/null
	@grep -F "YVEX is a C local runtime" README.md >/dev/null
	@grep -F "Model selection in canonical REPL" docs/spine.md >/dev/null
	@grep -F "docs/api.md, docs/contract.md, docs/spine.md" docs/spine.md >/dev/null
	@grep -F "YVEX API" docs/api.md >/dev/null
	@grep -F "YVEX Runtime Contract" docs/contract.md >/dev/null

check-guardrails:
	@test ! -d docs/spines
	@test ! -d docs/integration
	@test ! -d docs/benchmark
	@test ! -e docs/README.md
	@test ! -e docs/backend-contract.md
	@test ! -e docs/cli-commands.md
	@test ! -e docs/cli-interface-spine.md
	@test ! -e docs/cli-runtime.md
	@test ! -e docs/runtime-filesystem.md
	@test ! -d benches
	@test ! -d examples
	@test ! -d protocols
	@test ! -d src
	@test ! -d cli
	@test ! -d server
	@test ! -e tests/README.md
	@test ! -e include/yvex/sampler.h
	@test ! -d backends
	@test -d cuda
	@test -d gguf
	@test -d models
	@test -d tests/vectors
	@test -f include/yvex/server.h
	@test ! -d fixtures
	@test -f yvex_cli.c
	@test -f yvexd.c
	@test -f yvex_server.c
	@test ! -d ui
	@test ! -d app
	@test ! -d desktop
	@! grep -RIn -E "N[E]T\\.SPINE|N[E]T moves streams|C[L]ORI|c[l]ori-codename|docs/arc[h]ive|c[l]ori_|libc[l]ori|c[l]orid|include/c[l]ori|~/\\.config/c[l]ori|github\\.com/yailabs/c[l]ori|yailabs/c[l]ori" --exclude-dir=.git --exclude-dir=build . >/dev/null
	@! grep -Ei "production-read[y]|implemented infer[e]nce|implemented ser[v]er|supports C[U]DA|supports M[e]tal|supports M[L]X|supports llama\\.cpp|O[p]enAI-compatible ser[v]er" README.md >/dev/null
	@! grep -Ei "benchmark results" README.md | grep -vi "benchmark results: none" >/dev/null

clean:
	@rm -rf $(BUILD_DIR) ./yvex ./yvexd ./*.o
