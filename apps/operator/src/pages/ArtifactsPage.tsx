/*
 * Owner: apps/operator Artifacts workbench.
 * Owns: unified inventory, class filters, durable selection, detail inspection, admission boundary, empty state, and provenance.
 * Does not own: filesystem discovery, integrity, materialization, writes, publication, or support admission.
 * Invariants: exact CLI artifact classes map to explicit terminology and local paths remain adapter-redacted.
 * Boundary: a present tensor proof artifact is not a complete or supported model artifact.
 */
import { Filter, ShieldQuestion } from "lucide-react";
import { useState } from "react";

import type { ArtifactInventory } from "../../shared/contracts.ts";
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
import { useOperatorState } from "../state/operator-state.tsx";
import {
  DataTable,
  EmptyState,
  Provenance,
  RefreshButton,
  reportOf,
  useDomainProjection,
} from "./PageSupport.tsx";

type ArtifactRow = ArtifactInventory["artifacts"][number];
type ArtifactClass = "all" | "proof" | "complete" | "supported";

/** Maps only known physical inventory classes into canonical Operator terminology. */
function artifactClass(row: ArtifactRow): Exclude<ArtifactClass, "all"> {
  if (row.artifact_class === "yvex-selected-gguf") return "proof";
  if (row.artifact_class === "supported-model-gguf") return "supported";
  return "complete";
}

/** Renders one inventory rather than separate empty dashboards for each artifact class. */
export function ArtifactsPage() {
  const projection = useDomainProjection("artifacts");
  const app = useOperatorState();
  const [filter, setFilter] = useState<ArtifactClass>("all");
  const page = pageMetadata.artifacts;
  return (
    <div className="page">
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        actions={<RefreshButton refresh={projection.refresh} refreshing={projection.refreshing} />}
      />
      <ResourceBoundary resource={projection}>
        {(data) => {
          const envelope = reportOf(data, "artifact-inventory");
          if (!envelope || !envelope.data)
            return (
              <RecoveryState
                state={envelope?.availability ?? data.availability}
                retry={projection.refresh}
              />
            );
          const rows = envelope.data.artifacts;
          const visible = rows.filter((row) => filter === "all" || artifactClass(row) === filter);
          const selectedKey = app.selectedArtifact;
          const selected =
            rows.find((row) => `${row.target_id}:${row.artifact_class}` === selectedKey) ?? null;
          return (
            <>
              <MetricStrip
                items={[
                  {
                    label: "Inventory",
                    value: rows.length,
                    detail: "Typed rows",
                    status: envelope.availability.status,
                  },
                  {
                    label: "Proof",
                    value: rows.filter((row) => artifactClass(row) === "proof").length,
                    detail: "Bounded tensor subsets",
                  },
                  {
                    label: "Complete",
                    value: rows.filter(
                      (row) =>
                        artifactClass(row) === "complete" && row.artifact_status === "present",
                    ).length,
                    detail: "Present complete rows",
                  },
                  {
                    label: "Supported",
                    value: rows.filter((row) => artifactClass(row) === "supported").length,
                    detail: "All release gates",
                  },
                ]}
              />
              <div className="inventory-layout">
                <Panel
                  title="Artifact inventory"
                  description="Filter and inspect one machine-reported inventory."
                  actions={
                    <div
                      className="segmented-control"
                      role="group"
                      aria-label="Artifact class filter"
                    >
                      <Filter aria-hidden="true" size={15} />
                      {(["all", "proof", "complete", "supported"] as const).map((value) => (
                        <button
                          type="button"
                          className={filter === value ? "active" : ""}
                          onClick={() => setFilter(value)}
                          key={value}
                        >
                          {value}
                        </button>
                      ))}
                    </div>
                  }
                >
                  {visible.length ? (
                    <DataTable label="Artifact inventory">
                      <thead>
                        <tr>
                          <th>Target</th>
                          <th>Class</th>
                          <th>Classification</th>
                          <th>Status</th>
                          <th>Prepare</th>
                          <th>Path</th>
                        </tr>
                      </thead>
                      <tbody>
                        {visible.map((row) => {
                          const key = `${row.target_id}:${row.artifact_class}`;
                          return (
                            <tr
                              className={selectedKey === key ? "selected-row" : ""}
                              onClick={() => app.setSelectedArtifact(key)}
                              key={key}
                            >
                              <td>
                                <button
                                  type="button"
                                  className="table-link"
                                  onClick={() => app.setSelectedArtifact(key)}
                                >
                                  {row.target_id}
                                </button>
                              </td>
                              <td>{row.artifact_class}</td>
                              <td>
                                <span className="class-label">{artifactClass(row)}</span>
                              </td>
                              <td>
                                <StatusBadge value={row.artifact_status} />
                              </td>
                              <td>
                                <StatusBadge value={row.prepare_status} />
                              </td>
                              <td>
                                <code>{row.path || "Not disclosed"}</code>
                              </td>
                            </tr>
                          );
                        })}
                      </tbody>
                    </DataTable>
                  ) : (
                    <EmptyState detail="No artifact row matches this classification. Absence is expected until its owner reports one." />
                  )}
                  <Provenance envelope={envelope} />
                </Panel>
                <aside className="detail-inspector" aria-label="Artifact detail">
                  {selected ? (
                    <>
                      <header>
                        <span>{artifactClass(selected)} artifact</span>
                        <h2>{selected.target_id}</h2>
                        <StatusBadge value={selected.artifact_status} />
                      </header>
                      <FactGrid>
                        <Fact label="Family" value={selected.family} />
                        <Fact label="Physical class" value={selected.artifact_class} mono />
                        <Fact label="Prepare" value={selected.prepare_status} />
                        <Fact label="Path" value={selected.path || "Not disclosed"} mono />
                        <Fact label="Top blocker" value={selected.top_blocker || "None reported"} />
                      </FactGrid>
                      <div className="admission-note">
                        <ShieldQuestion aria-hidden="true" size={18} />
                        <p>
                          {artifactClass(selected) === "proof"
                            ? "This bounded tensor proof cannot enter complete-model support gates."
                            : artifactClass(selected) === "complete"
                              ? "Completeness does not establish integrity, runtime, generation, evaluation, benchmark, or release admission."
                              : "Supported classification requires every downstream gate."}
                        </p>
                      </div>
                    </>
                  ) : (
                    <EmptyState
                      title="Select an artifact"
                      detail="Choose one inventory row to inspect its exact class, status, blocker, and redacted provenance."
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
