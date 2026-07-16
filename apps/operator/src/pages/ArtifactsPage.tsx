/*
 * Owner: apps/operator artifact inventory workbench.
 * Owns: typed inventory, canonical class filters, active-workspace selection, detail inspection, admission boundary, and safe provenance.
 * Does not own: filesystem discovery, path disclosure, integrity, materialization, writes, publication, or support admission.
 * Invariants: only a present artifact for the active target can be selected and proof artifacts remain distinct from complete/supported artifacts.
 * Boundary: inventory presence and Operator selection do not establish runtime or generation support.
 */
import { Filter, ShieldQuestion } from "lucide-react";
import { useState } from "react";

import type { WorkspaceArtifact } from "../../shared/contracts.ts";
import { operatorApi, OperatorApiError } from "../api.ts";
import {
  Fact,
  FactGrid,
  MetricStrip,
  PageHeader,
  Panel,
  RecoveryState,
  ResourceBoundary,
} from "../components/Primitives.tsx";
import { StatusBadge } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { useApiResource } from "../resource.ts";
import { useOperatorState } from "../state/operator-state.tsx";
import { DataTable, EmptyState } from "./PageSupport.tsx";

type ArtifactClass = "all" | WorkspaceArtifact["classification"];

/** Renders one coherent artifact inventory against the authoritative workspace selection. */
export function ArtifactsPage() {
  const app = useOperatorState();
  const inventory = useApiResource("workspace-artifacts", operatorApi.workspaceArtifacts);
  const [filter, setFilter] = useState<ArtifactClass>("all");
  const [selectionError, setSelectionError] = useState<string | null>(null);
  const [selecting, setSelecting] = useState<string | null>(null);
  const page = pageMetadata.artifacts;
  const activeTarget = app.workspace.data?.activeTarget?.id ?? null;
  const selectedId = app.workspace.data?.activeArtifact?.id ?? null;

  /** Commits one present same-target artifact through the server workspace authority. */
  const selectArtifact = async (artifactId: string): Promise<void> => {
    setSelecting(artifactId);
    setSelectionError(null);
    try {
      await operatorApi.selectArtifact(artifactId);
      app.refreshAll();
      inventory.refresh();
    } catch (error) {
      setSelectionError(
        error instanceof OperatorApiError
          ? `${error.code}: ${error.message}`
          : "Artifact selection failed.",
      );
    } finally {
      setSelecting(null);
    }
  };

  return (
    <div className="page">
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        actions={
          <button type="button" className="button secondary" onClick={inventory.refresh}>
            Refresh inventory
          </button>
        }
      />
      <ResourceBoundary resource={inventory}>
        {(data) => {
          if (!data.artifacts.length && data.availability.status !== "empty")
            return <RecoveryState state={data.availability} retry={inventory.refresh} />;
          const visible = data.artifacts.filter(
            (row) => filter === "all" || row.classification === filter,
          );
          const selected = data.artifacts.find((row) => row.id === selectedId) ?? null;
          return (
            <>
              <MetricStrip
                items={[
                  {
                    label: "Inventory",
                    value: data.artifacts.length,
                    detail: data.producerId,
                    status: data.availability.status,
                  },
                  {
                    label: "Active target",
                    value: activeTarget ?? "None",
                    detail: "Workspace authority",
                    status: activeTarget ? "ready" : "blocked",
                  },
                  {
                    label: "Proof",
                    value: data.artifacts.filter((row) => row.classification === "proof").length,
                    detail: "Bounded subsets",
                  },
                  {
                    label: "Complete present",
                    value: data.artifacts.filter(
                      (row) =>
                        row.classification === "complete" && row.artifactStatus === "present",
                    ).length,
                    detail: "Not necessarily supported",
                  },
                  {
                    label: "Supported",
                    value: data.artifacts.filter((row) => row.classification === "supported")
                      .length,
                    detail: "All release gates",
                  },
                ]}
              />
              <div className="inventory-layout">
                <Panel
                  title="Artifact inventory"
                  description="Local paths are not exposed; select only present artifacts bound to the active target."
                  actions={
                    <div
                      className="segmented-control"
                      role="group"
                      aria-label="Artifact class filter"
                    >
                      <Filter aria-hidden="true" size={15} />
                      {(["all", "proof", "complete", "supported", "unclassified"] as const).map(
                        (value) => (
                          <button
                            type="button"
                            className={filter === value ? "active" : ""}
                            onClick={() => setFilter(value)}
                            key={value}
                          >
                            {value}
                          </button>
                        ),
                      )}
                    </div>
                  }
                >
                  {visible.length ? (
                    <DataTable label="Artifact inventory">
                      <thead>
                        <tr>
                          <th>Target</th>
                          <th>Physical class</th>
                          <th>Classification</th>
                          <th>Artifact</th>
                          <th>Prepare</th>
                          <th>Workspace</th>
                        </tr>
                      </thead>
                      <tbody>
                        {visible.map((row) => {
                          const sameTarget = row.targetId === activeTarget;
                          const selectable = sameTarget && row.artifactStatus === "present";
                          return (
                            <tr
                              className={selectedId === row.id ? "selected-row" : ""}
                              key={row.id}
                            >
                              <td>
                                <strong>{row.targetId}</strong>
                                <small>{row.family}</small>
                              </td>
                              <td>
                                <code>{row.artifactClass}</code>
                              </td>
                              <td>
                                <span className="class-label">{row.classification}</span>
                              </td>
                              <td>
                                <StatusBadge value={row.artifactStatus} />
                              </td>
                              <td>
                                <StatusBadge value={row.prepareStatus} />
                              </td>
                              <td>
                                <button
                                  type="button"
                                  className="button secondary"
                                  disabled={!selectable || selecting !== null}
                                  title={
                                    !sameTarget
                                      ? "Artifact belongs to a different target"
                                      : row.artifactStatus !== "present"
                                        ? "Artifact is not present"
                                        : undefined
                                  }
                                  onClick={() => void selectArtifact(row.id)}
                                >
                                  {selectedId === row.id
                                    ? "Active"
                                    : selecting === row.id
                                      ? "Selecting…"
                                      : "Select"}
                                </button>
                              </td>
                            </tr>
                          );
                        })}
                      </tbody>
                    </DataTable>
                  ) : (
                    <EmptyState detail="No artifact row matches this classification." />
                  )}
                  {selectionError ? (
                    <p className="inline-error" role="alert">
                      {selectionError}
                    </p>
                  ) : null}
                  <div className="provenance-strip">
                    <span>Producer</span>
                    <code>{data.producerId}</code>
                    <span>Observed</span>
                    <time dateTime={data.observedAt}>
                      {new Date(data.observedAt).toLocaleString()}
                    </time>
                  </div>
                </Panel>
                <aside className="detail-inspector" aria-label="Artifact detail">
                  {selected ? (
                    <>
                      <header>
                        <span>{selected.classification} artifact</span>
                        <h2>{selected.targetId}</h2>
                        <StatusBadge value={selected.artifactStatus} />
                      </header>
                      <FactGrid>
                        <Fact label="Workspace ID" value={selected.id} mono />
                        <Fact label="Family" value={selected.family} />
                        <Fact label="Physical class" value={selected.artifactClass} mono />
                        <Fact label="Prepare" value={selected.prepareStatus} />
                        <Fact label="Local path" value="Not exposed" />
                        <Fact label="Top blocker" value={selected.topBlocker || "None reported"} />
                      </FactGrid>
                      <div className="admission-note">
                        <ShieldQuestion aria-hidden="true" size={18} />
                        <p>
                          {selected.classification === "proof"
                            ? "This bounded tensor proof cannot enter complete-model runtime gates."
                            : selected.classification === "complete"
                              ? "Completeness does not establish integrity, runtime, generation, evaluation, benchmark, or release admission."
                              : selected.classification === "supported"
                                ? "Supported classification requires every downstream gate."
                                : "This physical class has no admitted canonical artifact classification."}
                        </p>
                      </div>
                    </>
                  ) : (
                    <EmptyState
                      title="No active artifact"
                      detail="A present artifact for the active target is required before runtime binding."
                    />
                  )}
                </aside>
              </div>
            </>
          );
        }}
      </ResourceBoundary>
    </div>
  );
}
