/*
 * Owner: apps/operator Compilation surface.
 * Owns: safe logical model, architecture IR, tensor coverage, and lowering-gate projection.
 * Does not own: transformation construction, source rescans, contribution accounting, quantization, or artifact emission.
 * Invariants: completion fields are copied from the release decision and missing identities remain unavailable.
 * Boundary: logical/physical planning evidence does not imply payload conversion or a complete artifact.
 */
import { ArrowRight, Boxes, Braces, GitMerge, Workflow } from "lucide-react";

import {
  BoundaryNote,
  Card,
  KeyValue,
  MetricStrip,
  PageContent,
  PageHeader,
} from "../components/Primitives.tsx";
import { EnvelopeFailure, StatusBadge, statusTone } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { reportEnvelope, useOperatorView } from "../view-context.tsx";
import { FactList, MissingEvidence, ProducerStamp } from "./PageSupport.tsx";

/** Renders existing compilation gates without rebuilding IR, rescanning sources, or emitting artifacts. */
export function CompilationPage() {
  const { response } = useOperatorView();
  const decisionEnvelope = reportEnvelope(response, "releaseDecision");
  const detailEnvelope = reportEnvelope(response, "targetDetail");
  const decision = decisionEnvelope?.data;
  const detail = detailEnvelope?.data;
  const page = pageMetadata.compilation;
  const gates = [
    { label: "Logical model", value: detail?.target_id ?? "Unavailable", icon: Boxes },
    { label: "Transformation IR", value: decision?.architecture_ir ?? "Unavailable", icon: Braces },
    { label: "Tensor coverage", value: decision?.tensor_coverage ?? "Unavailable", icon: GitMerge },
    { label: "GGUF lowering", value: decision?.gguf_mapping ?? "Unavailable", icon: Workflow },
  ];

  return (
    <>
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        state={decision?.status ?? "Unavailable"}
        tabs={["Pipeline", "Identity", "Accounting"]}
      />
      <PageContent>
        <MetricStrip
          items={[
            {
              label: "Logical target",
              value: decision?.selected_target_id ?? "Unavailable",
              detail: detail?.class ?? "No validated class",
              tone: decision ? "cyan" : "amber",
            },
            {
              label: "Architecture IR",
              value: decision?.architecture_ir ?? "Unavailable",
              detail: "Artifact-neutral",
              tone: statusTone(decision?.architecture_ir),
            },
            {
              label: "Tensor coverage",
              value: decision?.tensor_coverage ?? "Unavailable",
              detail: "Decision gate",
              tone: statusTone(decision?.tensor_coverage),
            },
            {
              label: "Physical lowering",
              value: decision?.gguf_mapping ?? "Unavailable",
              detail: "GGUF mapping",
              tone: statusTone(decision?.gguf_mapping),
            },
          ]}
        />

        <Card id="pipeline" title="Compilation gate pipeline" eyebrow="Typed decision evidence">
          {decision ? (
            <>
              <div className="compilation-pipeline">
                {gates.map((gate, index) => {
                  const Icon = gate.icon;
                  return (
                    <div className="pipeline-gate" key={gate.label}>
                      <Icon aria-hidden="true" size={19} />
                      <span className="micro-label">{gate.label}</span>
                      <strong>{gate.value}</strong>
                      <StatusBadge value={index === 0 ? "selected" : gate.value} />
                      {index < gates.length - 1 ? (
                        <ArrowRight aria-hidden="true" className="pipeline-arrow" size={15} />
                      ) : null}
                    </div>
                  );
                })}
              </div>
              <BoundaryNote>
                These gates establish compilation evidence only. Payload transformation,
                quantization, and GGUF byte emission remain separate owners.
              </BoundaryNote>
            </>
          ) : (
            <EnvelopeFailure envelope={decisionEnvelope} />
          )}
        </Card>

        <div id="identity" className="grid grid-two">
          <Card
            title="Logical model"
            eyebrow="Artifact-neutral identity"
            action={detail ? <StatusBadge value={detail.class} tone="cyan" /> : null}
          >
            {detail ? (
              <FactList>
                <KeyValue label="Target" value={detail.target_id} mono />
                <KeyValue label="Family" value={detail.family} />
                <KeyValue
                  label="Release selected"
                  value={detail.release_selected ? "true" : "false"}
                  mono
                />
                <KeyValue label="Container" value="Not part of logical identity" />
              </FactList>
            ) : (
              <EnvelopeFailure envelope={detailEnvelope} />
            )}
          </Card>
          <Card title="Transformation identity" eyebrow="Missing safe field">
            <MissingEvidence id="transformationIdentityJson" />
          </Card>
        </div>

        <div id="accounting" className="grid grid-two">
          <Card title="Contribution accounting" eyebrow="No source rescan">
            <MissingEvidence id="tensorAccountingJson" />
          </Card>
          <Card title="Decision provenance" eyebrow="Audited command">
            {decisionEnvelope ? (
              <>
                <FactList>
                  <KeyValue label="Status" value={decisionEnvelope.data?.status ?? "Unavailable"} />
                  <KeyValue label="Last exit" value={decisionEnvelope.lastExit.code ?? "—"} mono />
                  <KeyValue label="Cache" value={decisionEnvelope.producer.cachePolicy} />
                  <KeyValue label="Observed" value={decisionEnvelope.observedAt} mono />
                </FactList>
                <ProducerStamp
                  command={decisionEnvelope.producer.displayCommand}
                  exit={decisionEnvelope.lastExit.code}
                />
              </>
            ) : (
              <EnvelopeFailure envelope={decisionEnvelope} />
            )}
          </Card>
        </div>
      </PageContent>
    </>
  );
}
