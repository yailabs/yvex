#!/usr/bin/env python3
"""Independent-reader build provenance and safe reuse; no network/model execution."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import platform
import tarfile
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    'reference', Path(__file__).resolve().parents[1] / 'tools/prepare_gguf_reference.py')
REFERENCE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REFERENCE)


class ReaderDependency(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='yvex-reader-build-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'config').mkdir()
        self.pin = {'revision': 'a' * 40, 'tree': 'b' * 40, 'archive_sha256': 'c' * 64}
        self.pin_path = self.root / 'config/gguf_reference.json'
        self.pin_path.write_text(json.dumps(self.pin))
        self.destination = self.root / 'cache/reader'
        self.addCleanup(patch.stopall)
        patch.object(REFERENCE, 'ROOT', self.root).start()
        patch.object(REFERENCE, 'command', side_effect=lambda args: args[0] + '-fixture').start()

    def identity(self):
        return dict(pin=self.pin, system=platform.system(), machine=platform.machine(),
                    cc='cc-fixture', cxx='c++-fixture', cmake='cmake-fixture',
                    script=REFERENCE.digest(Path(REFERENCE.__file__)))

    def installed(self):
        (self.destination / 'lib').mkdir(parents=True)
        (self.destination / 'lib/libggml-base.a').write_bytes(b'fixture library')
        (self.destination / 'LICENSE').write_bytes(b'fixture license')
        receipt = dict(build=self.identity(), sha256=REFERENCE.inventory(self.destination))
        (self.destination / 'receipt.json').write_text(json.dumps(receipt))

    def test_exact_reuse_has_no_network_or_rebuild(self):
        self.installed()
        before = {p.name: p.stat().st_mtime_ns for p in self.destination.rglob('*')}
        with patch.object(REFERENCE.urllib.request, 'urlopen') as network, \
                patch.object(REFERENCE.subprocess, 'run') as build:
            REFERENCE.prepare(self.destination)
            network.assert_not_called()
            build.assert_not_called()
        self.assertEqual(before, {p.name: p.stat().st_mtime_ns for p in self.destination.rglob('*')})

    def test_altered_missing_and_added_files_refuse_without_cleanup(self):
        self.installed()
        library = self.destination / 'lib/libggml-base.a'
        for mode in ('changed', 'missing', 'added'):
            with self.subTest(mode=mode):
                library.write_bytes(b'fixture library')
                extra = self.destination / 'unexpected'
                if extra.exists():
                    extra.unlink()
                if mode == 'changed':
                    library.write_bytes(b'changed library')
                elif mode == 'missing':
                    library.unlink()
                else:
                    extra.write_bytes(b'unrelated cache content')
                before = REFERENCE.inventory(self.destination)
                with self.assertRaisesRegex(RuntimeError, 'stale or altered'):
                    REFERENCE.prepare(self.destination)
                self.assertEqual(before, REFERENCE.inventory(self.destination))

    def test_stale_build_identity_refuses(self):
        self.installed()
        original = (self.destination / 'receipt.json').read_bytes()
        self.pin['revision'] = 'd' * 40
        self.pin_path.write_text(json.dumps(self.pin))
        with self.assertRaisesRegex(RuntimeError, 'stale or altered'):
            REFERENCE.prepare(self.destination)
        self.assertEqual(original, (self.destination / 'receipt.json').read_bytes())

    def test_symlinked_prefix_and_members_refuse(self):
        self.installed()
        alias = self.root / 'alias'
        alias.symlink_to(self.destination, target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, 'symlink'):
            REFERENCE.prepare(alias)
        (self.destination / 'alias').symlink_to(self.destination / 'LICENSE')
        with self.assertRaisesRegex(RuntimeError, 'symlink'):
            REFERENCE.prepare(self.destination)
        self.assertTrue(alias.is_symlink())

    def test_bad_archive_never_publishes_or_removes_other_cache(self):
        self.destination.parent.mkdir()
        unrelated = self.destination.parent / 'keep'
        unrelated.write_bytes(b'keep')
        with patch.object(REFERENCE.urllib.request, 'urlopen', return_value=io.BytesIO(b'bad')):
            with self.assertRaisesRegex(RuntimeError, 'integrity'):
                REFERENCE.prepare(self.destination)
        self.assertFalse(self.destination.exists())
        self.assertEqual(unrelated.read_bytes(), b'keep')
        self.assertEqual(sorted(p.name for p in self.destination.parent.iterdir()), ['keep', 'reader.lock'])

    def archive(self, traversal=False):
        data = io.BytesIO()
        with tarfile.open(fileobj=data, mode='w:gz') as bundle:
            for name in ('LICENSE', 'include/gguf.h', 'include/ggml.h'):
                entry = tarfile.TarInfo('../escape' if traversal else f"ggml-{self.pin['revision']}/{name}")
                payload = b'authenticated fixture'
                entry.size = len(payload)
                bundle.addfile(entry, io.BytesIO(payload))
        self.pin['archive_sha256'] = hashlib.sha256(data.getvalue()).hexdigest()
        self.pin_path.write_text(json.dumps(self.pin))
        data.seek(0)
        return data

    def test_unsafe_archive_refuses_before_build_or_publication(self):
        with patch.object(REFERENCE.urllib.request, 'urlopen', return_value=self.archive(True)), \
                patch.object(REFERENCE.subprocess, 'run') as build:
            with self.assertRaises(tarfile.FilterError):
                REFERENCE.prepare(self.destination)
            build.assert_not_called()
        self.assertFalse(self.destination.exists())
        self.assertFalse((self.destination.parent / 'escape').exists())

    def test_authenticated_build_records_all_material_and_reuses(self):
        def compile_fixture(args, check):
            self.assertTrue(check)
            if args[1] == '--build':
                build = Path(args[2])
                (build / 'src').mkdir(parents=True)
                (build / 'src/libggml-base.a').write_bytes(b'compiled fixture')
            else:
                for flag in ('-DGGML_CUDA=OFF', '-DGGML_METAL=OFF', '-DBUILD_SHARED_LIBS=OFF'):
                    self.assertIn(flag, args)
        with patch.object(REFERENCE.urllib.request, 'urlopen', return_value=self.archive()) as network, \
                patch.object(REFERENCE.subprocess, 'run', side_effect=compile_fixture) as build:
            REFERENCE.prepare(self.destination)
            REFERENCE.prepare(self.destination)
            self.assertEqual(network.call_count, 1)
            self.assertEqual(build.call_count, 2)
        receipt = json.loads((self.destination / 'receipt.json').read_text())
        self.assertEqual(receipt['build'], self.identity())
        self.assertEqual(receipt['sha256'], REFERENCE.inventory(self.destination))
        self.assertEqual(sorted(receipt['sha256']),
                         ['LICENSE', 'include/ggml.h', 'include/gguf.h', 'lib/libggml-base.a'])


if __name__ == '__main__':
    unittest.main()
