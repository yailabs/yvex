/*
 * Owner: apps/operator Models workbench.
 * Owns: searchable typed target catalog, durable selection, target detail, release identity, and producer provenance.
 * Does not own: target discovery, model support, source IO, artifact creation, runtime execution, or fabricated rows.
 * Invariants: target facts come only from validated producer envelopes and missing binary yields one recovery surface.
 * Boundary: catalog presence and selection are not runtime or generation support.
 */
import { Check, Search } from "lucide-react";
import { useState } from "react";
import { useSearchParams } from "react-router-dom";

import {
  MetricStrip,
  PageHeader,
  Panel,
  RecoveryState,
  ResourceBoundary,
  RouteTabs,
  useRouteTab,
  Fact,
  FactGrid,
} from "../components/Primitives.tsx";
import { StatusBadge } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { useOperatorState } from "../state/operator-state.tsx";
import {
  DataTable,
  EmptyState,
  ProjectionState,
  Provenance,
  RefreshButton,
  reportOf,
  useDomainProjection,
} from "./PageSupport.tsx";

const tabs = [
  { id: "catalog", label: "Catalog" },
  { id: "detail", label: "Target detail" },
] as const;

/** Renders target selection and detail as distinct URL-persisted panels. */
export function ModelsPage() {
  const projection = useDomainProjection("models");
  const app = useOperatorState();
  const [search, setSearch] = useState("");
  const [params, setParams] = useSearchParams();
  const tab = useRouteTab(tabs, "catalog");
  const page = pageMetadata.models;

  return (
    <div className="page">
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        actions={<RefreshButton refresh={projection.refresh} refreshing={projection.refreshing} />}
      />
      <RouteTabs tabs={tabs} defaultTab="catalog" label="Model sections" />
      {tab === "catalog" ? (
        <div id="model-tab-catalog" role="tabpanel">
          {" "}
          <ResourceBoundary resource={projection}>
            {(data) => {
              const envelope = reportOf(data, "target-catalog");
              if (!envelope || !envelope.data)
                return (
                  <RecoveryState
                    state={envelope?.availability ?? data.availability}
                    retry={projection.refresh}
                  />
                );
              const targets = envelope.data.targets.filter((target) =>
                `${target.target_id} ${target.family} ${target.class}`
                  .toLowerCase()
                  .includes(search.toLowerCase()),
              );
              const selectedId =
                params.get("target") ??
                app.selectedTarget ??
                envelope.data.targets.find((target) => target.release_selected)?.target_id ??
                null;
              return (
                <>
                  <MetricStrip
                    items={[
                      {
                        label: "Reported targets",
                        value: envelope.data.targets.length,
                        detail: "Typed catalog",
                        status: envelope.availability.status,
                      },
                      {
                        label: "Release target",
                        value:
                          envelope.data.targets.find((target) => target.release_selected)
                            ?.target_id ?? "None",
                        detail: "v0.1.0 selection",
                      },
                      {
                        label: "Selected",
                        value: selectedId ?? "None",
                        detail: "Operator context",
                      },
                    ]}
                  />
                  <Panel
                    title="Target catalog"
                    description="Search and select only targets returned by YVEX."
                    actions={<ProjectionState projection={data} />}
                  >
                    <label className="search-field">
                      <Search aria-hidden="true" size={16} />
                      <span className="sr-only">Search targets</span>
                      <input
                        value={search}
                        onChange={(event) => setSearch(event.target.value)}
                        placeholder="Search target, family, class…"
                      />
                    </label>
                    {targets.length ? (
                      <DataTable label="YVEX target catalog">
                        <thead>
                          <tr>
                            <th>Target</th>
                            <th>Family / class</th>
                            <th>Runtime</th>
                            <th>Generation</th>
                            <th>Release</th>
                            <th>
                              <span className="sr-only">Select</span>
                            </th>
                          </tr>
                        </thead>
                        <tbody>
                          {targets.map((target) => (
                            <tr
                              className={selectedId === target.target_id ? "selected-row" : ""}
                              key={target.target_id}
                            >
                              <td>
                                <strong>{target.target_id}</strong>
                              </td>
                              <td>
                                {target.family}
                                <small>{target.class}</small>
                              </td>
                              <td>
                                <StatusBadge value={target.runtime} />
                              </td>
                              <td>
                                <StatusBadge value={target.generation} />
                              </td>
                              <td>
                                {target.release_selected ? (
                                  <span className="selection-mark">
                                    <Check aria-hidden="true" size={14} /> v0.1.0
                                  </span>
                                ) : (
                                  "Engineering scope"
                                )}
                              </td>
                              <td>
                                <button
                                  type="button"
                                  className="button secondary"
                                  onClick={() => {
                                    app.setSelectedTarget(target.target_id);
                                    const next = new URLSearchParams(params);
                                    next.set("target", target.target_id);
                                    next.set("tab", "detail");
                                    setParams(next);
                                  }}
                                >
                                  Select
                                </button>
                              </td>
                            </tr>
                          ))}
                        </tbody>
                      </DataTable>
                    ) : (
                      <EmptyState detail="No reported target matches the current filter." />
                    )}
                    <Provenance envelope={envelope} />
                  </Panel>
                </>
              );
            }}
          </ResourceBoundary>
        </div>
      ) : null}
      {tab === "detail" ? (
        <div id="model-tab-detail" role="tabpanel">
          <ResourceBoundary resource={projection}>
            {(data) => {
              const catalog = reportOf(data, "target-catalog")?.data;
              const detailEnvelope = reportOf(data, "target-detail");
              const targetId =
                params.get("target") ??
                app.selectedTarget ??
                catalog?.targets.find((target) => target.release_selected)?.target_id;
              const target = catalog?.targets.find((item) => item.target_id === targetId);
              if (!target)
                return (
                  <EmptyState
                    title="No target selected"
                    detail="Select one machine-reported target from the Catalog tab."
                  />
                );
              const detail =
                detailEnvelope?.data?.target_id === target.target_id ? detailEnvelope.data : null;
              return (
                <div className="detail-layout">
                  <Panel
                    title={target.target_id}
                    description={`${target.family} · ${target.class}`}
                    actions={<StatusBadge status={detail ? "ready" : "degraded"} />}
                  >
                    <FactGrid>
                      <Fact
                        label="Release selected"
                        value={target.release_selected ? "yes" : "no"}
                      />
                      <Fact
                        label="Upstream"
                        value={detail?.upstream_repository ?? "Not reported for this target"}
                        mono
                      />
                      <Fact label="Source state" value={detail?.source_status ?? "Not reported"} />
                      <Fact
                        label="Artifact state"
                        value={detail?.artifact_status ?? "Not reported"}
                      />
                      <Fact label="Runtime" value={target.runtime} />
                      <Fact label="Generation" value={target.generation} />
                    </FactGrid>
                    {detailEnvelope ? <Provenance envelope={detailEnvelope} /> : null}
                  </Panel>
                  <Panel
                    title="Capability boundary"
                    description="Selection does not promote model support."
                  >
                    <p className="body-copy">
                      This target remains at the exact runtime and generation stages returned by the
                      catalog. The Operator does not infer support from family, release identity, or
                      a selected row.
                    </p>
                  </Panel>
                </div>
              );
            }}
          </ResourceBoundary>
        </div>
      ) : null}
    </div>
  );
}
