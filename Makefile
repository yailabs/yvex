.PHONY: info check check-docs check-guardrails clean

CURRENT_DOCS := README.md NOTICE.md docs/README.md docs/roadmap.md docs/validation.md \
	docs/api.md docs/runtime-filesystem.md docs/cli-runtime.md docs/cli-layout.md \
	docs/logging-tracing.md docs/metrics.md docs/model-ladder.md docs/cuda-first.md \
	docs/backend-contract.md docs/yai-provider-boundary.md docs/failure-taxonomy.md \
	docs/delivery-box-template.md

info:
	@echo "yvex: C local inference engine"
	@echo "status: pre-implementation spine"
	@echo "interface: CLI-only"
	@echo "inference: not implemented"
	@echo "cuda: not implemented"
	@echo "server: not implemented"

check: check-docs check-guardrails
	@echo "yvex check: ok"

check-docs:
	@test -f README.md
	@test -f NOTICE.md
	@test -f docs/README.md
	@test -f docs/spine.md
	@test -f docs/roadmap.md
	@test -f docs/validation.md
	@test -f docs/api.md
	@test -f docs/runtime-filesystem.md
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
	@test -f docs/archive/clori-codename.md
	@grep -F "YVEX Implementation Spine" docs/spine.md >/dev/null
	@grep -F "YVEX is CLI-only" docs/spine.md >/dev/null
	@grep -F "YVEX is a C local inference engine" README.md >/dev/null
	@grep -F "YVEX Roadmap" docs/roadmap.md >/dev/null
	@grep -F "YVEX Validation" docs/validation.md >/dev/null
	@grep -F "YVEX API" docs/api.md >/dev/null
	@grep -F "YVEX Runtime Filesystem" docs/runtime-filesystem.md >/dev/null
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
	@! find . -path './.git' -prune -o \( -path './tui' -o -path './src/tui' -o -path './include/yvex/tui.h' -o -path './docs/tui.md' -o -path './docs/tui-*.md' \) -print | grep .
	@! find . -path './.git' -prune -o -name 'panel_*.c' -print | grep .
	@! grep -I -n -E '#include[ <"]n[c]urses|l[n]curses|N[C]URSES' $(CURRENT_DOCS) >/dev/null
	@! grep -F "NET moves streams." $(CURRENT_DOCS) >/dev/null
	@! grep -F "CLORI executes neural computation." $(CURRENT_DOCS) >/dev/null
	@! grep -F "Reference version: NET.SPINE.0.3" $(CURRENT_DOCS) >/dev/null
	@! grep -Ei "production-ready|implemented inference|implemented server|supports CUDA|supports Metal|supports MLX|supports llama\\.cpp|OpenAI-compatible server" README.md >/dev/null
	@! grep -Ei "benchmark results" README.md | grep -vi "benchmark results: none" >/dev/null

clean:
	@rm -f yvex yvexd yvex_bench libyvex.a
