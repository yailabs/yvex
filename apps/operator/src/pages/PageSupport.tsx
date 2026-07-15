/*
 * Owner: apps/operator page evidence helpers.
 * Owns: missing-producer lookup, producer stamps, byte formatting, and compact fact lists.
 * Does not own: producer definitions, status truth, fetching, navigation, or capability inference.
 * Invariants: absent evidence is rendered explicitly and never converted into a default success value.
 * Boundary: formatting local probe values does not create YVEX domain evidence.
 */
import { FileQuestion, TerminalSquare } from "lucide-react";
import type { ReactNode } from "react";

import type { MissingProducerId } from "../../shared/contracts.ts";
import { useOperatorView } from "../view-context.tsx";
import { EvidenceState } from "../components/Status.tsx";

export function MissingEvidence({
  id,
  compact = false,
}: {
  id: MissingProducerId;
  compact?: boolean;
}) {
  const { response } = useOperatorView();
  const record = response?.missingProducers.find((producer) => producer.id === id);
  if (!record) {
    return (
      <EvidenceState
        availability="unavailable"
        title="Producer unavailable"
        detail="No stable machine-readable producer is attached to this view."
        compact={compact}
      />
    );
  }
  return (
    <EvidenceState
      availability={record.availability}
      title={record.label}
      detail={record.reason}
      compact={compact}
    />
  );
}

export function ProducerStamp({ command, exit }: { command: string; exit: number | null }) {
  return (
    <div className="producer-stamp">
      <TerminalSquare aria-hidden="true" size={14} />
      <code>{command}</code>
      <span>exit {exit === null ? "—" : exit}</span>
    </div>
  );
}

export function FactList({ children }: { children: ReactNode }) {
  return <dl className="fact-list">{children}</dl>;
}

export function EmptyCollection({ detail }: { detail: string }) {
  return (
    <div className="empty-collection">
      <FileQuestion aria-hidden="true" size={20} />
      <strong>No validated rows</strong>
      <p>{detail}</p>
    </div>
  );
}

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
