/*
 * Owner: apps/operator semantic status presentation.
 * Owns: exact status labels, tones, dots, and compact state badges.
 * Does not own: capability calculation, producer execution, retries, layout, or readiness promotion.
 * Invariants: semantic color follows closed status values and raw machine values remain visible.
 * Boundary: presentation cannot convert missing evidence into success.
 */
import type { AvailabilityStatus } from "../../shared/contracts.ts";

export type StatusTone = "ready" | "info" | "warning" | "danger" | "neutral" | "running";

const statusToneMap: Readonly<Record<AvailabilityStatus, StatusTone>> = {
  loading: "running",
  ready: "ready",
  empty: "neutral",
  degraded: "warning",
  blocked: "warning",
  unsupported: "neutral",
  unavailable: "neutral",
  failed: "danger",
  stale: "warning",
};

/** Maps exact availability vocabulary to one restrained visual tone. */
export function availabilityTone(status: AvailabilityStatus): StatusTone {
  return statusToneMap[status];
}

/** Maps exact machine status values for visual contrast only, without changing their text or semantics. */
export function machineTone(value: string | null | undefined): StatusTone {
  if (!value) return "neutral";
  if (["complete", "verified", "present", "ready", "available", "admitted", "pass"].includes(value))
    return "ready";
  if (["selected", "mapping-specified", "source-intake", "report-only"].includes(value))
    return "info";
  if (["blocked", "not-produced", "unselected", "degraded", "stale"].includes(value))
    return "warning";
  if (["failed", "invalid", "corrupt", "refused"].includes(value)) return "danger";
  return "neutral";
}

/** Renders one exact borrowed label with either an explicit or availability-derived tone. */
export function StatusBadge({
  value,
  status,
  tone,
}: {
  value?: string;
  status?: AvailabilityStatus;
  tone?: StatusTone;
}) {
  const label = value ?? status ?? "unavailable";
  const resolvedTone = tone ?? (status ? availabilityTone(status) : machineTone(value));
  return (
    <span className={`status-badge tone-${resolvedTone}`} data-status={status ?? value}>
      <span className="status-dot" aria-hidden="true" />
      {label}
    </span>
  );
}

/** Renders a compact status dot with an accessible borrowed label. */
export function StatusDot({ status, label }: { status: AvailabilityStatus; label: string }) {
  return (
    <span
      className={`semantic-dot tone-${availabilityTone(status)}`}
      aria-label={`${label}: ${status}`}
      title={`${label}: ${status}`}
    />
  );
}
