/*
 * Owner: apps/operator System Health workbench.
 * Owns: full YVEX topology, binary resolution trace, adapter/version/bind security, host facts, and recovery links.
 * Does not own: binary resolution policy, backend discovery, comparison tests, authentication secrets, or health mutation.
 * Invariants: browser, adapter, active YVEX binary, backend, runtime, and generation remain distinct nodes.
 * Boundary: adapter/process health and host architecture are not backend or runtime evidence.
 */
import { AlertTriangle, ArrowDown, LockKeyhole, Network } from "lucide-react";

import type { BinaryCandidate } from "../../shared/contracts.ts";
import { operatorApi } from "../api.ts";
import {
  Fact,
  FactGrid,
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
import { DataTable, formatBytes } from "./PageSupport.tsx";

const tabs = [
  { id: "topology", label: "Topology" },
  { id: "binary", label: "Binary resolution" },
  { id: "security", label: "Host & security" },
] as const;

/** Renders one bounded candidate slice while retaining the original resolver order. */
function CandidateTable({
  candidates,
  offset = 0,
  label,
}: {
  candidates: readonly BinaryCandidate[];
  offset?: number;
  label: string;
}) {
  return (
    <DataTable label={label}>
      <thead>
        <tr>
          <th>Order / source</th>
          <th>Safe path</th>
          <th>File</th>
          <th>Executable</th>
          <th>Identity</th>
          <th>Version</th>
          <th>Disposition</th>
        </tr>
      </thead>
      <tbody>
        {candidates.map((candidate, index) => (
          <tr key={candidate.id}>
            <td>
              <strong>{offset + index + 1}</strong>
              <small>{candidate.source}</small>
            </td>
            <td>
              <code>{candidate.displayPath}</code>
            </td>
            <td>
              {candidate.exists && candidate.regularFile
                ? "regular"
                : candidate.exists
                  ? "invalid"
                  : "missing"}
            </td>
            <td>{candidate.executable ? "yes" : "no"}</td>
            <td>
              <StatusBadge
                status={
                  candidate.identityStatus === "ready"
                    ? "ready"
                    : candidate.identityStatus === "not-probed"
                      ? "empty"
                      : candidate.identityStatus === "incompatible"
                        ? "blocked"
                        : "failed"
                }
                value={candidate.identityStatus}
              />
            </td>
            <td>{candidate.version ?? "—"}</td>
            <td>{candidate.rejectionReason ?? "selected"}</td>
          </tr>
        ))}
      </tbody>
    </DataTable>
  );
}

/** Renders each primary connectivity and YVEX execution layer from typed health/resolution data. */
export function SystemHealthPage() {
  const app = useOperatorState();
  const resolution = useApiResource("binary-resolution", operatorApi.binaryResolution);
  const tab = useRouteTab(tabs, "topology");
  const page = pageMetadata.environment;
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
            onClick={() =>
              void operatorApi.reload().then(() => {
                app.refreshAll();
                resolution.refresh();
              })
            }
          >
            Reload configuration
          </button>
        }
      />
      <RouteTabs tabs={tabs} defaultTab="topology" label="System health sections" />
      {tab === "topology" ? (
        <div role="tabpanel">
          <ResourceBoundary resource={app.health}>
            {(health) => {
              const operatorNodes = health.topology.filter((node) => node.branch === "operator");
              const yvexNodes = health.topology.filter((node) => node.branch === "yvex");
              const branch = (nodes: typeof health.topology, layout: "horizontal" | "dense") => (
                <div className={`topology-branch ${layout}`}>
                  {nodes.map((node, index) => (
                    <div key={node.id}>
                      <article>
                        <div>
                          <span className="topology-index">
                            {String(index + 1).padStart(2, "0")}
                          </span>
                          <strong>{node.label}</strong>
                          <code>{node.reasonCode}</code>
                        </div>
                        <StatusBadge status={node.status} />
                        <p>{node.message}</p>
                      </article>
                      {layout === "horizontal" && index < nodes.length - 1 ? (
                        <ArrowDown aria-hidden="true" size={15} />
                      ) : null}
                    </div>
                  ))}
                </div>
              );
              return (
                <>
                  <Panel
                    title="Control-plane topology"
                    description="A healthy adapter does not collapse downstream YVEX runtime states."
                  >
                    <div className="topology-root">
                      <section>
                        <h3>Operator control plane</h3>
                        {branch(operatorNodes, "horizontal")}
                      </section>
                      <section>
                        <h3>YVEX build and execution topology</h3>
                        {branch(yvexNodes, "dense")}
                      </section>
                    </div>
                  </Panel>
                </>
              );
            }}
          </ResourceBoundary>
        </div>
      ) : null}
      {tab === "binary" ? (
        <div role="tabpanel">
          <ResourceBoundary resource={resolution}>
            {(data) => (
              <Panel
                title="Deterministic resolution trace"
                description="Persisted setting → environment → repository candidate → known build outputs → controlled PATH."
              >
                <div className="resolution-summary">
                  <StatusBadge status={data.availability.status} />
                  <strong>{data.selectedLabel ?? "No compatible binary selected"}</strong>
                  <p>{data.availability.message}</p>
                </div>
                <CandidateTable
                  candidates={data.candidates.slice(0, 10)}
                  label="Primary YVEX binary candidates"
                />
                {data.candidates.length > 10 ? (
                  <details className="resolution-overflow">
                    <summary>
                      Inspect {data.candidates.length - 10} additional controlled PATH candidates
                    </summary>
                    <CandidateTable
                      candidates={data.candidates.slice(10)}
                      offset={10}
                      label="Additional controlled PATH candidates"
                    />
                  </details>
                ) : null}
              </Panel>
            )}
          </ResourceBoundary>
        </div>
      ) : null}
      {tab === "security" ? (
        <div role="tabpanel">
          <ResourceBoundary resource={app.health}>
            {(health) => (
              <div className="detail-layout">
                <Panel
                  title="Adapter exposure"
                  description="Loopback is the default; remote mode fails closed."
                >
                  <FactGrid>
                    <Fact label="Bind address" value={health.adapter.bindAddress} mono />
                    <Fact label="Mode" value={health.adapter.bindMode} />
                    <Fact
                      label="Remote exposure"
                      value={health.adapter.remoteExposure ? "enabled" : "disabled"}
                    />
                    <Fact
                      label="Authentication required"
                      value={health.adapter.authenticationRequired ? "yes" : "no"}
                    />
                    <Fact
                      label="Authentication configured"
                      value={health.adapter.authenticationConfigured ? "yes" : "no"}
                    />
                    <Fact
                      label="API / adapter"
                      value={`${health.adapter.apiVersion} / ${health.adapter.version}`}
                    />
                  </FactGrid>
                  {health.adapter.remoteExposure ? (
                    <div className="security-warning">
                      <AlertTriangle aria-hidden="true" size={18} />
                      Remote exposure is active. Origin allowlisting and bearer authentication are
                      enforced.
                    </div>
                  ) : (
                    <div className="security-ok">
                      <LockKeyhole aria-hidden="true" size={18} />
                      The adapter is local-only.
                    </div>
                  )}
                </Panel>
                <Panel title="Host topology" description="Local process facts only; no CUDA claim.">
                  <FactGrid>
                    <Fact label="Platform" value={health.host.platform} />
                    <Fact label="Architecture" value={health.host.architecture} />
                    <Fact label="Logical processors" value={health.host.logicalProcessors} />
                    <Fact
                      label="Physical memory"
                      value={formatBytes(health.host.totalMemoryBytes)}
                    />
                    <Fact label="Node runtime" value={health.host.nodeRuntime} />
                  </FactGrid>
                  <div className="boundary-copy">
                    <Network aria-hidden="true" size={15} /> Host architecture is never presented as
                    CPU/CUDA backend evidence.
                  </div>
                </Panel>
              </div>
            )}
          </ResourceBoundary>
        </div>
      ) : null}
    </div>
  );
}
