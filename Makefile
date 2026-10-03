# YVEX: C/CUDA engine and Rust product shell. GNU Make >= 4.3.
# Production membership: config/source_owners.tsv; qualification: config/qa/.
# Developer entry points below; toolchain, rules and qualification stay separate.
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.SUFFIXES:

include config/make/config.mk
include config/make/rules.mk
include config/make/qa.mk
include config/make/docs.mk
include config/make/distribution.mk

# The parsed authored build inputs are a projection, not a second file inventory.
YVEX_AUTHORED_MAKEFILES := $(filter Makefile config/make/%.mk,$(MAKEFILE_LIST))

.PHONY: all lib client help info print-build-inputs
all: generate-source-manifest generate-operator-registry lib client
lib: $(LIBYVEX)
client: generate-operator-registry $(YVEX_BIN)

help:
	@printf '%s\n' \
		'YVEX build and qualification' \
		'  make [all]          Build the library and sole product executable' \
		'  make lib | client   Build an individual software product' \
		'  make cuda           Build CUDA images and their test runner' \
		'  make check          Software/structural checks (not release evidence)' \
		'  make qa-fast        Registered bounded software lane' \
		'  make qa-doctor      Show tool/hardware/evidence prerequisites' \
		'  make docs-check     Validate documentation/project control' \
		'  make package        Stage an UNQUALIFIED software candidate' \
		'  make install        Install that candidate; supports DESTDIR/prefix' \
		'  make clean          Remove only the owned build tree/product' \
		'' \
		'Overrides: CC AR CPPFLAGS CFLAGS LDFLAGS LDLIBS NVCC NVCCFLAGS' \
		'           CARGO RUSTC RUSTFLAGS RUST_PROFILE RUST_CARGO_TARGET_DIR' \
		'           BUILD_DIR YVEX_CUDA_ARCH prefix bindir datadir DESTDIR' \
		'Guide: docs/guides/build.md; QA: docs/evaluation/qa.md'

info:
	@printf '%s\n' \
		'product: $(YVEX_BIN); static engine library: $(LIBYVEX)' \
		'project control: docs/project-control/TASKS.md (ROADMAP.md: macro horizon)' \
		'release readiness: consult docs/project-control/STATUS.md' \
		'build directory: $(BUILD_DIR)' \
		'CUDA: $(if $(filter yes,$(NVCC_AVAILABLE)),$(if $(CUDA_EFFECTIVE_ARCH),$(CUDA_EFFECTIVE_ARCH),portable-ptx),compiler unavailable)'

print-build-inputs:
	@printf '%s\n' $(YVEX_AUTHORED_MAKEFILES)

-include $(DEPENDENCY_FILES) $(CUDA_PTX:%=%.d) $(CUDA_CUBIN:%=%.d)
