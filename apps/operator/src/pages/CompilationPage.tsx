/*
 * Owner: apps/operator Compilation workbench.
 * Owns: distinct pipeline, artifact-neutral identity, and contribution-accounting panels.
 * Does not own: IR construction, source rescans, mapping, quantization, payload conversion, or artifact emission.
 * Invariants: pipeline stages use exact release-decision fields and missing identities/accounting remain unavailable.
 * Boundary: complete planning evidence is not byte transformation or a complete artifact.
 */
import { ArrowRight, Boxes, Braces, GitMerge, Workflow } from "lucide-react";

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
import { StatusBadge, machineTone } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { useOperatorState } from "../state/operator-state.tsx";
import { Provenance, RefreshButton, reportOf, useDomainProjection } from "./PageSupport.tsx";

const tabs = [
  { id: "pipeline", label: "Pipeline" },
  { id: "identity", label: "Identity" },
  { id: "accounting", label: "Accounting" },
] as const;

/** Renders compilation planning surfaces without executing or rescanning lower owners. */
export function CompilationPage() {
  const projection = useDomainProjection("compilation");
  const app = useOperatorState();
  const tab = useRouteTab(tabs, "pipeline");
  const page = pageMetadata.compilation;
  return (
    <div className="page">
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        actions={<RefreshButton refresh={projection.refresh} refreshing={projection.refreshing} />}
      />
      <RouteTabs tabs={tabs} defaultTab="pipeline" label="Compilation sections" />
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
          if (tab === "pipeline") {
            const stages = [
              {
                label: "Logical model",
                value: detailEnvelope?.data?.target_id ?? "Unavailable",
                icon: Boxes,
                status: detailEnvelope?.data ? "selected" : "unavailable",
              },
              {
                label: "Transformation IR",
                value: decision.architecture_ir,
                icon: Braces,
                status: decision.architecture_ir,
              },
              {
                label: "Tensor coverage",
                value: decision.tensor_coverage,
                icon: GitMerge,
                status: decision.tensor_coverage,
              },
              {
                label: "Physical lowering",
                value: decision.gguf_mapping,
                icon: Workflow,
                status: decision.gguf_mapping,
              },
            ];
            return (
              <div role="tabpanel">
                <Panel
                  title="Compilation pipeline"
                  description="Artifact-neutral planning followed by format-specific physical lowering."
                >
                  <div className="pipeline-line">
                    {stages.map((stage, index) => {
                      const Icon = stage.icon;
                      return (
                        <article key={stage.label}>
                          <Icon aria-hidden="true" size={19} />
                          <span>{String(index + 1).padStart(2, "0")}</span>
                          <strong>{stage.label}</strong>
                          <code>{stage.value}</code>
                          <StatusBadge value={stage.status} tone={machineTone(stage.status)} />
                          {index < stages.length - 1 ? (
                            <ArrowRight className="pipeline-arrow" aria-hidden="true" size={15} />
                          ) : null}
                        </article>
                      );
                    })}
                  </div>
                  <p className="boundary-copy">
                    The pipeline stops at immutable planning evidence. It does not read payloads,
                    choose a release qtype, quantize tensors, or emit GGUF bytes.
                  </p>
                  {decisionEnvelope ? <Provenance envelope={decisionEnvelope} /> : null}
                </Panel>
              </div>
            );
          }
          if (tab === "identity")
            return (
              <div role="tabpanel" className="detail-layout">
                <Panel
                  title="Logical model identity"
                  description="Independent from a physical container."
                >
                  <FactGrid>
                    <Fact label="Target" value={decision.selected_target_id} mono />
                    <Fact label="Family" value={detailEnvelope?.data?.family ?? "Not reported"} />
                    <Fact label="Class" value={detailEnvelope?.data?.class ?? "Not reported"} />
                    <Fact label="Container" value="Not part of logical identity" />
                  </FactGrid>
                </Panel>
                <Panel
                  title="Transformation identity"
                  description="Stable artifact-neutral identifier."
                >
                  <RecoveryState
                    state={{
                      status: "unavailable",
                      reasonCode: "transformation-identity-contract-missing",
                      message:
                        "The admitted target decision exposes completion but no stable transformation identity field.",
                      observedAt: data.observedAt,
                      source: "producer-registry",
                    }}
                  />
                </Panel>
              </div>
            );
          const coverage = capabilityById(app.capabilities.data, "compilation.tensor-coverage");
          return (
            <div role="tabpanel" className="detail-layout">
              <Panel
                title="Coverage gate"
                description="Typed completion status from the release decision."
              >
                <FactGrid>
                  <Fact label="Tensor coverage" value={decision.tensor_coverage} />
                  <Fact label="Capability" value={coverage?.status ?? "loading"} />
                  <Fact label="Source" value={coverage?.source ?? "capability manifest"} />
                </FactGrid>
              </Panel>
              <Panel
                title="Contribution accounting"
                description="Exact contribution and descriptor totals require a dedicated producer."
              >
                <RecoveryState
                  state={{
                    status: "blocked",
                    reasonCode: "tensor-accounting-contract-missing",
                    message:
                      "Contribution accounting is blocked because the current detailed command re-verifies source headers.",
                    observedAt: data.observedAt,
                    source: "producer-registry",
                  }}
                />
              </Panel>
            </div>
          );
        }}
      </ResourceBoundary>
    </div>
  );
}
