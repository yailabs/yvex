/*
 * Owner: apps/operator reusable workbench primitives.
 * Owns: page headers, URL tabs, panels, data rows, metrics, recovery states, resource boundaries, and loading presentation.
 * Does not own: route metadata, server truth, capability calculation, process execution, or page-specific facts.
 * Invariants: tabs are URL-addressable and every server panel preserves distinct loading/error/stale states.
 * Boundary: visual structure does not imply capability readiness.
 */
import { AlertCircle, ArrowRight, LoaderCircle, RefreshCw, type LucideIcon } from "lucide-react";
import { Children, useCallback, useRef, type KeyboardEvent, type ReactNode } from "react";
import { Link, useLocation, useNavigate, useSearchParams } from "react-router-dom";

import type { Availability, Capability } from "../../shared/contracts.ts";
import { capabilityDisplayLabel } from "../capability-labels.ts";
import type { ResourceState } from "../resource.ts";
import { StatusBadge } from "./Status.tsx";

export interface TabDefinition<T extends string = string> {
  id: T;
  label: string;
  count?: number;
}

/** Renders route identity and optional globally truthful mode/status controls. */
export function PageHeader({
  eyebrow,
  title,
  summary,
  actions,
}: {
  eyebrow: string;
  title: string;
  summary: string;
  actions?: ReactNode;
}) {
  return (
    <header className="page-header">
      <div>
        <span className="page-eyebrow">{eyebrow}</span>
        <h1>{title}</h1>
        <p>{summary}</p>
      </div>
      {actions ? <div className="page-actions">{actions}</div> : null}
    </header>
  );
}

/** Provides keyboard-operable URL-persisted tabs with deep-link and history semantics. */
export function useRouteTab<T extends string>(
  tabs: readonly TabDefinition<T>[],
  defaultTab: T,
  parameter = "tab",
): T {
  const [params] = useSearchParams();
  const requested = params.get(parameter);
  return tabs.some((tab) => tab.id === requested) ? (requested as T) : defaultTab;
}

/** Renders the visual/navigation half of URL tabs; callers use useRouteTab for panel selection. */
export function RouteTabs<T extends string>({
  tabs,
  defaultTab,
  parameter = "tab",
  label,
}: {
  tabs: readonly TabDefinition<T>[];
  defaultTab: T;
  parameter?: string;
  label: string;
}) {
  const [params] = useSearchParams();
  const location = useLocation();
  const navigate = useNavigate();
  const anchors = useRef<Array<HTMLAnchorElement | null>>([]);
  const requested = params.get(parameter);
  const selected = tabs.some((tab) => tab.id === requested) ? (requested as T) : defaultTab;

  const hrefFor = useCallback(
    (id: T): string => {
      const next = new URLSearchParams(params);
      next.set(parameter, id);
      return `${location.pathname}?${next.toString()}`;
    },
    [location.pathname, parameter, params],
  );

  const onKeyDown = (event: KeyboardEvent<HTMLDivElement>): void => {
    if (!["ArrowLeft", "ArrowRight", "Home", "End"].includes(event.key)) return;
    event.preventDefault();
    const current = tabs.findIndex((tab) => tab.id === selected);
    const nextIndex =
      event.key === "Home"
        ? 0
        : event.key === "End"
          ? tabs.length - 1
          : (current + (event.key === "ArrowRight" ? 1 : -1) + tabs.length) % tabs.length;
    const next = tabs[nextIndex];
    if (!next) return;
    void navigate(hrefFor(next.id));
    requestAnimationFrame(() => anchors.current[nextIndex]?.focus());
  };

  return (
    <div className="route-tabs" role="tablist" aria-label={label} onKeyDown={onKeyDown}>
      {tabs.map((tab, index) => (
        <Link
          ref={(node) => {
            anchors.current[index] = node;
          }}
          role="tab"
          aria-selected={selected === tab.id}
          tabIndex={selected === tab.id ? 0 : -1}
          className={selected === tab.id ? "active" : ""}
          to={hrefFor(tab.id)}
          key={tab.id}
        >
          {tab.label}
          {tab.count !== undefined ? <span>{tab.count}</span> : null}
        </Link>
      ))}
    </div>
  );
}

/** Renders one low-chrome engineering panel from caller-owned facts and actions. */
export function Panel({
  title,
  description,
  actions,
  children,
  className = "",
}: {
  title: string;
  description?: string;
  actions?: ReactNode;
  children: ReactNode;
  className?: string;
}) {
  return (
    <section className={`panel ${className}`.trim()}>
      <header className="panel-header">
        <div>
          <h2>{title}</h2>
          {description ? <p>{description}</p> : null}
        </div>
        {actions ? <div className="panel-actions">{actions}</div> : null}
      </header>
      <div className="panel-body">{children}</div>
    </section>
  );
}

/** Renders a compact horizontal summary without card nesting or inferred status. */
export function MetricStrip({
  items,
}: {
  items: readonly {
    label: string;
    value: ReactNode;
    detail?: string;
    status?: Availability["status"];
  }[];
}) {
  return (
    <section className="metric-strip" aria-label="Current system summary">
      {items.map((item) => (
        <div className="metric-item" key={item.label}>
          <span>{item.label}</span>
          <strong>{item.value}</strong>
          {item.detail ? <small>{item.detail}</small> : null}
          {item.status ? <StatusBadge status={item.status} /> : null}
        </div>
      ))}
    </section>
  );
}

/** Renders caller-owned definition facts in a dense responsive grid. */
export function FactGrid({ children }: { children: ReactNode }) {
  return <dl className="fact-grid">{children}</dl>;
}

/** Renders one exact definition-list fact and optional monospace value. */
export function Fact({
  label,
  value,
  mono = false,
}: {
  label: string;
  value: ReactNode;
  mono?: boolean;
}) {
  return (
    <div>
      <dt>{label}</dt>
      <dd className={mono ? "mono" : undefined}>{value}</dd>
    </div>
  );
}

/** Renders a purposeful negative state with stable code, recovery, and retry controls. */
export function RecoveryState({
  state,
  retry,
  compact = false,
}: {
  state: Availability;
  retry?: () => void;
  compact?: boolean;
}) {
  return (
    <div className={`recovery-state${compact ? " compact" : ""}`} data-status={state.status}>
      <AlertCircle aria-hidden="true" size={18} />
      <div>
        <div className="recovery-heading">
          <strong>{state.message}</strong>
          <StatusBadge status={state.status} />
        </div>
        {state.detail ? <p>{state.detail}</p> : null}
        <code>{state.reasonCode}</code>
        <div className="recovery-actions">
          {retry ? (
            <button type="button" className="button secondary" onClick={retry}>
              <RefreshCw aria-hidden="true" size={14} /> Retry
            </button>
          ) : null}
          {state.recovery?.href ? (
            <Link className="text-link" to={state.recovery.href}>
              {state.recovery.label} <ArrowRight aria-hidden="true" size={14} />
            </Link>
          ) : null}
        </div>
      </div>
    </div>
  );
}

/** Selects initial-loading, failed, stale, refreshing, and ready presentation around validated data. */
export function ResourceBoundary<T>({
  resource,
  children,
}: {
  resource: ResourceState<T>;
  children: (data: T) => ReactNode;
}) {
  if (resource.initialLoading && !resource.data) return <LoadingPanel />;
  if (resource.error && !resource.data) {
    const now = new Date().toISOString();
    return (
      <RecoveryState
        retry={resource.refresh}
        state={{
          status: "failed",
          reasonCode: resource.error.code,
          message: resource.error.message,
          ...(resource.error.detail ? { detail: resource.error.detail } : {}),
          observedAt: now,
          source: resource.error.requestId ?? "browser-transport",
        }}
      />
    );
  }
  if (!resource.data) return <LoadingPanel />;
  return (
    <>
      {resource.stale ? (
        <div className="stale-banner" role="status">
          <AlertCircle aria-hidden="true" size={15} />
          <span>Showing the last valid response. Refresh failed: {resource.error?.code}</span>
          <button type="button" onClick={resource.refresh}>
            Retry
          </button>
        </div>
      ) : null}
      {resource.refreshing ? (
        <div className="refresh-indicator" role="status">
          <LoaderCircle aria-hidden="true" size={14} /> Refreshing
        </div>
      ) : null}
      {children(resource.data)}
    </>
  );
}

/** Renders a low-motion deterministic initial loading surface. */
export function LoadingPanel() {
  return (
    <div className="loading-panel" aria-busy="true" aria-label="Loading Operator data">
      <LoaderCircle aria-hidden="true" size={18} />
      <span>Loading current Operator state…</span>
    </div>
  );
}

/** Renders a compact list of stable capability facts and exact dependency reasons. */
export function CapabilityRows({ capabilities }: { capabilities: readonly Capability[] }) {
  return (
    <div className="capability-rows">
      {capabilities.map((capability) => (
        <article key={capability.id}>
          <div>
            <strong>{capabilityDisplayLabel(capability)}</strong>
            <p>{capability.reason}</p>
            <code>{capability.id}</code>
          </div>
          <StatusBadge status={capability.status} />
        </article>
      ))}
    </div>
  );
}

/** Returns a stable child count for dense table/list summaries without mutating children. */
export function itemCount(children: ReactNode): number {
  return Children.count(children);
}

export type PanelIcon = LucideIcon;
