/*
 * Owner: apps/operator page projection helpers.
 * Owns: domain-resource loading, typed report access, observation formatting, empty tables, and provenance rows.
 * Does not own: API routes, producer schemas, capability status, page tabs, selection mutation, or process execution.
 * Invariants: unavailable report data remains null and every displayed command is adapter-redacted provenance.
 * Boundary: formatting evidence never creates native capability.
 */
import { FileQuestion, RefreshCw, TerminalSquare } from "lucide-react";
import { useCallback, type ReactNode } from "react";

import type {
  CliProducerId,
  DomainId,
  DomainProjection,
  ProducerDataMap,
  ProducerEnvelope,
} from "../../shared/contracts.ts";
import { operatorApi } from "../api.ts";
import { useApiResource, type ResourceState } from "../resource.ts";
import { StatusBadge } from "../components/Status.tsx";

/** Loads one fixed engineering projection and preserves stale data during refresh. */
export function useDomainProjection(domain: DomainId): ResourceState<DomainProjection> {
  const load = useCallback((signal: AbortSignal) => operatorApi.domain(domain, signal), [domain]);
  return useApiResource(`projection:${domain}`, load);
}

/** Returns one typed producer envelope or null without constructing fallback data. */
export function reportOf<K extends CliProducerId>(
  projection: DomainProjection,
  id: K,
): ProducerEnvelope<ProducerDataMap[K]> | null {
  return (projection.reports[id] as ProducerEnvelope<ProducerDataMap[K]> | undefined) ?? null;
}

/** Renders the exact producer command, exit, cache state, duration, and timestamp. */
export function Provenance({ envelope }: { envelope: ProducerEnvelope<unknown> }) {
  return (
    <div className="provenance-row">
      <TerminalSquare aria-hidden="true" size={14} />
      <code>{envelope.provenance.command}</code>
      <span>
        {envelope.exit.state}
        {envelope.exit.code === null ? "" : ` · ${envelope.exit.code}`}
      </span>
      <span>{envelope.cache.hit ? "cache hit" : envelope.cache.policy}</span>
      <span>{envelope.durationMs === null ? "—" : `${envelope.durationMs} ms`}</span>
      <time dateTime={envelope.observedAt}>
        {new Date(envelope.observedAt).toLocaleTimeString()}
      </time>
    </div>
  );
}

/** Renders one intentional empty collection with optional recovery action. */
export function EmptyState({
  title = "No validated rows",
  detail,
  action,
}: {
  title?: string;
  detail: string;
  action?: ReactNode;
}) {
  return (
    <div className="empty-state">
      <FileQuestion aria-hidden="true" size={21} />
      <div>
        <strong>{title}</strong>
        <p>{detail}</p>
        {action}
      </div>
    </div>
  );
}

/** Renders a table wrapper with a responsive overflow affordance and caller-owned semantics. */
export function DataTable({ label, children }: { label: string; children: ReactNode }) {
  return (
    <div className="table-scroll" role="region" aria-label={label} tabIndex={0}>
      <table>
        <caption className="sr-only">{label}</caption>
        {children}
      </table>
    </div>
  );
}

/** Formats bytes from a typed local probe and refuses negative/non-finite values. */
export function formatBytes(bytes: number): string {
  if (!Number.isFinite(bytes) || bytes < 0) return "Unavailable";
  const units = ["B", "KiB", "MiB", "GiB", "TiB"];
  let value = bytes;
  let unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit += 1;
  }
  return `${value >= 10 || unit === 0 ? value.toFixed(0) : value.toFixed(1)} ${units[unit]}`;
}

/** Renders a compact manual refresh action for one panel-scoped server resource. */
export function RefreshButton({
  refresh,
  refreshing,
}: {
  refresh: () => void;
  refreshing: boolean;
}) {
  return (
    <button type="button" className="button secondary" onClick={refresh} disabled={refreshing}>
      <RefreshCw aria-hidden="true" size={14} />
      {refreshing ? "Refreshing" : "Refresh"}
    </button>
  );
}

/** Renders one exact projection state beside its server observation timestamp. */
export function ProjectionState({ projection }: { projection: DomainProjection }) {
  return (
    <div className="projection-state">
      <StatusBadge status={projection.availability.status} />
      <time dateTime={projection.observedAt}>
        {new Date(projection.observedAt).toLocaleString()}
      </time>
    </div>
  );
}
