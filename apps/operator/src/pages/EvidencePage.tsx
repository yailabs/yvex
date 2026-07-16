/*
 * Owner: apps/operator Evidence workbench.
 * Owns: producer registry, explicit safe runs, run/job history, cache state, command provenance, missing contracts, and detail inspection.
 * Does not own: producer definitions, arbitrary command entry, native facts, validation execution, or capability promotion.
 * Invariants: labels lead the UI while stable IDs remain secondary provenance; run actions submit only producer IDs.
 * Boundary: producer availability and successful adaptation are not runtime readiness.
 */
import { Check, Clipboard, Play, TerminalSquare } from "lucide-react";
import { useState } from "react";

import type { ProducerDescriptor, ProducerRun } from "../../shared/contracts.ts";
import { operatorApi } from "../api.ts";
import { capabilityDisplayLabel } from "../capability-labels.ts";
import {
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
import { DataTable, EmptyState } from "./PageSupport.tsx";

const tabs = [
  { id: "producers", label: "Producers" },
  { id: "runs", label: "Recent runs" },
  { id: "cache", label: "Cache & gaps" },
] as const;

/** Renders one local producer detail inspector with copy and controlled run actions. */
function ProducerInspector({
  producer,
  latest,
  onRun,
}: {
  producer: ProducerDescriptor | null;
  latest: ProducerRun | null;
  onRun: (id: string) => void;
}) {
  const [copied, setCopied] = useState(false);
  if (!producer)
    return (
      <aside className="detail-inspector">
        <EmptyState
          title="Select a producer"
          detail="Inspect its command provenance, cache policy, dependencies, and latest structured result."
        />
      </aside>
    );
  return (
    <aside className="detail-inspector producer-detail">
      <header>
        <span>{producer.domain}</span>
        <h2>{producer.displayName}</h2>
        <StatusBadge status={producer.availability.status} />
      </header>
      <p>{producer.description}</p>
      <dl>
        <div>
          <dt>Stable ID</dt>
          <dd>
            <code>{producer.id}</code>
          </dd>
        </div>
        <div>
          <dt>Cache</dt>
          <dd>
            {producer.cachePolicy}
            {producer.ttlMs === null ? "" : ` · ${producer.ttlMs} ms`}
          </dd>
        </div>
        <div>
          <dt>Timeout</dt>
          <dd>{producer.timeoutMs} ms</dd>
        </div>
        <div>
          <dt>Output ceiling</dt>
          <dd>{producer.maxOutputBytes.toLocaleString()} bytes</dd>
        </div>
        <div>
          <dt>Latest exit</dt>
          <dd>{latest?.envelope?.exit.state ?? producer.lastExit.state}</dd>
        </div>
      </dl>
      <div className="command-box">
        <TerminalSquare aria-hidden="true" size={15} />
        <code>{producer.displayCommand}</code>
        <button
          type="button"
          aria-label={`Copy ${producer.displayName} command`}
          onClick={() =>
            void navigator.clipboard.writeText(producer.displayCommand).then(() => setCopied(true))
          }
        >
          {copied ? (
            <Check aria-hidden="true" size={15} />
          ) : (
            <Clipboard aria-hidden="true" size={15} />
          )}
        </button>
      </div>
      {latest?.envelope?.refusal ? (
        <div className="structured-refusal">
          <strong>{latest.envelope.refusal.code}</strong>
          <p>{latest.envelope.refusal.message}</p>
        </div>
      ) : null}
      <button
        type="button"
        className="button primary"
        disabled={producer.availability.status !== "ready"}
        onClick={() => onRun(producer.id)}
      >
        <Play aria-hidden="true" size={14} /> Run safe producer
      </button>
    </aside>
  );
}

/** Operates the allowlisted producer evidence lane and exposes bounded recent history. */
export function EvidencePage() {
  const producers = useApiResource("producer-registry", operatorApi.producers);
  const app = useOperatorState();
  const tab = useRouteTab(tabs, "producers");
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [running, setRunning] = useState<string | null>(null);
  const page = pageMetadata.evidence;
  const evidenceJobs = (app.jobs.data?.jobs ?? []).filter(
    (job) => job.type !== "reference-comparison" && job.type !== "comparison-test",
  );
  const run = async (id: string): Promise<void> => {
    setRunning(id);
    try {
      await operatorApi.runProducer(id);
      app.refreshAll();
    } finally {
      setRunning(null);
    }
  };
  return (
    <div className="page">
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        actions={
          <button
            type="button"
            className="button secondary"
            onClick={() => {
              producers.refresh();
              app.refreshAll();
            }}
          >
            Refresh evidence
          </button>
        }
      />
      <RouteTabs tabs={tabs} defaultTab="producers" label="Evidence sections" />
      {tab === "producers" ? (
        <div role="tabpanel">
          <ResourceBoundary resource={producers}>
            {(response) => {
              const selected =
                response.producers.find((producer) => producer.id === selectedId) ?? null;
              const latest =
                app.runs.data?.runs.find((item) => item.producerId === selected?.id) ?? null;
              return (
                <div className="inventory-layout">
                  <Panel
                    title="Producer registry"
                    description="Only adapter-owned machine-readable commands can run."
                  >
                    {response.producers.length ? (
                      <DataTable label="Allowlisted producers">
                        <thead>
                          <tr>
                            <th>Producer</th>
                            <th>Domain</th>
                            <th>Availability</th>
                            <th>Last run</th>
                            <th>Exit</th>
                            <th>Action</th>
                          </tr>
                        </thead>
                        <tbody>
                          {response.producers.map((producer) => (
                            <tr
                              className={selected?.id === producer.id ? "selected-row" : ""}
                              key={producer.id}
                            >
                              <td>
                                <button
                                  type="button"
                                  className="table-link"
                                  onClick={() => setSelectedId(producer.id)}
                                >
                                  {producer.displayName}
                                </button>
                                <small>{producer.description}</small>
                              </td>
                              <td>{producer.domain}</td>
                              <td>
                                <StatusBadge status={producer.availability.status} />
                              </td>
                              <td>
                                {producer.lastExecutionAt
                                  ? new Date(producer.lastExecutionAt).toLocaleTimeString()
                                  : "Never"}
                              </td>
                              <td>{producer.lastExit.state}</td>
                              <td>
                                <button
                                  type="button"
                                  className="button secondary"
                                  disabled={
                                    producer.availability.status !== "ready" || running !== null
                                  }
                                  onClick={() => void run(producer.id)}
                                >
                                  {running === producer.id ? "Running…" : "Run"}
                                </button>
                              </td>
                            </tr>
                          ))}
                        </tbody>
                      </DataTable>
                    ) : (
                      <EmptyState detail="No producer definition is registered." />
                    )}
                  </Panel>
                  <ProducerInspector
                    producer={selected}
                    latest={latest}
                    onRun={(id) => void run(id)}
                  />
                </div>
              );
            }}
          </ResourceBoundary>
        </div>
      ) : null}
      {tab === "runs" ? (
        <div role="tabpanel" className="split-grid">
          <Panel title="Producer runs" description="Explicit runs with job and result references.">
            {app.runs.data?.runs.length ? (
              <DataTable label="Recent producer runs">
                <thead>
                  <tr>
                    <th>Producer</th>
                    <th>Started</th>
                    <th>Duration</th>
                    <th>Availability</th>
                    <th>Job</th>
                  </tr>
                </thead>
                <tbody>
                  {app.runs.data.runs.map((record) => (
                    <tr key={record.runId}>
                      <td>
                        {producers.data?.producers.find((item) => item.id === record.producerId)
                          ?.displayName ?? record.producerId}
                      </td>
                      <td>{new Date(record.startedAt).toLocaleString()}</td>
                      <td>{record.envelope?.durationMs ?? "—"} ms</td>
                      <td>
                        <StatusBadge status={record.envelope?.availability.status ?? "loading"} />
                      </td>
                      <td>
                        <code>{record.jobId.slice(0, 8)}</code>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </DataTable>
            ) : (
              <EmptyState detail="No explicit producer run has been requested." />
            )}
          </Panel>
          <Panel
            title="Jobs"
            description="Queued, running, cancellation, and terminal control-plane state."
          >
            {evidenceJobs.length ? (
              <div className="job-list">
                {evidenceJobs.map((job) => (
                  <article key={job.id}>
                    <div>
                      <strong>{job.type}</strong>
                      <span>
                        {job.executionOwner} · {job.phase ?? "—"}
                      </span>
                    </div>
                    <StatusBadge
                      value={job.state}
                      status={
                        ["queued", "starting", "running", "cancelling"].includes(job.state)
                          ? "loading"
                          : job.state === "completed"
                            ? "ready"
                            : job.state === "failed"
                              ? "failed"
                              : "degraded"
                      }
                    />
                  </article>
                ))}
              </div>
            ) : (
              <EmptyState detail="No bounded Operator job exists." />
            )}
          </Panel>
        </div>
      ) : null}
      {tab === "cache" ? (
        <div role="tabpanel" className="detail-layout">
          <Panel
            title="Cache observations"
            description="Success caching never conceals missing binaries or malformed producer output."
          >
            <ul className="plain-list">
              <li>
                Immutable: target catalog, release decision, and target detail for this adapter
                process.
              </li>
              <li>Short TTL: artifact inventory and binary resolution.</li>
              <li>
                Never success-cache: timeout, malformed JSON, schema mismatch, refusal, or missing
                binary.
              </li>
              <li>
                Settings mutations invalidate affected resolver, provider, and producer
                observations.
              </li>
            </ul>
            <button
              type="button"
              className="button secondary"
              onClick={() => void operatorApi.clearCache().then(app.refreshAll)}
            >
              Clear safe Operator caches
            </button>
          </Panel>
          <Panel
            title="Missing machine-readable contracts"
            description="Grouped by stable capability rather than repeated empty cards."
          >
            <div className="missing-contracts">
              {(app.workspace.data?.capabilities ?? app.capabilities.data?.capabilities ?? [])
                .filter(
                  (item) =>
                    ["unavailable", "unsupported"].includes(item.status) &&
                    !item.id.startsWith("runtime.") &&
                    !item.id.startsWith("generation."),
                )
                .map((item) => (
                  <article key={item.id}>
                    <div>
                      <strong>{capabilityDisplayLabel(item)}</strong>
                      <p>{item.reason}</p>
                      <code>{item.id}</code>
                    </div>
                    <StatusBadge status={item.status} />
                  </article>
                ))}
            </div>
          </Panel>
        </div>
      ) : null}
    </div>
  );
}
