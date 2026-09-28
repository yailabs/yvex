<!-- docs:metadata
title: Distribution legal material
id: yvex.releases.distribution-legal
document: evaluation
status: current
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Distribution legal material

`tools/distribution_legal.py` owns the `distribution.legal.v1` packaging record.
It is an artifact-bound review and enforcement mechanism, not a license oracle
or proof that Cargo metadata equals linked contents. The three repositories
retain independent first-party and release authorities; matching copies of this
MIT packaging tool need no sibling checkout at qualification time.

## Authority and workflow

1. Produce a candidate using the actual package/build recipe. Preserve
   linker/archive membership, generated-input and asset evidence. A lockfile
   alone cannot establish which bytes are delivered.
2. On the clean source snapshot, `capture` inventories the actual staged
   payload, build-input hashes, source commit/tree and specified target-filtered
   Cargo graphs. Pass every concrete Cargo root and one explicit target.
   Normal/build/dev edges and procedural-macro targets remain distinguished;
   none is automatically labelled shipped or excluded. Source-only archives do
   not distribute their merely referenced Cargo dependencies.
3. Review that exact closure. Account for every file, Cargo node, native/static
   and toolchain contribution, generated asset and external source. Non-shipped
   dependencies need a disposition and evidence. Assess generated output
   separately from its generator. Unknown membership or terms stay unresolved.
4. `bundle` refuses unresolved, stale or incomplete reviews; copies original
   notices, full license texts and corresponding-source archives into
   `LEGAL/materials`; writes deterministic records and recipient instructions;
   verifies the copy before atomically publishing a new directory.
5. `verify` rejects missing/extra/altered payload or legal members, source or
   notice loss, unsupported terms, malformed source archives, stale graph
   dispositions, unsafe paths and symlinks. Repository qualification also
   compares first-party terms and license bytes against repository authority.

Use `python3 tools/distribution_legal.py --help` and each subcommand's `--help`.
Payloads, generated records and corresponding source stay outside Git.
Qualification does not itself authorize publication.

`tools/distribution_policy.json` owns required build inputs, Cargo roots and
unresolved conditions per artifact profile. Qualification checks its exact hash
and first-party posture; a caller cannot clear an unresolved profile by supplying
a more optimistic package review. The YVEX product profile is currently blocked.

## Review record

Required fields: `schema`, `closure_sha256`, `reviewer`, a hashed
`build_membership_evidence` material, `unresolved`, `file_components`,
`cargo_dispositions`, `components` and `materials` (relative path to SHA-256).
Components record exact `id`, `version`, `origin`, distributed `form`, upstream
`license_expression`, selected `licenses`, `selection_rationale`, original
`notices` and complete `license_texts` keyed by selected license. Preserve all
applicable AND terms; an OR alternative is a choice, not an additional obligation.
Unsupported/custom terms require policy qualification, never relabeling as MIT.
The first-party component must match the captured canonical license.

MPL `corresponding_source` requires an included archive, immutable upstream
identity, explicit `modified` boolean, `source_form_complete=true` and recipient
instructions. Completeness is a reviewed fact requiring source/build evidence,
not something a JSON boolean proves. Modified covered files must be provided in
their actual Source Code Form; an upstream archive alone is then insufficient.
The checker validates structure and reviewed bytes, not legal completeness.

`registry-source` copies a `.crate` only if its hash matches the exact version's
Cargo.lock checksum, preserving original files/notices. This proves source
availability, not distribution membership or build modification status. The
recipient gets the archive, not just a mutable URL.

## Current unqualified closures

YVEX remains MIT. Its normal package does not distribute model weights or a
CUDA installation. The consumed REPLAI revision's original MIT notice remains
historical and intact; Unicode terms remain in `NOTICE.md`.

The existing REPLAI build receipt identifies libraries but does not retain the
complete Rust/toolchain/native contribution closure. No current YVEX binary
is declared distribution-ready by this mechanism alone. That exact closure and
its complete notice material still require qualification. NVIDIA-generated
material, system-required versus redistributed libraries, model/tokenizer/data
additions and external assets cannot inherit MIT merely by appearing in a
package. Automatic linked/frontend membership discovery is not implemented by
this checker. Those repository-addressable evidence gaps must not be mislabeled
as completed legal readiness or solely external legal review.

Brand/logo provenance and professional legal review remain external; the future
YAI corporation owns no IP by implication.
