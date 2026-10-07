# Toolchain, material identities and source-relative build configuration.
DOCS_PYTHON ?= $(if $(wildcard build/docs-venv/bin/python),build/docs-venv/bin/python,python3)
export DOCS_PYTHON

CC ?= cc
AR ?= ar
NVCC ?= nvcc
CUOBJDUMP ?= $(CUDA_HOME)/bin/cuobjdump
CUDA_HOME ?= /usr/local/cuda
NVCCFLAGS ?= -O3
CUDA_LDFLAGS ?=
YVEX_CUDA_ARCH ?= auto
CUDA_AUTO_ARCH ?= $(shell caps=$$(command -v nvidia-smi >/dev/null 2>&1 && \
	nvidia-smi --query-gpu=compute_cap --format=csv,noheader,nounits 2>/dev/null | \
	sed -n 's/[^0-9.]//g; s/[.]//g; /^[0-9][0-9]*$$/p' | sort -u); \
	set -- $$caps; if test "$$#" -eq 1 && command -v $(NVCC) >/dev/null 2>&1 && \
		$(NVCC) --list-gpu-arch 2>/dev/null | grep -Fx "compute_$$1" >/dev/null; \
	then printf 'sm_%s' "$$1"; fi)
CUDA_EFFECTIVE_ARCH := $(if $(filter auto,$(YVEX_CUDA_ARCH)),$(CUDA_AUTO_ARCH),$(YVEX_CUDA_ARCH))
NVCC_AVAILABLE := $(shell command -v $(NVCC) >/dev/null 2>&1 && echo yes || echo no)
YVEX_PROTOCOL_VERSION := $(shell sed -n \
	's/^#define YVEX_LOCAL_PROTOCOL_VERSION \([0-9][0-9]*\)u$$/\1/p' \
	include/yvex/server.h)

YVEX_HOST_OS := $(shell uname -s)
YVEX_HOST_ARCH := $(shell uname -m)
# Native Objective-C Metal sources are never passed to Linux/CUDA toolchains.
YVEX_NATIVE_METAL := $(if $(filter Darwin-arm64,$(YVEX_HOST_OS)-$(YVEX_HOST_ARCH)),yes,no)
METAL_CFLAGS ?=
override METAL_CFLAGS += -fobjc-arc
YVEX_ARCHIVER := $(if $(filter Darwin,$(YVEX_HOST_OS)),python3 tools/archive_darwin.py --ar '$(AR)',$(AR) rcsP)
CPPFLAGS ?=
ifeq ($(YVEX_HOST_OS),Darwin)
override CPPFLAGS += -D_DARWIN_C_SOURCE
endif
override CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_POSIX_C_SOURCE=200809L -Iinclude -I.
YVEX_BUILD_COMMIT ?= $(shell git rev-parse --verify HEAD 2>/dev/null || printf unknown)
YVEX_BUILD_SOURCE_TREE ?= $(shell git rev-parse --verify 'HEAD^{tree}' 2>/dev/null || printf unknown)
YVEX_BUILD_SOURCE_DELTA_IDENTITY ?= $(shell { \
	git diff --binary --no-ext-diff HEAD -- . 2>/dev/null; \
	git ls-files --others --exclude-standard 2>/dev/null | LC_ALL=C sort | \
		grep -v '__pycache__/' | grep -v '[.]pyc$$' | \
		while IFS= read -r path; do \
			printf 'untracked\t%s\t' "$$path"; $(if $(filter Darwin,$(YVEX_HOST_OS)),/usr/bin/stat -f 'mode=%Lp',stat -c 'mode=%a') "$$path"; \
			sha256sum "$$path"; \
			done; \
	} | sha256sum | cut -d' ' -f1)
YVEX_BUILD_SOURCE_STATE ?= $(if $(filter \
	e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855,\
	$(YVEX_BUILD_SOURCE_DELTA_IDENTITY)),clean,dirty)
YVEX_BUILD_IDENTITY ?= $(shell printf '%s\n' \
	'source-tree=$(YVEX_BUILD_SOURCE_TREE)' \
	'source-delta=$(YVEX_BUILD_SOURCE_DELTA_IDENTITY)' \
	'source-owners=$(shell sha256sum config/source_owners.tsv 2>/dev/null | cut -d" " -f1)' \
	'operator-registry=$(shell sha256sum config/operator/registry.json 2>/dev/null | cut -d" " -f1)' \
	'cc=$(CC)' 'cc-version=$(shell $(CC) --version 2>/dev/null | head -1)' \
	'cc-target=$(shell $(CC) -dumpmachine 2>/dev/null)' \
	'cppflags=$(YVEX_BUILD_CPPFLAGS)' 'cflags=$(YVEX_BUILD_CFLAGS)' \
	'ldflags=$(YVEX_BUILD_LDFLAGS)' 'ldlibs=$(YVEX_BUILD_LDLIBS)' \
	'linker-version=$(shell $(CC) -Wl,--version 2>/dev/null | head -1)' \
	'nvcc=$(NVCC)' 'nvcc-version=$(shell $(NVCC) --version 2>/dev/null | tail -1)' \
	'nvccflags=$(YVEX_BUILD_NVCCFLAGS)' 'cuda-ldflags=$(YVEX_BUILD_CUDA_LDFLAGS)' \
	'metal-cflags=$(METAL_CFLAGS)' 'native-metal=$(YVEX_NATIVE_METAL)' \
	'cuda-arch-request=$(YVEX_CUDA_ARCH)' 'cuda-arch=$(CUDA_EFFECTIVE_ARCH)' | \
	sha256sum | cut -d' ' -f1)
YVEX_BUILD_SOURCE_ROOT ?= $(shell pwd -P)
CFLAGS ?= -O3 -std=c11 -Wall -Wextra -pedantic -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wshadow -Wformat=2 \
	-Wundef -Wvla -pthread
DEPFLAGS ?= -MMD -MP
LDFLAGS ?=
ifeq ($(YVEX_HOST_OS),Darwin)
# libSystem provides pthreads; clang's -pthread has no link-time effect here.
LDLIBS ?= -ldl -lm -lz
else
LDLIBS ?= -ldl -pthread -lm -lz
endif
ifeq ($(YVEX_NATIVE_METAL),yes)
override LDLIBS += -framework Foundation -framework Metal
endif
TEST_CPPFLAGS := $(CPPFLAGS)

BUILD_DIR ?= build
OBJ_DIR ?= $(BUILD_DIR)/obj
LIB_DIR ?= $(BUILD_DIR)/lib
TEST_DIR ?= $(BUILD_DIR)/tests
BUILD_COMMIT_HEADER := $(BUILD_DIR)/generated/build_commit.h
C_BUILD_CONFIG := $(BUILD_DIR)/generated/c_build_config
LINK_BUILD_CONFIG := $(BUILD_DIR)/generated/link_build_config
CUDA_BUILD_CONFIG := $(BUILD_DIR)/generated/cuda_build_config
SOURCE_OWNER_MANIFEST := config/source_owners.tsv
SOURCE_MANIFEST_GENERATOR := tools/generate_source_manifest.py
SOURCE_MANIFEST_MK := $(BUILD_DIR)/generated/sources.mk
SOURCE_FAMILY_HEADER := $(BUILD_DIR)/generated/source/families.h
QA_REGISTRY_SOURCE := config/qa/registry.json
QA_REGISTRY_GENERATOR := tools/generate_qa_registry.py
QA_REGISTRY_DIR := $(BUILD_DIR)/generated/qa
QA_REGISTRY_MK := $(QA_REGISTRY_DIR)/registry.mk
QA_REGISTRY_HEADER := $(QA_REGISTRY_DIR)/test_declarations.h
QA_REGISTRY_PROJECTIONS := $(QA_REGISTRY_HEADER) \
	$(QA_REGISTRY_DIR)/unit_registry.inc $(QA_REGISTRY_DIR)/cuda_registry.inc \
	$(QA_REGISTRY_DIR)/metal_registry.inc \
	$(QA_REGISTRY_DIR)/quant_registry.inc $(QA_REGISTRY_DIR)/artifact_registry.inc \
	$(QA_REGISTRY_DIR)/inventory.tsv $(QA_REGISTRY_DIR)/make_targets.tsv \
	$(QA_REGISTRY_DIR)/registry.sha256
OPERATOR_REGISTRY_SOURCE := config/operator/registry.json
OPERATOR_REGISTRY_GENERATOR := tools/generate_operator_registry.py
OPERATOR_REGISTRY_DIR := $(BUILD_DIR)/generated/operator
OPERATOR_REGISTRY_HEADER := $(OPERATOR_REGISTRY_DIR)/registry.h
OPERATOR_REGISTRY_C := $(OPERATOR_REGISTRY_DIR)/registry.c
OPERATOR_REGISTRY_IDENTITY := $(OPERATOR_REGISTRY_DIR)/registry.sha256
OPERATOR_REGISTRY_JSON := $(OPERATOR_REGISTRY_DIR)/registry.json
OPERATOR_REGISTRY_OBJ := $(OBJ_DIR)/generated/operator/registry.o
DEEPSEEK_SOURCE ?= $(HOME)/lab/models/hf/deepseek/DeepSeek-V4-Flash-DSpark
DEEPSEEK_MODELS_ROOT ?= $(HOME)/lab/models/gguf
DEEPSEEK_SOURCE_MANIFEST ?= $(DEEPSEEK_MODELS_ROOT)/deepseek/deepseek-v4-flash-dspark-source-manifest.json
DEEPSEEK_OPERATOR_MODELS_ROOT ?= $(HOME)/lab/models
DEEPSEEK_SELECTED_ARTIFACT ?= $(DEEPSEEK_MODELS_ROOT)/deepseek/deepseek-v4-flash-dspark-bootstrap-q2-v1.gguf
# The legacy attention oracle admits this exact bootstrap, not an arbitrary
# selected release representation. Keep its QA prerequisite explicit.
DEEPSEEK_ATTENTION_ARTIFACT ?= $(DEEPSEEK_MODELS_ROOT)/deepseek/deepseek-v4-flash-dspark-bootstrap-q2-v1.gguf
export DEEPSEEK_ATTENTION_ARTIFACT
YVEX_QUANT_DSPARK_PRESET ?= deepseek-v4-flash-dspark-bootstrap-q2-v1
YVEX_VARIANT_ARTIFACT ?=
YVEX_VARIANT_BINDING_DIR ?=
YVEX_RUNTIME_BENCHMARK_DIR ?=
YVEX_RUNTIME_BINDING ?=
YVEX_TOKENIZER_REFERENCE_PYTHON ?= /tmp/yvex-tokenizer-oracle/bin/python
MINIMAX_H3_TEXT_ENCODER_TOKENS ?= 1
MINIMAX_H3_OMNI_VIDEO_ROWS ?= 37
MINIMAX_H3_OMNI_AUDIO_ROWS ?= 414
MINIMAX_H3_OMNI_TEXT_ROWS ?= 15
MINIMAX_H3_OMNI_BLOCKS ?= 50
MINIMAX_H3_OMNI_TIMESTEPS ?= 2
MINIMAX_H3_LATENT_BLOCKS ?= 50
MINIMAX_H3_LATENT_STEPS ?= 2
MINIMAX_H3_LATENT_FIXTURE_ROOT ?=
MINIMAX_H3_LATENT_WIDTH ?= 32
MINIMAX_H3_LATENT_HEIGHT ?= 32
MINIMAX_H3_LATENT_FRAMES ?= 124
MINIMAX_H3_LATENT_SEED ?= 42
PINNED_GGML_ROOT ?= /tmp/yvex-ggml-af97976
PINNED_GGML_BUILD ?= $(PINNED_GGML_ROOT)/build-yvex

LIBYVEX ?= $(LIB_DIR)/libyvex.a
YVEX_BIN ?= ./yvex

CARGO ?= cargo
RUSTC ?= rustc
RUST_PROFILE ?= release
RUST_CARGO_TARGET_DIR ?= $(BUILD_DIR)/cargo
RUST_PROFILE_DIR := $(if $(filter dev test,$(RUST_PROFILE)),debug,$(RUST_PROFILE))
RUST_SHELL_BIN := $(RUST_CARGO_TARGET_DIR)/$(RUST_PROFILE_DIR)/yvex
REPLAI_RUST_SOURCE := build/external/replai-source
RUST_BUILD_CONFIG := $(BUILD_DIR)/generated/rust_build_config
# Shell/toolchain identity is separate from the C library's material identity.
# These recursively expanded values never require Rust for `make lib`.
YVEX_SHELL_BUILD_IDENTITY = $(shell printf '%s\n' \
	'native=$(YVEX_BUILD_IDENTITY)' \
	'rustc=$(shell $(RUSTC) -vV 2>/dev/null)' \
	'cargo=$(shell $(CARGO) --version 2>/dev/null)' \
	'profile=$(RUST_PROFILE)' 'rustflags=$(RUSTFLAGS)' \
	'ldflags=$(YVEX_BUILD_LDFLAGS)' 'ldlibs=$(YVEX_BUILD_LDLIBS)' \
	'cargo-lock=$(shell sha256sum Cargo.lock 2>/dev/null | cut -d" " -f1)' \
	'replai-pin=$(shell sha256sum config/replai.json 2>/dev/null | cut -d" " -f1)' \
	| sha256sum | cut -d' ' -f1)

ifneq ($(filter-out clean help info print-build-inputs print-archive-layout,$(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)),)
include $(SOURCE_MANIFEST_MK)
include $(QA_REGISTRY_MK)
endif

override CPPFLAGS += -I$(BUILD_DIR)/generated
TEST_CPPFLAGS += -I$(BUILD_DIR)/generated

# Attention wrappers own a collision-free temporary root and delete only that
# root after validating its canonical parent and generated basename.
define ATTENTION_OWNED_TMP_BEGIN
tmp_parent=$${TMPDIR:-/tmp}; \
case "$$tmp_parent" in /*) ;; *) echo "attention temp parent must be absolute: $$tmp_parent" >&2; exit 1;; esac; \
test -d "$$tmp_parent" && test ! -L "$$tmp_parent"; \
tmp_parent=$$(cd "$$tmp_parent" && pwd -P); \
tmp_dir=$$(mktemp -d "$$tmp_parent/yvex-$$tmp_tag.XXXXXX"); \
case "$$tmp_dir" in "$$tmp_parent"/yvex-"$$tmp_tag".*) ;; *) echo "attention temp ownership mismatch: $$tmp_dir" >&2; exit 1;; esac; \
cleanup_attention_tmp() { \
	status=$$?; \
	trap - EXIT HUP INT TERM; \
	case "$$tmp_dir" in "$$tmp_parent"/yvex-"$$tmp_tag".*) ;; *) echo "refusing unowned attention cleanup: $$tmp_dir" >&2; exit 1;; esac; \
	if test -e "$$tmp_dir"; then \
		test -d "$$tmp_dir" && test ! -L "$$tmp_dir" || { echo "refusing unsafe attention cleanup: $$tmp_dir" >&2; exit 1; }; \
		find "$$tmp_dir" -xdev -mindepth 1 -delete || exit 1; \
		rmdir "$$tmp_dir" || exit 1; \
	fi; \
	exit $$status; \
}; \
trap cleanup_attention_tmp EXIT; \
trap 'exit 129' HUP; \
trap 'exit 130' INT; \
trap 'exit 143' TERM;
endef

OPENAI_ADAPTER_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(OPENAI_ADAPTER_SRCS))

CUDA_ARCH_FLAG := $(if $(CUDA_EFFECTIVE_ARCH),-arch=$(CUDA_EFFECTIVE_ARCH))
CUDA_PTX := $(patsubst %.cu,$(OBJ_DIR)/%.ptx,$(CUDA_CU_SRCS))
CUDA_PTX_INC := $(OBJ_DIR)/generated/cuda_kernels_ptx.inc
CUDA_EXPORT_INC := $(OBJ_DIR)/generated/cuda_kernel_exports.inc
CUDA_NATIVE_ARCH := $(filter sm_%,$(CUDA_EFFECTIVE_ARCH))
CUDA_CUBIN := $(if $(CUDA_NATIVE_ARCH),$(patsubst %.cu,$(OBJ_DIR)/%.cubin,$(CUDA_CU_SRCS)))
CUDA_CUBIN_INC := $(OBJ_DIR)/generated/cuda_kernels_cubin.inc

CORE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
CORE_OBJS += $(OPENAI_ADAPTER_OBJS)
CUDA_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CUDA_SRCS))
CORE_OBJS += $(CUDA_OBJS)
ifeq ($(YVEX_NATIVE_METAL),yes)
CORE_OBJS += $(patsubst %.m,$(OBJ_DIR)/%.o,$(METAL_SRCS))
endif

ifeq ($(NVCC_AVAILABLE),yes)
override CPPFLAGS += -DYVEX_HAVE_CUDA_KERNEL_PTX=1
$(OBJ_DIR)/src/backend/cuda/capability.o: override CPPFLAGS += -I$(OBJ_DIR)/generated
$(OBJ_DIR)/src/backend/cuda/capability.o: $(CUDA_PTX_INC) $(CUDA_EXPORT_INC) $(CUDA_BUILD_CONFIG)
ifneq ($(CUDA_NATIVE_ARCH),)
override CPPFLAGS += -DYVEX_HAVE_CUDA_KERNEL_CUBIN=1
$(OBJ_DIR)/src/backend/cuda/capability.o: $(CUDA_CUBIN_INC)
endif
endif

# Freeze invocation-wide material flags before target-specific additions can be
# inherited by build_commit.h through whichever consumer Make visits first.
YVEX_BUILD_CPPFLAGS := $(CPPFLAGS)
YVEX_BUILD_CFLAGS := $(CFLAGS)
YVEX_BUILD_LDFLAGS := $(LDFLAGS)
YVEX_BUILD_LDLIBS := $(LDLIBS)
YVEX_BUILD_NVCCFLAGS := $(NVCCFLAGS)
YVEX_BUILD_CUDA_LDFLAGS := $(CUDA_LDFLAGS)

$(OBJ_DIR)/src/runtime/benchmark.o: override CPPFLAGS += -I$(BUILD_DIR)/generated
$(OBJ_DIR)/src/runtime/benchmark.o: $(BUILD_COMMIT_HEADER)
$(OBJ_DIR)/src/runtime/evidence.o: $(BUILD_COMMIT_HEADER)
$(OBJ_DIR)/src/runtime/generation_context.o: override CPPFLAGS += -I$(BUILD_DIR)/generated
$(OBJ_DIR)/src/runtime/generation_context.o: $(BUILD_COMMIT_HEADER)

TEST_RUNNER := $(TEST_DIR)/test
QUANT_TEST_RUNNER := $(TEST_DIR)/test_quant
ARTIFACT_TEST_RUNNER := $(TEST_DIR)/test_artifact_writer
SOURCE_PAYLOAD_LIVE_RUNNER := $(TEST_DIR)/source_payload_deepseek
QUANT_LIVE_RUNNER := $(TEST_DIR)/quant_deepseek
ARTIFACT_LIVE_RUNNER := $(TEST_DIR)/artifact_deepseek
MATERIALIZE_LIVE_RUNNER := $(TEST_DIR)/materialize_deepseek
MINIMAX_AUDIO_LIVE_RUNNER := $(TEST_DIR)/minimax_h3_audio
MINIMAX_VIDEO_LIVE_RUNNER := $(TEST_DIR)/minimax_h3_video
MINIMAX_TEXT_LIVE_RUNNER := $(TEST_DIR)/minimax_h3_text
MINIMAX_TRANSFORMER_LIVE_RUNNER := $(TEST_DIR)/minimax_h3_transformer
ATTENTION_LIVE_RUNNER := $(TEST_DIR)/attention_deepseek
PREFILL_LIVE_RUNNER := $(TEST_DIR)/prefill_deepseek
MOE_LIVE_RUNNER := $(TEST_DIR)/moe_deepseek
TRANSFORMER_LIVE_RUNNER := $(TEST_DIR)/transformer_deepseek
DECODE_LIVE_RUNNER := $(TEST_DIR)/decode_deepseek
LOGITS_LIVE_RUNNER := $(TEST_DIR)/logits_deepseek
TOKENIZER_LIVE_RUNNER := $(TEST_DIR)/tokenizer_deepseek
GENERATION_LIVE_RUNNER := $(TEST_DIR)/generation_deepseek
DECISION_READOUT_LIVE_RUNNER := $(TEST_DIR)/decision_readout
QWEN_ADMISSION_LIVE_RUNNER := $(TEST_DIR)/qwen_admission
OPENAI_FAKE_HOST := $(TEST_DIR)/openai_host
OPENAI_ADAPTER_HOST := $(TEST_DIR)/openai_adapter
TINY_VERTICAL_COMPILER := $(TEST_DIR)/tiny_compile
NATIVE_TURN_TEST := $(TEST_DIR)/native_turn
OFFICIAL_GGUF_CHECKER := $(TEST_DIR)/ggml_gguf_check
CUDA_TEST_RUNNER := $(TEST_DIR)/test_cuda
METAL_TEST_RUNNER := $(TEST_DIR)/test_metal
METAL_TEST_MAIN_OBJ := $(OBJ_DIR)/tests/metal_runner.o
METAL_TEST_UNIT_OBJ := $(OBJ_DIR)/tests/unit/backend_metal.o

TEST_UNIT_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_UNIT_SRCS))
TEST_REFERENCE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_REFERENCE_SRCS))
TEST_MAIN_OBJ := $(OBJ_DIR)/tests/test.o
QUANT_TEST_UNIT_SRCS := $(QA_QUANT_TEST_SRCS)
QUANT_TEST_UNIT_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(QUANT_TEST_UNIT_SRCS))
QUANT_TEST_RUNNER_OBJ := $(OBJ_DIR)/tests/unit/quant_runner.o
ARTIFACT_TEST_RUNNER_OBJ := $(OBJ_DIR)/tests/unit/artifact_writer_runner.o

CUDA_TEST_UNIT_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CUDA_TEST_UNIT_SRCS))
CUDA_TEST_MAIN_OBJ := $(OBJ_DIR)/tests/test_cuda.o

SOURCE_PAYLOAD_LIVE_OBJ := $(OBJ_DIR)/tests/live/source_payload_deepseek.o
QUANT_LIVE_OBJ := $(OBJ_DIR)/tests/live/quant_deepseek.o
ARTIFACT_LIVE_OBJ := $(OBJ_DIR)/tests/live/artifact_deepseek.o
MATERIALIZE_LIVE_OBJ := $(OBJ_DIR)/tests/live/materialize_deepseek.o
MINIMAX_AUDIO_LIVE_OBJ := $(OBJ_DIR)/tests/live/minimax_h3_audio.o
MINIMAX_VIDEO_LIVE_OBJ := $(OBJ_DIR)/tests/live/minimax_h3_video.o
PROGRAM_FORWARD_LIVE_OBJ := $(OBJ_DIR)/tests/live/program_forward.o
LAYA_NATIVE_LIVE_OBJ := $(OBJ_DIR)/tests/live/laya_native.o
MINIMAX_TEXT_LIVE_OBJ := $(OBJ_DIR)/tests/live/minimax_h3_text.o
MINIMAX_TRANSFORMER_LIVE_OBJ := $(OBJ_DIR)/tests/live/minimax_h3_transformer.o
ATTENTION_LIVE_OBJ := $(OBJ_DIR)/tests/live/attention_deepseek.o
PREFILL_LIVE_OBJ := $(OBJ_DIR)/tests/live/prefill_deepseek.o
MOE_LIVE_OBJ := $(OBJ_DIR)/tests/live/moe_deepseek.o
TRANSFORMER_LIVE_OBJ := $(OBJ_DIR)/tests/live/transformer_deepseek.o
DECODE_LIVE_OBJ := $(OBJ_DIR)/tests/live/decode_deepseek.o
LOGITS_LIVE_OBJ := $(OBJ_DIR)/tests/live/logits_deepseek.o
TOKENIZER_LIVE_OBJ := $(OBJ_DIR)/tests/live/tokenizer_deepseek.o
GENERATION_LIVE_OBJ := $(OBJ_DIR)/tests/live/generation_deepseek.o
DECISION_READOUT_LIVE_OBJ := $(OBJ_DIR)/tests/live/decision_readout.o
QWEN_ADMISSION_LIVE_OBJ := $(OBJ_DIR)/tests/live/qwen_admission.o
$(GENERATION_LIVE_OBJ): override CPPFLAGS += -I$(BUILD_DIR)/generated
$(GENERATION_LIVE_OBJ): $(BUILD_COMMIT_HEADER)
OPENAI_FAKE_HOST_OBJ := $(OBJ_DIR)/tests/integration/openai_host.o
OPENAI_ADAPTER_HOST_OBJ := $(OBJ_DIR)/tests/integration/openai_adapter.o
TINY_VERTICAL_COMPILER_OBJ := $(OBJ_DIR)/tests/integration/tiny_compile.o
NATIVE_TURN_TEST_OBJ := $(OBJ_DIR)/tests/integration/native_turn.o
RUST_BENCHMARK_FIXTURE := $(TEST_DIR)/rust_benchmark_fixture
RUST_BENCHMARK_FIXTURE_OBJ := $(OBJ_DIR)/tests/integration/runtime_benchmark.o
PRODUCT_MEASUREMENT_CLIENT := $(TEST_DIR)/product_client
PRODUCT_MEASUREMENT_CLIENT_OBJ := $(OBJ_DIR)/tests/integration/product_client.o
$(RUST_BENCHMARK_FIXTURE_OBJ): tests/test.h $(QA_REGISTRY_HEADER)

RUNNER_OBJS := $(TEST_MAIN_OBJ) $(QUANT_TEST_RUNNER_OBJ) \
	$(ARTIFACT_TEST_RUNNER_OBJ) $(CUDA_TEST_MAIN_OBJ) $(METAL_TEST_MAIN_OBJ) \
	$(SOURCE_PAYLOAD_LIVE_OBJ) $(QUANT_LIVE_OBJ) $(ARTIFACT_LIVE_OBJ) \
	$(MATERIALIZE_LIVE_OBJ) $(MINIMAX_AUDIO_LIVE_OBJ) $(MINIMAX_VIDEO_LIVE_OBJ) \
	$(MINIMAX_TEXT_LIVE_OBJ) $(MINIMAX_TRANSFORMER_LIVE_OBJ) $(PROGRAM_FORWARD_LIVE_OBJ) \
	$(LAYA_NATIVE_LIVE_OBJ) \
	$(ATTENTION_LIVE_OBJ) \
	$(PREFILL_LIVE_OBJ) $(MOE_LIVE_OBJ) \
	$(TRANSFORMER_LIVE_OBJ) $(DECODE_LIVE_OBJ) $(LOGITS_LIVE_OBJ) $(TOKENIZER_LIVE_OBJ) \
	$(GENERATION_LIVE_OBJ) $(DECISION_READOUT_LIVE_OBJ) $(QWEN_ADMISSION_LIVE_OBJ) \
	$(OPENAI_FAKE_HOST_OBJ) $(OPENAI_ADAPTER_HOST_OBJ) \
	$(TINY_VERTICAL_COMPILER_OBJ) $(NATIVE_TURN_TEST_OBJ) $(RUST_BENCHMARK_FIXTURE_OBJ) \
	$(PRODUCT_MEASUREMENT_CLIENT_OBJ)
DEPENDENCY_FILES := $(CORE_OBJS:.o=.d) $(TEST_UNIT_OBJS:.o=.d) \
	$(TEST_REFERENCE_OBJS:.o=.d) $(QUANT_TEST_UNIT_OBJS:.o=.d) \
	$(CUDA_TEST_UNIT_OBJS:.o=.d) $(RUNNER_OBJS:.o=.d)

CLI_TEST := tests/cli.sh
CLIENT_CUTOVER_TEST := tests/client_cutover.sh
REPL_PTY_TEST := tests/repl_pty.sh
CLIENT_REFOUNDATION_LIVE_TEST := tests/live/client_refoundation.sh
OPENAI_INTEGRATION_TEST := tests/integration/openai.sh
TINY_VERTICAL_TEST := tests/integration/tiny_vertical.sh
