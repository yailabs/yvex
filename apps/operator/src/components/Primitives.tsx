/*
 * Owner: apps/operator reusable information layout.
 * Owns: cards, key/value rows, metric strips, page headers, tabs, and loading/connectivity boundaries.
 * Does not own: route data, status truth, navigation, process execution, or command provenance.
 * Invariants: compact primitives preserve semantic landmarks and explicit labels.
 * Boundary: visual hierarchy does not imply capability hierarchy.
 */
import { RefreshCw, WifiOff } from "lucide-react";
import type { ReactNode } from "react";

import { useOperatorView } from "../view-context.tsx";
import { StatusBadge, type StatusTone } from "./Status.tsx";

/** Renders a semantic section from borrowed children and performs no IO, allocation ownership, or mutation. */
export function Card({
  title,
  eyebrow,
  action,
  children,
  className = "",
  id,
}: {
  title: string;
  eyebrow?: string;
  action?: ReactNode;
  children: ReactNode;
  className?: string;
  id?: string;
}) {
  return (
    <section id={id} className={`card ${className}`.trim()}>
      <header className="card-header">
        <div>
          {eyebrow ? <span className="micro-label">{eyebrow}</span> : null}
          <h2>{title}</h2>
        </div>
        {action ? <div className="card-action">{action}</div> : null}
      </header>
      <div className="card-body">{children}</div>
    </section>
  );
}

/** Renders one borrowed definition row and preserves the caller's exact value. */
export function KeyValue({
  label,
  value,
  mono = false,
}: {
  label: string;
  value: ReactNode;
  mono?: boolean;
}) {
  return (
    <div className="key-value">
      <dt>{label}</dt>
      <dd className={mono ? "mono" : undefined}>{value}</dd>
    </div>
  );
}

export interface MetricItem {
  label: string;
  value: string;
  detail?: string;
  tone?: StatusTone;
}

/** Renders fixed summary segments without computing or promoting their evidence states. */
export function MetricStrip({ items }: { items: readonly MetricItem[] }) {
  return (
    <section className="metric-strip" aria-label="Current evidence summary">
      {items.map((item) => (
        <div className="metric-segment" key={item.label}>
          <span className="micro-label">{item.label}</span>
          <div className="metric-value">
            <span>{item.value}</span>
            {item.tone ? (
              <span className={`metric-dot status-${item.tone}`} aria-hidden="true" />
            ) : null}
          </div>
          {item.detail ? <small>{item.detail}</small> : null}
        </div>
      ))}
    </section>
  );
}

/** Renders page identity, boundary state, and anchor tabs with no route or data side effects. */
export function PageHeader({
  eyebrow,
  title,
  summary,
  state,
  tabs,
}: {
  eyebrow: string;
  title: string;
  summary: string;
  state: string;
  tabs?: readonly string[];
}) {
  return (
    <>
      <header className="page-header">
        <div>
          <span className="page-eyebrow">{eyebrow}</span>
          <h1>{title}</h1>
          <p>{summary}</p>
        </div>
        <StatusBadge value={state} />
      </header>
      {tabs?.length ? (
        <nav className="page-tabs" aria-label={`${title} sections`}>
          {tabs.map((tab, index) => (
            <a
              href={`#${tab.toLowerCase().replaceAll(" ", "-")}`}
              className={index === 0 ? "active" : ""}
              key={tab}
            >
              {tab}
            </a>
          ))}
        </nav>
      ) : null}
    </>
  );
}

/** Selects loading/connectivity presentation around borrowed children and only retry mutates view state. */
export function PageContent({ children }: { children: ReactNode }) {
  const { loading, error, retry } = useOperatorView();
  return (
    <>
      {error ? (
        <div className="connectivity-banner" role="alert">
          <WifiOff aria-hidden="true" size={18} />
          <div>
            <strong>Adapter connectivity unavailable</strong>
            <span>{error} No fixture fallback was used.</span>
          </div>
          <button type="button" onClick={retry}>
            <RefreshCw aria-hidden="true" size={15} /> Retry
          </button>
        </div>
      ) : null}
      {loading ? <LoadingGrid /> : children}
    </>
  );
}

/** Renders a deterministic loading placeholder and allocates no evidence facts. */
export function LoadingGrid() {
  return (
    <div className="loading-grid" aria-label="Loading operator evidence" aria-busy="true">
      <div className="loading-strip" />
      <div className="loading-card" />
      <div className="loading-card" />
      <span className="sr-only">Loading current machine-readable evidence</span>
    </div>
  );
}

/** Renders caller-owned boundary prose without changing domain status. */
export function BoundaryNote({ children }: { children: ReactNode }) {
  return (
    <div className="boundary-note">
      <span className="micro-label">Capability boundary</span>
      <p>{children}</p>
    </div>
  );
}
