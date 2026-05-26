.PHONY: info check

info:
	@echo "clori: standalone neural execution engine"
	@echo "status: repository skeleton"
	@echo "inference: not implemented"
	@echo "net-compatible: planned"

check:
	@test -f README.md
	@test -f docs/spines/clori-spine.md
	@test -f docs/spines/yai-net-spine-reference.md
	@test -f docs/integration/yai-net-compatibility.md
	@test -f docs/benchmark/benchmark-canon.md
	@grep -R "YAI controls authority" README.md docs/boundary.md >/dev/null
	@grep -R "NET moves streams" README.md docs/boundary.md >/dev/null
	@grep -R "CLORI executes neural computation" README.md docs/boundary.md >/dev/null
	@grep -R "No inference implementation exists" README.md docs >/dev/null
	@! grep -R "inference: implemented\\|implemented inference\\|server implemented" README.md docs src benches examples tests protocols 2>/dev/null >/dev/null
	@echo "clori check: ok"
