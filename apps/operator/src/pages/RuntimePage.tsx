/*
 * Owner: apps/operator Runtime surface.
 * Owns: truthful runtime, generation, evaluation, and benchmark boundary projection.
 * Does not own: diagnostic execution, backend probes, generation, evaluation, benchmarks, or controls.
 * Invariants: downstream stages use exact target-decision values and expose no launch action.
 * Boundary: diagnostic CLI surfaces are not full-model runtime support.
 */
import { Activity, Ban, BarChart3, Cpu, PlayCircle } from "lucide-react";

import {
  BoundaryNote,
  Card,
  MetricStrip,
  PageContent,
  PageHeader,
} from "../components/Primitives.tsx";
import { EnvelopeFailure, StatusBadge, statusTone } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { reportEnvelope, useOperatorView } from "../view-context.tsx";
import { MissingEvidence, ProducerStamp } from "./PageSupport.tsx";

/** Projects downstream decision fields and exposes no backend, runtime, generation, or prompt side effect. */
export function RuntimePage() {
  const { response } = useOperatorView();
  const decisionEnvelope = reportEnvelope(response, "releaseDecision");
  const decision = decisionEnvelope?.data;
  const page = pageMetadata.runtime;
  const stages = [
    { label: "Runtime binding", value: decision?.runtime ?? "Unavailable", icon: Cpu },
    { label: "Generation", value: decision?.generation ?? "Unavailable", icon: PlayCircle },
    { label: "Evaluation", value: decision?.evaluation ?? "Unavailable", icon: Activity },
    { label: "Benchmark", value: decision?.benchmark ?? "Unavailable", icon: BarChart3 },
  ];

  return (
    <>
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        state={decision?.runtime ?? "Unavailable"}
        tabs={["Stages", "Backend", "Controls"]}
      />
      <PageContent>
        <MetricStrip
          items={stages.map((stage) => ({
            label: stage.label,
            value: stage.value,
            detail: "Target decision",
            tone: statusTone(stage.value),
          }))}
        />
        <Card id="stages" title="Downstream execution stages" eyebrow="Exact baseline states">
          {decision ? (
            <div className="runtime-stages">
              {stages.map((stage) => {
                const Icon = stage.icon;
                return (
                  <article key={stage.label}>
                    <Icon aria-hidden="true" size={20} />
                    <span className="micro-label">{stage.label}</span>
                    <strong>{stage.value}</strong>
                    <StatusBadge value={stage.value} />
                  </article>
                );
              })}
            </div>
          ) : (
            <EnvelopeFailure envelope={decisionEnvelope} />
          )}
          {decisionEnvelope ? (
            <ProducerStamp
              command={decisionEnvelope.producer.displayCommand}
              exit={decisionEnvelope.lastExit.code}
            />
          ) : null}
        </Card>
        <div className="grid grid-two">
          <Card id="backend" title="Backend capability" eyebrow="Machine-readable gap">
            <MissingEvidence id="backendCapabilityJson" />
          </Card>
          <Card
            id="controls"
            title="Execution controls"
            eyebrow="Read-only boundary"
            action={<StatusBadge value="none" tone="cyan" />}
          >
            <div className="no-control-state">
              <Ban aria-hidden="true" size={25} />
              <div>
                <strong>No generation or runtime launch path</strong>
                <p>
                  The API exposes no prompt, model path, backend selector, arbitrary command, or
                  execution endpoint.
                </p>
              </div>
            </div>
          </Card>
        </div>
        <BoundaryNote>
          Runtime and generation remain unsupported on the release decision. Diagnostic primitives
          and selected-slice proofs are not promoted here.
        </BoundaryNote>
      </PageContent>
    </>
  );
}
