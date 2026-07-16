/*
 * Owner: apps/operator build-chain workbench.
 * Owns: URL-selected build stages, pipeline navigation, stage identity, dependency/evidence inspection, and safe producer reruns.
 * Does not own: stage computation, producer argv, source IO, compilation, quantization, GGUF writing, or capability promotion.
 * Invariants: every stage is distinct, every unknown fact remains unreported, and only registry-owned producers can run.
 * Boundary: a completed report producer is evidence, not execution of the represented build stage.
 */
import { ArrowRight, Play, RefreshCw } from "lucide-react";
import { useState } from "react";
import { Link } from "react-router-dom";

import type { BuildStageId } from "../../shared/contracts.ts";
import { operatorApi, OperatorApiError } from "../api.ts";
import {
  Fact,
  FactGrid,
  MetricStrip,
  PageHeader,
  Panel,
  RecoveryState,
  ResourceBoundary,
  RouteTabs,
  useRouteTab,
} from "../components/Primitives.tsx";
import { StatusBadge } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { useApiResource } from "../resource.ts";
import { useOperatorState } from "../state/operator-state.tsx";
import { EmptyState } from "./PageSupport.tsx";

const stageTabs: readonly { id: BuildStageId; label: string }[] = [
  { id: "source", label: "Source" },
  { id: "architecture", label: "Architecture" },
  { id: "transformation-ir", label: "Transformation IR" },
  { id: "physical-lowering", label: "Lowering" },
  { id: "quantization", label: "Quantization" },
  { id: "gguf-writer", label: "GGUF Writer" },
];

/** Renders the active target build as one process instead of independent report pages. */
export function BuildPage() {
  const app = useOperatorState();
  const key = app.workspace.data?.activeTarget?.id ?? "none";
  const stages = useApiResource(`build-stages:${key}`, operatorApi.buildStages);
  const defaultStage = app.workspace.data?.activeBuild?.currentStage ?? "source";
  const selectedId = useRouteTab(stageTabs, defaultStage, "stage");
  const [runState, setRunState] = useState<{
    state: "idle" | "running" | "complete" | "failed";
    message: string;
  }>({ state: "idle", message: "" });
  const page = pageMetadata.build;

  /** Runs only the stage's registry-owned report producer and refreshes observed build truth. */
  const runProducer = async (producerId: string): Promise<void> => {
    setRunState({ state: "running", message: "Producer running…" });
    try {
      const run = await operatorApi.runProducer(producerId);
      setRunState({
        state: "complete",
        message: `${run.producerId} completed as ${run.envelope?.availability.status ?? "unknown"}.`,
      });
      stages.refresh();
      app.refreshAll();
    } catch (error) {
      setRunState({
        state: "failed",
        message:
          error instanceof OperatorApiError
            ? `${error.code}: ${error.message}`
            : "Producer run failed.",
      });
    }
  };

  return (
    <div className="page build-page">
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        actions={
          <button type="button" className="button secondary" onClick={stages.refresh}>
            <RefreshCw aria-hidden="true" size={14} /> Refresh build
          </button>
        }
      />
      <RouteTabs
        tabs={stageTabs}
        defaultTab={defaultStage}
        parameter="stage"
        label="Build stages"
      />
      <ResourceBoundary resource={stages}>
        {(response) => {
          if (!response.stages.length)
            return <RecoveryState state={response.availability} retry={stages.refresh} />;
          const selected = response.stages.find((stage) => stage.id === selectedId);
          if (!selected)
            return (
              <EmptyState
                title="Build stage unavailable"
                detail="The requested stage is not in the active build contract."
              />
            );
          const readyCount = response.stages.filter((stage) =>
            ["ready", "empty"].includes(stage.availability.status),
          ).length;
          return (
            <>
              <MetricStrip
                items={[
                  {
                    label: "Active target",
                    value: response.targetId ?? "None",
                    detail: "OperatorWorkspaceState.activeTarget",
                    status: response.targetId ? "ready" : "blocked",
                  },
                  {
                    label: "Observed stages",
                    value: response.stages.length,
                    detail: `${readyCount} ready`,
                    status: response.availability.status,
                  },
                  {
                    label: "Selected stage",
                    value: selected.label,
                    detail: selected.availability.reasonCode,
                    status: selected.availability.status,
                  },
                  {
                    label: "Producer",
                    value: selected.producerId ?? "Missing contract",
                    detail: selected.requiredCapability ?? "No capability producer",
                    status: selected.producerId ? "ready" : "unavailable",
                  },
                ]}
              />

              <Panel
                className="build-chain-panel"
                title="Active build chain"
                description="Pipeline order is fixed; observed readiness never skips missing owners."
              >
                <div className="build-chain" role="list" aria-label="Active target build chain">
                  {response.stages.map((stage, index) => (
                    <div className="build-chain-node-wrap" key={stage.id}>
                      <Link
                        role="listitem"
                        className={`build-chain-node${stage.id === selected.id ? " active" : ""}`}
                        data-status={stage.availability.status}
                        to={`/build?stage=${stage.id}`}
                      >
                        <span>{String(index + 1).padStart(2, "0")}</span>
                        <strong>{stage.label}</strong>
                        <StatusBadge status={stage.availability.status} />
                        <small>{stage.outputIdentity ?? stage.availability.reasonCode}</small>
                      </Link>
                      {index < response.stages.length - 1 ? (
                        <ArrowRight aria-hidden="true" size={15} />
                      ) : null}
                    </div>
                  ))}
                </div>
              </Panel>

              <div className="build-stage-layout">
                <Panel
                  title={`${selected.label} inspector`}
                  description={selected.availability.message}
                  actions={<StatusBadge status={selected.availability.status} />}
                >
                  <FactGrid>
                    <Fact label="Stage ID" value={selected.id} mono />
                    <Fact label="Reason code" value={selected.availability.reasonCode} mono />
                    <Fact label="Producer" value={selected.producerId ?? "Unavailable"} mono />
                    <Fact
                      label="Required capability"
                      value={selected.requiredCapability ?? "No stable capability contract"}
                      mono
                    />
                    <Fact
                      label="Input identity"
                      value={selected.inputIdentity ?? "Not reported"}
                      mono
                    />
                    <Fact
                      label="Output identity"
                      value={selected.outputIdentity ?? "Not reported"}
                      mono
                    />
                    <Fact
                      label="Descriptor count"
                      value={selected.descriptorCount ?? "Not measured"}
                    />
                    <Fact
                      label="Tensor coverage"
                      value={selected.tensorCoverage ?? "Not reported"}
                    />
                  </FactGrid>
                  {selected.refusal ? (
                    <div className="stage-refusal" data-status={selected.availability.status}>
                      <strong>{selected.refusal.code}</strong>
                      <p>{selected.refusal.message}</p>
                    </div>
                  ) : null}
                  <div className="inline-actions">
                    {selected.producerId ? (
                      <button
                        type="button"
                        className="button primary"
                        disabled={runState.state === "running"}
                        onClick={() => void runProducer(selected.producerId as string)}
                      >
                        <Play aria-hidden="true" size={14} /> Run evidence producer
                      </button>
                    ) : (
                      <button type="button" className="button secondary" disabled>
                        No executable producer
                      </button>
                    )}
                    <Link className="button secondary" to="/evidence?tab=producers">
                      Inspect producer registry
                    </Link>
                  </div>
                  {runState.state !== "idle" ? (
                    <p
                      className={runState.state === "failed" ? "inline-error" : "operation-note"}
                      role="status"
                    >
                      {runState.message}
                    </p>
                  ) : null}
                </Panel>

                <Panel
                  title="Contract trace"
                  description="Dependencies and evidence fields belong to this stage only."
                >
                  <div className="contract-trace">
                    <section>
                      <h3>Dependencies</h3>
                      {selected.dependencies.length ? (
                        selected.dependencies.map((dependency) => (
                          <Link to={`/build?stage=${dependency}`} key={dependency}>
                            <code>{dependency}</code>
                            <ArrowRight aria-hidden="true" size={13} />
                          </Link>
                        ))
                      ) : (
                        <p>Root stage; no earlier build dependency.</p>
                      )}
                    </section>
                    <section>
                      <h3>Evidence</h3>
                      {selected.evidence.length ? (
                        selected.evidence.map((item) => <code key={item}>{item}</code>)
                      ) : (
                        <p>No machine-readable evidence producer is admitted.</p>
                      )}
                    </section>
                    <section>
                      <h3>Capability boundary</h3>
                      <p>
                        This surface reports the lowest observed stage. It does not run source IO,
                        lowering, quantization, or artifact writing through the browser.
                      </p>
                    </section>
                  </div>
                </Panel>
              </div>
            </>
          );
        }}
      </ResourceBoundary>
    </div>
  );
}
