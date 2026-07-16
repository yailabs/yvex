/*
 * Owner: apps/operator Sources workbench.
 * Owns: distinct identity, trust, and accounting panels over typed release/source capabilities.
 * Does not own: source scanning, manifests, model roots, payload reads, downloads, or verification policy.
 * Invariants: missing accounting remains explicitly unavailable and no browser value becomes a source path.
 * Boundary: source identity and verification do not imply payload trust, artifact support, or runtime readiness.
 */
import { Fingerprint, ShieldCheck, Sigma } from "lucide-react";

import { capabilityById } from "../../shared/contracts.ts";
import {
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
import { Provenance, RefreshButton, reportOf, useDomainProjection } from "./PageSupport.tsx";

const tabs = [
  { id: "identity", label: "Identity" },
  { id: "trust", label: "Trust" },
  { id: "accounting", label: "Accounting" },
] as const;

/** Renders each source concern only in its URL-selected panel. */
export function SourcesPage() {
  const projection = useDomainProjection("sources");
  const app = useOperatorState();
  const tab = useRouteTab(tabs, "identity");
  const page = pageMetadata.sources;
  return (
    <div className="page">
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        actions={<RefreshButton refresh={projection.refresh} refreshing={projection.refreshing} />}
      />
      <RouteTabs tabs={tabs} defaultTab="identity" label="Source sections" />
      <ResourceBoundary resource={projection}>
        {(data) => {
          const decisionEnvelope = reportOf(data, "release-decision");
          const detailEnvelope = reportOf(data, "target-detail");
          const decision = decisionEnvelope?.data;
          if (!decision)
            return (
              <RecoveryState
                state={decisionEnvelope?.availability ?? data.availability}
                retry={projection.refresh}
              />
            );
          if (tab === "identity")
            return (
              <div role="tabpanel" className="detail-layout">
                <Panel
                  title="Release source identity"
                  description="Machine-readable fields from the release target decision."
                  actions={<Fingerprint aria-hidden="true" size={19} />}
                >
                  <FactGrid>
                    <Fact label="Release" value={decision.release} mono />
                    <Fact label="Target" value={decision.selected_target_id} mono />
                    <Fact label="Repository" value={decision.upstream_repository} mono />
                    <Fact label="Family" value={detailEnvelope?.data?.family ?? "Not reported"} />
                    <Fact
                      label="Source state"
                      value={detailEnvelope?.data?.source_status ?? "Not reported"}
                    />
                    <Fact label="Revision" value="Not exposed by an admitted snapshot producer" />
                  </FactGrid>
                  {decisionEnvelope ? <Provenance envelope={decisionEnvelope} /> : null}
                </Panel>
                <Panel
                  title="Browser isolation"
                  description="Source roots and model files remain below the adapter boundary."
                >
                  <ul className="plain-list">
                    <li>No browser-selected model paths</li>
                    <li>No source header rescan from this panel</li>
                    <li>No tensor payload reads</li>
                    <li>Only schema-validated report fields cross the API</li>
                  </ul>
                </Panel>
              </div>
            );
          if (tab === "trust") {
            const identity = capabilityById(app.capabilities.data, "source.identity");
            const trust = capabilityById(app.capabilities.data, "source.trust");
            return (
              <div role="tabpanel">
                <Panel
                  title="Source trust chain"
                  description="Identity and verification remain separately inspectable."
                >
                  <div className="stage-list">
                    <article>
                      <Fingerprint aria-hidden="true" size={18} />
                      <div>
                        <strong>Repository identity</strong>
                        <p>{identity?.reason}</p>
                      </div>
                      <StatusBadge status={identity?.status ?? "loading"} />
                    </article>
                    <article>
                      <ShieldCheck aria-hidden="true" size={18} />
                      <div>
                        <strong>Verification decision</strong>
                        <p>{trust?.reason}</p>
                      </div>
                      <StatusBadge status={trust?.status ?? "loading"} />
                    </article>
                    <article>
                      <Sigma aria-hidden="true" size={18} />
                      <div>
                        <strong>Payload trust snapshot</strong>
                        <p>No non-scanning snapshot producer is currently admitted to Operator.</p>
                      </div>
                      <StatusBadge status="blocked" />
                    </article>
                  </div>
                </Panel>
              </div>
            );
          }
          const accounting = capabilityById(app.capabilities.data, "source.accounting");
          return (
            <div role="tabpanel">
              <Panel
                title="Descriptor and shard accounting"
                description="Accounting appears only when a stable non-scanning producer exists."
              >
                <RecoveryState
                  state={{
                    status: accounting?.status ?? "unavailable",
                    reasonCode: accounting?.refusalCode ?? "source-accounting-contract-missing",
                    message: accounting?.reason ?? "Source accounting is unavailable.",
                    observedAt: accounting?.lastObservedAt ?? data.observedAt,
                    source: accounting?.source ?? "capability-manifest",
                  }}
                />
                <p className="boundary-copy">
                  No descriptor count, shard count, or byte total is shown because the current
                  compatibility registry cannot produce those facts without reopening source
                  headers.
                </p>
              </Panel>
            </div>
          );
        }}
      </ResourceBoundary>
    </div>
  );
}
