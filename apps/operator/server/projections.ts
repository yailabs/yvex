/*
 * Owner: apps/operator engineering-domain projections.
 * Owns: fixed domain-to-producer composition, domain capability filtering, availability aggregation, and typed response shape.
 * Does not own: argv, process execution, capability calculation, route parsing, rendering, or unsupported field fabrication.
 * Invariants: each projection runs only registry-owned producers declared for that domain.
 * Boundary: composing reports does not promote their native evidence stage.
 */
import {
  API_VERSION,
  SCHEMA_VERSION,
  availability,
  type DomainId,
  type DomainProjection,
  type ProducerEnvelope,
} from "../shared/contracts.ts";
import type { OperatorAdapter } from "./adapter.ts";
import type { CapabilityService } from "./capabilities.ts";
import { DOMAIN_PRODUCERS } from "./producers.ts";

/** Aggregates report availability using explicit status precedence and no prose parsing. */
function projectionStatus(
  reports: readonly ProducerEnvelope<unknown>[],
  capabilityStatuses: readonly string[],
): DomainProjection["availability"]["status"] {
  const statuses = [...reports.map((report) => report.availability.status), ...capabilityStatuses];
  if (statuses.includes("failed")) return "failed";
  if (statuses.includes("ready")) return "ready";
  if (statuses.includes("empty")) return "empty";
  if (statuses.includes("degraded")) return "degraded";
  if (statuses.includes("blocked")) return "blocked";
  if (statuses.includes("unsupported")) return "unsupported";
  return "unavailable";
}

/** Builds one domain response from its immutable registry membership and normalized capabilities. */
export async function buildDomainProjection(
  adapter: OperatorAdapter,
  capabilityService: CapabilityService,
  domain: DomainId,
  signal?: AbortSignal,
  clock: () => number = Date.now,
): Promise<DomainProjection> {
  const observedAt = new Date(clock()).toISOString();
  const [entries, manifest] = await Promise.all([
    Promise.all(
      DOMAIN_PRODUCERS[domain].map(async (id) => [id, await adapter.get(id, { signal })] as const),
    ),
    capabilityService.manifest(),
  ]);
  const reports = Object.fromEntries(entries) as Record<string, ProducerEnvelope<unknown>>;
  const domainCapabilities = manifest.capabilities.filter(
    (item) =>
      item.domain ===
      (domain === "models"
        ? "operator"
        : domain === "backends"
          ? "backend"
          : domain === "artifacts"
            ? "artifact"
            : domain === "evidence"
              ? "operator"
              : domain),
  );
  const status = projectionStatus(
    Object.values(reports),
    domainCapabilities.map((item) => item.status),
  );
  return {
    apiVersion: API_VERSION,
    schemaVersion: SCHEMA_VERSION,
    domain,
    observedAt,
    availability: availability(
      status,
      `projection-${status}`,
      `The ${domain} projection is ${status}.`,
      observedAt,
      { source: `${domain}-projection` },
    ),
    reports,
    capabilities: domainCapabilities,
  };
}
