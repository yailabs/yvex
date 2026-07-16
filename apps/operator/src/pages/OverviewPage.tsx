/*
 * Owner: apps/operator operational Overview surface.
 * Owns: current system mode, lane readiness, lifecycle pipeline, active work, recent failures, blockers, and quick actions.
 * Does not own: capability calculation, jobs, producer execution, binary settings, provider transport, or native controls.
 * Invariants: lifecycle stages borrow stable capabilities and no repeated fallback cards fabricate missing facts.
 * Boundary: provider readiness remains independent from native YVEX readiness.
 */
import { ArrowRight, Command, MessageSquare, RefreshCw, Settings2 } from "lucide-react";
import { Link } from "react-router-dom";

import { capabilityById, type CapabilityId } from "../../shared/contracts.ts";
import {
  MetricStrip,
  PageHeader,
  Panel,
  RecoveryState,
  ResourceBoundary,
} from "../components/Primitives.tsx";
import { StatusBadge } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { useOperatorState } from "../state/operator-state.tsx";
import { reportOf, useDomainProjection } from "./PageSupport.tsx";

const lifecycle: readonly { id: CapabilityId; label: string; route: string }[] = [
  { id: "source.trust", label: "Source trust", route: "/sources?tab=trust" },
  {
    id: "compilation.transformation-ir",
    label: "Transformation IR",
    route: "/compilation?tab=pipeline",
  },
  {
    id: "compilation.physical-lowering",
    label: "Physical lowering",
    route: "/compilation?tab=pipeline",
  },
  { id: "quantization.policy", label: "Quantization", route: "/quantization?tab=policy" },
  { id: "artifact.admitted", label: "Artifact admission", route: "/artifacts" },
  { id: "runtime.binding", label: "Runtime binding", route: "/runtime?tab=stages" },
  { id: "generation.native", label: "Native generation", route: "/runtime?tab=controls" },
];

/** Projects global server truth into one compact operational landing page. */
export function OverviewPage() {
  const app = useOperatorState();
  const models = useDomainProjection("models");
  const page = pageMetadata.overview;
  const manifest = app.capabilities.data;
  const binary = capabilityById(manifest, "system.yvex-version-compatible");
  const runtime = capabilityById(manifest, "runtime.binding");
  const provider = capabilityById(manifest, "provider.streaming");
  const activeJobs =
    app.jobs.data?.jobs.filter(
      (job) => !["cancelled", "completed", "failed"].includes(job.state),
    ) ?? [];
  const recentFailures = [
    ...(app.jobs.data?.jobs
      .filter((job) => job.state === "failed")
      .map((job) => ({
        id: job.id,
        label: job.error?.message ?? `${job.type} failed`,
        time: job.endedAt ?? job.createdAt,
      })) ?? []),
    ...(app.events.data?.events
      .filter((event) => event.severity === "error")
      .map((event) => ({ id: event.id, label: event.message, time: event.observedAt })) ?? []),
  ].slice(0, 4);
  const blockers = (manifest?.capabilities ?? [])
    .filter(
      (item) =>
        ["blocked", "failed"].includes(item.status) &&
        ["system", "artifact", "runtime", "generation", "provider"].includes(item.domain),
    )
    .slice(0, 5);

  return (
    <div className="page">
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        actions={
          <button type="button" className="button secondary" onClick={app.refreshAll}>
            <RefreshCw aria-hidden="true" size={14} /> Refresh system
          </button>
        }
      />
      <ResourceBoundary resource={models}>
        {(projection) => {
          const decision = reportOf(projection, "release-decision")?.data;
          return (
            <>
              {!binary || binary.status !== "ready" ? (
                <RecoveryState
                  state={{
                    status: binary?.status ?? "unavailable",
                    reasonCode: binary?.refusalCode ?? "yvex-binary-unresolved",
                    message: binary?.reason ?? "YVEX binary is unresolved.",
                    observedAt: binary?.lastObservedAt ?? projection.observedAt,
                    recovery: {
                      id: "configure-yvex",
                      label: "Configure YVEX",
                      href: "/settings?section=yvex",
                    },
                    source: "capability-manifest",
                  }}
                />
              ) : null}
              <MetricStrip
                items={[
                  {
                    label: "System mode",
                    value:
                      app.health.data?.adapter.bindMode === "remote"
                        ? "Remote secured"
                        : "Local only",
                    detail: app.health.data?.adapter.bindAddress,
                    status: app.health.data?.availability.status ?? "loading",
                  },
                  {
                    label: "YVEX",
                    value: binary?.status ?? "loading",
                    detail: binary?.reason,
                    status: binary?.status ?? "loading",
                  },
                  {
                    label: "Native runtime",
                    value: runtime?.status ?? "loading",
                    detail: decision?.runtime ?? "No decision",
                    status: runtime?.status ?? "loading",
                  },
                  {
                    label: "Reference lane",
                    value: provider?.status ?? "loading",
                    detail: app.provider.data?.displayName ?? "Not configured",
                    status: provider?.status ?? "loading",
                  },
                ]}
              />

              <Panel
                title="Release lifecycle"
                description="Each stage is a separate typed capability; selection or proof does not skip downstream gates."
              >
                <div className="lifecycle-line">
                  {lifecycle.map((stage, index) => {
                    const capability = capabilityById(manifest, stage.id);
                    return (
                      <Link to={stage.route} className="lifecycle-node" key={stage.id}>
                        <span>{String(index + 1).padStart(2, "0")}</span>
                        <strong>{stage.label}</strong>
                        <StatusBadge status={capability?.status ?? "loading"} />
                        <small>{capability?.reason ?? "Loading capability…"}</small>
                        {index < lifecycle.length - 1 ? (
                          <ArrowRight
                            aria-hidden="true"
                            className="lifecycle-connector"
                            size={15}
                          />
                        ) : null}
                      </Link>
                    );
                  })}
                </div>
              </Panel>

              <div className="split-grid overview-grid">
                <Panel
                  title="Current context"
                  description="Durable selections never override server capability truth."
                >
                  <dl className="context-list">
                    <div>
                      <dt>Release target</dt>
                      <dd>{decision?.selected_target_id ?? "Not reported"}</dd>
                    </div>
                    <div>
                      <dt>Selected target</dt>
                      <dd>{app.selectedTarget ?? "Not selected"}</dd>
                    </div>
                    <div>
                      <dt>Selected artifact</dt>
                      <dd>{app.selectedArtifact ?? "Not selected"}</dd>
                    </div>
                    <div>
                      <dt>Selected backend</dt>
                      <dd>{app.selectedBackend ?? "Not selected"}</dd>
                    </div>
                    <div>
                      <dt>Execution lane</dt>
                      <dd>
                        {app.selectedLane === "reference-provider"
                          ? "Reference provider"
                          : "Native YVEX"}
                      </dd>
                    </div>
                  </dl>
                </Panel>
                <Panel
                  title="Actionable blockers"
                  description="Highest-impact control-plane and native dependencies."
                >
                  <div className="blocker-list">
                    {blockers.length ? (
                      blockers.map((blocker) => (
                        <Link
                          to={blocker.recoveryAction?.href ?? "/system-health"}
                          key={blocker.id}
                        >
                          <div>
                            <strong>{blocker.id}</strong>
                            <p>{blocker.reason}</p>
                          </div>
                          <StatusBadge status={blocker.status} />
                        </Link>
                      ))
                    ) : (
                      <p className="quiet-copy">
                        No blocked or failed capability is currently observed.
                      </p>
                    )}
                  </div>
                </Panel>
              </div>

              <div className="split-grid overview-grid">
                <Panel
                  title="Active work"
                  description="Progress remains indeterminate unless an owner reports an exact measure."
                >
                  {activeJobs.length ? (
                    <div className="job-list">
                      {activeJobs.map((job) => (
                        <article key={job.id}>
                          <div>
                            <strong>{job.type}</strong>
                            <span>
                              {job.executionOwner} · {job.phase ?? "starting"}
                            </span>
                          </div>
                          <StatusBadge status="loading" value={job.state} />
                        </article>
                      ))}
                    </div>
                  ) : (
                    <p className="quiet-copy">
                      No active jobs. Recent work remains available in Evidence.
                    </p>
                  )}
                </Panel>
                <Panel
                  title="Recent failures"
                  description="Structured local failures; no raw stack traces."
                >
                  {recentFailures.length ? (
                    <div className="failure-list">
                      {recentFailures.map((failure) => (
                        <article key={failure.id}>
                          <strong>{failure.label}</strong>
                          <time dateTime={failure.time}>
                            {new Date(failure.time).toLocaleString()}
                          </time>
                        </article>
                      ))}
                    </div>
                  ) : (
                    <p className="quiet-copy">No recent structured failures.</p>
                  )}
                </Panel>
              </div>

              <Panel
                title="Quick actions"
                description="Only fixed control-plane actions are available."
              >
                <div className="quick-actions">
                  <Link className="quick-action" to="/settings?section=yvex">
                    <Settings2 aria-hidden="true" size={19} />
                    <strong>Resolve YVEX</strong>
                    <span>Configure or retry the trusted binary.</span>
                  </Link>
                  <Link className="quick-action" to="/settings?section=reference-provider">
                    <MessageSquare aria-hidden="true" size={19} />
                    <strong>Reference provider</strong>
                    <span>Configure, test, and open the chat lane.</span>
                  </Link>
                  <Link className="quick-action" to="/evidence?tab=producers">
                    <Command aria-hidden="true" size={19} />
                    <strong>Inspect producers</strong>
                    <span>Run allowlisted machine-readable reports.</span>
                  </Link>
                </div>
              </Panel>
            </>
          );
        }}
      </ResourceBoundary>
    </div>
  );
}
