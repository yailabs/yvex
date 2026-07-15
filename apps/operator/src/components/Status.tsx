/*
 * Owner: apps/operator semantic status presentation.
 * Owns: compact status badges, availability labels, and evidence-empty states.
 * Does not own: status calculation, producer execution, capability promotion, or page layout.
 * Invariants: raw YVEX values remain visible and semantic colors are used consistently.
 * Boundary: color and labels never turn missing evidence into success.
 */
import { AlertTriangle, Ban, CircleDashed, CircleOff, Info } from "lucide-react";

import type { Availability, EvidenceEnvelope } from "../../shared/contracts.ts";

export type StatusTone = "cyan" | "green" | "amber" | "red" | "neutral";

/** Maps exact status vocabulary to visual tone without changing the displayed value. */
export function statusTone(value: string | null | undefined): StatusTone {
  const status = (value ?? "").toLowerCase();
  if (/unsupported|refused|invalid|corrupt/.test(status)) return "red";
  if (/blocked|unavailable|missing|unselected|not-produced/.test(status)) return "amber";
  if (/complete|verified|available|present|pass|admitted/.test(status)) return "green";
  if (/selected|mapping-specified|source-intake/.test(status)) return "cyan";
  return "neutral";
}

/** Purely renders an exact borrowed status string; it performs no state or capability mutation. */
export function StatusBadge({ value, tone }: { value: string; tone?: StatusTone }) {
  return <span className={`status-badge status-${tone ?? statusTone(value)}`}>{value}</span>;
}

/** Converts the closed availability vocabulary to a visible badge without changing its semantics. */
export function AvailabilityBadge({ value }: { value: Availability }) {
  const label: Record<Availability, string> = {
    available: "Available",
    unavailable: "Unavailable",
    blocked: "Blocked",
    unsupported: "Unsupported",
    "not-measured": "Not measured",
  };
  return <StatusBadge value={label[value]} />;
}

const availabilityIcon = {
  available: Info,
  unavailable: CircleOff,
  blocked: AlertTriangle,
  unsupported: Ban,
  "not-measured": CircleDashed,
} as const;

/** Renders an explicit empty/refusal state and performs no IO or fallback data allocation. */
export function EvidenceState({
  availability,
  title,
  detail,
  compact = false,
}: {
  availability: Availability;
  title?: string;
  detail: string;
  compact?: boolean;
}) {
  const Icon = availabilityIcon[availability];
  return (
    <div
      className={`evidence-state${compact ? " evidence-state-compact" : ""}`}
      data-availability={availability}
    >
      <Icon aria-hidden="true" size={compact ? 16 : 19} />
      <div>
        <div className="evidence-state-heading">
          <strong>{title ?? availability.replace("-", " ")}</strong>
          <AvailabilityBadge value={availability} />
        </div>
        <p>{detail}</p>
      </div>
    </div>
  );
}

/** Projects an unavailable envelope issue without parsing or reclassifying its producer failure. */
export function EnvelopeFailure({ envelope }: { envelope: EvidenceEnvelope<unknown> | null }) {
  const availability = envelope?.availability ?? "unavailable";
  return (
    <EvidenceState
      availability={availability}
      title={envelope?.producer.label ?? "Producer unavailable"}
      detail={envelope?.issue?.summary ?? "No validated machine-readable evidence is available."}
    />
  );
}
