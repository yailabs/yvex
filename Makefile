# YVEX - Build and validation entrypoint
#
# File: Makefile
# Layer: build system
#
# Purpose:
#   Builds the YVEX C core library, filesystem/artifact/GGUF/model layers,
#   CLI bootstrap, and tests. Also runs documentation and guardrail validation.
#
# Primary commands:
#   make info
#   make lib
#   make cli
#   make test
#   make test-core
#   make test-cli
#   make smoke
#   make check
#   make clean
#
# Interface policy:
#   - YVEX is CLI-only.
#   - build/bin/yvex is the current user-facing executable surface.

.PHONY: info lib cli test test-core test-cli smoke check check-docs check-guardrails clean

CC ?= cc
AR ?= ar

CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic
LDFLAGS ?=

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
LIB_DIR := $(BUILD_DIR)/lib
BIN_DIR := $(BUILD_DIR)/bin
TEST_DIR := $(BUILD_DIR)/tests

LIBYVEX := $(LIB_DIR)/libyvex.a
YVEX_BIN := $(BIN_DIR)/yvex

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
	src/model/descriptor.c

CORE_OBJS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))

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
	tests/test_model_descriptor.c

TEST_BINS := $(patsubst tests/%.c,$(TEST_DIR)/%,$(TEST_SRCS))

CURRENT_DOCS := README.md NOTICE.md docs/README.md docs/spine.md \
	docs/api.md docs/backend-contract.md docs/runtime-filesystem.md docs/cli-runtime.md

info:
	@echo "yvex: C local inference engine"
	@echo "status: D0 tensor/model descriptor layer"
	@echo "interface: CLI-only"
	@echo "library: libyvex.a"
	@echo "filesystem: implemented"
	@echo "artifact: open/read implemented"
	@echo "gguf: metadata/tensor directory parsing implemented"
	@echo "model: descriptor-only implemented"
	@echo "inference: not implemented"
	@echo "cuda: not implemented"
	@echo "server: not implemented"

lib: $(LIBYVEX)

cli: $(YVEX_BIN)

test-core: $(TEST_BINS)
	@for test_bin in $(TEST_BINS); do \
		echo "$$test_bin"; \
		"$$test_bin"; \
	done

test-cli: $(YVEX_BIN) tests/test_cli.sh
	YVEX_BIN=$(YVEX_BIN) sh tests/test_cli.sh

test: test-core test-cli

smoke: test-cli

check: check-docs check-guardrails lib cli test smoke
	@echo "yvex check: ok"

$(LIBYVEX): $(CORE_OBJS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(YVEX_BIN): cli/yvex_cli.c $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIBYVEX) $(LDFLAGS) -o $@

$(TEST_DIR)/%: tests/%.c $(LIBYVEX) tests/test.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIBYVEX) $(LDFLAGS) -o $@

check-docs:
	@test -f README.md
	@test -f NOTICE.md
	@test -f docs/README.md
	@test -f docs/spine.md
	@test -f docs/api.md
	@test -f docs/backend-contract.md
	@test -f docs/runtime-filesystem.md
	@test -f docs/cli-runtime.md
	@! find docs -maxdepth 1 -type f -name '*.md' \
		! -name README.md \
		! -name spine.md \
		! -name api.md \
		! -name backend-contract.md \
		! -name runtime-filesystem.md \
		! -name cli-runtime.md \
		-print | grep .
	@grep -F "YVEX Implementation Spine" docs/spine.md >/dev/null
	@grep -F "YVEX is CLI-only" docs/spine.md >/dev/null
	@grep -F "YVEX is a C local inference engine" README.md >/dev/null
	@grep -F "Completed Milestones" docs/spine.md >/dev/null
	@grep -F "C1 - GGUF metadata and tensor directory" docs/spine.md >/dev/null
	@grep -F "### C1 - GGUF metadata and tensor directory" docs/spine.md >/dev/null
	@grep -F "D0 - Tensor and model layer" docs/spine.md >/dev/null
	@grep -F "Implemented by:" docs/spine.md >/dev/null
	@grep -F "YVEX API" docs/api.md >/dev/null
	@grep -F "YVEX Backend Contract" docs/backend-contract.md >/dev/null
	@grep -F "YVEX Runtime Filesystem" docs/runtime-filesystem.md >/dev/null
	@grep -F "YVEX CLI Runtime" docs/cli-runtime.md >/dev/null
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
	@test ! -e include/yvex/tokenizer.h
	@test ! -e include/yvex/backend.h
	@test ! -e include/yvex/session.h
	@test ! -e include/yvex/server.h
	@test ! -d src/tokenizer
	@test ! -d src/graph
	@test ! -d src/backend
	@test ! -d src/session
	@test ! -d src/server
	@test ! -d backends
	@test ! -d fixtures
	@test -d cli
	@test -f cli/yvex_cli.c
	@test ! -d ui
	@test ! -d app
	@test ! -d desktop
	@! grep -RIn -E "N[E]T\\.SPINE|N[E]T moves streams|C[L]ORI|c[l]ori-codename|docs/arc[h]ive|c[l]ori_|libc[l]ori|c[l]orid|include/c[l]ori|~/\\.config/c[l]ori|github\\.com/yailabs/c[l]ori|yailabs/c[l]ori" --exclude-dir=.git --exclude-dir=build . >/dev/null
	@! grep -Ei "production-read[y]|implemented infer[e]nce|implemented ser[v]er|supports C[U]DA|supports M[e]tal|supports M[L]X|supports llama\\.cpp|OpenAI-compatible ser[v]er" README.md >/dev/null
	@! grep -Ei "benchmark results" README.md | grep -vi "benchmark results: none" >/dev/null

clean:
	@rm -rf $(BUILD_DIR)
