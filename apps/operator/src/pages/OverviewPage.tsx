/*
 * Owner: apps/operator Overview surface.
 * Owns: lifecycle projection across source, logical model, IR, lowering, artifact, runtime, and evidence stages.
 * Does not own: stage decisions, execution, model files, quantization progress, or capability promotion.
 * Invariants: every lifecycle value comes from the target decision/detail envelopes or is explicitly unavailable.
 * Boundary: complete mapping evidence is not artifact production or runtime generation.
 */
import {
  ArrowRight,
  Box,
  Braces,
  Database,
  FileOutput,
  Gauge,
  Network,
  PlayCircle,
} from "lucide-react";

import { pageMetadata } from "../navigation.ts";
import { reportEnvelope, useOperatorView } from "../view-context.tsx";
import {
  BoundaryNote,
  Card,
  KeyValue,
  MetricStrip,
  PageContent,
  PageHeader,
} from "../components/Primitives.tsx";
import { EnvelopeFailure, StatusBadge, statusTone } from "../components/Status.tsx";
import { FactList, MissingEvidence, ProducerStamp } from "./PageSupport.tsx";

const lifecycleIcons = [Database, Box, Braces, Network, FileOutput, Gauge, PlayCircle] as const;

/** Projects borrowed report envelopes into lifecycle UI without IO, mutation, or inferred success state. */
export function OverviewPage() {
  const { response } = useOperatorView();
  const decisionEnvelope = reportEnvelope(response, "releaseDecision");
  const detailEnvelope = reportEnvelope(response, "targetDetail");
  const decision = decisionEnvelope?.data;
  const detail = detailEnvelope?.data;
  const page = pageMetadata.overview;
  const state =
    decisionEnvelope?.availability === "available"
      ? (decision?.status ?? "Available")
      : "Unavailable";
  const lifecycle = [
    {
      label: "Source",
      value: decision?.source_verification ?? "Unavailable",
      detail: "release verification",
    },
    {
      label: "Logical model",
      value: detail?.release_selected ? "selected" : "Unavailable",
      detail: detail?.target_id ?? "no target fact",
    },
    {
      label: "Transformation IR",
      value: decision?.architecture_ir ?? "Unavailable",
      detail: "artifact-neutral",
    },
    {
      label: "Physical lowering",
      value: decision?.gguf_mapping ?? "Unavailable",
      detail: "GGUF mapping gate",
    },
    {
      label: "Artifact",
      value: decision?.artifact_status ?? "Unavailable",
      detail: "container state",
    },
    {
      label: "Runtime binding",
      value: decision?.runtime ?? "Unavailable",
      detail: "execution boundary",
    },
    {
      label: "Execution evidence",
      value: decision?.benchmark ?? "Unavailable",
      detail: "benchmark state",
    },
  ];

  return (
    <>
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        state={state}
        tabs={["Lifecycle", "Boundaries", "Provenance"]}
      />
      <PageContent>
        <MetricStrip
          items={[
            {
              label: "Release target",
              value: decision?.selected_target_id ?? "Unavailable",
              detail: decision?.release ?? "No validated decision",
              tone: decision ? "cyan" : "amber",
            },
            {
              label: "Source verification",
              value: decision?.source_verification ?? "Unavailable",
              detail: "Target decision",
              tone: statusTone(decision?.source_verification),
            },
            {
              label: "Transformation IR",
              value: decision?.architecture_ir ?? "Unavailable",
              detail: "Artifact-neutral",
              tone: statusTone(decision?.architecture_ir),
            },
            {
              label: "Artifact state",
              value: decision?.artifact_status ?? "Unavailable",
              detail: "No readiness inference",
              tone: statusTone(decision?.artifact_status),
            },
          ]}
        />

        <Card title="YVEX evidence lifecycle" eyebrow="Lifecycle" className="lifecycle-card">
          <div id="lifecycle" className="lifecycle-rail">
            {lifecycle.map((stage, index) => {
              const Icon = lifecycleIcons[index];
              return (
                <div className="lifecycle-stage" key={stage.label}>
                  <div className={`lifecycle-icon status-${statusTone(stage.value)}`}>
                    {Icon ? <Icon aria-hidden="true" size={17} /> : null}
                  </div>
                  <span className="micro-label">{stage.label}</span>
                  <strong>{stage.value}</strong>
                  <small>{stage.detail}</small>
                  {index < lifecycle.length - 1 ? (
                    <ArrowRight className="lifecycle-arrow" aria-hidden="true" size={15} />
                  ) : null}
                </div>
              );
            })}
          </div>
          <BoundaryNote>
            Source verification, logical compilation, physical lowering, artifact presence, and
            execution remain separate gates.
          </BoundaryNote>
        </Card>

        <div id="boundaries" className="grid grid-two">
          <Card
            title="Release target"
            eyebrow="Selection"
            action={detail ? <StatusBadge value={detail.class} tone="cyan" /> : null}
          >
            {detail ? (
              <>
                <FactList>
                  <KeyValue label="Target" value={detail.target_id} mono />
                  <KeyValue label="Family" value={detail.family} />
                  <KeyValue
                    label="Upstream"
                    value={detail.upstream_repository ?? "Unavailable"}
                    mono
                  />
                  <KeyValue label="Next required row" value={decision?.next ?? detail.next} mono />
                </FactList>
                {detailEnvelope ? (
                  <ProducerStamp
                    command={detailEnvelope.producer.displayCommand}
                    exit={detailEnvelope.lastExit.code}
                  />
                ) : null}
              </>
            ) : (
              <EnvelopeFailure envelope={detailEnvelope} />
            )}
          </Card>
          <Card
            title="Persistent safety boundary"
            eyebrow="Operator control"
            action={<StatusBadge value="read-only" tone="cyan" />}
          >
            <ul className="check-list">
              <li>No quantization start or progress simulation</li>
              <li>No artifact write, publish, registry, or model mutation</li>
              <li>No generation, arbitrary shell, or tensor payload access</li>
              <li>Fixed loopback API and immutable command allowlist</li>
            </ul>
          </Card>
        </div>

        <div id="provenance" className="grid grid-three">
          <Card title="Source trust" eyebrow="Evidence gap">
            <MissingEvidence id="sourceManifestSnapshotJson" compact />
          </Card>
          <Card title="Quantization policy" eyebrow="Evidence gap">
            <MissingEvidence id="qtypePolicyJson" compact />
          </Card>
          <Card title="Backend capability" eyebrow="Evidence gap">
            <MissingEvidence id="backendCapabilityJson" compact />
          </Card>
        </div>
      </PageContent>
    </>
  );
}
