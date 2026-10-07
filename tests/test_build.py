#!/usr/bin/env python3
"""Exercise the real Make rules with disposable C/CUDA/package inputs."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import platform
import sys

ROOT = Path(__file__).resolve().parents[1]


class BuildContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="yvex-make-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / "build"
        self.inputs = []

    def make(self, *args, succeeds=True):
        result = subprocess.run(
            ["make", "--no-print-directory", f"BUILD_DIR={self.build}", *self.inputs, *args],
            cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            timeout=120,
        )
        self.assertEqual(result.returncode == 0, succeeds, result.stdout)
        return result.stdout

    def fixture(self, cuda=False):
        header = self.root / "value.h"
        header.write_text("#define FIXTURE_VALUE 7\n")
        source = self.root / ("probe.cu" if cuda else "probe.c")
        if cuda:
            body = '#include "value.h"\n__global__ void probe(int *p) { *p = FIXTURE_VALUE; }\n'
        else:
            body = ('#include "value.h"\n#include "yvex/backend.h"\n'
                    '#ifndef PACKAGER_FLAG\n#error packager flag lost\n#endif\n'
                    'int probe(void);\nint probe(void) { return FIXTURE_VALUE; }\n')
        source.write_text(body)
        # An isolated source projection drives the same canonical dependency
        # include as production; no hand-included fixture dependency shortcut.
        self.inputs = [f"{'CUDA_CU_SRCS' if cuda else 'CORE_SRCS'}={source}"]
        suffix = ".ptx" if cuda else ".o"
        # The production pattern preserves the entire source-relative identity,
        # including this absolute temporary source, instead of flattening it.
        target = str(self.build / "obj") + "/" + str(source.with_suffix(suffix))
        return header, target

    def test_c_flags_and_incremental_dependencies(self):
        header, target = self.fixture()
        args = ("CPPFLAGS=-DPACKAGER_FLAG=1", target)
        self.assertIn(" -c ", self.make(*args))
        before = Path(target).stat().st_mtime_ns
        self.assertNotIn(" -c ", self.make(*args))
        self.assertEqual(before, Path(target).stat().st_mtime_ns)
        header.write_text("#define FIXTURE_VALUE 8\n")
        self.assertIn(" -c ", self.make(*args))
        self.assertIn(" -c ", self.make("CPPFLAGS=-DPACKAGER_FLAG=2", target))
        self.assertIn(" -c ", self.make("CPPFLAGS=-DPACKAGER_FLAG=2", "CFLAGS=-O0 -std=c11", target))
        self.assertIn(str(header), Path(target).with_suffix(".d").read_text())

    def test_structural_archive_identity_follows_build_layout(self):
        archive = self.root / "alternate/libyvex.a"
        objects = self.root / "alternate/objects"
        output = self.make(f"LIBYVEX={archive}", f"OBJ_DIR={objects}", "-s", "print-archive-layout")
        self.assertEqual(output.splitlines()[-2:], [str(archive), str(objects) + "/"])

    def test_ubsan_recursive_flags_are_argv_not_shell_continuations(self):
        # cc-rs consumes exported CFLAGS without a shell. A continuation inside
        # the quoted Make value becomes a literal backslash argument, unlike
        # the ordinary C recipe. Observe the real recursive invocation here;
        # sanitizer/runtime semantics remain covered by the real sanitizer lane.
        import json
        import shlex
        fake = self.root / "recursive-make-fixture"
        fake.write_text('#!/usr/bin/env python3\n'
                        'import json, pathlib, sys\n'
                        'args = dict(a.split("=", 1) for a in sys.argv[1:] if "=" in a)\n'
                        'print("SANITIZER_FLAGS " + json.dumps(args["CFLAGS"]))\n'
                        'runner = pathlib.Path(args["BUILD_DIR"]) / "tests/test"\n'
                        'runner.parent.mkdir(parents=True)\n'
                        'runner.write_text("#!/bin/sh\\nexit 0\\n")\n'
                        'runner.chmod(0o755)\n')
        fake.chmod(0o755)
        for target in ("test-runtime-ubsan", "test-quant-ubsan"):
            with self.subTest(target=target):
                output = self.make(f"MAKE={fake}", target)
                flags = json.loads(next(line[len("SANITIZER_FLAGS "):] for line in output.splitlines()
                                        if line.startswith("SANITIZER_FLAGS ")))
                self.assertNotIn("\\", flags)
                self.assertNotIn("\n", flags)
                self.assertIn("-fsanitize=undefined", shlex.split(flags))
                self.assertIn("-fno-sanitize-recover=undefined", shlex.split(flags))

    def test_platform_thread_link_defaults_preserve_overrides(self):
        projection = "--eval=print-link-defaults:;@printf '%s\\n' '$(LDLIBS)'"
        darwin = self.make("YVEX_HOST_OS=Darwin", "YVEX_HOST_ARCH=x86_64",
                           projection, "print-link-defaults")
        linux = self.make("YVEX_HOST_OS=Linux", projection, "print-link-defaults")
        self.assertEqual(darwin.splitlines()[-1], "-ldl -lm -lz")
        self.assertEqual(linux.splitlines()[-1], "-ldl -pthread -lm -lz")
        override = self.make("YVEX_HOST_OS=Darwin", "YVEX_HOST_ARCH=x86_64", "LDLIBS=-lconsumer",
                             projection, "print-link-defaults")
        self.assertEqual(override.splitlines()[-1], "-lconsumer")
        metal = self.make("YVEX_HOST_OS=Darwin", "YVEX_HOST_ARCH=arm64",
                          projection, "print-link-defaults")
        self.assertEqual(metal.splitlines()[-1],
                         "-ldl -lm -lz -framework Foundation -framework Metal")
        metal_override = self.make("YVEX_HOST_OS=Darwin", "YVEX_HOST_ARCH=arm64",
                                   "LDLIBS=-lconsumer", projection, "print-link-defaults")
        self.assertEqual(metal_override.splitlines()[-1],
                         "-lconsumer -framework Foundation -framework Metal")

    def test_database_inspection_does_not_build_the_default_product(self):
        output = self.make("-pn", "print-build-inputs", "CARGO=false", "RUSTC=false",
                           "YVEX_CUDA_ARCH=auto", "CUDA_AUTO_ARCH=sm_121")
        self.assertIn("CUDA_EFFECTIVE_ARCH := sm_121", output)
        self.assertFalse((self.build / "lib/libyvex.a").exists())
        self.assertFalse((self.build / "cargo").exists())

    def test_library_remains_independent_of_rust_and_terminal_source(self):
        _, target = self.fixture()
        archive = self.build / "lib/libyvex.a"
        self.make("CPPFLAGS=-DPACKAGER_FLAG=1", "CARGO=false", "RUSTC=false",
                  "OPENAI_ADAPTER_SRCS=", "CUDA_SRCS=", "NVCC=__unavailable__", "lib")
        self.assertTrue(archive.is_file())
        self.assertTrue(Path(target).is_file())
        self.assertFalse((self.build / "cargo").exists())

    def test_rust_configuration_is_material_and_content_stable(self):
        stamp = self.build / "generated/rust_build_config"
        # Explicit baselines also work when this test inherits MAKEFLAGS from
        # a dev-profile parent qualification invocation.
        self.make("RUST_PROFILE=release", str(stamp))
        before = stamp.stat().st_mtime_ns
        self.make("RUST_PROFILE=release", str(stamp))
        self.assertEqual(before, stamp.stat().st_mtime_ns)
        self.make("RUST_PROFILE=release", "RUSTFLAGS=-C debuginfo=0", str(stamp))
        self.assertNotEqual(before, stamp.stat().st_mtime_ns)
        self.assertIn("rustflags=-C debuginfo=0", stamp.read_text())
        identity = stamp.read_text().splitlines()[0]
        self.make("RUSTFLAGS=-C debuginfo=0", "RUST_PROFILE=dev", str(stamp))
        self.assertNotEqual(identity, stamp.read_text().splitlines()[0])

    def test_rust_product_publication_keeps_previous_on_cargo_failure(self):
        product = self.root / "bin/yvex"
        product.parent.mkdir()
        product.write_bytes(b"previous complete product")
        # Only the Make/Cargo publication contract is mocked here. Production
        # compilation, FFI and CLI semantics are qualified by the real Rust lane.
        fake = self.root / "cargo-fixture"
        fake.write_text('#!/bin/sh\nset -eu\n'
                        'if test "$1" = --version; then echo fixture-cargo; exit 0; fi\n'
                        'test "$1" = build\n'
                        'mkdir -p "$CARGO_TARGET_DIR/debug"\n'
                        'printf "qualified Make publication fixture\\n" >"$CARGO_TARGET_DIR/debug/yvex"\n')
        fake.chmod(0o755)
        bypass = ("-o", "lib", "-o", "generate-operator-registry", "-o", "replai-rust-dependency",
                  "-o", str(self.build / "generated/build_commit.h"))
        self.make(*bypass, f"YVEX_BIN={product}", f"CARGO={fake}", "RUST_PROFILE=dev", "client")
        self.assertEqual(product.read_text(), "qualified Make publication fixture\n")
        self.assertEqual(product.stat().st_mode & 0o777, 0o755)
        before = product.stat().st_mtime_ns
        self.make(*bypass, f"YVEX_BIN={product}", f"CARGO={fake}", "RUST_PROFILE=dev", "client")
        self.assertEqual(before, product.stat().st_mtime_ns)
        self.make(*bypass, f"YVEX_BIN={product}", "CARGO=false", "RUST_PROFILE=dev", "client", succeeds=False)
        self.assertEqual(product.read_text(), "qualified Make publication fixture\n")
        self.assertEqual(list(product.parent.glob("*.tmp.*")), [])

    @unittest.skipUnless(shutil.which("nvcc"), "CUDA compiler not installed")
    def test_cuda_transitive_header_and_flags(self):
        header, target = self.fixture(cuda=True)
        self.assertIn(" -ptx ", self.make(target))
        before = Path(target).stat().st_mtime_ns
        self.assertNotIn(" -ptx ", self.make(target))
        self.assertEqual(before, Path(target).stat().st_mtime_ns)
        header.write_text("#define FIXTURE_VALUE 9\n")
        self.assertIn(" -ptx ", self.make(target))
        self.assertIn(" -ptx ", self.make("NVCCFLAGS=-O0", target))
        self.assertIn(str(header), Path(target + ".d").read_text())

    @unittest.skipUnless(sys.platform == "darwin" and platform.machine() == "arm64",
                         "native Metal Objective-C build requires macOS arm64")
    def test_metal_objective_c_flags_and_dependencies(self):
        header = self.root / "value.h"
        header.write_text("#define FIXTURE_VALUE 7\n")
        source = self.root / "native.m"
        source.write_text('#import <Foundation/Foundation.h>\n#include "value.h"\n'
                          'int probe(void);\nint probe(void) { return FIXTURE_VALUE + (int)[@"a" length]; }\n')
        self.inputs = [f"METAL_SRCS={source}"]
        target = str(self.build / "obj") + "/" + str(source.with_suffix(".o"))
        self.assertIn("-fobjc-arc", self.make("METAL_CFLAGS=-O0", target))
        self.assertNotIn(" -c ", self.make("METAL_CFLAGS=-O0", target))
        header.write_text("#define FIXTURE_VALUE 8\n")
        self.assertIn(" -c ", self.make("METAL_CFLAGS=-O0", target))
        self.assertIn(" -c ", self.make("METAL_CFLAGS=-O1", target))
        self.assertIn(str(header), Path(target).with_suffix(".d").read_text())

    def test_linux_rules_have_no_metal_toolchain(self):
        output = self.make("YVEX_HOST_OS=Linux", "YVEX_HOST_ARCH=aarch64",
                           "NVCC=yvex-no-nvcc", "-n", "lib")
        self.assertNotIn("src/backend/metal/native.o", output)
        self.assertNotIn("src/backend/metal/native.m", output)
        self.assertNotIn("-framework", output)

    def test_install_manifest_and_destdir(self):
        paths = [line.split("\t")[0] for line in
                 (ROOT / "config/package_manifest.tsv").read_text().splitlines()[1:]]
        package = self.build / "package/product"
        for name in paths:
            path = package / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("UNQUALIFIED fixture " + name)
        stage = self.root / "stage"
        self.make("-o", "package", "install", f"DESTDIR={stage}",
                  "prefix=/opt/yvex", "bindir=/opt/bin", "datadir=/usr/share")
        expected = set()
        for name in paths:
            destination = ("opt/bin/" + name[4:]) if name.startswith("bin/") else ("usr/" + name)
            expected.add(destination)
            self.assertEqual((stage / destination).read_bytes(), (package / name).read_bytes())
        self.assertEqual({str(p.relative_to(stage)) for p in stage.rglob("*") if p.is_file()}, expected)
        self.assertEqual((stage / "opt/bin/yvex").stat().st_mode & 0o777, 0o755)
        self.assertEqual((stage / "usr/share/yvex/LICENSE").stat().st_mode & 0o777, 0o644)
        self.make("-o", "package", "install", "prefix=relative", succeeds=False)
        self.make("-o", "package", "install", "DESTDIR=relative", succeeds=False)

    def test_clean_preserves_external_files(self):
        self.build.mkdir()
        (self.build / "owned").write_text("owned")
        foreign = self.root / "foreign.o"
        foreign.write_text("foreign")
        self.make("clean")
        self.assertFalse(self.build.exists())
        self.assertEqual(foreign.read_text(), "foreign")
        self.make("BUILD_DIR=/", "clean", succeeds=False)
        self.build.mkdir()
        self.make(f"BUILD_DIR={self.build}/../build", "clean", succeeds=False)
        self.assertTrue(self.build.is_dir())
        rules = (ROOT / "config/make/rules.mk").read_text()
        self.assertNotIn("./*.o", rules)

    def test_package_refuses_unsafe_roots_before_deletion(self):
        foreign = self.root / "foreign"
        package = foreign / "package"
        package.mkdir(parents=True)
        marker = package / "keep"
        marker.write_text("foreign")
        self.build.symlink_to(foreign, target_is_directory=True)
        # Skip compilation only; exercise the real package deletion guard.
        self.make("-o", "client", "package", succeeds=False)
        self.assertEqual(marker.read_text(), "foreign")
        self.make("-o", "client", f"BUILD_DIR={foreign}/../foreign", "package", succeeds=False)
        self.assertEqual(marker.read_text(), "foreign")

    def test_failed_archive_keeps_previous_complete_product(self):
        _, target = self.fixture()
        self.make("CPPFLAGS=-DPACKAGER_FLAG=1", target)
        archive = self.build / "lib/probe.a"
        archive.parent.mkdir()
        archive.write_bytes(b"previous complete fixture")
        import os
        os.utime(archive, ns=(1, 1))
        self.make("CPPFLAGS=-DPACKAGER_FLAG=1", f"LIBYVEX={archive}",
                  "AR=false", str(archive), succeeds=False)
        self.assertEqual(archive.read_bytes(), b"previous complete fixture")
        self.assertEqual(list(archive.parent.glob("*.tmp.*")), [])

    def test_link_flags_are_material_incremental_inputs(self):
        stamp = self.build / "generated/link_build_config"
        self.make(str(stamp))
        before = stamp.stat().st_mtime_ns
        self.make(str(stamp))
        self.assertEqual(before, stamp.stat().st_mtime_ns)
        self.make("LDFLAGS=-Wl,--as-needed", str(stamp))
        self.assertIn("ldflags=-Wl,--as-needed", stamp.read_text())
        self.assertNotEqual(before, stamp.stat().st_mtime_ns)
        self.make("LDLIBS=-lm", str(stamp))
        self.assertIn("ldlibs=-lm", stamp.read_text())


if __name__ == "__main__":
    unittest.main()
