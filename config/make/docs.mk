.PHONY: docs-setup docs-check docs-diagrams docs-benchmarks docs-site docs-pdf
docs-setup:
	python3 -m venv build/docs-venv
	build/docs-venv/bin/python -m pip install -r tools/docs/requirements.txt

docs-check: test-documentation-architecture test-project-control

docs-diagrams:
	python3 tools/render_diagrams.py

docs-benchmarks:
	$(DOCS_PYTHON) tools/docs/benchmarks.py

docs-site:
	$(DOCS_PYTHON) tools/docs/site.py

docs-pdf:
	$(DOCS_PYTHON) tools/docs/site.py --pdf $(if $(DOCS_BROWSER),--browser '$(DOCS_BROWSER)')
