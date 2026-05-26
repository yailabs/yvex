.PHONY: info check

info:
	@echo "clori: neural execution engine"
	@echo "status: repository scaffold"
	@echo "inference: not implemented"
	@echo "server: not implemented"
	@echo "net-compatible: planned"

check:
	@test -f README.md
	@test -f docs/README.md
	@test -f docs/architecture.md
	@test -f docs/boundary.md
	@test -f docs/terminology.md
	@test -f docs/spines/clori-spine.md
	@test -f docs/spines/yai-net-spine-reference.md
	@test -f docs/integration/yai-net-compatibility.md
	@test -f docs/benchmark/benchmark-canon.md
	@grep -F "Reference version: NET.SPINE.0.3" docs/spines/yai-net-spine-reference.md >/dev/null
	@grep -F "Source path: work/spines/net-spine.md" docs/spines/yai-net-spine-reference.md >/dev/null
	@grep -F "YAI controls authority." docs/spines/clori-spine.md >/dev/null
	@grep -F "NET moves streams." docs/spines/clori-spine.md >/dev/null
	@grep -F "CLORI executes neural computation." docs/spines/clori-spine.md >/dev/null
	@! grep -E "production-ready" README.md | grep -v "not production-ready" >/dev/null
	@! grep -Ei "fastest|implemented inference|implemented server|supports CUDA|supports Metal|supports MLX|supports llama\\.cpp|OpenAI-compatible server|transparent" README.md >/dev/null
	@! grep -Ei "benchmark results" README.md | grep -vi "no benchmark results" >/dev/null
	@echo "clori check: ok"
