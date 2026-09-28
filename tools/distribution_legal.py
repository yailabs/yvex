#!/usr/bin/env python3
"""Freeze a staged artifact closure; assemble and verify recipient legal material.

MIT License

Copyright (c) 2026 Francesco Maiomascio

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
This packaging tool is also maintained as an identical first-party tool in
YAI and YVEX. It is not a runtime or a shared canonical licensing authority.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import tomllib


SCHEMA = "distribution.legal.v1"
# These identifiers describe supported obligation handling, not a blanket
# approval of every work that happens to declare one of them. Compound terms
# must be resolved by the reviewer into all applicable selected licenses.
LICENSES = {"MIT", "Apache-2.0", "BSD-2-Clause", "BSD-3-Clause", "ISC",
            "Unicode-3.0", "Unicode-DFS-2016", "Zlib", "CC0-1.0",
            "MPL-2.0", "CDLA-Permissive-2.0", "OpenSSL",
            "Apache-2.0 WITH LLVM-exception", "LicenseRef-YAI-Evaluation"}


def encoded(value):
    return (json.dumps(value, sort_keys=True, indent=2, ensure_ascii=True) + "\n").encode()


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def identity(value):
    return hashlib.sha256(encoded(value)).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def member(root, relative):
    name = PurePosixPath(relative)
    require(relative and not name.is_absolute() and ".." not in name.parts
            and str(name) == relative and "\\" not in relative,
            f"unsafe member: {relative}")
    path = root / relative
    for ancestor in (path, *path.parents):
        if ancestor == root:
            break
        require(not ancestor.is_symlink(), f"symlink not supported: {relative}")
    require(path.is_file(), f"missing regular file: {relative}")
    return path


def inventory(root):
    require(root.is_dir() and not root.is_symlink(), "payload must be a real directory")
    files = {}
    for path in sorted(root.rglob("*")):
        require(not path.is_symlink(), f"symlink not supported: {path.name}")
        if path.is_dir():
            continue
        require(path.is_file(), f"unsupported member: {path.name}")
        name = path.relative_to(root).as_posix()
        require(name != "LEGAL" and not name.startswith("LEGAL/"), "reserved LEGAL member")
        files[name] = {"sha256": digest(path), "bytes": path.stat().st_size}
    require(files, "empty payload")
    return files


def command(*args, cwd=None):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def cargo_graph(manifest, target):
    """Target-filtered build graph, not an assertion of binary membership.

    Preserve build/dev/proc-macro distinctions. The reviewed build recipe must
    disposition each node; generated output is not automatically excluded.
    """
    data = json.loads(command("cargo", "metadata", "--locked", "--format-version", "1",
                              "--manifest-path", str(manifest), "--filter-platform", target))
    nodes = {node["id"]: node for node in data["resolve"]["nodes"]}
    packages = {item["id"]: item for item in data["packages"]}
    root = data["resolve"]["root"]
    require(root, "use a concrete Cargo package manifest, not a virtual workspace")
    reached, pending = set(), [root]
    while pending:
        current = pending.pop()
        if current in reached:
            continue
        reached.add(current)
        pending.extend(edge["pkg"] for edge in nodes[current]["deps"])
    # Cargo IDs for path packages contain local paths. Replace these with
    # workspace-relative identities; no developer home is recipient metadata.
    workspace = Path(data["workspace_root"])
    names = {}
    for key in reached:
        package = packages[key]
        origin = package["source"]
        if origin is None:
            origin = "workspace:" + Path(package["manifest_path"]).relative_to(workspace).as_posix()
        names[key] = f'{package["name"]}@{package["version"]}#{origin}'
    result = []
    for key in sorted(reached, key=names.get):
        package, node = packages[key], nodes[key]
        result.append({"id": names[key], "license_expression": package["license"],
                       "target_kinds": sorted({kind for t in package["targets"] for kind in t["kind"]}),
                       "features": sorted(node["features"]),
                       "dependencies": sorted(
                           [{"id": names[edge["pkg"]], "kinds": edge["dep_kinds"]}
                            for edge in node["deps"]], key=lambda edge: edge["id"])})
    lock = workspace / "Cargo.lock"
    return {"root": names[root], "target": target, "lock_sha256": digest(lock), "nodes": result}


def collect_registry_source(lockfile, cache, name, version, output):
    """Copy an immutable Cargo source distribution, never substitute a URL.

    This resolves source availability, NOT whether the component is shipped or
    whether a build used modified covered files. Those require build evidence.
    """
    require(not output.exists(), "source output must not exist")
    rows = [p for p in tomllib.loads(lockfile.read_text())["package"]
            if p["name"] == name and p["version"] == version]
    require(len(rows) == 1 and rows[0].get("source", "").startswith("registry+")
            and rows[0].get("checksum"), "no unique locked registry source")
    checksum = rows[0]["checksum"]
    candidates = list(cache.glob(f"*/{name}-{version}.crate"))
    candidates = [path for path in candidates if not path.is_symlink() and digest(path) == checksum]
    require(candidates, "exact corresponding source archive unavailable")
    source = candidates[0]
    prefix = name + "-" + version + "/"
    with tarfile.open(source, "r:gz") as archive:
        members = archive.getmembers()
        names = [item.name for item in members]
        require(len(names) == len(set(names)), "duplicate source member")
        for item in members:
            require(item.name.startswith(prefix) and ".." not in PurePosixPath(item.name).parts
                    and (item.isfile() or item.isdir()), "unsafe source archive")
        require(prefix + "Cargo.toml" in names, "source archive missing manifest")
    shutil.copyfile(source, output)
    require(digest(output) == checksum, "source changed during copy")
    return {"name": name, "version": version, "registry": rows[0]["source"],
            "source_archive_sha256": checksum, "license_notices": "preserved inside original archive",
            "distribution_membership": "NOT_DETERMINED", "build_modification_status": "NOT_DETERMINED"}


def make_closure(repo, payload, inputs, cargo, target, profile):
    require(not command("git", "status", "--porcelain", "--untracked-files=normal", cwd=repo),
            "freeze the owned source first; closure requires a clean repository")
    policy_path = member(repo, "tools/distribution_policy.json")
    policy = json.loads(policy_path.read_text())
    require(profile in policy["profiles"], "unsupported distribution profile")
    recipe = policy["profiles"][profile]
    require(not recipe.get("unresolved"), "profile unresolved: " + "; ".join(recipe.get("unresolved", [])))
    check_recipe_payload(repo, payload, recipe)
    inputs = sorted(set(inputs) | set(recipe["build_inputs"]) | {"tools/distribution_policy.json"})
    cargo = sorted(set(cargo) | set(recipe["cargo_manifests"]))
    require(not cargo or target, "Cargo inventory requires one explicit target")
    require(inputs and set(cargo) <= set(inputs), "record the packaging recipe and every Cargo manifest as build inputs")
    first_party = {"id": policy["product"], "terms": policy["first_party_terms"],
                   "license_sha256": digest(member(repo, policy["first_party_license"]))}
    return {"schema": SCHEMA, "source_commit": command("git", "rev-parse", "HEAD", cwd=repo),
            "source_tree": command("git", "rev-parse", "HEAD^{tree}", cwd=repo),
            "profile": profile, "policy_sha256": digest(policy_path),
            "files": inventory(payload), "first_party": first_party,
            "build_inputs": {name: digest(member(repo, name)) for name in sorted(inputs)},
            "cargo_manifests": cargo,
            "cargo": [cargo_graph(member(repo, name), target) for name in sorted(cargo)]}


def check_recipe_payload(repo, payload, recipe):
    files = inventory(payload)
    if "payload_names" in recipe:
        require(set(files) == set(recipe["payload_names"]), "payload differs from canonical recipe")
    if "reproduce" in recipe:
        require(len(files) == 1, "source reproduction requires one archive")
        name = next(iter(files))
        with tempfile.TemporaryDirectory(prefix="legal-reproduce-") as temporary:
            artifact = Path(temporary) / name
            argv = [str(artifact) if item == "{artifact}" else item for item in recipe["reproduce"]]
            command(*argv, cwd=repo)
            require(digest(artifact) == files[name]["sha256"], "source package differs from canonical recipe")


def check_policy(closure, policy, policy_hash):
    require(policy.get("schema") == "distribution.policy.v1", "unsupported repository policy")
    require(closure.get("policy_sha256") == policy_hash, "stale distribution policy")
    recipe = policy["profiles"][closure["profile"]]
    require(not recipe.get("unresolved"), "distribution profile has unresolved obligations")
    require(closure["first_party"]["id"] == policy["product"]
            and closure["first_party"]["terms"] == policy["first_party_terms"],
            "foreign first-party product posture")
    require(set(recipe["build_inputs"]) <= set(closure["build_inputs"]), "missing required recipe inputs")
    manifests = closure.get("cargo_manifests", [])
    require(set(recipe["cargo_manifests"]) <= set(manifests)
            and len(manifests) == len(closure["cargo"]), "missing required dependency graph")
    if "payload_names" in recipe:
        require(set(closure["files"]) == set(recipe["payload_names"]), "foreign payload for profile")
    return recipe


def validate(closure, review, materials):
    require(closure.get("schema") == SCHEMA and review.get("schema") == SCHEMA, "wrong schema")
    require(all(re.fullmatch(r"[0-9a-f]{40}", closure.get(key, ""))
                for key in ("source_commit", "source_tree")) and closure.get("build_inputs"),
            "missing exact source / build recipe identity")
    require(review.get("closure_sha256") == identity(closure), "stale closure review")
    require(review.get("reviewer") and review.get("build_membership_evidence"),
            "missing accountable review / exact build membership evidence")
    require(not review.get("unresolved", ["missing unresolved classification"]),
            "unresolved distribution conditions")
    files = set(closure["files"])
    require(set(review.get("file_components", {})) == files, "unclassified or stale payload file")
    components = review.get("components", [])
    ids = [item["id"] for item in components]
    require(ids and len(ids) == len(set(ids)), "missing or duplicate component identity")
    by_id = dict(zip(ids, components))
    first = closure.get("first_party", {})
    require(first.get("id") in by_id and first.get("license_sha256") and first.get("terms") in LICENSES,
            "missing canonical first-party posture")
    primary = by_id[first["id"]]
    require(primary.get("licenses") == [first["terms"]], "first-party terms changed")
    primary_text = primary.get("license_texts", {}).get(first["terms"])
    require(review.get("materials", {}).get(primary_text) == first["license_sha256"],
            "first-party license text changed")
    for name, population in review["file_components"].items():
        require(population and len(population) == len(set(population))
                and set(population) <= set(ids), f"invalid file/component population: {name}")
    required = {node["id"] for graph in closure["cargo"] for node in graph["nodes"]}
    dispositions = review.get("cargo_dispositions", {})
    require(set(dispositions) == required, "Cargo dependency disposition missing or stale")
    used = set()
    for dep, disposition in dispositions.items():
        require(disposition.get("evidence"), f"missing distribution evidence: {dep}")
        if disposition.get("shipped") is True:
            require(disposition.get("component") in by_id, f"missing shipped component: {dep}")
            used.add(disposition["component"])
        else:
            require(disposition.get("shipped") is False and disposition.get("reason") in
                    ("build-only", "development-only", "target-excluded", "not-linked", "system-provided"),
                    f"unclassified dependency: {dep}")
    mapped = {item for values in review["file_components"].values() for item in values}
    require(set(ids) == mapped and used <= mapped, "orphan or unrepresented shipped component")
    required_materials = set()
    for component in components:
        require(component.get("version") and component.get("origin"), "component identity incomplete")
        licenses = component.get("licenses", [])
        require(licenses and set(licenses) <= LICENSES, f'unsupported legal terms: {component["id"]}')
        require(component.get("license_expression") and component.get("selection_rationale"),
                "missing upstream expression / selected-term rationale")
        require(component.get("form") in ("source", "executable", "data"), "missing distributed form")
        notices = component.get("notices", [])
        texts = component.get("license_texts", {})
        require(notices and set(texts) == set(licenses), "missing original notices or full license text")
        required_materials.update(notices)
        required_materials.update(texts.values())
        if "MPL-2.0" in licenses:
            source = component.get("corresponding_source", {})
            require(type(source.get("modified")) is bool and source.get("upstream_identity")
                    and source.get("source_form_complete") is True and source.get("archive"),
                    "MPL corresponding Source Code Form unresolved")
            require(source.get("recipient_instructions"), "missing MPL recipient source instructions")
            required_materials.add(source["archive"])
    required_materials.add(review["build_membership_evidence"])
    material_records = review.get("materials", {})
    require(set(material_records) == required_materials, "missing or unreferenced legal material")
    for name, expected in material_records.items():
        path = member(materials, name)
        require(path.stat().st_size > 0 and digest(path) == expected,
                f"missing, empty or altered legal material: {name}")
    for component in components:
        if "MPL-2.0" in component["licenses"]:
            with tarfile.open(member(materials, component["corresponding_source"]["archive"]), "r:*") as bundle:
                entries = bundle.getmembers()
                names = [entry.name for entry in entries]
                require(entries and len(names) == len(set(names)), "empty or duplicate MPL source archive")
                for entry in entries:
                    path = PurePosixPath(entry.name)
                    require(not path.is_absolute() and ".." not in path.parts
                            and (entry.isfile() or entry.isdir()), "unsafe MPL source archive")


def recipient_text(review):
    lines = ["Distribution legal material", "", "First-party and component terms remain separate.",
             "See review.json for exact identities, selected terms and their rationale.", ""]
    for component in review["components"]:
        lines += [f'{component["id"]} {component["version"]}: {", ".join(component["licenses"])}']
        if source := component.get("corresponding_source"):
            lines += ["Corresponding source (included): materials/" + source["archive"],
                      source["recipient_instructions"]]
    return "\n".join(lines) + "\n"


def assemble(payload, closure, review, materials, output):
    require(inventory(payload) == closure["files"], "payload drift since closure capture")
    validate(closure, review, materials)
    require(not output.exists() and not output.is_symlink(), "output must not exist")
    require(not output.resolve().is_relative_to(payload.resolve()), "output must be outside payload")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".legal-", dir=output.parent) as temp:
        staged = Path(temp) / "package"
        shutil.copytree(payload, staged)
        legal = staged / "LEGAL"
        legal.mkdir()
        (legal / "closure.json").write_bytes(encoded(closure))
        (legal / "review.json").write_bytes(encoded(review))
        for name in review["materials"]:
            destination = legal / "materials" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(member(materials, name), destination)
        (legal / "README.txt").write_text(recipient_text(review))
        verify(staged)
        staged.rename(output)


def verify(package):
    legal = package / "LEGAL"
    closure = json.loads(member(package, "LEGAL/closure.json").read_text())
    review = json.loads(member(package, "LEGAL/review.json").read_text())
    validate(closure, review, legal / "materials")
    require(member(package, "LEGAL/README.txt").read_text() == recipient_text(review),
            "recipient instructions changed")
    expected = set(closure["files"]) | {"LEGAL/closure.json", "LEGAL/review.json", "LEGAL/README.txt"}
    expected |= {"LEGAL/materials/" + name for name in review["materials"]}
    actual = set()
    for path in package.rglob("*"):
        require(not path.is_symlink(), "symlink in distribution")
        if not path.is_dir():
            require(path.is_file(), "non-regular distribution member")
            actual.add(path.relative_to(package).as_posix())
    require(actual == expected, "unclassified / missing package members")
    for name, record in closure["files"].items():
        path = member(package, name)
        require(path.stat().st_size == record["bytes"] and digest(path) == record["sha256"],
                f"payload identity mismatch: {name}")
    return identity(closure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    capture = sub.add_parser("capture", help="capture actual payload + graph; this does NOT approve distribution")
    capture.add_argument("--repo", type=Path, default=Path.cwd())
    capture.add_argument("--payload", type=Path, required=True)
    capture.add_argument("--build-input", action="append", default=[])
    capture.add_argument("--cargo-manifest", action="append", default=[])
    capture.add_argument("--target")
    capture.add_argument("--profile", required=True)
    capture.add_argument("--output", type=Path, required=True)
    bundle = sub.add_parser("bundle", help="publish a new local staged package only after exact review")
    for name in ("payload", "closure", "review", "materials", "output"):
        bundle.add_argument("--" + name, type=Path, required=True)
    check = sub.add_parser("verify")
    check.add_argument("package", type=Path)
    check.add_argument("--first-party-license", type=Path)
    check.add_argument("--first-party-terms", choices=sorted(LICENSES))
    check.add_argument("--policy", type=Path)
    source = sub.add_parser("registry-source", help="resolve exact original Cargo source; no membership claim")
    source.add_argument("--lockfile", type=Path, required=True)
    source.add_argument("--cache", type=Path, required=True)
    source.add_argument("--name", required=True)
    source.add_argument("--version", required=True)
    source.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.action == "capture":
            require(not args.output.exists(), "closure output must not exist")
            closure = make_closure(args.repo.resolve(), args.payload, args.build_input,
                                   args.cargo_manifest, args.target, args.profile)
            args.output.write_bytes(encoded(closure))
            print("CAPTURED (not approved):", identity(closure))
        elif args.action == "bundle":
            assemble(args.payload, json.loads(args.closure.read_text()), json.loads(args.review.read_text()),
                     args.materials, args.output)
            print("PASS: exact reviewed package assembled; external legal review is separate")
        elif args.action == "registry-source":
            print(encoded(collect_registry_source(args.lockfile, args.cache, args.name,
                                                 args.version, args.output)).decode(), end="")
        else:
            if args.policy:
                closed = json.loads(member(args.package, "LEGAL/closure.json").read_text())
                policy = json.loads(args.policy.read_text())
                check_policy(closed, policy, digest(args.policy))
            if args.first_party_license or args.first_party_terms:
                require(args.first_party_license and args.first_party_terms, "provide both first-party checks")
                closed = json.loads(member(args.package, "LEGAL/closure.json").read_text())
                require(closed["first_party"]["license_sha256"] == digest(args.first_party_license)
                        and closed["first_party"]["terms"] == args.first_party_terms,
                        "package differs from repository first-party authority")
            print("PASS:", verify(args.package))
    except (OSError, ValueError, KeyError, TypeError, tarfile.TarError, subprocess.CalledProcessError) as error:
        print(f"BLOCKED: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
