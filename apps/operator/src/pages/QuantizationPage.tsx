/*
 * Owner: apps/operator Quantization surface.
 * Owns: baseline qtype selection state and explicit policy/role/dequant producer refusals.
 * Does not own: quantization jobs, live progress, qtype policy, reference dequantization, or artifact writes.
 * Invariants: null release qtype remains unselected and no worktree/process telemetry is synthesized.
 * Boundary: qtype policy is not quantization and reference proof is not backend arithmetic support.
 */
import { Ban, CircleSlash2, GaugeCircle, ShieldAlert } from "lucide-react";

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

/** Renders exact qtype/refusal evidence and performs no job launch, progress polling, or file mutation. */
export function QuantizationPage() {
  const { response } = useOperatorView();
  const decisionEnvelope = reportEnvelope(response, "releaseDecision");
  const decision = decisionEnvelope?.data;
  const qtype = decision ? (decision.release_qtype ?? "Unselected") : "Unavailable";
  const page = pageMetadata.quantization;

  return (
    <>
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        state={qtype}
        tabs={["Policy", "Role support", "Reference evidence"]}
      />
      <PageContent>
        <MetricStrip
          items={[
            {
              label: "Release qtype",
              value: qtype,
              detail: "Null in target decision",
              tone: decision?.release_qtype ? "cyan" : "amber",
            },
            {
              label: "Artifact",
              value: decision?.artifact_status ?? "Unavailable",
              detail: "No quantization inference",
              tone: statusTone(decision?.artifact_status),
            },
            {
              label: "Runtime",
              value: decision?.runtime ?? "Unavailable",
              detail: "Independent gate",
              tone: statusTone(decision?.runtime),
            },
            {
              label: "Benchmark",
              value: decision?.benchmark ?? "Unavailable",
              detail: "No measurements",
              tone: statusTone(decision?.benchmark),
            },
          ]}
        />

        <div id="policy" className="grid grid-two">
          <Card
            title="Baseline qtype decision"
            eyebrow="Exact JSON field"
            action={<StatusBadge value={qtype} />}
          >
            {decision ? (
              <>
                <FactList>
                  <KeyValue label="Release" value={decision.release} mono />
                  <KeyValue label="Target" value={decision.selected_target_id} mono />
                  <KeyValue
                    label="release_qtype"
                    value={decision.release_qtype === null ? "null" : decision.release_qtype}
                    mono
                  />
                  <KeyValue label="Next row" value={decision.next} mono />
                </FactList>
                {decisionEnvelope ? (
                  <ProducerStamp
                    command={decisionEnvelope.producer.displayCommand}
                    exit={decisionEnvelope.lastExit.code}
                  />
                ) : null}
              </>
            ) : (
              <EnvelopeFailure envelope={decisionEnvelope} />
            )}
          </Card>
          <Card title="Qtype policy producer" eyebrow="CLI contract refusal">
            <MissingEvidence id="qtypePolicyJson" />
          </Card>
        </div>

        <div id="role-support" className="grid grid-two">
          <Card title="Role / qtype combinations" eyebrow="Support matrix">
            <MissingEvidence id="qtypeRoleSupportJson" />
          </Card>
          <Card
            title="No live progress surface"
            eyebrow="Parallel-lane isolation"
            action={<StatusBadge value="enforced" tone="cyan" />}
          >
            <div className="no-progress-state">
              <GaugeCircle aria-hidden="true" size={28} />
              <div>
                <strong>Progress is intentionally absent</strong>
                <p>
                  The operator does not inspect the primary worktree, running quantizer, output
                  files, or process state.
                </p>
              </div>
            </div>
          </Card>
        </div>

        <div id="reference-evidence" className="grid grid-three">
          <Card title="Reference dequantization" eyebrow="Evidence producer">
            <MissingEvidence id="referenceDequantJson" compact />
          </Card>
          <Card title="Write control" eyebrow="Safety">
            <div className="compact-boundary">
              <Ban aria-hidden="true" size={18} />
              <span>No GGUF emission or registry publication</span>
            </div>
          </Card>
          <Card title="Execution claim" eyebrow="Safety">
            <div className="compact-boundary">
              <CircleSlash2 aria-hidden="true" size={18} />
              <span>{decision?.runtime ?? "Unsupported evidence unavailable"}</span>
            </div>
          </Card>
        </div>
        <BoundaryNote>
          <ShieldAlert aria-hidden="true" size={14} /> Qtype selection, quantization execution,
          reference decoding, artifact emission, and backend compute support remain separate
          evidence gates.
        </BoundaryNote>
      </PageContent>
    </>
  );
}
