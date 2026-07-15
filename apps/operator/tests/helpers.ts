/*
 * Owner: apps/operator automated tests.
 * Owns: test-only adapter configuration and typed browser response factories.
 * Does not own: production fallback, runtime configuration, source facts, or model IO.
 * Invariants: every fixture is explicitly selected by a test and remains below tests/.
 * Boundary: fixture data validates operator behavior only.
 */
import { resolve } from "node:path";

import type {
  AdapterHealth,
  ArtifactInventory,
  EvidenceEnvelope,
  HostProbe,
  MissingProducerDescriptor,
  OperatorViewResponse,
  ProducerDataMap,
  ProducerId,
  ReleaseDecision,
  ReportMap,
  TargetCatalog,
  TargetDetail,
  ViewId,
} from "../shared/contracts.ts";
import type { OperatorConfig } from "../server/config.ts";

export const fakeYvexPath = resolve(process.cwd(), "tests/fixtures/fake-yvex.mjs");

export function testConfig(
  scenario = "default",
  overrides: Partial<OperatorConfig> = {},
): OperatorConfig {
  return {
    host: "127.0.0.1",
    port: 4317,
    binaryRequest: fakeYvexPath,
    binarySearchPath: process.env.PATH ?? "",
    modelsRoot: undefined,
    immutableTimeoutMs: 500,
    mutableTimeoutMs: 500,
    maxOutputBytes: 32_768,
    mutableTtlMs: 250,
    binaryLookupTtlMs: 250,
    childEnvironment: {
      PATH: process.env.PATH,
      LC_ALL: "C",
      YVEX_FAKE_SCENARIO: scenario,
    },
    ...overrides,
  };
}

export const decisionFixture: ReleaseDecision = {
  status: "target-selected-mapping-specified",
  release: "v0.1.0",
  selected_target_id: "deepseek4-v4-flash",
  upstream_repository: "deepseek-ai/DeepSeek-V4-Flash",
  source_verification: "complete",
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

export const detailFixture: TargetDetail = {
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

export const catalogFixture: TargetCatalog = {
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
  ],
};

export const artifactsFixture: ArtifactInventory = {
  status: "artifacts-list",
  artifacts: [
    {
      target_id: "deepseek4-v4-flash-selected-embed",
      family: "deepseek",
      artifact_class: "yvex-selected-gguf",
      artifact_status: "present",
      prepare_status: "ready",
      top_blocker: "none",
      path: "[local]/deepseek-selected.gguf",
    },
  ],
};

export const healthFixture: AdapterHealth = {
  adapterVersion: "0.1.0",
  apiVersion: "v1",
  state: "available",
  bindAddress: "127.0.0.1",
  binaryLabel: "yvex",
  binaryResolution: "configured-path",
  binaryExecutable: true,
  uptimeSeconds: 12,
};

export const hostFixture: HostProbe = {
  platform: "linux",
  architecture: "arm64",
  logicalProcessors: 8,
  totalMemoryBytes: 17_179_869_184,
  nodeRuntime: "v24.18.0",
};

const producerCommands: Record<ProducerId, string[]> = {
  targetCatalog: ["yvex", "model-target", "list", "--output", "json"],
  releaseDecision: ["yvex", "model-target", "decision", "--release", "v0.1.0", "--output", "json"],
  targetDetail: ["yvex", "model-target", "inspect", "deepseek4-v4-flash", "--output", "json"],
  artifactInventory: ["yvex", "models", "artifacts", "list", "--output", "json"],
  adapterHealth: [],
  hostProbe: [],
};

export function testEnvelope<K extends ProducerId>(
  id: K,
  data: ProducerDataMap[K],
): EvidenceEnvelope<ProducerDataMap[K]> {
  const command = producerCommands[id];
  return {
    availability: "available",
    data,
    producer: {
      id,
      label: id,
      evidenceClass: command.length ? "cli-json" : "local-probe",
      description: `${id} test producer`,
      command,
      displayCommand: command.length ? command.join(" ") : "Local typed probe",
      cachePolicy:
        id === "artifactInventory" || id === "adapterHealth" || id === "hostProbe"
          ? "short-ttl"
          : "immutable",
      ttlMs:
        id === "artifactInventory" || id === "adapterHealth" || id === "hostProbe" ? 5_000 : null,
    },
    observedAt: "2026-07-15T00:00:00.000Z",
    fromCache: false,
    lastExit: {
      code: command.length ? 0 : null,
      state: command.length ? "ok" : "not-run",
      durationMs: command.length ? 3 : null,
    },
  };
}

const allReports: ReportMap = {
  targetCatalog: testEnvelope("targetCatalog", catalogFixture),
  releaseDecision: testEnvelope("releaseDecision", decisionFixture),
  targetDetail: testEnvelope("targetDetail", detailFixture),
  artifactInventory: testEnvelope("artifactInventory", artifactsFixture),
  adapterHealth: testEnvelope("adapterHealth", healthFixture),
  hostProbe: testEnvelope("hostProbe", hostFixture),
};

export const missingFixtures: MissingProducerDescriptor[] = [
  {
    id: "qtypePolicyJson",
    label: "Release qtype policy",
    surface: "Quantization",
    availability: "unsupported",
    auditedCommand: "yvex model-target quant-policy ... --output json",
    reason: "The baseline CLI explicitly refuses JSON for this report.",
  },
  {
    id: "qtypeRoleSupportJson",
    label: "Role/qtype support matrix",
    surface: "Quantization",
    availability: "unsupported",
    auditedCommand: "yvex model-target quant-policy ... --role-support --output json",
    reason: "The baseline CLI explicitly returns a JSON-output refusal.",
  },
  {
    id: "referenceDequantJson",
    label: "Reference dequantization evidence",
    surface: "Quantization",
    availability: "unavailable",
    auditedCommand: null,
    reason: "No stable machine-readable producer is present.",
  },
];

export function viewFixture(view: ViewId): OperatorViewResponse {
  return {
    apiVersion: "v1",
    adapterVersion: "0.1.0",
    view,
    observedAt: "2026-07-15T00:00:00.000Z",
    reports: allReports,
    missingProducers: missingFixtures,
  };
}
