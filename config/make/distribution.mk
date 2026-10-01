package: client config/package_manifest.tsv LICENSE NOTICE.md
	@set -eu; \
	package_root='$(BUILD_DIR)/package'; \
	package_dir='$(BUILD_DIR)/package/product'; \
	case '/$(BUILD_DIR)/' in */../*|*/./*) \
		echo 'package: BUILD_DIR may not contain dot components' >&2; exit 1;; esac; \
	case '$(BUILD_DIR)' in ''|/|.) \
		echo 'package: refusing broad BUILD_DIR' >&2; exit 1;; esac; \
	if ! python3 tools/check_build_path.py "$$package_root"; then \
		echo 'package: refusing symlink ancestor' >&2; exit 1; fi; \
	if test -L "$$package_root"; then echo 'package root may not be a symlink' >&2; exit 1; fi; \
	if test -d "$$package_root"; then find "$$package_root" -depth -mindepth 1 -delete; fi; \
	mkdir -p "$$package_dir/bin" "$$package_dir/share/yvex"; \
	cp '$(YVEX_BIN)' "$$package_dir/bin/yvex"; \
	cp config/package_manifest.tsv LICENSE NOTICE.md "$$package_dir/share/yvex/"; \
	mkdir -p "$$package_dir/share/licenses/replai"; \
	cp '$(REPLAI_PREFIX)/share/licenses/replai/LICENSE' "$$package_dir/share/licenses/replai/"; \
	cp '$(REPLAI_PREFIX)/replai-build.json' "$$package_dir/share/yvex/"; \
	printf '%s\n' 'yvex package: command and foreground model server' \
		> "$$package_dir/share/yvex/profile"; \
	commit=$$(git rev-parse HEAD); \
	client_sha=$$(sha256sum '$(YVEX_BIN)' | awk '{print $$1}'); \
	library_sha=$$(sha256sum '$(LIBYVEX)' | awk '{print $$1}'); \
	registry_identity=$$(cat '$(OPERATOR_REGISTRY_IDENTITY)'); \
	package_identity=$$(printf '%s\n' "$$commit" '$(YVEX_PROTOCOL_VERSION)' 'cpu+cuda-dynamic' \
		"$$registry_identity" "$$client_sha" "$$library_sha" | \
		sha256sum | awk '{print $$1}'); \
	{ printf 'field\tvalue\n'; \
	  printf 'profile\tproduct\nsource_commit\t%s\n' "$$commit"; \
	  printf 'package_identity\t%s\n' "$$package_identity"; \
	  printf 'protocol_version\t%s\noperator_registry_identity\t%s\nbackend\t%s\n' \
		'$(YVEX_PROTOCOL_VERSION)' "$$registry_identity" 'cpu+cuda-dynamic'; \
	  printf 'yvex_sha256\t%s\nlibyvex_sha256\t%s\n' \
		"$$client_sha" "$$library_sha"; \
	  printf 'distribution_legal_status\tUNQUALIFIED\n'; \
	} > "$$package_dir/share/yvex/build.tsv"

# `package` is a software candidate, not permission to distribute it.
# Recipient material is assembled by tools/distribution_legal.py bundle;
# qualification verifies the exact resulting package and first-party authority.
.PHONY: qualify-distribution test-distribution-legal
qualify-distribution:
	@test -n "$(DISTRIBUTION_PACKAGE)" || { echo 'DISTRIBUTION_PACKAGE is required' >&2; exit 1; }
	python3 tools/distribution_legal.py verify '$(DISTRIBUTION_PACKAGE)' --first-party-license LICENSE --first-party-terms MIT --policy tools/distribution_policy.json

test-distribution-legal:
	python3 -B tests/test_distribution_legal.py

# GNU installation directories. This stages a software candidate, not a
# legal/release qualification; its UNQUALIFIED receipt travels with the files.
prefix ?= /usr/local
exec_prefix ?= $(prefix)
bindir ?= $(exec_prefix)/bin
datarootdir ?= $(prefix)/share
datadir ?= $(datarootdir)
DESTDIR ?=
INSTALL ?= install
INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA ?= $(INSTALL) -m 644

.PHONY: install
install: package
	@set -eu; \
	case '$(prefix):$(bindir):$(datadir)' in /*:/*:/*) ;; \
		*) echo 'install: prefix, bindir and datadir must be absolute' >&2; exit 1;; esac; \
	case '$(DESTDIR)' in ''|/*) ;; \
		*) echo 'install: DESTDIR must be absolute or empty' >&2; exit 1;; esac; \
	$(INSTALL) -d '$(DESTDIR)$(bindir)' '$(DESTDIR)$(datadir)/yvex' \
		'$(DESTDIR)$(datadir)/licenses/replai'; \
	while IFS="$$(printf '\t')" read -r path profile owner tracked; do \
		case "/$$path/" in */../*|*/./*|*//*) \
			echo "install: non-canonical manifest path: $$path" >&2; exit 1;; esac; \
		case "$$path" in path) continue;; bin/*) destination='$(DESTDIR)$(bindir)'/$${path#bin/};; \
			share/*) destination='$(DESTDIR)$(datadir)'/$${path#share/};; \
			*) echo "install: unsupported manifest path: $$path" >&2; exit 1;; esac; \
		case "$$path" in bin/*) $(INSTALL_PROGRAM) '$(BUILD_DIR)/package/product/'"$$path" "$$destination";; \
			*) $(INSTALL_DATA) '$(BUILD_DIR)/package/product/'"$$path" "$$destination";; esac; \
	done <config/package_manifest.tsv
