.PHONY: info check

info:
	@echo "clori: neural execution engine"
	@echo "status: repository scaffold"
	@echo "inference: not implemented"
	@echo "server: not implemented"
	@echo "net-compatible: planned"

check:
	@test -f README.md
	@test -f docs/spines/clori-spine.md
	@test -f docs/spines/yai-net-spine-reference.md
	@test -f docs/integration/yai-net-compatibility.md
	@test -f docs/benchmark/benchmark-canon.md
	@grep -F "Reference version: NET.SPINE.0.2" docs/spines/yai-net-spine-reference.md >/dev/null
	@grep -F "YAI controls authority." docs/spines/clori-spine.md >/dev/null
	@grep -F "NET moves streams." docs/spines/clori-spine.md >/dev/null
	@grep -F "CLORI executes neural computation." docs/spines/clori-spine.md >/dev/null
	@! grep -E "production-ready|production ready" README.md | grep -v "not production-ready" >/dev/null
	@! grep -E "inference is implemented|implements inference|implemented inference" README.md >/dev/null
	@! grep -E "server is implemented|implements server|implemented server" README.md >/dev/null
	@echo "clori check: ok"
