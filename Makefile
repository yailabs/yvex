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

.PHONY: all info lib cli server cuda-info cuda-kernels cuda test-cuda test-cuda-no-nvcc smoke-cuda check-cuda test test-core test-cli test-source-payload-live-plan test-source-payload-live test-gguf-artifact-abi test-gguf-layout-integrity test-gguf-qtype-abi test-layout test-code-natural test-project-ledger test-docs-surface test-surface smoke check check-docs check-guardrails clean

CC ?= cc
AR ?= ar
NVCC ?= nvcc
CUDA_HOME ?= /usr/local/cuda
NVCCFLAGS ?=
CUDA_LDFLAGS ?=
YVEX_CUDA_ARCH ?= auto
NVCC_AVAILABLE := $(shell command -v $(NVCC) >/dev/null 2>&1 && echo yes || echo no)

CPPFLAGS ?= -D_FILE_OFFSET_BITS=64 -D_POSIX_C_SOURCE=200809L -Iinclude -I. -Isrc/core -Isrc/cli -Isrc/cli/input -Isrc/cli/io -Isrc/cli/model_artifacts -Isrc/cli/render -Isrc/source -Isrc/io -Isrc/backend -Isrc/backend/cuda -Isrc/runtime -Isrc/server -Isrc/gguf -Isrc/generation -Isrc/graph -Isrc/model/artifacts -Isrc/model/target
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -pthread
LDFLAGS ?=
LDLIBS ?= -ldl -pthread
TEST_CPPFLAGS := $(CPPFLAGS) -Itests

BUILD_DIR ?= build
OBJ_DIR ?= $(BUILD_DIR)/obj
LIB_DIR ?= $(BUILD_DIR)/lib
TEST_DIR ?= $(BUILD_DIR)/tests
DEEPSEEK_SOURCE ?= $(HOME)/lab/models/hf/deepseek/DeepSeek-V4-Flash
DEEPSEEK_MODELS_ROOT ?= $(HOME)/lab/models/gguf
DEEPSEEK_SOURCE_MANIFEST ?= $(DEEPSEEK_MODELS_ROOT)/deepseek/deepseek-source-manifest.json

LIBYVEX ?= $(LIB_DIR)/libyvex.a
YVEX_BIN ?= ./yvex
YVEXD_BIN ?= ./yvexd

CLI_COMMAND_SRCS := src/cli/commands/yvex_generate_cli.c \
	src/cli/commands/yvex_graph_cli.c \
	src/cli/commands/yvex_kv_cli.c \
	src/cli/commands/yvex_model_artifacts_cli.c \
	src/cli/commands/yvex_model_target_cli.c \
	src/cli/commands/yvex_sampling_cli.c \
	$(sort $(filter-out src/cli/commands/yvex_generate_cli.c src/cli/commands/yvex_graph_cli.c src/cli/commands/yvex_kv_cli.c src/cli/commands/yvex_model_artifacts_cli.c src/cli/commands/yvex_model_target_cli.c src/cli/commands/yvex_sampling_cli.c,$(wildcard src/cli/commands/*.c)))
CLI_INPUT_SRCS := src/cli/input/yvex_generate_args.c \
	src/cli/input/yvex_graph_args.c \
	src/cli/input/yvex_kv_args.c \
	src/cli/input/yvex_model_artifacts_args.c \
	src/cli/input/yvex_model_target_args.c \
	src/cli/input/yvex_sampling_args.c \
	$(sort $(filter-out src/cli/input/yvex_generate_args.c src/cli/input/yvex_graph_args.c src/cli/input/yvex_kv_args.c src/cli/input/yvex_model_artifacts_args.c src/cli/input/yvex_model_target_args.c src/cli/input/yvex_sampling_args.c,$(wildcard src/cli/input/*.c)))
CLI_RENDER_SRCS := src/cli/render/yvex_generate_render.c \
	src/cli/render/yvex_generate_trace_render.c \
	src/cli/render/yvex_graph_render.c \
	src/cli/render/yvex_kv_render.c \
	src/cli/render/yvex_model_artifacts_render.c \
	src/cli/render/yvex_model_target_render.c \
	src/cli/render/yvex_sampling_render.c \
	$(sort $(filter-out src/cli/render/yvex_generate_render.c src/cli/render/yvex_generate_trace_render.c src/cli/render/yvex_graph_render.c src/cli/render/yvex_kv_render.c src/cli/render/yvex_model_artifacts_render.c src/cli/render/yvex_model_target_render.c src/cli/render/yvex_sampling_render.c,$(wildcard src/cli/render/*.c)))
CLI_MODEL_ARTIFACT_SRCS := $(sort $(wildcard src/cli/model_artifacts/*.c))
CLI_IO_SRCS := $(sort $(wildcard src/cli/io/*.c))

CORE_SRCS := \
	src/core/yvex_core.c \
	src/core/yvex_fs.c \
	src/core/yvex_sha256.c \
	src/core/yvex_shard_index.c \
	src/accounts/yvex_accounts.c \
	src/artifact/yvex_artifact.c \
	src/artifact/yvex_artifact_descriptor.c \
	src/artifact/yvex_artifact_identity.c \
	src/artifact/yvex_artifact_integrity.c \
	src/artifact/yvex_artifact_materialize.c \
	src/artifact/yvex_artifact_report.c \
	src/artifact/yvex_artifact_roundtrip_gate.c \
	src/backend/yvex_backend.c \
	src/backend/yvex_backend_qtype.c \
	src/backend/yvex_backend_report.c \
	src/backend/yvex_backend_tensor.c \
	src/bench/yvex_bench.c \
	src/eval/yvex_eval.c \
	src/generation/yvex_decode.c \
	src/generation/yvex_generation.c \
	src/generation/yvex_generation_report.c \
	src/generation/yvex_generation_trace.c \
	src/generation/yvex_kv.c \
	src/generation/yvex_kv_report.c \
	src/generation/yvex_logits.c \
	src/generation/yvex_sampling.c \
	src/generation/yvex_sampling_report.c \
	src/gguf/naming.c \
	src/gguf/gguf.c \
	src/gguf/conversion.c \
	src/gguf/quant.c \
	src/gguf/tools.c \
	src/gguf/yvex_gguf_container.c \
	src/gguf/yvex_gguf_descriptor.c \
	src/gguf/yvex_gguf_layout_integrity.c \
	src/gguf/yvex_gguf_layout_map.c \
	src/gguf/yvex_gguf_metadata.c \
	src/gguf/yvex_gguf_name_map.c \
	src/gguf/yvex_gguf_qtype.c \
	src/gguf/yvex_gguf_range_map.c \
	src/gguf/yvex_gguf_reader.c \
	src/gguf/yvex_gguf_report.c \
	src/gguf/yvex_gguf_roundtrip.c \
	src/gguf/yvex_gguf_tensor_info.c \
	src/gguf/yvex_gguf_writer.c \
	src/graph/yvex_graph_bind.c \
	src/graph/yvex_graph.c \
	src/graph/yvex_graph_execute.c \
	src/graph/yvex_graph_guard.c \
	src/graph/yvex_graph_plan.c \
	src/graph/yvex_graph_primitive.c \
	src/graph/yvex_graph_report.c \
	src/graph/yvex_memory_plan.c \
	src/io/yvex_json_writer.c \
	src/metrics/yvex_metrics.c \
	src/metrics/yvex_profile.c \
	src/model/yvex_model.c \
	src/model/yvex_model_artifacts.c \
	src/model/architecture/yvex_deepseek_v4_ir.c \
	src/model/yvex_runtime_descriptor.c \
	src/model/yvex_runtime_descriptor_report.c \
	src/model/artifacts/yvex_model_artifact_check_report.c \
	src/model/artifacts/yvex_model_artifact_gate.c \
	src/model/artifacts/yvex_model_artifact_list_report.c \
	src/model/artifacts/yvex_model_artifact_ref.c \
	src/model/artifacts/yvex_model_artifact_registry.c \
	src/model/artifacts/yvex_model_artifact_report.c \
	src/model/artifacts/yvex_model_artifact_status_report.c \
	src/model/artifacts/yvex_model_artifact_write.c \
	src/model/target/yvex_mapping_gate_report.c \
	src/model/target/yvex_deepseek_payload_handoff.c \
	src/model/target/yvex_deepseek_gguf_map.c \
	src/model/target/yvex_deepseek_tensor_coverage.c \
	src/model/target/yvex_missing_role_report.c \
	src/model/target/yvex_model_class_profile.c \
	src/model/target/yvex_model_target_candidates.c \
	src/model/target/yvex_model_target_catalog.c \
	src/model/target/yvex_model_target_decision.c \
	src/model/target/yvex_model_target_report.c \
	src/model/target/yvex_model_target_sidecar_write.c \
	src/model/target/yvex_output_head_map_report.c \
	src/model/target/yvex_qtype_policy_report.c \
	src/model/target/yvex_qtype_role_support_report.c \
	src/model/target/yvex_tensor_collection_report.c \
	src/model/target/yvex_tensor_naming_report.c \
	src/model/target/yvex_tokenizer_map_report.c \
	src/runtime/yvex_chat.c \
	src/runtime/yvex_runtime.c \
	src/source/yvex_native_weights.c \
	src/source/yvex_safetensors_header.c \
	src/source/yvex_source.c \
	src/source/yvex_source_deepseek.c \
	src/source/yvex_source_inventory.c \
	src/source/yvex_source_json.c \
	src/source/yvex_source_manifest.c \
	src/source/yvex_source_payload.c \
	src/source/yvex_source_payload_identity.c \
	src/source/yvex_source_payload_plan.c \
	src/source/yvex_source_payload_stream.c \
	src/source/yvex_source_provenance.c \
	src/source/yvex_source_report.c \
	src/source/yvex_source_scan.c \
	src/source/yvex_source_verify.c \
	src/source/yvex_source_write.c \
	src/generation/yvex_prefill.c \
	src/tokenizer/yvex_token_input.c \
	src/tokenizer/yvex_tokenizer.c \
	src/server/yvex_server.c

CLI_SRCS := \
	src/cli/yvex_cli.c \
	$(CLI_COMMAND_SRCS) \
	$(CLI_INPUT_SRCS) \
	$(CLI_MODEL_ARTIFACT_SRCS) \
	$(CLI_RENDER_SRCS) \
	$(CLI_IO_SRCS)

CUDA_SRCS := \
	src/backend/cuda/cuda_backend.c \
	src/backend/cuda/cuda_capability.c \
	src/backend/cuda/cuda_tensor.c \
	src/backend/cuda/cuda_ops.c \
	src/backend/cuda/cuda_info.c \
	src/backend/cuda/cuda_qtype.c \
	src/backend/cuda/cuda_errors.c

CUDA_CU_SRCS := \
	src/backend/cuda/cuda_kernels.cu

CUDA_ARCH_FLAG := $(if $(filter auto,$(YVEX_CUDA_ARCH)),,-arch=$(YVEX_CUDA_ARCH))
CUDA_PTX := $(patsubst %.cu,$(OBJ_DIR)/%.ptx,$(CUDA_CU_SRCS))
CUDA_PTX_C := $(OBJ_DIR)/src/backend/cuda/cuda_kernels_ptx.c
CUDA_PTX_OBJ := $(OBJ_DIR)/src/backend/cuda/cuda_kernels_ptx.o

CORE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
CUDA_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CUDA_SRCS))
CORE_OBJS += $(CUDA_OBJS)

ifeq ($(NVCC_AVAILABLE),yes)
CPPFLAGS += -DYVEX_HAVE_CUDA_KERNEL_PTX=1
CORE_OBJS += $(CUDA_PTX_OBJ)
endif

TEST_RUNNER := $(TEST_DIR)/test
SOURCE_PAYLOAD_LIVE_RUNNER := $(TEST_DIR)/source_payload_deepseek
CUDA_TEST_RUNNER := $(TEST_DIR)/test_cuda

TEST_UNIT_SRCS := $(sort $(wildcard tests/unit/*.c))
TEST_UNIT_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_UNIT_SRCS))

CUDA_TEST_UNIT_SRCS := $(sort $(wildcard tests/unit/cuda/*.c))
CUDA_TEST_UNIT_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CUDA_TEST_UNIT_SRCS))

CLI_TEST := tests/cli.sh

CURRENT_DOCS := README.md AGENTS.md PROJECT.md MODEL_ARTIFACTS.md NOTICE.md \
	docs/api.md docs/contract.md docs/model-families.md \
	docs/operator-runbook.md docs/cli-output-architecture.md \
	docs/reference-architecture.md docs/v010-release-doctrine.md \
	docs/topology-closure-audit.md docs/system-target.md

info:
	@echo "yvex: C local inference engine"
	@echo "status: selected tensor materialization, engine weight attachment, fixture graph execution, real selected graph segments, explicit token input boundary, prefill state foundation, minimal KV binding, decode/logits/sampling diagnostics, and bounded diagnostic generation loop with explicit append accounting"
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

test-cuda-no-nvcc: tests/test_cuda_failclosed.sh
	$(MAKE) BUILD_DIR=build/no-nvcc \
		YVEX_BIN=build/no-nvcc/yvex \
		YVEXD_BIN=build/no-nvcc/yvexd \
		NVCC=__yvex_nvcc_unavailable__ all
	YVEX_BIN=build/no-nvcc/yvex sh tests/test_cuda_failclosed.sh

test-core: $(TEST_RUNNER)
	$(TEST_RUNNER)

test-cli: $(YVEX_BIN) $(YVEXD_BIN) $(CLI_TEST)
	YVEX_BIN=$(YVEX_BIN) YVEXD_BIN=$(YVEXD_BIN) sh $(CLI_TEST)

test-source-payload-live-plan: $(SOURCE_PAYLOAD_LIVE_RUNNER)
	$(SOURCE_PAYLOAD_LIVE_RUNNER) --plan-only "$(DEEPSEEK_SOURCE)" "$(DEEPSEEK_MODELS_ROOT)" "$(DEEPSEEK_SOURCE_MANIFEST)"

test-source-payload-live: $(SOURCE_PAYLOAD_LIVE_RUNNER)
	$(SOURCE_PAYLOAD_LIVE_RUNNER) "$(DEEPSEEK_SOURCE)" "$(DEEPSEEK_MODELS_ROOT)" "$(DEEPSEEK_SOURCE_MANIFEST)"

test: test-core test-cli

test-gguf-artifact-abi: $(TEST_RUNNER) tests/test_gguf_artifact_abi.sh
	sh tests/test_gguf_artifact_abi.sh

test-gguf-layout-integrity: $(TEST_RUNNER) tests/test_gguf_layout_integrity.sh
	sh tests/test_gguf_layout_integrity.sh

test-gguf-qtype-abi: $(TEST_RUNNER) tests/test_gguf_qtype_abi.sh
	sh tests/test_gguf_qtype_abi.sh

test-layout: tests/test_source_layout.sh
	sh tests/test_source_layout.sh

test-code-natural: tests/test_code_natural.sh
	sh tests/test_code_natural.sh

test-project-ledger: tests/test_project_ledger.sh PROJECT.md
	sh tests/test_project_ledger.sh

test-docs-surface: tests/test_docs_surface.sh
	sh tests/test_docs_surface.sh

test-surface: tests/test_surface.sh
	sh tests/test_surface.sh

smoke: test-cli

check: check-docs check-guardrails lib cli server test test-cuda-no-nvcc test-gguf-artifact-abi test-gguf-layout-integrity test-gguf-qtype-abi test-layout test-code-natural test-project-ledger test-docs-surface test-surface smoke
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

$(CUDA_PTX_C): $(CUDA_PTX) src/backend/cuda/cuda_kernels.h
	@mkdir -p $(@D)
	@{ \
		printf '#include "cuda_kernels.h"\n'; \
		printf 'const char yvex_cuda_kernels_ptx[] =\n'; \
		sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $(CUDA_PTX); \
		printf ';\n'; \
	} > $@

$(CUDA_PTX_OBJ): $(CUDA_PTX_C)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(YVEX_BIN): $(CLI_SRCS) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CLI_SRCS) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(YVEXD_BIN): src/daemon/yvexd.c $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_RUNNER): tests/test.c $(TEST_UNIT_OBJS) $(LIBYVEX) tests/test.h
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) tests/test.c $(TEST_UNIT_OBJS) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(SOURCE_PAYLOAD_LIVE_RUNNER): tests/live/source_payload_deepseek.c $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) $< $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(CUDA_TEST_RUNNER): tests/test_cuda.c $(CUDA_TEST_UNIT_OBJS) $(LIBYVEX) tests/test.h
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) tests/test_cuda.c $(CUDA_TEST_UNIT_OBJS) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

check-docs:
	@test -f README.md
	@test -f NOTICE.md
	@test -f AGENTS.md
	@test -f PROJECT.md
	@test -f MODEL_ARTIFACTS.md
	@test ! -e docs/spine.md
	@test -f docs/api.md
	@test -f docs/contract.md
	@test -f docs/model-families.md
	@test -f docs/operator-runbook.md
	@test -f docs/v010-release-doctrine.md
	@test -f docs/topology-closure-audit.md
	@test -f docs/system-target.md
	@test -f docs/reference-architecture.md
	@test -z "$$(find docs -maxdepth 1 -type d -name repair -print -quit)"
	@! find docs -maxdepth 1 -type f -name '*.md' \
		! -name api.md \
		! -name contract.md \
		! -name model-families.md \
		! -name operator-runbook.md \
		! -name cli-output-architecture.md \
		! -name v010-release-doctrine.md \
		! -name topology-closure-audit.md \
		! -name system-target.md \
		! -name reference-architecture.md \
		-print | grep .
	@grep -F "YVEX Project Control" PROJECT.md >/dev/null
	@grep -F "## 7. Track Registry And Dashboard" PROJECT.md >/dev/null
	@grep -F "## 8. First-Class Milestone Roadmap" PROJECT.md >/dev/null
	@grep -F "## 9. Complete Track/Wave Ledger" PROJECT.md >/dev/null
	@grep -F "native C inference engine for local open-weight models" README.md >/dev/null
	@sh tests/test_project_ledger.sh >/dev/null
	@grep -F "YVEX System Target" docs/system-target.md >/dev/null
	@grep -F "YVEX Reference Architecture Map" docs/reference-architecture.md >/dev/null
	@grep -F "YVEX API" docs/api.md >/dev/null
	@grep -F "YVEX Runtime Contract" docs/contract.md >/dev/null
	@grep -F "YVEX Operator Runbook" docs/operator-runbook.md >/dev/null

check-guardrails:
	@test ! -e docs/spine.md
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
	@test ! -d cli
	@test ! -d server
	@test ! -e tests/README.md
	@test ! -e include/yvex/sampler.h
	@test ! -d backends
	@test -d src
	@test -d src/app
	@test -d src/cli
	@test -d src/cli/commands
	@test -d src/cli/render
	@test -d src/cli/io
	@test -d src/cli/catalog
	@test -d src/cli/schema
	@test -d src/core
	@test -d src/artifact
	@test -d src/backend
	@test -d src/backend/cuda
	@test -d src/server
	@test -d src/gguf
	@test -d src/model
	@test -d src/tokenizer
	@test -d src/runtime
	@test -d src/generation
	@test -d src/eval
	@test -d src/bench
	@test ! -d cuda
	@test ! -d gguf
	@test ! -d models
	@test -f src/gguf/families.h
	@test -d tests/vectors
	@test -f tests/vectors/manifest.json
	@test -f tests/test.c
	@test -f tests/test_cuda.c
	@test -f tests/cli.sh
	@test "$$(find tests -maxdepth 1 -type f \( -name 'test.c' -o -name 'test_*.c' \) | wc -l | tr -d ' ')" -le "2"
	@test "$$(find tests -maxdepth 1 -type f -name 'test_cli*.sh' | wc -l | tr -d ' ')" = "0"
	@test -f include/yvex/server.h
	@test ! -d fixtures
	@test -f src/cli/yvex_cli.c
	@test -f src/daemon/yvexd.c
	@test -f src/server/yvex_server.c
	@test -z "$$(git ls-files 'yvex_*.c')"
	@test -z "$$(git ls-files 'yvex_*_private.h')"
	@test ! -d ui
	@test ! -d app
	@test ! -d desktop
	@! grep -RIn -E "N[E]T\\.SPINE|N[E]T moves streams|C[L]ORI|c[l]ori-codename|docs/arc[h]ive|c[l]ori_|libc[l]ori|c[l]orid|include/c[l]ori|~/\\.config/c[l]ori|github\\.com/yailabs/c[l]ori|yailabs/c[l]ori" --exclude-dir=.git --exclude-dir=build . >/dev/null
	@! grep -Ei "production-read[y]|implemented infer[e]nce|implemented ser[v]er|supports C[U]DA|supports M[e]tal|supports M[L]X|supports llama\\.cpp|O[p]enAI-compatible ser[v]er" README.md >/dev/null
	@! grep -Ei "benchmark results" README.md | grep -vi "benchmark results: none" >/dev/null

clean:
	@rm -rf $(BUILD_DIR) ./yvex ./yvexd ./*.o
