#!/usr/bin/env node
/*
 * Owner: apps/operator automated adapter tests.
 * Owns: deterministic JSON, refusal, timeout, and output-limit fixtures for audited commands.
 * Does not own: production fallback, YVEX facts, browser configuration, shell execution, or model IO.
 * Invariants: this executable is selected only by automated test configuration.
 * Boundary: fixture success is adapter/UI evidence and never YVEX capability evidence.
 */
import { readFile, writeFile } from "node:fs/promises";

const args = process.argv.slice(2);
const scenario = process.env.YVEX_FAKE_SCENARIO ?? "default";

if (args[0] === "operator-contract" && args[1] === "--output" && args[2] === "json") {
  if (scenario === "incompatible-identity") {
    process.stdout.write(
      `${JSON.stringify({ schemaVersion: "2", protocolVersion: "99", yvexVersion: "9.0.0", product: "yvex" })}\n`,
    );
    process.exit(0);
  }
  process.stdout.write(
    `${JSON.stringify({ schemaVersion: "1", protocolVersion: "1", yvexVersion: "0.1.0", product: "yvex" })}\n`,
  );
  process.exit(0);
}

if (process.env.YVEX_FAKE_COUNT_FILE) {
  let count = 0;
  try {
    count = Number(await readFile(process.env.YVEX_FAKE_COUNT_FILE, "utf8")) || 0;
  } catch {
    // The counter intentionally starts at zero for a new isolated test.
  }
  await writeFile(process.env.YVEX_FAKE_COUNT_FILE, String(count + 1), "utf8");
}

if (scenario === "timeout") {
  await new Promise((resolve) => setTimeout(resolve, 5_000));
}
if (scenario === "oversized") {
  process.stdout.write("x".repeat(2_000_000));
  process.exit(0);
}
if (scenario === "malformed") {
  process.stdout.write('{"status":');
  process.exit(0);
}
if (scenario === "nonzero") {
  process.stderr.write("yvex: typed producer refusal\n");
  process.exit(3);
}

const decision = {
  status:
    scenario === "refusal" ? "source-verification-blocked" : "target-selected-mapping-specified",
  release: "v0.1.0",
  selected_target_id: "deepseek4-v4-flash",
  upstream_repository: "deepseek-ai/DeepSeek-V4-Flash",
  source_verification: scenario === "refusal" ? "blocked" : "complete",
  architecture_ir: "complete",
  tensor_coverage: "complete",
  gguf_mapping: "complete",
  release_qtype: null,
  artifact_status: "not-produced",
  runtime: "unsupported",
  generation: "unsupported",
  evaluation: "not-run",
  benchmark: "not-measured",
  next: "V010.SOURCE.PAYLOAD.STREAM.0",
};

let output;
if (args[0] === "model-target" && args[1] === "list") {
  output = {
    status: "model-target-list",
    targets: [
      {
        target_id: "deepseek4-v4-flash",
        family: "DeepSeek",
        class: "release-source-target",
        release_selected: true,
        runtime: "unsupported",
        generation: "unsupported",
      },
      {
        target_id: "qwen3-8b",
        family: "Qwen",
        class: "source-model-candidate",
        release_selected: false,
        runtime: "unsupported",
        generation: "unsupported",
      },
    ],
  };
} else if (args[0] === "model-target" && args[1] === "decision") {
  output = decision;
} else if (args[0] === "model-target" && args[1] === "inspect") {
  output = {
    status: "model-target",
    target_id: "deepseek4-v4-flash",
    family: "DeepSeek",
    class: "release-source-target",
    release_selected: true,
    upstream_repository: "deepseek-ai/DeepSeek-V4-Flash",
    source_status: "verification-required",
    artifact_status: "not-produced",
    runtime: "unsupported",
    generation: "unsupported",
    next: "V010.SOURCE.PAYLOAD.STREAM.0",
  };
} else if (args[0] === "models" && args[1] === "artifacts" && args[2] === "list") {
  output = {
    status: "artifacts-list",
    artifacts: [
      {
        target_id: "deepseek4-v4-flash-selected-embed",
        family: "deepseek",
        artifact_class: "yvex-selected-gguf",
        artifact_status: "present",
        prepare_status: "ready",
        top_blocker: "none",
        path: "/private/model/proofs/deepseek-selected.gguf",
      },
      {
        target_id: "deepseek4-v4-flash",
        family: "deepseek",
        artifact_class: "planned-full-gguf",
        artifact_status: "missing",
        prepare_status: "blocked",
        top_blocker: "source-payload-trust",
        path: "/private/model/gguf/deepseek4-v4-flash.gguf",
      },
    ],
  };
} else {
  process.stderr.write("yvex: forbidden fake command selection\n");
  process.exit(2);
}

process.stdout.write(`${JSON.stringify(output)}\n`);
if (scenario === "refusal") {
  process.stderr.write("yvex: source verification blocked\n");
  process.exitCode = 5;
}
