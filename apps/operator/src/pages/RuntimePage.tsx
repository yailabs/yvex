/*
 * Owner: apps/operator YVEX runtime workbench.
 * Owns: readiness, backend evidence, session context, generation gating, evaluation/benchmark boundaries, active-operation and control presentation.
 * Does not own: backend probes, materialization, model load, runtime sessions, generation, evaluation, benchmark, or comparison endpoints.
 * Invariants: readiness, active work, historical evidence, control availability, and blockers remain separate facts.
 * Boundary: host topology, diagnostic primitives, and external endpoints are not YVEX runtime support.
 */
import { Ban, Cpu, TerminalSquare } from "lucide-react";
import { Link } from "react-router-dom";

import { capabilityById, type CapabilityId } from "../../shared/contracts.ts";
import {
  CapabilityRows,
  Fact,
  FactGrid,
  PageHeader,
  Panel,
  RecoveryState,
  ResourceBoundary,
  RouteTabs,
  useRouteTab,
} from "../components/Primitives.tsx";
import { StatusBadge } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { useOperatorState } from "../state/operator-state.tsx";
import {
  EmptyState,
  formatBytes,
  Provenance,
  RefreshButton,
  reportOf,
  useDomainProjection,
} from "./PageSupport.tsx";

const tabs = [
  { id: "readiness", label: "Readiness" },
  { id: "backend", label: "Backend" },
  { id: "sessions", label: "Sessions" },
  { id: "generation", label: "Generation" },
  { id: "evaluation", label: "Evaluation" },
  { id: "benchmark", label: "Benchmark" },
] as const;

const stages: readonly {
  id: CapabilityId;
  label: string;
  control: { label: string; href: string } | null;
}[] = [
  {
    id: "artifact.admitted",
    label: "Artifact admission",
    control: { label: "Inspect", href: "/artifacts" },
  },
  { id: "runtime.materialization", label: "Materialization", control: null },
  {
    id: "runtime.binding",
    label: "Runtime binding",
    control: { label: "Backend", href: "/runtime?tab=backend" },
  },
  {
    id: "runtime.model-load",
    label: "Model load",
    control: { label: "Sessions", href: "/runtime?tab=sessions" },
  },
  { id: "runtime.prefill", label: "Prefill", control: null },
  { id: "runtime.kv", label: "KV state", control: null },
  { id: "runtime.decode", label: "Decode", control: null },
  { id: "runtime.logits", label: "Logits", control: null },
  { id: "runtime.sampling", label: "Sampling", control: null },
  {
    id: "generation.native",
    label: "Generation",
    control: { label: "Console", href: "/runtime?tab=generation" },
  },
  {
    id: "evaluation.available",
    label: "Evaluation",
    control: { label: "Inspect", href: "/runtime?tab=evaluation" },
  },
  {
    id: "benchmark.available",
    label: "Benchmark",
    control: { label: "Inspect", href: "/runtime?tab=benchmark" },
  },
];

/** Renders the runtime as an operational YVEX workbench with no provider lane. */
export function RuntimePage() {
  const projection = useDomainProjection("runtime");
  const app = useOperatorState();
  const tab = useRouteTab(tabs, "readiness");
  const page = pageMetadata.runtime;
  const workspace = app.workspace.data;
  /** Resolves primary runtime truth from the workspace snapshot, with standalone manifest fallback during bootstrap only. */
  const observedCapability = (id: CapabilityId) =>
    workspace?.capabilities.find((item) => item.id === id) ??
    capabilityById(app.capabilities.data, id);

  return (
    <div className="page runtime-page">
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        actions={<RefreshButton refresh={projection.refresh} refreshing={projection.refreshing} />}
      />
      <RouteTabs tabs={tabs} defaultTab="readiness" label="Runtime workbench sections" />
      <ResourceBoundary resource={projection}>
        {(data) => {
          const envelope = reportOf(data, "release-decision");
          const decision = envelope?.data;
          if (!decision)
            return (
              <RecoveryState
                state={envelope?.availability ?? data.availability}
                retry={projection.refresh}
              />
            );

          if (tab === "readiness")
            return (
              <div role="tabpanel">
                <Panel
                  title="YVEX execution readiness"
                  description="Capability, operation, evidence, control, and blocker are independent columns."
                >
                  <div
                    className="runtime-stage-table"
                    role="table"
                    aria-label="YVEX runtime readiness"
                  >
                    <div role="row" className="runtime-stage-head">
                      <span role="columnheader">Stage</span>
                      <span role="columnheader">Readiness</span>
                      <span role="columnheader">Active operation</span>
                      <span role="columnheader">Evidence</span>
                      <span role="columnheader">Control</span>
                      <span role="columnheader">Exact blocker</span>
                    </div>
                    {stages.map((stage) => {
                      const capability = observedCapability(stage.id);
                      const active = workspace?.activeJobs.find(
                        (job) =>
                          job.executionOwner === "YVEX" &&
                          job.phase?.includes(stage.label.toLowerCase()),
                      );
                      const ready = capability?.status === "ready";
                      return (
                        <div role="row" key={stage.id}>
                          <strong role="cell">{stage.label}</strong>
                          <div role="cell">
                            <StatusBadge status={capability?.status ?? "loading"} />
                          </div>
                          <span role="cell">{active ? `${active.id} · ${active.phase}` : "—"}</span>
                          <code role="cell">{capability?.source ?? "No observation"}</code>
                          <span role="cell">
                            {stage.control ? (
                              <Link className="table-action" to={stage.control.href}>
                                {stage.control.label}
                              </Link>
                            ) : (
                              "—"
                            )}
                          </span>
                          <span role="cell" className={ready ? "ready-copy" : "blocker-copy"}>
                            {ready ? "No blocker" : (capability?.reason ?? "Capability unobserved")}
                          </span>
                        </div>
                      );
                    })}
                  </div>
                  {envelope ? <Provenance envelope={envelope} /> : null}
                </Panel>
              </div>
            );

          if (tab === "backend") {
            const cpu = observedCapability("backend.cpu");
            const cuda = observedCapability("backend.cuda");
            const selected = observedCapability("backend.selected");
            const host = app.health.data?.host;
            return (
              <div role="tabpanel" className="detail-layout">
                <Panel
                  title="Host topology"
                  description="Host facts are context, never backend evidence."
                  actions={<Cpu aria-hidden="true" size={19} />}
                >
                  <FactGrid>
                    <Fact label="Platform" value={host?.platform ?? "Loading"} />
                    <Fact label="Architecture" value={host?.architecture ?? "Loading"} />
                    <Fact label="Logical processors" value={host?.logicalProcessors ?? "Loading"} />
                    <Fact
                      label="Physical memory"
                      value={host ? formatBytes(host.totalMemoryBytes) : "Loading"}
                    />
                    <Fact label="Node runtime" value={host?.nodeRuntime ?? "Loading"} />
                    <Fact
                      label="Selected backend"
                      value={workspace?.activeBackend?.label ?? "Not bound"}
                    />
                  </FactGrid>
                </Panel>
                <Panel
                  title="YVEX backend topology"
                  description="Only machine-readable backend producers may enable selection."
                >
                  <CapabilityRows
                    capabilities={[cpu, cuda, selected].filter((item) => item !== null)}
                  />
                </Panel>
              </div>
            );
          }

          if (tab === "sessions") {
            const materialization = observedCapability("runtime.materialization");
            const binding = observedCapability("runtime.binding");
            const load = observedCapability("runtime.model-load");
            return (
              <div role="tabpanel" className="runtime-session-layout">
                <Panel
                  title="Active runtime context"
                  description="The session inherits the active workspace; no provider or model chooser is present."
                >
                  <FactGrid>
                    <Fact
                      label="Target"
                      value={workspace?.activeTarget?.id ?? "Not selected"}
                      mono
                    />
                    <Fact
                      label="Artifact"
                      value={workspace?.activeArtifact?.artifactClass ?? "Not selected"}
                      mono
                    />
                    <Fact label="Backend" value={workspace?.activeBackend?.label ?? "Not bound"} />
                    <Fact
                      label="Session"
                      value={workspace?.activeRuntimeSession?.id ?? "No session"}
                      mono
                    />
                    <Fact
                      label="State"
                      value={workspace?.activeRuntimeSession?.state ?? "unloaded"}
                    />
                    <Fact label="Runtime decision" value={decision.runtime} />
                  </FactGrid>
                  <div className="inline-actions">
                    <Link className="button secondary" to="/workspace">
                      Select target
                    </Link>
                    <Link className="button secondary" to="/artifacts">
                      Select artifact
                    </Link>
                  </div>
                </Panel>
                <Panel
                  title="Session controls"
                  description="Controls remain disabled until a real adapter endpoint and every dependency exist."
                >
                  <div className="control-grid">
                    {["Prepare runtime", "Create session", "Load model", "Unload model"].map(
                      (label) => (
                        <button type="button" className="button secondary" disabled key={label}>
                          <Ban aria-hidden="true" size={14} /> {label}
                        </button>
                      ),
                    )}
                  </div>
                  <CapabilityRows
                    capabilities={[materialization, binding, load].filter((item) => item !== null)}
                  />
                </Panel>
                <Panel
                  title="Session timeline"
                  description="No lifecycle event is synthesized before a YVEX runtime session exists."
                >
                  <EmptyState
                    title="No YVEX runtime session"
                    detail="Session events will appear only after the YVEX runtime owner exposes a typed endpoint."
                  />
                </Panel>
              </div>
            );
          }

          if (tab === "generation") {
            const requirements = [
              "runtime.binding",
              "runtime.model-load",
              "generation.tokenizer",
              "generation.native",
              "generation.streaming",
              "generation.cancellation",
            ]
              .map((id) => observedCapability(id as CapabilityId))
              .filter((item) => item !== null);
            return (
              <div role="tabpanel" className="detail-layout">
                <Panel
                  title="Generation Console context"
                  description="YVEX owns execution; the console inherits this exact runtime context."
                  actions={<TerminalSquare aria-hidden="true" size={19} />}
                >
                  <FactGrid>
                    <Fact label="Execution owner" value="YVEX" />
                    <Fact
                      label="Target"
                      value={workspace?.activeTarget?.id ?? "Not selected"}
                      mono
                    />
                    <Fact
                      label="Artifact"
                      value={workspace?.activeArtifact?.artifactClass ?? "Not selected"}
                      mono
                    />
                    <Fact label="Backend" value={workspace?.activeBackend?.label ?? "Not bound"} />
                    <Fact
                      label="Session"
                      value={workspace?.activeRuntimeSession?.state ?? "unloaded"}
                    />
                    <Fact label="Generation" value={decision.generation} />
                  </FactGrid>
                  <button
                    type="button"
                    className="button primary"
                    onClick={() =>
                      window.dispatchEvent(new CustomEvent("yvex:open-generation-console"))
                    }
                  >
                    <TerminalSquare aria-hidden="true" size={14} /> Open Generation Console
                  </button>
                </Panel>
                <Panel
                  title="Generation dependency chain"
                  description="No request is redirected to an external comparison endpoint."
                >
                  <CapabilityRows capabilities={requirements} />
                </Panel>
              </div>
            );
          }

          const capabilityId =
            tab === "evaluation" ? "evaluation.available" : "benchmark.available";
          const capability = observedCapability(capabilityId);
          return (
            <div role="tabpanel" className="detail-layout">
              <Panel
                title={tab === "evaluation" ? "Evaluation" : "Benchmark"}
                description="Execution evidence is bound to an exact YVEX variant and runtime session."
                actions={<StatusBadge status={capability?.status ?? "unavailable"} />}
              >
                <FactGrid>
                  <Fact
                    label="Active target"
                    value={workspace?.activeTarget?.id ?? "Not selected"}
                    mono
                  />
                  <Fact
                    label="Active artifact"
                    value={workspace?.activeArtifact?.artifactClass ?? "Not selected"}
                    mono
                  />
                  <Fact
                    label="Runtime session"
                    value={workspace?.activeRuntimeSession?.id ?? "Unavailable"}
                    mono
                  />
                  <Fact label="Producer source" value={capability?.source ?? "No producer"} mono />
                  <Fact label="Status" value={capability?.status ?? "unavailable"} />
                  <Fact
                    label="Decision"
                    value={tab === "evaluation" ? decision.evaluation : decision.benchmark}
                  />
                </FactGrid>
              </Panel>
              <Panel
                title="Exact boundary"
                description={capability?.reason ?? "Capability unobserved."}
              >
                <p className="body-copy">
                  No control is exposed because no typed {tab} execution endpoint is admitted. The
                  Operator does not synthesize measurements or success evidence.
                </p>
              </Panel>
            </div>
          );
        }}
      </ResourceBoundary>
    </div>
  );
}
