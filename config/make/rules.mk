# Cargo owns Rust incrementality; Make owns the C archive and authenticated source.
.PHONY: replai-rust-dependency gguf-reference-dependency rust-client
replai-rust-dependency:
	python3 tools/prepare_replai.py --rust-source '$(REPLAI_RUST_SOURCE)'

gguf-reference-dependency:
	python3 tools/prepare_gguf_reference.py --prefix '$(BUILD_DIR)/external/gguf-reference'

rust-client: lib generate-operator-registry $(BUILD_COMMIT_HEADER) $(RUST_BUILD_CONFIG) replai-rust-dependency gguf-reference-dependency
	+YVEX_NATIVE_BUILD_DIR='$(abspath $(BUILD_DIR))' \
		YVEX_NATIVE_LDFLAGS='$(YVEX_BUILD_LDFLAGS)' YVEX_NATIVE_LDLIBS='$(YVEX_BUILD_LDLIBS)' \
		RUSTC='$(RUSTC)' RUSTFLAGS='$(RUSTFLAGS)' \
		CARGO_TARGET_DIR='$(abspath $(RUST_CARGO_TARGET_DIR))' \
		$(CARGO) build --locked --profile '$(RUST_PROFILE)' --bin yvex

# Structural checks must inspect this invocation's archive and object root,
# not silently compare an isolated build to the default build directory.
.PHONY: print-archive-layout
print-archive-layout:
	@printf '%s\n' '$(LIBYVEX)' '$(OBJ_DIR)/'

generate-source-manifest: $(SOURCE_MANIFEST_MK) $(SOURCE_FAMILY_HEADER)
check-source-manifest: $(SOURCE_MANIFEST_MK) $(SOURCE_FAMILY_HEADER)
	python3 $(SOURCE_MANIFEST_GENERATOR) --manifest $(SOURCE_OWNER_MANIFEST) \
		--output $(SOURCE_MANIFEST_MK) --family-header $(SOURCE_FAMILY_HEADER) --check
	@set -eu; \
	. tests/support/cleanup.sh; \
	first=$$(mktemp "$${TMPDIR:-/tmp}/yvex-sources.XXXXXX"); \
	second=$$(mktemp "$${TMPDIR:-/tmp}/yvex-sources.XXXXXX"); \
	first_header=$$(mktemp "$${TMPDIR:-/tmp}/yvex-families.XXXXXX"); \
	second_header=$$(mktemp "$${TMPDIR:-/tmp}/yvex-families.XXXXXX"); \
	trap 'yvex_test_cleanup "$$first" "$$second" "$$first_header" "$$second_header"' \
		EXIT HUP INT TERM; \
	python3 $(SOURCE_MANIFEST_GENERATOR) --manifest $(SOURCE_OWNER_MANIFEST) \
		--output "$$first" --family-header "$$first_header"; \
	python3 $(SOURCE_MANIFEST_GENERATOR) --manifest $(SOURCE_OWNER_MANIFEST) \
		--output "$$second" --family-header "$$second_header"; \
	cmp "$$first" "$$second"; \
	cmp "$$first_header" "$$second_header"

generate-operator-registry: $(OPERATOR_REGISTRY_HEADER) $(OPERATOR_REGISTRY_C) \
	$(OPERATOR_REGISTRY_IDENTITY) $(OPERATOR_REGISTRY_JSON)

check-operator-registry: generate-operator-registry
	python3 $(OPERATOR_REGISTRY_GENERATOR) --registry $(OPERATOR_REGISTRY_SOURCE) \
		--output $(OPERATOR_REGISTRY_DIR) --check
	@set -eu; \
	. tests/support/cleanup.sh; \
	first=$$(mktemp -d "$${TMPDIR:-/tmp}/yvex-operator-registry.XXXXXX"); \
	second=$$(mktemp -d "$${TMPDIR:-/tmp}/yvex-operator-registry.XXXXXX"); \
	trap 'yvex_test_cleanup "$$first" "$$second"' EXIT HUP INT TERM; \
	python3 $(OPERATOR_REGISTRY_GENERATOR) --registry $(OPERATOR_REGISTRY_SOURCE) --output "$$first"; \
	python3 $(OPERATOR_REGISTRY_GENERATOR) --registry $(OPERATOR_REGISTRY_SOURCE) --output "$$second"; \
	diff -ru "$$first" "$$second" >/dev/null
$(LIBYVEX): $(CORE_OBJS) $(if $(filter Darwin,$(YVEX_HOST_OS)),tools/archive_darwin.py)
	@mkdir -p $(@D)
	@set -eu; tmp="$@.tmp.$$$$"; trap 'rm -f "$$tmp"' EXIT HUP INT TERM; \
		$(YVEX_ARCHIVER) "$$tmp" $(CORE_OBJS); mv "$$tmp" "$@"; trap - EXIT HUP INT TERM

# Linker changes must invalidate products even when no C source changed.
$(YVEX_BIN) $(TEST_RUNNER) $(QUANT_TEST_RUNNER) $(ARTIFACT_TEST_RUNNER) \
	$(CUDA_TEST_RUNNER) $(METAL_TEST_RUNNER) $(patsubst $(OBJ_DIR)/tests/live/%.o,$(TEST_DIR)/%,\
		$(filter $(OBJ_DIR)/tests/live/%.o,$(RUNNER_OBJS))) \
	$(OPENAI_FAKE_HOST) $(OPENAI_ADAPTER_HOST) $(TINY_VERTICAL_COMPILER) \
	$(NATIVE_TURN_TEST): $(LINK_BUILD_CONFIG)

$(OBJ_DIR)/%.o: %.c $(C_BUILD_CONFIG)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

ifeq ($(YVEX_NATIVE_METAL),yes)
$(OBJ_DIR)/%.o: %.m $(C_BUILD_CONFIG)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(METAL_CFLAGS) $(DEPFLAGS) -c $< -o $@
endif

$(SOURCE_MANIFEST_MK) $(SOURCE_FAMILY_HEADER) &: \
		$(SOURCE_OWNER_MANIFEST) $(SOURCE_MANIFEST_GENERATOR) config/qa/registry.json
	@mkdir -p $(@D)
	python3 $(SOURCE_MANIFEST_GENERATOR) --manifest $(SOURCE_OWNER_MANIFEST) \
		--output $(SOURCE_MANIFEST_MK) --family-header $(SOURCE_FAMILY_HEADER)

$(OBJ_DIR)/src/graph/catalog.o: $(SOURCE_FAMILY_HEADER)

$(QA_REGISTRY_MK) $(QA_REGISTRY_PROJECTIONS) &: \
		$(QA_REGISTRY_SOURCE) $(QA_REGISTRY_GENERATOR) Makefile $(wildcard config/make/*.mk)
	python3 $(QA_REGISTRY_GENERATOR) --registry $(QA_REGISTRY_SOURCE) \
		--output-dir $(QA_REGISTRY_DIR)

$(OPERATOR_REGISTRY_HEADER) $(OPERATOR_REGISTRY_C) $(OPERATOR_REGISTRY_IDENTITY) \
		$(OPERATOR_REGISTRY_JSON) &: \
		$(OPERATOR_REGISTRY_SOURCE) $(OPERATOR_REGISTRY_GENERATOR) src/cli/rust/management.rs
	python3 $(OPERATOR_REGISTRY_GENERATOR) --registry $(OPERATOR_REGISTRY_SOURCE) \
		--output $(OPERATOR_REGISTRY_DIR)

$(OPERATOR_REGISTRY_OBJ): $(OPERATOR_REGISTRY_C) $(OPERATOR_REGISTRY_HEADER) $(C_BUILD_CONFIG)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -I$(BUILD_DIR)/generated $(CFLAGS) $(DEPFLAGS) -c \
		$(OPERATOR_REGISTRY_C) -o $@

.PHONY: FORCE
FORCE:

print-build-identity:
	@printf '%s\n' '$(YVEX_BUILD_IDENTITY)'

# Recompile every C consumer when material compiler flags change, not just
# provenance-bearing consumers. Target-local flags remain source-owned.
$(C_BUILD_CONFIG): FORCE
	@mkdir -p $(@D)
	@set -eu; tmp="$@.tmp.$$$$"; trap 'rm -f "$$tmp"' EXIT HUP INT TERM; \
	printf '%s\n' 'cc=$(CC)' 'cc-version=$(shell $(CC) --version 2>/dev/null | head -1)' \
		'cppflags=$(YVEX_BUILD_CPPFLAGS)' 'cflags=$(YVEX_BUILD_CFLAGS)' \
		'metal-cflags=$(METAL_CFLAGS)' 'native-metal=$(YVEX_NATIVE_METAL)' >"$$tmp"; \
	if test -r "$@" && cmp -s "$$tmp" "$@"; then rm -f "$$tmp"; \
	else mv "$$tmp" "$@"; fi; trap - EXIT HUP INT TERM

$(LINK_BUILD_CONFIG): FORCE
	@mkdir -p $(@D)
	@set -eu; tmp="$@.tmp.$$$$"; trap 'rm -f "$$tmp"' EXIT HUP INT TERM; \
	printf '%s\n' 'cc=$(CC)' 'ldflags=$(YVEX_BUILD_LDFLAGS)' \
		'ldlibs=$(YVEX_BUILD_LDLIBS)' >"$$tmp"; \
	if test -r "$@" && cmp -s "$$tmp" "$@"; then rm -f "$$tmp"; \
	else mv "$$tmp" "$@"; fi; trap - EXIT HUP INT TERM

$(RUST_BUILD_CONFIG): FORCE gguf-reference-dependency
	@mkdir -p $(@D)
	@set -eu; tmp="$@.tmp.$$$$"; trap 'rm -f "$$tmp"' EXIT HUP INT TERM; \
	printf '%s\n' 'shell-identity=$(YVEX_SHELL_BUILD_IDENTITY)' \
		'cargo=$(CARGO)' 'rustc=$(RUSTC)' 'profile=$(RUST_PROFILE)' \
		'rustflags=$(RUSTFLAGS)' 'ldflags=$(YVEX_BUILD_LDFLAGS)' 'ldlibs=$(YVEX_BUILD_LDLIBS)' >"$$tmp"; \
	if test -r "$@" && cmp -s "$$tmp" "$@"; then rm -f "$$tmp"; \
	else mv "$$tmp" "$@"; fi; trap - EXIT HUP INT TERM

# CUDA images follow resolved architecture, compiler and transitive headers.
$(CUDA_BUILD_CONFIG): FORCE
	@mkdir -p $(@D)
	@set -eu; tmp="$@.tmp.$$$$"; trap 'rm -f "$$tmp"' EXIT HUP INT TERM; \
	printf '%s\n' \
		'nvcc=$(NVCC)' 'nvcc-version=$(shell $(NVCC) --version 2>/dev/null | tail -1)' \
		'cppflags=$(YVEX_BUILD_CPPFLAGS)' 'nvccflags=$(NVCCFLAGS)' \
		'cuda_arch_request=$(YVEX_CUDA_ARCH)' \
		'cuda_arch_effective=$(CUDA_EFFECTIVE_ARCH)' >"$$tmp"; \
	if test -r "$@" && cmp -s "$$tmp" "$@"; then rm -f "$$tmp"; \
	else mv "$$tmp" "$@"; fi; trap - EXIT HUP INT TERM

# Revalidate commit and source cleanliness on every invocation; replace the
# generated header only when exact provenance changes.
$(BUILD_COMMIT_HEADER): FORCE
	@mkdir -p $(@D)
	@set -eu; tmp="$@.tmp.$$$$"; trap 'rm -f "$$tmp"' EXIT HUP INT TERM; \
	printf '#ifndef YVEX_BUILD_PROVENANCE_INCLUDED\n#define YVEX_BUILD_PROVENANCE_INCLUDED\n#define YVEX_BUILD_COMMIT "%s"\n#define YVEX_BUILD_SOURCE_TREE "%s"\n#define YVEX_BUILD_SOURCE_STATE "%s"\n#define YVEX_BUILD_SOURCE_DELTA_IDENTITY "%s"\n#define YVEX_BUILD_IDENTITY "%s"\n#define YVEX_BUILD_SOURCE_ROOT "%s"\n#endif\n' \
		'$(YVEX_BUILD_COMMIT)' '$(YVEX_BUILD_SOURCE_TREE)' '$(YVEX_BUILD_SOURCE_STATE)' \
		'$(YVEX_BUILD_SOURCE_DELTA_IDENTITY)' '$(YVEX_BUILD_IDENTITY)' \
		'$(YVEX_BUILD_SOURCE_ROOT)' >"$$tmp"; \
	if test -r "$@" && cmp -s "$$tmp" "$@"; then rm -f "$$tmp"; \
	else mv "$$tmp" "$@"; fi; trap - EXIT HUP INT TERM

$(OBJ_DIR)/tests/unit/%.o: tests/unit/%.c tests/test.h $(QA_REGISTRY_HEADER) $(C_BUILD_CONFIG)
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJ_DIR)/tests/unit/cuda/%.o: tests/unit/cuda/%.c tests/test.h $(QA_REGISTRY_HEADER) $(C_BUILD_CONFIG)
	@mkdir -p $(@D)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJ_DIR)/%.ptx: %.cu include/yvex/qtype.h src/backend/cuda/kernel_primitives.h \
		$(CUDA_BUILD_CONFIG)
	@mkdir -p $(@D)
	$(NVCC) $(CPPFLAGS) $(NVCCFLAGS) $(CUDA_ARCH_FLAG) -MMD -MP -MF $@.d -MT $@ -ptx $< -o $@

$(OBJ_DIR)/%.cubin: %.cu include/yvex/qtype.h src/backend/cuda/kernel_primitives.h \
		$(CUDA_BUILD_CONFIG)
	@mkdir -p $(@D)
	$(NVCC) $(CPPFLAGS) $(NVCCFLAGS) $(CUDA_ARCH_FLAG) -MMD -MP -MF $@.d -MT $@ -cubin $< -o $@
	@$(CUOBJDUMP) --list-elf $@ | grep -F '$(CUDA_NATIVE_ARCH)' >/dev/null || { \
		echo "native CUDA image does not contain $(CUDA_NATIVE_ARCH): $@" >&2; exit 1; }
	@$(CUOBJDUMP) --dump-sass $@ | grep -F 'Function :' >/dev/null || { \
		echo "native CUDA image contains no SASS functions: $@" >&2; exit 1; }

$(CUDA_EXPORT_INC): $(CUDA_PTX) tools/generate_cuda_exports.py
	@mkdir -p $(@D)
	@set -eu; tmp="$@.tmp.$$$$"; trap 'rm -f "$$tmp"' EXIT HUP INT TERM; \
		python3 tools/generate_cuda_exports.py $(CUDA_PTX) >"$$tmp"; \
		mv "$$tmp" "$@"; trap - EXIT HUP INT TERM

$(CUDA_PTX_INC): $(CUDA_PTX)
	@mkdir -p $(@D)
	@tmp="$@.tmp.$$$$"; trap 'rm -f "$$tmp"' EXIT HUP INT TERM; { \
		index=0; names=''; \
		for image in $(CUDA_PTX); do \
			name="cuda_kernel_ptx_$${index}"; names="$$names $$name"; \
			printf 'static const unsigned char %s[] = {\n' "$$name"; \
			{ cat "$$image"; printf '\0'; } | xxd -i; \
			printf '};\n'; index=$$((index + 1)); \
		done; \
		printf 'static const unsigned char *const cuda_kernel_ptx_images[] = {\n'; \
		for name in $$names; do printf '    %s,\n' "$$name"; done; \
		printf '};\nstatic const unsigned long long cuda_kernel_ptx_image_bytes[] = {\n'; \
		for name in $$names; do printf '    sizeof(%s) - 1u,\n' "$$name"; done; \
		printf '};\n#define CUDA_KERNEL_PTX_IMAGE_COUNT %s\n' "$$index"; \
	} >"$$tmp"; mv "$$tmp" "$@"; trap - EXIT HUP INT TERM

$(CUDA_CUBIN_INC): $(CUDA_CUBIN)
	@mkdir -p $(@D)
	@tmp="$@.tmp.$$$$"; trap 'rm -f "$$tmp"' EXIT HUP INT TERM; { \
		index=0; names=''; \
		for image in $(CUDA_CUBIN); do \
			name="cuda_kernel_cubin_$${index}"; names="$$names $$name"; \
			printf 'static const unsigned char %s[] = {\n' "$$name"; \
			cat "$$image" | xxd -i; \
			printf '};\n'; index=$$((index + 1)); \
		done; \
		printf 'static const unsigned char *const cuda_kernel_cubin_images[] = {\n'; \
		for name in $$names; do printf '    %s,\n' "$$name"; done; \
		printf '};\nstatic const unsigned long long cuda_kernel_cubin_image_bytes[] = {\n'; \
		for name in $$names; do printf '    sizeof(%s),\n' "$$name"; done; \
		printf '};\n#define CUDA_KERNEL_CUBIN_IMAGE_COUNT %s\n' "$$index"; \
		printf 'static const char cuda_kernels_cubin_arch[] = "%s";\n' \
			'$(CUDA_NATIVE_ARCH)'; \
	} >"$$tmp"; mv "$$tmp" "$@"; trap - EXIT HUP INT TERM

$(YVEX_BIN): rust-client
	@mkdir -p $(@D)
	@set -eu; \
	if test -r "$@" && cmp -s '$(RUST_SHELL_BIN)' "$@"; then exit 0; fi; \
	tmp="$@.tmp.$$$$"; trap 'rm -f "$$tmp"' EXIT HUP INT TERM; \
	cp '$(RUST_SHELL_BIN)' "$$tmp"; chmod 755 "$$tmp"; \
	mv "$$tmp" "$@"; trap - EXIT HUP INT TERM

$(TEST_MAIN_OBJ) $(CUDA_TEST_MAIN_OBJ): override CPPFLAGS += -I$(BUILD_DIR)/generated
$(TEST_MAIN_OBJ) $(CUDA_TEST_MAIN_OBJ): $(QA_REGISTRY_HEADER)
$(TEST_MAIN_OBJ): $(QA_REGISTRY_DIR)/unit_registry.inc tests/support/runner.h
$(CUDA_TEST_MAIN_OBJ): $(QA_REGISTRY_DIR)/cuda_registry.inc tests/support/runner.h
$(QUANT_TEST_RUNNER_OBJ): $(QA_REGISTRY_DIR)/quant_registry.inc tests/support/runner.h
$(ARTIFACT_TEST_RUNNER_OBJ): $(QA_REGISTRY_DIR)/artifact_registry.inc tests/support/runner.h

$(TEST_RUNNER): $(TEST_MAIN_OBJ) $(TEST_UNIT_OBJS) $(TEST_REFERENCE_OBJS) \
	$(OPENAI_ADAPTER_OBJS) $(LIBYVEX) tests/test.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(TEST_MAIN_OBJ) $(TEST_UNIT_OBJS) $(TEST_REFERENCE_OBJS) \
		$(OPENAI_ADAPTER_OBJS) $(LIBYVEX) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(QUANT_TEST_RUNNER): $(QUANT_TEST_RUNNER_OBJ) $(QUANT_TEST_UNIT_OBJS) $(LIBYVEX) tests/test.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(QUANT_TEST_RUNNER_OBJ) $(QUANT_TEST_UNIT_OBJS) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(ARTIFACT_TEST_RUNNER): $(ARTIFACT_TEST_RUNNER_OBJ) \
	$(patsubst %.c,$(OBJ_DIR)/%.o,$(QA_ARTIFACT_TEST_SRCS)) $(LIBYVEX) tests/test.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(ARTIFACT_TEST_RUNNER_OBJ) \
		$(patsubst %.c,$(OBJ_DIR)/%.o,$(QA_ARTIFACT_TEST_SRCS)) \
		$(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(OPENAI_FAKE_HOST): $(OPENAI_FAKE_HOST_OBJ) $(OPENAI_ADAPTER_OBJS) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(OPENAI_FAKE_HOST_OBJ) $(OPENAI_ADAPTER_OBJS) $(LIBYVEX) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(OPENAI_ADAPTER_HOST): $(OPENAI_ADAPTER_HOST_OBJ) $(OPENAI_ADAPTER_OBJS) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(OPENAI_ADAPTER_HOST_OBJ) $(OPENAI_ADAPTER_OBJS) $(LIBYVEX) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(TINY_VERTICAL_COMPILER): $(TINY_VERTICAL_COMPILER_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(TINY_VERTICAL_COMPILER_OBJ) $(LIBYVEX) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(NATIVE_TURN_TEST): $(NATIVE_TURN_TEST_OBJ) $(OPENAI_ADAPTER_OBJS) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(NATIVE_TURN_TEST_OBJ) $(OPENAI_ADAPTER_OBJS) $(LIBYVEX) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(SOURCE_PAYLOAD_LIVE_RUNNER): $(SOURCE_PAYLOAD_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(SOURCE_PAYLOAD_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(QUANT_LIVE_RUNNER): $(QUANT_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(QUANT_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(ARTIFACT_LIVE_RUNNER): $(ARTIFACT_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(ARTIFACT_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(MATERIALIZE_LIVE_RUNNER): $(MATERIALIZE_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(MATERIALIZE_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(MINIMAX_AUDIO_LIVE_RUNNER): $(MINIMAX_AUDIO_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(MINIMAX_AUDIO_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_DIR)/program_forward: $(PROGRAM_FORWARD_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(PROGRAM_FORWARD_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_DIR)/laya_native: $(LAYA_NATIVE_LIVE_OBJ) $(OPENAI_ADAPTER_OBJS) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LAYA_NATIVE_LIVE_OBJ) $(OPENAI_ADAPTER_OBJS) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

.PHONY: test-laya-native-live
test-laya-native-live: $(TEST_DIR)/laya_native
	@test -n "$(LAYA_TYPED_SOURCE)" || { echo "LAYA_TYPED_SOURCE is required" >&2; exit 2; }
	$(TEST_DIR)/laya_native "$(LAYA_TYPED_SOURCE)"

.PHONY: test-program-forward-live
test-program-forward-live: $(TEST_DIR)/program_forward
	@test -n "$(YVEX_PROGRAM_ARTIFACT)" -a -n "$(YVEX_PROGRAM_BINDING)" -a -n "$(YVEX_PROGRAM_TARGET)" || { \
		echo "YVEX_PROGRAM_ARTIFACT, YVEX_PROGRAM_BINDING and YVEX_PROGRAM_TARGET are required" >&2; exit 2; }
	$(TEST_DIR)/program_forward "$(YVEX_PROGRAM_ARTIFACT)" "$(YVEX_PROGRAM_BINDING)" "$(YVEX_PROGRAM_TARGET)"

$(MINIMAX_VIDEO_LIVE_RUNNER): $(MINIMAX_VIDEO_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(MINIMAX_VIDEO_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(MINIMAX_TEXT_LIVE_RUNNER): $(MINIMAX_TEXT_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(MINIMAX_TEXT_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(MINIMAX_TRANSFORMER_LIVE_RUNNER): $(MINIMAX_TRANSFORMER_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(MINIMAX_TRANSFORMER_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(ATTENTION_LIVE_RUNNER): $(ATTENTION_LIVE_OBJ) $(TEST_REFERENCE_OBJS) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(ATTENTION_LIVE_OBJ) $(TEST_REFERENCE_OBJS) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(PREFILL_LIVE_RUNNER): $(PREFILL_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(PREFILL_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(MOE_LIVE_RUNNER): $(MOE_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(MOE_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(TRANSFORMER_LIVE_RUNNER): $(TRANSFORMER_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(TRANSFORMER_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(DECODE_LIVE_RUNNER): $(DECODE_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DECODE_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(LOGITS_LIVE_RUNNER): $(LOGITS_LIVE_OBJ) $(TEST_REFERENCE_OBJS) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LOGITS_LIVE_OBJ) $(TEST_REFERENCE_OBJS) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(TOKENIZER_LIVE_RUNNER): $(TOKENIZER_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(TOKENIZER_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(GENERATION_LIVE_RUNNER): $(GENERATION_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(GENERATION_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(DECISION_READOUT_LIVE_RUNNER): $(DECISION_READOUT_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DECISION_READOUT_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(QWEN_ADMISSION_LIVE_RUNNER): $(QWEN_ADMISSION_LIVE_OBJ) $(LIBYVEX)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(QWEN_ADMISSION_LIVE_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

$(OFFICIAL_GGUF_CHECKER): tests/external/ggml_gguf_check.cpp
	@test "$$(git -C "$(PINNED_GGML_ROOT)" rev-parse HEAD)" = af97976c7810cdabb1863172f31c432dab767de7
	@test -z "$$(git -C "$(PINNED_GGML_ROOT)" status --porcelain --untracked-files=no)"
	cmake -S "$(PINNED_GGML_ROOT)" -B "$(PINNED_GGML_BUILD)" \
		-DGGML_BUILD_TESTS=OFF -DGGML_BUILD_EXAMPLES=OFF \
		-DGGML_BUILD_TOOLS=OFF -DGGML_BUILD_SERVER=OFF \
		-DGGML_CUDA=OFF -DGGML_METAL=OFF -DGGML_OPENMP=OFF \
		-DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
	cmake --build "$(PINNED_GGML_BUILD)" -j4
	c++ -std=c++17 -Wall -Wextra -pedantic \
		-I"$(PINNED_GGML_ROOT)/include" $< \
		"$(PINNED_GGML_BUILD)/src/libggml-base.a" \
		$(if $(filter Darwin,$(YVEX_HOST_OS)),,-ldl) -pthread -lm -o $@

$(CUDA_TEST_RUNNER): $(CUDA_TEST_MAIN_OBJ) $(CUDA_TEST_UNIT_OBJS) $(LIBYVEX) tests/test.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CUDA_TEST_MAIN_OBJ) $(CUDA_TEST_UNIT_OBJS) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	@set -eu; \
	build_dir='$(BUILD_DIR)'; \
	temporary_dir=$${TMPDIR:-/tmp}; temporary_dir=$${temporary_dir%/}; \
	case "/$$build_dir/" in */../*|*/./*) \
		printf 'clean: refusing non-canonical BUILD_DIR: %s\n' "$$build_dir" >&2; exit 1;; esac; \
	case "$$build_dir" in *//*) \
		printf 'clean: refusing repeated path separators: %s\n' "$$build_dir" >&2; exit 1;; esac; \
	case "$$build_dir" in \
		build|build/*|/tmp/yvex-*/build|/tmp/yvex.*/build|"$$temporary_dir"/yvex-*/build|"$$temporary_dir"/yvex.*/build) ;; \
		*) printf 'clean: refusing unowned BUILD_DIR: %s\n' "$$build_dir" >&2; exit 1 ;; \
	esac; \
	if [ -L "$$build_dir" ]; then \
		printf 'clean: refusing symlink BUILD_DIR: %s\n' "$$build_dir" >&2; exit 1; \
	fi; \
	if ! python3 tools/check_build_path.py "$$build_dir"; then \
		printf 'clean: refusing symlink ancestor: %s\n' "$$build_dir" >&2; exit 1; \
	fi; \
	if [ -d "$$build_dir" ]; then \
		find "$$build_dir" -depth -mindepth 1 -delete; \
		rmdir "$$build_dir"; \
	elif [ -e "$$build_dir" ]; then \
		printf 'clean: refusing non-directory BUILD_DIR: %s\n' "$$build_dir" >&2; exit 1; \
	fi; \
	if test "$$build_dir" = build; then \
		rm -f -- ./yvex; \
	fi

$(METAL_TEST_MAIN_OBJ): $(QA_REGISTRY_HEADER) $(QA_REGISTRY_DIR)/metal_registry.inc tests/support/runner.h
$(METAL_TEST_RUNNER): $(METAL_TEST_MAIN_OBJ) $(METAL_TEST_UNIT_OBJ) $(LIBYVEX) tests/test.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(METAL_TEST_MAIN_OBJ) $(METAL_TEST_UNIT_OBJ) $(LIBYVEX) $(LDFLAGS) $(LDLIBS) -o $@
