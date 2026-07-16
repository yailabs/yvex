/*
 * Owner: apps/operator lifecycle-centered engineering workspace.
 * Owns: target navigator, complete YVEX lifecycle, stage selection, context inspector, and bottom evidence panel.
 * Does not own: workspace persistence, producer execution, native build/runtime actions, capability inference, or comparison endpoints.
 * Invariants: every visible context fact derives from the authoritative workspace response and every stage preserves its exact status.
 * Boundary: selecting a target or inspecting a stage does not promote build, artifact, runtime, or generation readiness.
 */
import {
  Activity,
  ArrowRight,
  Boxes,
  Braces,
  CircleDot,
  Cpu,
  Database,
  FileCode2,
  Gauge,
  Layers3,
  RefreshCw,
  TerminalSquare,
} from "lucide-react";
import { useState } from "react";
import { Link, useSearchParams } from "react-router-dom";

import {
  availability,
  capabilityById,
  type Availability,
  type BuildStage,
  type CapabilityId,
  type WorkspaceTargetKind,
} from "../../shared/contracts.ts";
import { operatorApi, OperatorApiError } from "../api.ts";
import {
  Fact,
  FactGrid,
  MetricStrip,
  PageHeader,
  Panel,
  ResourceBoundary,
  RouteTabs,
  useRouteTab,
} from "../components/Primitives.tsx";
import { StatusBadge } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { useApiResource } from "../resource.ts";
import { useOperatorState } from "../state/operator-state.tsx";
import { EmptyState } from "./PageSupport.tsx";

const targetKindOrder: readonly WorkspaceTargetKind[] = [
  "release-target",
  "runtime-target",
  "engineering-slice",
  "proof-target",
  "source-candidate",
  "unclassified",
];

const bottomTabs = [
  { id: "jobs", label: "Jobs" },
  { id: "events", label: "Events" },
  { id: "evidence", label: "Evidence" },
] as const;

interface LifecycleStage {
  id: string;
  label: string;
  shortLabel: string;
  availability: Availability;
  buildStage: BuildStage | null;
  capabilityId: CapabilityId | null;
  route: string;
  icon: typeof Database;
}

/** Converts one capability observation into the same inspector vocabulary as build stages. */
function capabilityAvailability(
  app: ReturnType<typeof useOperatorState>,
  capabilityId: CapabilityId,
  label: string,
): Availability {
  const item =
    app.workspace.data?.capabilities.find((candidate) => candidate.id === capabilityId) ??
    capabilityById(app.capabilities.data, capabilityId) ??
    undefined;
  const observedAt = app.workspace.data?.observedAt ?? new Date().toISOString();
  return item
    ? availability(item.status, item.refusalCode ?? `${item.id}-ready`, item.reason, observedAt, {
        source: item.source,
        ...(item.recoveryAction ? { recovery: item.recoveryAction } : {}),
      })
    : availability(
        "unavailable",
        `${capabilityId.replaceAll(".", "-")}-unobserved`,
        `${label} has no current capability observation.`,
        observedAt,
        { source: "operator-workspace" },
      );
}

/** Builds the product-wide chain from active build stages plus downstream YVEX capabilities. */
function lifecycleStages(app: ReturnType<typeof useOperatorState>): LifecycleStage[] {
  const build = app.workspace.data?.activeBuild;
  const observedAt = app.workspace.data?.observedAt ?? new Date().toISOString();
  const buildDefinition: readonly [BuildStage["id"], string, string, string, typeof Database][] = [
    ["source", "Source", "Source", "/build?stage=source", Database],
    ["architecture", "Logical model", "Model", "/build?stage=architecture", Layers3],
    ["transformation-ir", "Transformation IR", "IR", "/build?stage=transformation-ir", Braces],
    [
      "physical-lowering",
      "Physical lowering",
      "Lowering",
      "/build?stage=physical-lowering",
      FileCode2,
    ],
    ["quantization", "Quantization plan", "Quant", "/build?stage=quantization", Gauge],
    ["gguf-writer", "GGUF writer", "GGUF", "/build?stage=gguf-writer", Boxes],
  ];
  const stages = buildDefinition.map(([id, label, shortLabel, route, icon]) => {
    const buildStage = build?.stages.find((item) => item.id === id) ?? null;
    return {
      id,
      label,
      shortLabel,
      availability:
        buildStage?.availability ??
        availability(
          "unavailable",
          "active-build-unavailable",
          "No build projection exists for the active target.",
          observedAt,
          { source: "operator-workspace" },
        ),
      buildStage,
      capabilityId: buildStage?.requiredCapability ?? null,
      route,
      icon,
    };
  });
  return [
    ...stages,
    {
      id: "artifact-admission",
      label: "Artifact admission",
      shortLabel: "Admission",
      availability: capabilityAvailability(app, "artifact.admitted", "Artifact admission"),
      buildStage: null,
      capabilityId: "artifact.admitted",
      route: "/artifacts",
      icon: Boxes,
    },
    {
      id: "backend-binding",
      label: "Backend binding",
      shortLabel: "Backend",
      availability: capabilityAvailability(app, "backend.selected", "Backend binding"),
      buildStage: null,
      capabilityId: "backend.selected",
      route: "/runtime?tab=backend",
      icon: Cpu,
    },
    {
      id: "runtime-session",
      label: "Runtime session",
      shortLabel: "Session",
      availability: capabilityAvailability(app, "runtime.binding", "Runtime session"),
      buildStage: null,
      capabilityId: "runtime.binding",
      route: "/runtime?tab=sessions",
      icon: CircleDot,
    },
    {
      id: "generation",
      label: "Generation",
      shortLabel: "Generate",
      availability: capabilityAvailability(app, "generation.native", "Generation"),
      buildStage: null,
      capabilityId: "generation.native",
      route: "/runtime?tab=generation",
      icon: TerminalSquare,
    },
    {
      id: "execution-evidence",
      label: "Execution evidence",
      shortLabel: "Evidence",
      availability: capabilityAvailability(app, "evaluation.available", "Execution evidence"),
      buildStage: null,
      capabilityId: "evaluation.available",
      route: "/evidence",
      icon: Activity,
    },
  ];
}

/** Renders the active engineering context as target navigation, lifecycle canvas, and inspectable trace. */
export function WorkspacePage() {
  const app = useOperatorState();
  const targets = useApiResource("workspace-targets", operatorApi.targets);
  const [params, setParams] = useSearchParams();
  const panel = useRouteTab(bottomTabs, "jobs", "panel");
  const [selecting, setSelecting] = useState<string | null>(null);
  const [selectionError, setSelectionError] = useState<string | null>(null);
  const page = pageMetadata.workspace;
  const stages = lifecycleStages(app);
  const workspaceEvents = (app.events.data?.events ?? []).filter(
    (event) => !event.type.startsWith("comparison-"),
  );
  const requestedStage = params.get("stage");
  const selectedStage =
    stages.find((stage) => stage.id === requestedStage) ??
    stages.find((stage) => stage.id === app.workspace.data?.activeBuild?.currentStage) ??
    stages[0];

  /** Commits a target through the server authority and refreshes every dependent resource. */
  const selectTarget = async (targetId: string): Promise<void> => {
    setSelecting(targetId);
    setSelectionError(null);
    try {
      await operatorApi.selectTarget(targetId);
      app.refreshAll();
    } catch (error) {
      setSelectionError(
        error instanceof OperatorApiError
          ? `${error.code}: ${error.message}`
          : "Target selection failed.",
      );
    } finally {
      setSelecting(null);
    }
  };

  return (
    <div className="page workspace-page">
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        actions={
          <button type="button" className="button secondary" onClick={app.refreshAll}>
            <RefreshCw aria-hidden="true" size={14} /> Refresh workspace
          </button>
        }
      />
      <ResourceBoundary resource={app.workspace}>
        {(workspace) => (
          <>
            <MetricStrip
              items={[
                {
                  label: "Target",
                  value: workspace.activeTarget?.id ?? "Not selected",
                  detail: workspace.activeTarget?.kindLabel ?? "No catalog authority",
                  status: workspace.activeTarget ? "ready" : "blocked",
                },
                {
                  label: "Build",
                  value: workspace.activeBuild?.currentStage ?? "No projection",
                  detail: workspace.activeBuild?.id,
                  status: workspace.activeBuild?.status ?? "unavailable",
                },
                {
                  label: "Artifact",
                  value: workspace.activeArtifact?.artifactClass ?? "Not selected",
                  detail: workspace.activeArtifact?.artifactStatus ?? "Admission required",
                  status: workspace.activeArtifact
                    ? (workspace.capabilities.find((item) => item.id === "artifact.admitted")
                        ?.status ?? "unavailable")
                    : "empty",
                },
                {
                  label: "Backend",
                  value: workspace.activeBackend?.label ?? "Not bound",
                  detail: "YVEX backend authority",
                  status: workspace.activeBackend?.status ?? "blocked",
                },
                {
                  label: "Session",
                  value: workspace.activeRuntimeSession?.state ?? "Unloaded",
                  detail: workspace.activeRuntimeSession?.id ?? "No runtime session",
                  status: workspace.activeRuntimeSession ? "ready" : "empty",
                },
              ]}
            />

            <div className="engineering-workspace">
              <aside className="workspace-navigator" aria-label="Target navigator">
                <Panel
                  title="Targets"
                  description="Typed roles from the YVEX catalog; unlike objects are never flattened into one model list."
                >
                  <ResourceBoundary resource={targets}>
                    {(catalog) => (
                      <div className="target-taxonomy">
                        {targetKindOrder.map((kind) => {
                          const rows = catalog.targets.filter((target) => target.kind === kind);
                          if (!rows.length) return null;
                          return (
                            <section key={kind}>
                              <h3>
                                {rows[0]?.kindLabel} <span>{rows.length}</span>
                              </h3>
                              {rows.map((target) => (
                                <button
                                  type="button"
                                  className={
                                    workspace.activeTarget?.id === target.id ? "active" : ""
                                  }
                                  disabled={selecting !== null}
                                  onClick={() => void selectTarget(target.id)}
                                  key={target.id}
                                >
                                  <span>{target.id}</span>
                                  <small>{target.family}</small>
                                  {selecting === target.id ? <em>selecting…</em> : null}
                                </button>
                              ))}
                            </section>
                          );
                        })}
                        {selectionError ? (
                          <p className="inline-error" role="alert">
                            {selectionError}
                          </p>
                        ) : null}
                      </div>
                    )}
                  </ResourceBoundary>
                </Panel>
              </aside>

              <Panel
                className="pipeline-workbench"
                title="YVEX transformation and execution chain"
                description="Select a stage to inspect its identity, evidence, dependencies, and exact boundary."
                actions={
                  <StatusBadge
                    status={workspace.activeBuild?.status ?? "unavailable"}
                    value={workspace.activeBuild ? "active build" : "no build projection"}
                  />
                }
              >
                <div className="pipeline-canvas" role="list" aria-label="YVEX lifecycle stages">
                  {stages.map((stage, index) => {
                    const Icon = stage.icon;
                    return (
                      <div className="pipeline-step-wrap" key={stage.id}>
                        <button
                          type="button"
                          role="listitem"
                          className={`pipeline-step${selectedStage?.id === stage.id ? " active" : ""}`}
                          data-status={stage.availability.status}
                          onClick={() => {
                            const next = new URLSearchParams(params);
                            next.set("stage", stage.id);
                            setParams(next);
                          }}
                        >
                          <span className="pipeline-index">
                            {String(index + 1).padStart(2, "0")}
                          </span>
                          <Icon aria-hidden="true" size={17} />
                          <strong>{stage.shortLabel}</strong>
                          <StatusBadge status={stage.availability.status} />
                        </button>
                        {index < stages.length - 1 ? (
                          <ArrowRight className="pipeline-arrow" aria-hidden="true" size={15} />
                        ) : null}
                      </div>
                    );
                  })}
                </div>
                <div className="pipeline-legend">
                  <span>Source interpretation</span>
                  <span>Build and artifact</span>
                  <span>Runtime and execution</span>
                </div>
              </Panel>

              <aside className="workspace-inspector" aria-label="Selected stage inspector">
                {selectedStage ? (
                  <Panel
                    title={selectedStage.label}
                    description={selectedStage.availability.message}
                    actions={<StatusBadge status={selectedStage.availability.status} />}
                  >
                    <FactGrid>
                      <Fact
                        label="Reason code"
                        value={selectedStage.availability.reasonCode}
                        mono
                      />
                      <Fact
                        label="Producer"
                        value={
                          selectedStage.buildStage?.producerId ??
                          selectedStage.availability.source ??
                          "None"
                        }
                        mono
                      />
                      <Fact
                        label="Input identity"
                        value={selectedStage.buildStage?.inputIdentity ?? "Not reported"}
                        mono
                      />
                      <Fact
                        label="Output identity"
                        value={selectedStage.buildStage?.outputIdentity ?? "Not reported"}
                        mono
                      />
                      <Fact
                        label="Descriptor count"
                        value={selectedStage.buildStage?.descriptorCount ?? "Not measured"}
                      />
                      <Fact
                        label="Tensor coverage"
                        value={selectedStage.buildStage?.tensorCoverage ?? "Not reported"}
                      />
                    </FactGrid>
                    {selectedStage.buildStage?.dependencies.length ? (
                      <div className="inspector-list">
                        <span>Dependencies</span>
                        {selectedStage.buildStage.dependencies.map((dependency) => (
                          <code key={dependency}>{dependency}</code>
                        ))}
                      </div>
                    ) : null}
                    {selectedStage.buildStage?.evidence.length ? (
                      <div className="inspector-list">
                        <span>Evidence fields</span>
                        {selectedStage.buildStage.evidence.map((evidence) => (
                          <code key={evidence}>{evidence}</code>
                        ))}
                      </div>
                    ) : null}
                    <Link className="button secondary inspector-action" to={selectedStage.route}>
                      Open stage workbench <ArrowRight aria-hidden="true" size={14} />
                    </Link>
                  </Panel>
                ) : (
                  <EmptyState title="No stage selected" detail="Select one lifecycle stage." />
                )}
              </aside>
            </div>

            <section className="workspace-bottom-panel" aria-label="Workspace activity">
              <RouteTabs
                tabs={bottomTabs}
                defaultTab="jobs"
                parameter="panel"
                label="Workspace activity"
              />
              {panel === "jobs" ? (
                <div className="bottom-panel-content" role="tabpanel">
                  {workspace.activeJobs.length ? (
                    workspace.activeJobs.map((job) => (
                      <article key={job.id}>
                        <div>
                          <strong>{job.type}</strong>
                          <span>{job.executionOwner}</span>
                        </div>
                        <code>{job.id}</code>
                        <span>{job.phase ?? "starting"}</span>
                        <StatusBadge status="loading" value={job.state} />
                      </article>
                    ))
                  ) : (
                    <EmptyState
                      title="No active YVEX jobs"
                      detail="Producer history remains under Evidence."
                    />
                  )}
                </div>
              ) : null}
              {panel === "events" ? (
                <div className="bottom-panel-content" role="tabpanel">
                  {workspaceEvents.slice(0, 8).map((event) => (
                    <article key={event.id}>
                      <time dateTime={event.observedAt}>
                        {new Date(event.observedAt).toLocaleTimeString()}
                      </time>
                      <code>{event.type}</code>
                      <span>{event.message}</span>
                      <StatusBadge
                        status={
                          event.severity === "error"
                            ? "failed"
                            : event.severity === "warning"
                              ? "degraded"
                              : "ready"
                        }
                        value={event.severity}
                      />
                    </article>
                  ))}
                  {!workspaceEvents.length ? (
                    <EmptyState
                      title="No recent events"
                      detail="The bounded event history is empty."
                    />
                  ) : null}
                </div>
              ) : null}
              {panel === "evidence" ? (
                <div className="bottom-panel-content" role="tabpanel">
                  {workspace.recentEvidence.map((run) => (
                    <article key={run.runId}>
                      <strong>{run.producerId}</strong>
                      <code>{run.runId}</code>
                      <span>{run.envelope?.availability.message ?? "Run in progress"}</span>
                      <StatusBadge status={run.envelope?.availability.status ?? "loading"} />
                    </article>
                  ))}
                  {!workspace.recentEvidence.length ? (
                    <EmptyState
                      title="No explicit producer runs"
                      detail="Open Evidence to inspect or run an allowlisted producer."
                    />
                  ) : null}
                </div>
              ) : null}
            </section>
          </>
        )}
      </ResourceBoundary>
    </div>
  );
}
