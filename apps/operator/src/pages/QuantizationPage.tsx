/*
 * Owner: apps/operator Quantization workbench.
 * Owns: policy, role-support, reference-evidence panels and capability-gated action explanation.
 * Does not own: qtype selection policy, quantization jobs, dequantization, artifact emission, backend compute, or fake progress.
 * Invariants: no Start action appears without a real endpoint and null release qtype remains visibly unselected.
 * Boundary: qtype policy, reference evidence, emission, and backend arithmetic remain separate gates.
 */
import { Ban, FunctionSquare, Rows3 } from "lucide-react";

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
  { id: "policy", label: "Policy" },
  { id: "role-support", label: "Role support" },
  { id: "reference-evidence", label: "Reference evidence" },
] as const;

/** Renders exact quantization contracts and explicit missing execution owners. */
export function QuantizationPage() {
  const projection = useDomainProjection("quantization");
  const app = useOperatorState();
  const tab = useRouteTab(tabs, "policy");
  const page = pageMetadata.quantization;
  return (
    <div className="page">
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        actions={<RefreshButton refresh={projection.refresh} refreshing={projection.refreshing} />}
      />
      <RouteTabs tabs={tabs} defaultTab="policy" label="Quantization sections" />
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
          if (tab === "policy") {
            const policy = capabilityById(app.capabilities.data, "quantization.policy");
            return (
              <div role="tabpanel" className="detail-layout">
                <Panel
                  title="Release qtype policy"
                  description="Exact selected field from the release decision."
                  actions={<StatusBadge status={policy?.status ?? "loading"} />}
                >
                  <FactGrid>
                    <Fact label="Release" value={decision.release} />
                    <Fact label="Target" value={decision.selected_target_id} mono />
                    <Fact label="Selected qtype" value={decision.release_qtype ?? "Unselected"} />
                    <Fact label="Artifact state" value={decision.artifact_status} />
                    <Fact label="Next dependency" value={decision.next} mono />
                  </FactGrid>
                  {envelope ? <Provenance envelope={envelope} /> : null}
                </Panel>
                <Panel
                  title="Execution action"
                  description="Quantization starts only through a real typed endpoint."
                >
                  <div className="disabled-control">
                    <Ban aria-hidden="true" size={20} />
                    <div>
                      <strong>No quantization execution endpoint is admitted</strong>
                      <p>{policy?.reason}</p>
                    </div>
                    <button type="button" className="button primary" disabled>
                      Start quantization
                    </button>
                  </div>
                </Panel>
              </div>
            );
          }
          if (tab === "role-support") {
            const support = capabilityById(app.capabilities.data, "quantization.role-support");
            return (
              <div role="tabpanel">
                <Panel
                  title="Role / qtype support"
                  description="A matrix is shown only from a stable machine-readable producer."
                >
                  <div className="icon-state">
                    <Rows3 aria-hidden="true" size={22} />
                    <RecoveryState
                      state={{
                        status: support?.status ?? "unavailable",
                        reasonCode: support?.refusalCode ?? "qtype-role-support-missing",
                        message: support?.reason ?? "Role support is unavailable.",
                        observedAt: support?.lastObservedAt ?? data.observedAt,
                        source: support?.source ?? "capability-manifest",
                      }}
                    />
                  </div>
                </Panel>
              </div>
            );
          }
          const reference = capabilityById(
            app.capabilities.data,
            "quantization.reference-evidence",
          );
          return (
            <div role="tabpanel" className="detail-layout">
              <Panel
                title="Reference dequantization"
                description="Numeric comparison evidence below backend compute."
              >
                <div className="icon-state">
                  <FunctionSquare aria-hidden="true" size={22} />
                  <RecoveryState
                    state={{
                      status: reference?.status ?? "unavailable",
                      reasonCode: reference?.refusalCode ?? "reference-dequant-missing",
                      message: reference?.reason ?? "Reference evidence is unavailable.",
                      observedAt: reference?.lastObservedAt ?? data.observedAt,
                      source: reference?.source ?? "capability-manifest",
                    }}
                  />
                </div>
              </Panel>
              <Panel title="Evidence boundary" description="No progress is synthesized.">
                <ul className="plain-list">
                  <li>Policy does not execute quantization.</li>
                  <li>Reference decoding does not prove backend arithmetic.</li>
                  <li>Tensor proof output would not be a complete artifact.</li>
                  <li>No percentage exists until an execution owner reports one.</li>
                </ul>
              </Panel>
            </div>
          );
        }}
      </ResourceBoundary>
    </div>
  );
}
