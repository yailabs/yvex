/*
 * Owner: apps/operator system-health projection.
 * Owns: adapter/host facts, topology graph, bind/auth disclosure, and capability-to-node mapping.
 * Does not own: binary resolution, provider IO, backend discovery, browser rendering, or security mutation.
 * Invariants: host architecture never becomes backend evidence and reference provider remains an independent branch.
 * Boundary: adapter health is connectivity evidence, not native runtime readiness.
 */
import { arch, cpus, platform, totalmem } from "node:os";

import {
  API_VERSION,
  SCHEMA_VERSION,
  availability,
  type CapabilityId,
  type CapabilityManifest,
  type SystemHealth,
  type TopologyNode,
  type TopologyNodeId,
} from "../shared/contracts.ts";
import { ADAPTER_VERSION, type OperatorConfig } from "./config.ts";

const topology: readonly [TopologyNodeId, string, TopologyNode["branch"], CapabilityId][] = [
  ["browser", "Browser client", "operator", "system.browser-reachable"],
  ["adapter-transport", "Adapter transport", "operator", "system.adapter-transport"],
  ["adapter-process", "Adapter process", "operator", "system.adapter-process"],
  ["adapter-api", "Adapter API", "operator", "system.adapter-api-compatible"],
  ["yvex-configured", "YVEX configured", "native", "system.yvex-configured"],
  ["yvex-resolved", "YVEX resolved", "native", "system.yvex-resolved"],
  ["yvex-executable", "YVEX executable", "native", "system.yvex-executable"],
  ["yvex-identity", "YVEX identity", "native", "system.yvex-identity"],
  ["yvex-version", "YVEX compatibility", "native", "system.yvex-version-compatible"],
  ["cpu-backend", "CPU backend", "native", "backend.cpu"],
  ["cuda-backend", "CUDA backend", "native", "backend.cuda"],
  ["target", "Release target", "native", "source.identity"],
  ["artifact", "Admitted artifact", "native", "artifact.admitted"],
  ["runtime", "Runtime binding", "native", "runtime.binding"],
  ["native-generation", "Native generation", "native", "generation.native"],
  ["reference-configured", "Reference configured", "reference", "provider.configured"],
  ["reference-reachable", "Reference reachable", "reference", "provider.reachable"],
];

/** Projects stable capability records into one topology node without changing status semantics. */
function topologyNodes(manifest: CapabilityManifest): TopologyNode[] {
  return topology.map(([id, label, branch, capabilityId]) => {
    const item = manifest.capabilities.find((candidate) => candidate.id === capabilityId);
    return {
      id,
      label,
      branch,
      status: item?.status ?? "unavailable",
      reasonCode: item?.refusalCode ?? `${capabilityId}-ready`,
      message: item?.reason ?? "Capability observation is unavailable.",
      observedAt: item?.lastObservedAt ?? manifest.observedAt,
    };
  });
}

/** Builds the complete browser/adapter/native/reference health topology from a normalized manifest. */
export function buildSystemHealth(
  config: OperatorConfig,
  manifest: CapabilityManifest,
  clock: () => number = Date.now,
): SystemHealth {
  const observedAt = new Date(clock()).toISOString();
  return {
    apiVersion: API_VERSION,
    schemaVersion: SCHEMA_VERSION,
    observedAt,
    availability: availability(
      "ready",
      "adapter-healthy",
      "The Operator adapter process and API are healthy.",
      observedAt,
      { source: "system-health" },
    ),
    adapter: {
      version: ADAPTER_VERSION,
      apiVersion: API_VERSION,
      uptimeSeconds: Math.floor(process.uptime()),
      bindAddress: config.host,
      bindMode: config.bindMode,
      remoteExposure: config.remoteExposure,
      authenticationRequired: config.remoteExposure,
      authenticationConfigured: !config.remoteExposure || config.authToken !== null,
    },
    host: {
      platform: platform(),
      architecture: arch(),
      logicalProcessors: cpus().length,
      totalMemoryBytes: totalmem(),
      nodeRuntime: process.version,
    },
    topology: topologyNodes(manifest),
  };
}
