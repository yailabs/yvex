#!/usr/bin/env python3
"""Discriminating package/legal-closure controls; no legal opinion or runtime test."""
import copy
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import tarfile
import unittest

SPEC = importlib.util.spec_from_file_location("legal", Path(__file__).resolve().parents[1] / "tools/distribution_legal.py")
LEGAL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LEGAL)


class LegalPackage(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.payload = self.root / "payload"
        self.payload.mkdir()
        (self.payload / "binary").write_bytes(b"synthetic executable fixture")
        self.materials = self.root / "materials"
        self.materials.mkdir()
        for name in ("license", "source.crate", "build-evidence"):
            (self.materials / name).write_text("synthetic owner fixture: " + name)
        with tarfile.open(self.materials / "source.crate", "w:gz") as archive:
            data = b"synthetic corresponding source"
            info = tarfile.TarInfo("component/src/lib.rs")
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))
        self.closure = {"schema": LEGAL.SCHEMA, "files": LEGAL.inventory(self.payload),
                        "source_commit": "a" * 40, "source_tree": "b" * 40,
                        "build_inputs": {"fixture-recipe": "c" * 64},
                        "first_party": {"id": "component", "terms": "MPL-2.0",
                                        "license_sha256": LEGAL.digest(self.materials / "license")},
                        "cargo": [{"nodes": [{"id": "component@1"}]}]}
        self.review = {
            "schema": LEGAL.SCHEMA, "closure_sha256": LEGAL.identity(self.closure),
            "reviewer": "synthetic test oracle", "build_membership_evidence": "build-evidence",
            "unresolved": [], "file_components": {"binary": ["component"]},
            "cargo_dispositions": {"component@1": {"shipped": True, "component": "component",
                                                     "evidence": "synthetic fixture membership"}},
            "components": [{"id": "component", "version": "1", "origin": "synthetic",
                            "licenses": ["MPL-2.0"], "license_expression": "MPL-2.0",
                            "selection_rationale": "single fixture license", "form": "executable",
                            "notices": ["license"], "license_texts": {"MPL-2.0": "license"},
                            "corresponding_source": {"archive": "source.crate", "modified": False,
                                                     "upstream_identity": "synthetic exact source",
                                                     "source_form_complete": True,
                                                     "recipient_instructions": "Read the included source."}}],
            "materials": {name: LEGAL.digest(self.materials / name)
                          for name in ("license", "source.crate", "build-evidence")}}

    def test_exact_bundle_reproducible(self):
        for directory in ("a", "b"):
            LEGAL.assemble(self.payload, self.closure, self.review, self.materials, self.root / directory)
        self.assertEqual(LEGAL.verify(self.root / "a"), LEGAL.identity(self.closure))
        for path in (self.root / "a").rglob("*"):
            if path.is_file():
                self.assertEqual(path.read_bytes(), (self.root / "b" / path.relative_to(self.root / "a")).read_bytes())

    def test_review_refusals(self):
        changes = [
            lambda r: r.update(schema="wrong"),
            lambda r: r.update(closure_sha256="stale"),
            lambda r: r.update(unresolved=["asset terms unknown"]),
            lambda r: r.pop("unresolved"),
            lambda r: r.update(reviewer=""),
            lambda r: r.update(file_components={}),
            lambda r: r.update(cargo_dispositions={}),
            lambda r: r["cargo_dispositions"]["component@1"].update(shipped=None),
            lambda r: r["cargo_dispositions"]["component@1"].update(evidence=""),
            lambda r: r["components"][0].update(licenses=["Unknown-1.0"]),
            lambda r: r["components"][0].update(licenses=["MIT"]),
            lambda r: r["components"][0].update(notices=[]),
            lambda r: r["components"][0].update(license_texts={}),
            lambda r: r["components"][0].pop("corresponding_source"),
            lambda r: r["components"][0]["corresponding_source"].update(modified=None),
            lambda r: r["components"][0]["corresponding_source"].update(source_form_complete=False),
            lambda r: r["components"][0]["corresponding_source"].update(recipient_instructions=""),
            lambda r: r["materials"].update({"license": "wrong"}),
            lambda r: r["components"].append(copy.deepcopy(r["components"][0])),
        ]
        for index, change in enumerate(changes):
            with self.subTest(index=index):
                review = copy.deepcopy(self.review)
                change(review)
                with self.assertRaises(ValueError):
                    LEGAL.validate(self.closure, review, self.materials)

    def test_payload_drift_atomic(self):
        (self.payload / "new-model.bin").write_bytes(b"unclassified")
        output = self.root / "out"
        with self.assertRaisesRegex(ValueError, "payload drift"):
            LEGAL.assemble(self.payload, self.closure, self.review, self.materials, output)
        self.assertFalse(output.exists())

    def test_postassembly_drift(self):
        for name in ("extra", "binary", "LEGAL/materials/source.crate", "LEGAL/README.txt"):
            with self.subTest(name=name):
                package = self.root / ("out" + str(len(list(self.root.iterdir()))))
                LEGAL.assemble(self.payload, self.closure, self.review, self.materials, package)
                (package / name).write_bytes(b"changed")
                with self.assertRaises(ValueError):
                    LEGAL.verify(package)

    def test_source_and_notice_missing(self):
        for name in ("source.crate", "license"):
            with self.subTest(name=name):
                path = self.materials / name
                original = path.read_bytes()
                path.unlink()
                with self.assertRaises(ValueError):
                    LEGAL.validate(self.closure, self.review, self.materials)
                path.write_bytes(original)

    def test_symlink_and_traversal(self):
        for name in ("../license", "/etc/passwd", "a/../../license", "a\\license"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                LEGAL.member(self.materials, name)
        (self.payload / "alias").symlink_to(self.materials / "license")
        with self.assertRaises(ValueError):
            LEGAL.inventory(self.payload)

    def test_unknown_package_fails_closed(self):
        with self.assertRaises(ValueError):
            LEGAL.verify(self.payload)

    def test_stale_dependency_graph(self):
        self.closure["cargo"][0]["nodes"].append({"id": "new@1"})
        self.review["closure_sha256"] = LEGAL.identity(self.closure)
        with self.assertRaisesRegex(ValueError, "dependency disposition"):
            LEGAL.validate(self.closure, self.review, self.materials)

    def test_locked_source_resolution(self):
        cache = self.root / "cache" / "registry"
        cache.mkdir(parents=True)
        source = cache / "fixture-1.0.0.crate"
        with tarfile.open(source, "w:gz") as archive:
            data = b"[package]\nname='fixture'\nversion='1.0.0'\n"
            info = tarfile.TarInfo("fixture-1.0.0/Cargo.toml")
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))
        lock = self.root / "Cargo.lock"
        lock.write_text('[[package]]\nname="fixture"\nversion="1.0.0"\n'
                        'source="registry+https://example.invalid/index"\n'
                        f'checksum="{LEGAL.digest(source)}"\n')
        output = self.root / "recipient.crate"
        result = LEGAL.collect_registry_source(lock, cache.parent, "fixture", "1.0.0", output)
        self.assertEqual(output.read_bytes(), source.read_bytes())
        self.assertEqual(result["distribution_membership"], "NOT_DETERMINED")
        source.write_bytes(b"corrupt")
        with self.assertRaisesRegex(ValueError, "unavailable"):
            LEGAL.collect_registry_source(lock, cache.parent, "fixture", "1.0.0", self.root / "bad.crate")

    def test_invalid_mpl_archive(self):
        source = self.materials / "source.crate"
        source.write_bytes(b"not source archive")
        self.review["materials"]["source.crate"] = LEGAL.digest(source)
        with self.assertRaises(tarfile.TarError):
            LEGAL.validate(self.closure, self.review, self.materials)

    def test_repository_policy_cannot_be_cleared_by_review(self):
        self.closure.update(profile="binary", policy_sha256="policy", cargo_manifests=["Cargo.toml"])
        policy = {"schema": "distribution.policy.v1", "product": "component", "first_party_terms": "MPL-2.0",
                  "profiles": {"binary": {"unresolved": [], "build_inputs": ["fixture-recipe"],
                                          "cargo_manifests": ["Cargo.toml"]}}}
        LEGAL.check_policy(self.closure, policy, "policy")
        with self.assertRaisesRegex(ValueError, "stale distribution policy"):
            LEGAL.check_policy(self.closure, policy, "different")
        policy["profiles"]["binary"]["unresolved"] = ["missing original notices"]
        with self.assertRaisesRegex(ValueError, "unresolved"):
            LEGAL.check_policy(self.closure, policy, "policy")
        policy["profiles"]["binary"]["unresolved"] = []
        self.closure["cargo_manifests"] = []
        with self.assertRaisesRegex(ValueError, "dependency graph"):
            LEGAL.check_policy(self.closure, policy, "policy")


if __name__ == "__main__":
    unittest.main()
