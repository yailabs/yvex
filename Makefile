# YVEX - Build and validation entrypoint
#
# Builds the YVEX C library, root binaries, CUDA kernel unit, and tests.
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

.PHONY: all info lib cli server cuda-info cuda-kernels cuda test-cuda smoke-cuda check-cuda test test-core test-cli test-layout test-code-natural test-docs-surface test-surface smoke check check-docs check-guardrails clean

CC ?= cc
AR ?= ar
NVCC ?= nvcc
CUDA_HOME ?= /usr/local/cuda
NVCCFLAGS ?=
CUDA_LDFLAGS ?=
YVEX_CUDA_ARCH ?= auto
NVCC_AVAILABLE := $(shell command -v $(NVCC) >/dev/null 2>&1 && echo yes || echo no)

CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L -Iinclude -I.
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic
LDFLAGS ?=
LDLIBS ?= -ldl
TEST_CPPFLAGS := $(CPPFLAGS) -Itests

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
	yvex_artifact_identity.c \
	yvex_artifact_integrity.c \
	yvex_fs.c \
	gguf/naming.c \
	gguf/gguf.c \
	gguf/tools.c \
	yvex_model.c \
	yvex_backend.c \
	yvex_graph.c \
	yvex_metrics.c \
	yvex_profile.c \
	yvex_runtime.c \
	yvex_prefill.c \
	yvex_kv.c \
	yvex_decode.c \
	yvex_logits.c \
	yvex_sampling.c \
	yvex_generation.c \
	yvex_token_input.c \
	yvex_tokenizer.c \
	yvex_chat.c \
	yvex_server.c \
	yvex_model_artifacts.c \
	yvex_eval.c \
	yvex_bench.c \
	gguf/conversion.c \
	gguf/quant.c \
	yvex_source.c

CUDA_SRCS := \
	cuda/cuda_backend.c \
	cuda/cuda_tensor.c \
	cuda/cuda_ops.c \
	cuda/cuda_info.c \
	cuda/cuda_errors.c

CUDA_CU_SRCS := \
	cuda/cuda_kernels.cu

CUDA_ARCH_FLAG := $(if $(filter auto,$(YVEX_CUDA_ARCH)),,-arch=$(YVEX_CUDA_ARCH))
CUDA_PTX := $(patsubst %.cu,$(OBJ_DIR)/%.ptx,$(CUDA_CU_SRCS))
CUDA_PTX_C := $(OBJ_DIR)/cuda/cuda_kernels_ptx.c
CUDA_PTX_OBJ := $(OBJ_DIR)/cuda/cuda_kernels_ptx.o

CORE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
CUDA_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CUDA_SRCS))
CORE_OBJS += $(CUDA_OBJS)

ifeq ($(NVCC_AVAILABLE),yes)
CPPFLAGS += -DYVEX_HAVE_CUDA_KERNEL_PTX=1
CORE_OBJS += $(CUDA_PTX_OBJ)
endif

TEST_RUNNER := $(TEST_DIR)/test
CUDA_TEST_RUNNER := $(TEST_DIR)/test_cuda

TEST_UNIT_SRCS := $(sort $(wildcard tests/unit/*.c))
TEST_UNIT_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_UNIT_SRCS))

CUDA_TEST_UNIT_SRCS := $(sort $(wildcard tests/unit/cuda/*.c))
CUDA_TEST_UNIT_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CUDA_TEST_UNIT_SRCS))

CLI_TEST := tests/cli.sh

CURRENT_DOCS := README.md AGENTS.md MODEL_ARTIFACTS.md NOTICE.md \
	docs/api.md docs/contract.md docs/operator-runbook.md docs/spine.md

info:
	@echo "yvex: C local inference engine"
	@echo "status: selected tensor materialization, engine weight attachment, fixture graph execution, real selected graph segments, explicit token input boundary, prefill state foundation, minimal KV binding, decode/logits/sampling diagnostics, and bounded diagnostic generation loop"
	@echo "interface: CLI-only"
	@echo "library: libyvex.a"
	@echo "filesystem: implemented"
	@echo "artifact: open/read implemented"
	@echo "gguf: metadata/tensor directory parsing implemented"
	@echo "model: descriptor-only implemented"
	@echo "tokenizer: fixture encode/decode implemented"
	@echo "token_input: explicit token boundary implemented"
	@echo "prefill_state: segment-summary foundation and minimal KV binding implemented"
	@echo "prompt: default renderer implemented"
	@echo "graph: partial planning, deterministic fixture execution, selected embedding partial execution, selected embedding RMSNorm segment execution, standalone RoPE, attention, matmul, and MLP primitives, controlled block, and controlled layer scheduler implemented"
	@echo "planner: estimate-only implemented"
	@echo "backend: CPU reference implemented"
	@echo "backend_cuda: CUDA backend dynamic driver attachment implemented"
	@echo "weights: fixture materialization implemented"
	@echo "engine: runtime object skeleton implemented"
	@echo "session: lifecycle diagnostics, engine attachment observer, and KV ownership implemented"
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
	@echo "kv: minimal session-owned append/read boundary implemented"
	@echo "decode: bounded diagnostic state step implemented"
	@echo "logits: bounded diagnostic buffer implemented"
	@echo "sampling: bounded greedy sampler implemented"
	@echo "generation: bounded diagnostic loop available; full model unsupported"
	@echo "inference: not implemented"
	@echo "cuda: tensor movement and F32/F16 embed, RMSNorm, RoPE, attention, matmul, and MLP primitives implemented when driver/device are available"
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

cuda-kernels: $(CUDA_PTX_OBJ)
	@echo "yvex cuda kernels: built from $(CUDA_CU_SRCS)"

cuda: cuda-kernels lib cli server $(CUDA_TEST_RUNNER)
	@echo "yvex cuda build: dynamic CUDA Driver API path plus CUDA kernel PTX"

test-cuda: cuda
	$(YVEX_BIN) cuda-info >/dev/null
	$(CUDA_TEST_RUNNER)

smoke-cuda: cuda
	YVEX_BIN=$(YVEX_BIN) YVEXD_BIN=$(YVEXD_BIN) sh $(CLI_TEST) --cuda

check-cuda: cuda-info test-cuda smoke-cuda
	@echo "yvex check-cuda: ok"

test-core: $(TEST_RUNNER)
	$(TEST_RUNNER)

test-cli: $(YVEX_BIN) $(YVEXD_BIN) $(CLI_TEST)
	YVEX_BIN=$(YVEX_BIN) YVEXD_BIN=$(YVEXD_BIN) sh $(CLI_TEST)

test: test-core test-cli

test-layout: tests/test_source_layout.sh
	sh tests/test_source_layout.sh

test-code-natural: tests/test_code_natural.sh
	sh tests/test_code_natural.sh

test-docs-surface: tests/test_docs_surface.sh
	sh tests/test_docs_surface.sh

test-surface: tests/test_surface.sh
	sh tests/test_surface.sh

smoke: test-cli

check: check-docs check-guardrails lib cli server test test-layout test-code-natural test-docs-surface test-surface smoke
	@echo "yvex check: ok"

$(LIBYVEX): $(CORE_OBJS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tests/unit/%.o: tests/unit/%.c tests/test.h
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tests/unit/cuda/%.o: tests/unit/cuda/%.c tests/test.h
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.ptx: %.cu
	@mkdir -p $(@D)
	$(NVCC) $(NVCCFLAGS) $(CUDA_ARCH_FLAG) -ptx $< -o $@

$(CUDA_PTX_C): $(CUDA_PTX) cuda/cuda_kernels.h
	@mkdir -p $(@D)
	@{ \
		printf '#include "cuda/cuda_kernels.h"\n'; \
		printf 'const char yvex_cuda_kernels_ptx[] =\n'; \
		sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $(CUDA_PTX); \
		printf ';\n'; \
	} > $@

$(CUDA_PTX_OBJ): $(CUDA_PTX_C)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

YVEX_CLI_SRCS := \
	yvex_cli.c

$(YVEX_BIN): $(YVEX_CLI_SRCS) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(YVEX_CLI_SRCS) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(YVEXD_BIN): yvexd.c $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_RUNNER): tests/test.c $(TEST_UNIT_OBJS) $(LIBYVEX) tests/test.h
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) tests/test.c $(TEST_UNIT_OBJS) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(CUDA_TEST_RUNNER): tests/test_cuda.c $(CUDA_TEST_UNIT_OBJS) $(LIBYVEX) tests/test.h
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) tests/test_cuda.c $(CUDA_TEST_UNIT_OBJS) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

check-docs:
	@test -f README.md
	@test -f NOTICE.md
	@test -f AGENTS.md
	@test -f MODEL_ARTIFACTS.md
	@test -f docs/spine.md
	@test -f docs/api.md
	@test -f docs/contract.md
	@test -f docs/operator-runbook.md
	@! find docs -maxdepth 1 -type f -name '*.md' \
		! -name spine.md \
		! -name api.md \
		! -name contract.md \
		! -name operator-runbook.md \
		-print | grep .
	@test "$$(find docs -maxdepth 1 -type f -name '*.md' | wc -l | tr -d ' ')" = "4"
	@grep -F "YVEX Inner Delivery Spine" docs/spine.md >/dev/null
	@grep -F "internal roadmap" docs/spine.md >/dev/null
	@grep -F "YVEX is a native C inference engine for local open-weight models." README.md >/dev/null
	@grep -F "Model selection in canonical REPL" docs/spine.md >/dev/null
	@grep -F "docs/api.md, docs/contract.md, docs/operator-runbook.md, docs/spine.md" docs/spine.md >/dev/null
	@grep -F "YVEX API" docs/api.md >/dev/null
	@grep -F "YVEX Runtime Contract" docs/contract.md >/dev/null
	@grep -F "YVEX Operator Runbook" docs/operator-runbook.md >/dev/null

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
	@test ! -d models
	@test -f gguf/families.h
	@test -d tests/vectors
	@test -f tests/vectors/manifest.json
	@test -f tests/test.c
	@test -f tests/test_cuda.c
	@test -f tests/cli.sh
	@test "$$(find tests -maxdepth 1 -type f \( -name 'test.c' -o -name 'test_*.c' \) | wc -l | tr -d ' ')" -le "2"
	@test "$$(find tests -maxdepth 1 -type f -name 'test_cli*.sh' | wc -l | tr -d ' ')" = "0"
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
