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
	@grep -F "Reference version: NET.SPINE.0.3" docs/spines/yai-net-spine-reference.md >/dev/null
	@grep -F "Source path: work/spines/net-spine.md" docs/spines/yai-net-spine-reference.md >/dev/null
	@grep -F "YAI controls authority." docs/spines/clori-spine.md >/dev/null
	@grep -F "NET moves streams." docs/spines/clori-spine.md >/dev/null
	@grep -F "CLORI executes neural computation." docs/spines/clori-spine.md >/dev/null
	@! grep -E "production-ready" README.md | grep -v "not production-ready" >/dev/null
	@! grep -E "fastest|benchmark result|implemented inference|implemented server|working inference|working server" README.md | grep -v "No benchmark results exist yet" | grep -v "no benchmark results" | grep -v "working inference engine" | grep -v "working server" >/dev/null
	@echo "clori check: ok"
