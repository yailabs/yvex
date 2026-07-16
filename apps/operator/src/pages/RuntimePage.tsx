/*
 * Owner: apps/operator Runtime workbench.
 * Owns: native stage readiness, backend evidence separation, durable selections, job context, and real endpoint gating.
 * Does not own: backend probes, materialization, model load, prefill, KV, decode, logits, sampling, generation, or fake controls.
 * Invariants: capability readiness, active job state, and historical evidence remain separate columns.
 * Boundary: host architecture and diagnostic primitives are not native runtime support.
 */
import { Ban, Cpu, ExternalLink, Square } from "lucide-react";
import { Link } from "react-router-dom";

import { capabilityById, type CapabilityId } from "../../shared/contracts.ts";
import { operatorApi } from "../api.ts";
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
  formatBytes,
  Provenance,
  RefreshButton,
  reportOf,
  useDomainProjection,
} from "./PageSupport.tsx";

const tabs = [
  { id: "stages", label: "Stages" },
  { id: "backend", label: "Backend" },
  { id: "controls", label: "Controls" },
] as const;
const stages: readonly { id: CapabilityId; label: string }[] = [
  { id: "artifact.admitted", label: "Artifact admission" },
  { id: "runtime.materialization", label: "Materialization" },
  { id: "runtime.binding", label: "Runtime binding" },
  { id: "runtime.model-load", label: "Model load" },
  { id: "runtime.prefill", label: "Prefill" },
  { id: "runtime.kv", label: "KV state" },
  { id: "runtime.decode", label: "Decode" },
  { id: "runtime.logits", label: "Logits" },
  { id: "runtime.sampling", label: "Sampling" },
  { id: "generation.native", label: "Generation" },
  { id: "evaluation.available", label: "Evaluation" },
  { id: "benchmark.available", label: "Benchmark" },
];

/** Renders native execution truth with every unavailable action tied to a missing capability. */
export function RuntimePage() {
  const projection = useDomainProjection("runtime");
  const app = useOperatorState();
  const tab = useRouteTab(tabs, "stages");
  const page = pageMetadata.runtime;
  return (
    <div className="page">
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        actions={<RefreshButton refresh={projection.refresh} refreshing={projection.refreshing} />}
      />
      <RouteTabs tabs={tabs} defaultTab="stages" label="Runtime sections" />
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
          if (tab === "stages")
            return (
              <div role="tabpanel">
                <Panel
                  title="Native execution stages"
                  description="Readiness, active work, and historical evidence are independent facts."
                >
                  <div
                    className="runtime-stage-table"
                    role="table"
                    aria-label="Native runtime stages"
                  >
                    <div role="row" className="runtime-stage-head">
                      <span role="columnheader">Stage</span>
                      <span role="columnheader">Capability readiness</span>
                      <span role="columnheader">Active job</span>
                      <span role="columnheader">Historical evidence</span>
                    </div>
                    {stages.map((stage, index) => {
                      const capability = capabilityById(app.capabilities.data, stage.id);
                      const active = app.jobs.data?.jobs.find(
                        (job) =>
                          !["cancelled", "completed", "failed"].includes(job.state) &&
                          job.executionOwner === "Native YVEX",
                      );
                      return (
                        <div role="row" key={`${stage.id}:${index}`}>
                          <strong role="cell">{stage.label}</strong>
                          <div role="cell">
                            <StatusBadge status={capability?.status ?? "loading"} />
                            <small>{capability?.reason}</small>
                          </div>
                          <span role="cell">{active?.phase ?? "None"}</span>
                          <span role="cell">{capability?.source ?? "No observation"}</span>
                        </div>
                      );
                    })}
                  </div>
                  {envelope ? <Provenance envelope={envelope} /> : null}
                </Panel>
              </div>
            );
          if (tab === "backend") {
            const cpu = capabilityById(app.capabilities.data, "backend.cpu");
            const cuda = capabilityById(app.capabilities.data, "backend.cuda");
            const host = app.health.data?.host;
            return (
              <div role="tabpanel" className="detail-layout">
                <Panel
                  title="Host topology"
                  description="Host facts are not backend evidence."
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
                  </FactGrid>
                </Panel>
                <Panel
                  title="Backend capability"
                  description="No host architecture is promoted to CPU or CUDA support."
                >
                  <CapabilityRows capabilities={[cpu, cuda].filter((item) => item !== null)} />
                </Panel>
              </div>
            );
          }
          const missing = [
            "artifact.admitted",
            "backend.selected",
            "runtime.binding",
            "runtime.model-load",
            "generation.tokenizer",
            "generation.streaming",
            "generation.cancellation",
          ]
            .map((id) => capabilityById(app.capabilities.data, id as CapabilityId))
            .filter((item) => item && item.status !== "ready");
          const activeJob = app.jobs.data?.jobs.find(
            (job) => job.cancellable && !["cancelled", "completed", "failed"].includes(job.state),
          );
          return (
            <div role="tabpanel" className="controls-layout">
              <Panel
                title="Execution context"
                description="Selections are durable context; they do not bypass admission."
              >
                <FactGrid>
                  <Fact label="Target" value={app.selectedTarget ?? "Not selected"} />
                  <Fact label="Artifact" value={app.selectedArtifact ?? "Not selected"} />
                  <Fact label="Backend" value={app.selectedBackend ?? "No reported backend"} />
                  <Fact label="Native decision" value={decision.runtime} />
                </FactGrid>
                <div className="inline-actions">
                  <Link className="button secondary" to="/models">
                    Select target
                  </Link>
                  <Link className="button secondary" to="/artifacts">
                    Select artifact
                  </Link>
                </div>
              </Panel>
              <Panel
                title="Native controls"
                description="Controls enable only when a real typed endpoint and every dependency are ready."
              >
                <div className="control-grid">
                  {["Prepare runtime", "Load model", "Unload model", "Open native session"].map(
                    (label) => (
                      <button type="button" className="button secondary" disabled key={label}>
                        <Ban aria-hidden="true" size={14} /> {label}
                      </button>
                    ),
                  )}
                  <button
                    type="button"
                    className="button danger"
                    disabled={!activeJob}
                    onClick={() =>
                      activeJob
                        ? void operatorApi.cancelJob(activeJob.id).then(app.refreshAll)
                        : undefined
                    }
                  >
                    <Square aria-hidden="true" size={14} /> Cancel active operation
                  </button>
                  <button
                    type="button"
                    className="button secondary"
                    onClick={() => {
                      app.setSelectedLane("native-yvex");
                      window.dispatchEvent(new CustomEvent("yvex:open-chat"));
                    }}
                  >
                    Open native chat lane
                  </button>
                </div>
                <div className="missing-dependencies">
                  <strong>Missing native dependencies</strong>
                  {missing.map((item) => (
                    <article key={item?.id}>
                      <span>{item?.id}</span>
                      <StatusBadge status={item?.status ?? "unavailable"} />
                      <p>{item?.reason}</p>
                    </article>
                  ))}
                </div>
                <Link className="reference-link" to="/settings?section=reference-provider">
                  Use the independent reference-provider lane{" "}
                  <ExternalLink aria-hidden="true" size={14} />
                </Link>
              </Panel>
            </div>
          );
        }}
      </ResourceBoundary>
    </div>
  );
}
