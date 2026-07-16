/*
 * Owner: apps/operator YVEX producer compatibility registry.
 * Owns: immutable producer IDs, trusted argv templates, schemas, domains, capability dependencies, limits, and cache policy.
 * Does not own: process spawning, binary resolution, browser values, job state, rendering, or native capability promotion.
 * Invariants: every executable vector is registry-owned, shell-free, read-only, and requires stable machine-readable JSON.
 * Boundary: registry admission proves transport compatibility only; producer output retains its lower native evidence stage.
 */
import type { ZodType } from "zod";

import {
  artifactInventorySchema,
  releaseDecisionSchema,
  targetCatalogSchema,
  targetDetailSchema,
  type ArtifactInventory,
  type CapabilityId,
  type CliProducerId,
  type DomainId,
  type ProducerDomain,
  type ReleaseDecision,
  type TargetCatalog,
  type TargetDetail,
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
  displayName: string;
  domain: ProducerDomain;
  description: string;
  buildArgs: (config: ProducerRuntimeConfig) => readonly string[];
  buildSafeCommand: (config: ProducerRuntimeConfig) => readonly string[];
  requiredCapabilities: readonly CapabilityId[];
  schema: ZodType<T>;
  cachePolicy: "immutable" | "short-ttl" | "none";
  ttlMs: (config: ProducerRuntimeConfig) => number | null;
  timeoutMs: (config: ProducerRuntimeConfig) => number;
  maxOutputBytes: (config: ProducerRuntimeConfig) => number;
}

/** Quotes a command only for provenance display; no returned string is passed to a shell. */
export function displayCommand(command: readonly string[]): string {
  return command
    .map((part) => (/^[a-zA-Z0-9_./:=<>-]+$/.test(part) ? part : JSON.stringify(part)))
    .join(" ");
}

/** Builds the only trusted artifact-inventory argv and adds a server-owned optional root literally. */
function artifactArgs(config: ProducerRuntimeConfig): readonly string[] {
  const args = ["models", "artifacts", "list"];
  if (config.modelsRoot) args.push("--models-root", config.modelsRoot);
  args.push("--output", "json");
  return args;
}

/** Builds the redacted diagnostic representation of the trusted artifact command. */
function artifactSafeCommand(config: ProducerRuntimeConfig): readonly string[] {
  const args = ["yvex", "models", "artifacts", "list"];
  if (config.modelsRoot) args.push("--models-root", "<configured-models-root>");
  args.push("--output", "json");
  return args;
}

const binaryDependencies = ["system.yvex-version-compatible"] as const;

const definitions = {
  "target-catalog": {
    id: "target-catalog",
    displayName: "Target catalog",
    domain: "models",
    description: "Typed target classes and their lowest truthful runtime boundaries.",
    buildArgs: () => ["model-target", "list", "--output", "json"],
    buildSafeCommand: () => ["yvex", "model-target", "list", "--output", "json"],
    requiredCapabilities: binaryDependencies,
    schema: targetCatalogSchema,
    cachePolicy: "immutable",
    ttlMs: () => null,
    timeoutMs: (config) => config.immutableTimeoutMs,
    maxOutputBytes: (config) => config.maxOutputBytes,
  } satisfies CliProducerDefinition<TargetCatalog>,
  "release-decision": {
    id: "release-decision",
    displayName: "v0.1.0 target decision",
    domain: "models",
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
    requiredCapabilities: binaryDependencies,
    schema: releaseDecisionSchema,
    cachePolicy: "immutable",
    ttlMs: () => null,
    timeoutMs: (config) => config.immutableTimeoutMs,
    maxOutputBytes: (config) => config.maxOutputBytes,
  } satisfies CliProducerDefinition<ReleaseDecision>,
  "target-detail": {
    id: "target-detail",
    displayName: "Release target detail",
    domain: "models",
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
    requiredCapabilities: binaryDependencies,
    schema: targetDetailSchema,
    cachePolicy: "immutable",
    ttlMs: () => null,
    timeoutMs: (config) => config.immutableTimeoutMs,
    maxOutputBytes: (config) => config.maxOutputBytes,
  } satisfies CliProducerDefinition<TargetDetail>,
  "artifact-inventory": {
    id: "artifact-inventory",
    displayName: "Artifact inventory",
    domain: "artifacts",
    description: "Known artifact classes, presence state, preparation boundary, and blockers.",
    buildArgs: artifactArgs,
    buildSafeCommand: artifactSafeCommand,
    requiredCapabilities: binaryDependencies,
    schema: artifactInventorySchema,
    cachePolicy: "short-ttl",
    ttlMs: (config) => config.mutableTtlMs,
    timeoutMs: (config) => config.mutableTimeoutMs,
    maxOutputBytes: (config) => config.maxOutputBytes,
  } satisfies CliProducerDefinition<ArtifactInventory>,
} as const;

export const CLI_PRODUCERS: Readonly<typeof definitions> = Object.freeze(definitions);

export const DOMAIN_PRODUCERS: Readonly<Record<DomainId, readonly CliProducerId[]>> = Object.freeze(
  {
    models: ["target-catalog", "target-detail", "release-decision"],
    sources: ["release-decision", "target-detail"],
    compilation: ["release-decision", "target-detail"],
    quantization: ["release-decision"],
    artifacts: ["artifact-inventory"],
    backends: [],
    runtime: ["release-decision"],
    evidence: ["target-catalog", "release-decision", "target-detail", "artifact-inventory"],
  },
);

/** Returns one immutable compile-time definition or null for any browser-supplied unknown ID. */
export function producerDefinition(id: string): CliProducerDefinition<unknown> | null {
  return Object.prototype.hasOwnProperty.call(CLI_PRODUCERS, id)
    ? CLI_PRODUCERS[id as CliProducerId]
    : null;
}
