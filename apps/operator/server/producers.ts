/*
 * Owner: apps/operator constrained adapter.
 * Owns: immutable audited CLI allowlist, JSON schemas, cache classes, and missing-producer audit.
 * Does not own: process spawning, frontend routes, YVEX source scanning, or domain inference.
 * Invariants: frontend values never contribute arguments; every command is read-only and shell-free.
 * Boundary: allowlisting a report does not raise its YVEX capability stage.
 */
import type { ZodType } from "zod";

import {
  artifactInventorySchema,
  releaseDecisionSchema,
  targetCatalogSchema,
  targetDetailSchema,
  type ArtifactInventory,
  type CachePolicy,
  type CliProducerId,
  type MissingProducerDescriptor,
  type ProducerDescriptor,
  type ReleaseDecision,
  type TargetCatalog,
  type TargetDetail,
  type ViewId,
} from "../shared/contracts.ts";

export interface ProducerRuntimeConfig {
  modelsRoot?: string;
  immutableTimeoutMs: number;
  mutableTimeoutMs: number;
  maxOutputBytes: number;
  mutableTtlMs: number;
}

export interface CliProducerDefinition<T> {
  id: CliProducerId;
  label: string;
  description: string;
  buildArgs: (config: ProducerRuntimeConfig) => string[];
  buildSafeCommand: (config: ProducerRuntimeConfig) => string[];
  schema: ZodType<T>;
  cachePolicy: CachePolicy;
  ttlMs: (config: ProducerRuntimeConfig) => number | null;
  timeoutMs: (config: ProducerRuntimeConfig) => number;
  maxOutputBytes: (config: ProducerRuntimeConfig) => number;
}

/** Quotes a command only for provenance display; it is never passed to a shell. */
export function displayCommand(command: readonly string[]): string {
  return command
    .map((part) => (/^[a-zA-Z0-9_./:=<>-]+$/.test(part) ? part : JSON.stringify(part)))
    .join(" ");
}

function artifactArgs(config: ProducerRuntimeConfig): string[] {
  const args = ["models", "artifacts", "list"];
  if (config.modelsRoot) {
    args.push("--models-root", config.modelsRoot);
  }
  args.push("--output", "json");
  return args;
}

function artifactSafeCommand(config: ProducerRuntimeConfig): string[] {
  const args = ["yvex", "models", "artifacts", "list"];
  if (config.modelsRoot) {
    args.push("--models-root", "<configured-models-root>");
  }
  args.push("--output", "json");
  return args;
}

const definitions = {
  targetCatalog: {
    id: "targetCatalog",
    label: "Target catalog",
    description: "Static target classes and their lowest truthful runtime boundaries.",
    buildArgs: () => ["model-target", "list", "--output", "json"],
    buildSafeCommand: () => ["yvex", "model-target", "list", "--output", "json"],
    schema: targetCatalogSchema,
    cachePolicy: "immutable",
    ttlMs: () => null,
    timeoutMs: (config) => config.immutableTimeoutMs,
    maxOutputBytes: (config) => config.maxOutputBytes,
  } satisfies CliProducerDefinition<TargetCatalog>,
  releaseDecision: {
    id: "releaseDecision",
    label: "v0.1.0 target decision",
    description: "Release target selection and source-to-lowering gate status.",
    buildArgs: () => ["model-target", "decision", "--release", "v0.1.0", "--output", "json"],
    buildSafeCommand: () => [
      "yvex",
      "model-target",
      "decision",
      "--release",
      "v0.1.0",
      "--output",
      "json",
    ],
    schema: releaseDecisionSchema,
    cachePolicy: "immutable",
    ttlMs: () => null,
    timeoutMs: (config) => config.immutableTimeoutMs,
    maxOutputBytes: (config) => config.maxOutputBytes,
  } satisfies CliProducerDefinition<ReleaseDecision>,
  targetDetail: {
    id: "targetDetail",
    label: "Release target detail",
    description: "Catalog identity and explicit artifact/runtime boundary for DeepSeek-V4-Flash.",
    buildArgs: () => ["model-target", "inspect", "deepseek4-v4-flash", "--output", "json"],
    buildSafeCommand: () => [
      "yvex",
      "model-target",
      "inspect",
      "deepseek4-v4-flash",
      "--output",
      "json",
    ],
    schema: targetDetailSchema,
    cachePolicy: "immutable",
    ttlMs: () => null,
    timeoutMs: (config) => config.immutableTimeoutMs,
    maxOutputBytes: (config) => config.maxOutputBytes,
  } satisfies CliProducerDefinition<TargetDetail>,
  artifactInventory: {
    id: "artifactInventory",
    label: "Artifact inventory",
    description: "Known artifact classes, presence state, preparation boundary, and blocker facts.",
    buildArgs: artifactArgs,
    buildSafeCommand: artifactSafeCommand,
    schema: artifactInventorySchema,
    cachePolicy: "short-ttl",
    ttlMs: (config) => config.mutableTtlMs,
    timeoutMs: (config) => config.mutableTimeoutMs,
    maxOutputBytes: (config) => config.maxOutputBytes,
  } satisfies CliProducerDefinition<ArtifactInventory>,
} as const;

export const CLI_PRODUCERS: Readonly<typeof definitions> = Object.freeze(definitions);

export const MISSING_PRODUCERS: readonly MissingProducerDescriptor[] = Object.freeze([
  {
    id: "buildIdentityJson",
    label: "Build identity",
    surface: "System health",
    availability: "unavailable",
    auditedCommand: "yvex info / yvex version",
    reason:
      "Both commands are human-formatted on this baseline and cannot be parsed by the operator.",
  },
  {
    id: "configuredRootsJson",
    label: "Configured model roots",
    surface: "Sources / Settings",
    availability: "unavailable",
    auditedCommand: "yvex paths",
    reason: "The path report has no stable JSON mode; startup configuration remains redacted.",
  },
  {
    id: "sourceManifestSnapshotJson",
    label: "Non-scanning source snapshot",
    surface: "Sources",
    availability: "blocked",
    auditedCommand: "yvex source-manifest report ... --output json",
    reason:
      "The JSON producer performs source metadata/header IO and is excluded by the no-model-tree-scan boundary.",
  },
  {
    id: "transformationIdentityJson",
    label: "Transformation IR identity",
    surface: "Compilation",
    availability: "unavailable",
    auditedCommand: null,
    reason:
      "The safe target decision exposes completion but not a stable non-scanning identity field.",
  },
  {
    id: "tensorAccountingJson",
    label: "Contribution and descriptor accounting",
    surface: "Compilation",
    availability: "blocked",
    auditedCommand: "yvex model-target tensor-map ... --output json",
    reason:
      "The detailed producer re-verifies source headers and is not admitted to this operator wave.",
  },
  {
    id: "qtypePolicyJson",
    label: "Release qtype policy",
    surface: "Quantization",
    availability: "unsupported",
    auditedCommand: "yvex model-target quant-policy ... --output json",
    reason:
      "The baseline CLI explicitly refuses JSON for this report and the release qtype is unselected.",
  },
  {
    id: "qtypeRoleSupportJson",
    label: "Role/qtype support matrix",
    surface: "Quantization",
    availability: "unsupported",
    auditedCommand: "yvex model-target quant-policy ... --role-support --output json",
    reason: "The baseline CLI explicitly returns a JSON-output refusal for role support.",
  },
  {
    id: "referenceDequantJson",
    label: "Reference dequantization evidence",
    surface: "Quantization",
    availability: "unavailable",
    auditedCommand: null,
    reason: "No stable machine-readable producer is present on the pinned baseline.",
  },
  {
    id: "supportedArtifactJson",
    label: "Supported artifact admission",
    surface: "Artifacts",
    availability: "unavailable",
    auditedCommand: null,
    reason:
      "No JSON producer asserts the complete integrity, materialization, runtime, generation, evaluation, benchmark, and release gate chain.",
  },
  {
    id: "backendCapabilityJson",
    label: "Backend and CUDA capability",
    surface: "System health / Runtime",
    availability: "unavailable",
    auditedCommand: "yvex backend cpu|cuda / yvex cuda-info",
    reason: "Available probes are human-formatted and are not parsed by the adapter.",
  },
  {
    id: "validationEvidenceJson",
    label: "Validation evidence catalog",
    surface: "Evidence",
    availability: "unavailable",
    auditedCommand: null,
    reason: "No stable JSON producer aggregates repository validation evidence on this baseline.",
  },
]);

export const VIEW_PRODUCERS: Readonly<Record<ViewId, readonly CliProducerId[]>> = Object.freeze({
  overview: ["releaseDecision", "targetDetail"],
  models: ["targetCatalog", "targetDetail"],
  sources: ["releaseDecision", "targetDetail"],
  compilation: ["releaseDecision", "targetDetail"],
  quantization: ["releaseDecision"],
  artifacts: ["artifactInventory"],
  runtime: ["releaseDecision"],
  evidence: ["targetCatalog", "releaseDecision", "targetDetail", "artifactInventory"],
  "system-health": [],
  settings: [],
});

const viewMissingIds: Readonly<Record<ViewId, readonly string[]>> = Object.freeze({
  overview: ["sourceManifestSnapshotJson", "qtypePolicyJson", "backendCapabilityJson"],
  models: [],
  sources: ["configuredRootsJson", "sourceManifestSnapshotJson"],
  compilation: ["transformationIdentityJson", "tensorAccountingJson"],
  quantization: ["qtypePolicyJson", "qtypeRoleSupportJson", "referenceDequantJson"],
  artifacts: ["supportedArtifactJson"],
  runtime: ["backendCapabilityJson"],
  evidence: MISSING_PRODUCERS.map((producer) => producer.id),
  "system-health": ["buildIdentityJson", "backendCapabilityJson"],
  settings: ["configuredRootsJson", "buildIdentityJson"],
});

/** Returns immutable audit records for the current view without executing any command. */
export function missingProducersForView(view: ViewId): MissingProducerDescriptor[] {
  const ids = new Set(viewMissingIds[view]);
  return MISSING_PRODUCERS.filter((producer) => ids.has(producer.id));
}

/** Builds the redacted descriptor returned to the browser for one audited command. */
export function producerDescriptor(
  definition: CliProducerDefinition<unknown>,
  config: ProducerRuntimeConfig,
): ProducerDescriptor {
  const safeCommand = definition.buildSafeCommand(config);
  return {
    id: definition.id,
    label: definition.label,
    evidenceClass: "cli-json",
    description: definition.description,
    command: safeCommand,
    displayCommand: displayCommand(safeCommand),
    cachePolicy: definition.cachePolicy,
    ttlMs: definition.ttlMs(config),
  };
}
