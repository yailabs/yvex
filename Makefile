.PHONY: info lib cli test check check-docs check-guardrails clean

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
	src/core/log.c

CORE_OBJS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))

TEST_SRCS := \
	tests/test_status.c \
	tests/test_error.c \
	tests/test_version.c \
	tests/test_log.c

TEST_BINS := $(patsubst tests/%.c,$(TEST_DIR)/%,$(TEST_SRCS))

CURRENT_DOCS := README.md NOTICE.md docs/README.md docs/roadmap.md docs/validation.md \
	docs/api.md docs/runtime-filesystem.md docs/cli-runtime.md docs/cli-layout.md \
	docs/logging-tracing.md docs/metrics.md docs/model-ladder.md docs/cuda-first.md \
	docs/backend-contract.md docs/yai-provider-boundary.md docs/failure-taxonomy.md \
	docs/delivery-box-template.md docs/runtime-system-design.md

info:
	@echo "yvex: C local inference engine"
	@echo "status: A0 core/CLI skeleton"
	@echo "interface: CLI-only"
	@echo "inference: not implemented"
	@echo "gguf: not implemented"
	@echo "cuda: not implemented"
	@echo "server: not implemented"

lib: $(LIBYVEX)

cli: $(YVEX_BIN)

test: $(TEST_BINS)
	@for test_bin in $(TEST_BINS); do \
		echo "$$test_bin"; \
		"$$test_bin"; \
	done

check: check-docs check-guardrails lib cli test
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
	@test -f docs/roadmap.md
	@test -f docs/validation.md
	@test -f docs/api.md
	@test -f docs/runtime-filesystem.md
	@test -f docs/runtime-system-design.md
	@test -f docs/cli-runtime.md
	@test -f docs/cli-layout.md
	@test -f docs/logging-tracing.md
	@test -f docs/metrics.md
	@test -f docs/model-ladder.md
	@test -f docs/cuda-first.md
	@test -f docs/backend-contract.md
	@test -f docs/yai-provider-boundary.md
	@test -f docs/failure-taxonomy.md
	@test -f docs/delivery-box-template.md
	@grep -F "YVEX Implementation Spine" docs/spine.md >/dev/null
	@grep -F "YVEX is CLI-only" docs/spine.md >/dev/null
	@grep -F "YVEX is a C local inference engine" README.md >/dev/null
	@grep -F "YVEX Roadmap" docs/roadmap.md >/dev/null
	@grep -F "YVEX Validation" docs/validation.md >/dev/null
	@grep -F "YVEX API" docs/api.md >/dev/null
	@grep -F "YVEX Runtime Filesystem" docs/runtime-filesystem.md >/dev/null
	@grep -F "YVEX Runtime System Design" docs/runtime-system-design.md >/dev/null
	@grep -F "YVEX CLI Runtime" docs/cli-runtime.md >/dev/null
	@grep -F "YVEX CLI Layout" docs/cli-layout.md >/dev/null
	@grep -F "YVEX Logging and Tracing" docs/logging-tracing.md >/dev/null
	@grep -F "YVEX Metrics" docs/metrics.md >/dev/null
	@grep -F "YVEX Model Ladder" docs/model-ladder.md >/dev/null
	@grep -F "YVEX CUDA-First Strategy" docs/cuda-first.md >/dev/null
	@grep -F "YVEX Backend Contract" docs/backend-contract.md >/dev/null
	@grep -F "YVEX / YAI Provider Boundary" docs/yai-provider-boundary.md >/dev/null
	@grep -F "YVEX Failure Taxonomy" docs/failure-taxonomy.md >/dev/null
	@grep -F "YVEX Delivery Box Template" docs/delivery-box-template.md >/dev/null

check-guardrails:
	@test ! -d docs/spines
	@test ! -d docs/integration
	@test ! -d docs/benchmark
	@test ! -d benches
	@test ! -d examples
	@test ! -d protocols
	@test ! -e src/README.md
	@test ! -e tests/README.md
	@test ! -e include/yvex/tui.h
	@test ! -e include/yvex/gguf.h
	@test ! -e include/yvex/model.h
	@test ! -e include/yvex/backend.h
	@test ! -e include/yvex/session.h
	@test ! -e include/yvex/server.h
	@test ! -d src/tui
	@test ! -d src/artifact
	@test ! -d src/formats
	@test ! -d src/model
	@test ! -d src/tokenizer
	@test ! -d src/graph
	@test ! -d src/backend
	@test ! -d src/session
	@test ! -d src/server
	@test ! -d backends
	@test ! -d fixtures
	@! find . -path './.git' -prune -o \( -path './tui' -o -path './src/tui' -o -path './include/yvex/tui.h' -o -path './docs/tui.md' -o -path './docs/tui-*.md' \) -print | grep .
	@! find . -path './.git' -prune -o -name 'panel_*.c' -print | grep .
	@! grep -I -n -E '#include[ <"]n[c]urses|l[n]curses|N[C]URSES' $(CURRENT_DOCS) >/dev/null
	@! grep -RIn -E "N[E]T\\.SPINE|N[E]T moves streams|C[L]ORI|c[l]ori-codename|docs/arc[h]ive|c[l]ori_|libc[l]ori|c[l]orid|include/c[l]ori|~/\\.config/c[l]ori|github\\.com/yailabs/c[l]ori|yailabs/c[l]ori" --exclude-dir=.git --exclude-dir=build . >/dev/null
	@! grep -Ei "production-ready|implemented inference|implemented server|supports CUDA|supports Metal|supports MLX|supports llama\\.cpp|OpenAI-compatible server" README.md >/dev/null
	@! grep -Ei "benchmark results" README.md | grep -vi "benchmark results: none" >/dev/null

clean:
	@rm -rf $(BUILD_DIR)
