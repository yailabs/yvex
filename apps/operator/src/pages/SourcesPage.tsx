/*
 * Owner: apps/operator Sources surface.
 * Owns: safe source identity, verification, trust-boundary, and unavailable accounting projection.
 * Does not own: source scans, manifests, model roots, payload reads, downloads, or verification policy.
 * Invariants: no source path is supplied by the browser and excluded header scans remain visibly blocked.
 * Boundary: source verification evidence is distinct from payload trust and artifact support.
 */
import { Database, FileLock2, FolderX, ShieldAlert } from "lucide-react";

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

/** Projects safe source decision fields and performs no path, header, payload, or manifest IO. */
export function SourcesPage() {
  const { response } = useOperatorView();
  const decisionEnvelope = reportEnvelope(response, "releaseDecision");
  const detailEnvelope = reportEnvelope(response, "targetDetail");
  const decision = decisionEnvelope?.data;
  const detail = detailEnvelope?.data;
  const page = pageMetadata.sources;

  return (
    <>
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        state={decision?.source_verification ?? "Unavailable"}
        tabs={["Identity", "Trust", "Accounting"]}
      />
      <PageContent>
        <MetricStrip
          items={[
            {
              label: "Target",
              value: decision?.selected_target_id ?? "Unavailable",
              detail: "Release decision",
              tone: decision ? "cyan" : "amber",
            },
            {
              label: "Repository",
              value: decision?.upstream_repository ?? "Unavailable",
              detail: "Official identity field",
              tone: decision ? "neutral" : "amber",
            },
            {
              label: "Verification",
              value: decision?.source_verification ?? "Unavailable",
              detail: "Decision evidence",
              tone: statusTone(decision?.source_verification),
            },
            {
              label: "Payload trust",
              value: "Unavailable",
              detail: "No safe snapshot producer",
              tone: "amber",
            },
          ]}
        />

        <div id="identity" className="grid grid-two">
          <Card
            title="Release source identity"
            eyebrow="Safe JSON projection"
            action={decision ? <StatusBadge value={decision.source_verification} /> : null}
          >
            {decision ? (
              <>
                <FactList>
                  <KeyValue label="Release" value={decision.release} mono />
                  <KeyValue label="Target" value={decision.selected_target_id} mono />
                  <KeyValue label="Repository" value={decision.upstream_repository} mono />
                  <KeyValue label="Revision" value="Unavailable" />
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
          <Card
            title="Browser isolation"
            eyebrow="Local safety contract"
            action={<StatusBadge value="enforced" tone="cyan" />}
          >
            <div className="icon-copy">
              <FileLock2 aria-hidden="true" size={24} />
              <div>
                <strong>No model-file access</strong>
                <p>
                  The browser receives validated report fields only. It cannot submit roots, source
                  paths, flags, or command fragments.
                </p>
              </div>
            </div>
            <BoundaryNote>
              Header and tensor payload reads are outside the operator process boundary.
            </BoundaryNote>
          </Card>
        </div>

        <Card id="trust" title="Source trust chain" eyebrow="Distinct evidence gates">
          <div className="trust-chain">
            <div className="trust-step">
              <Database aria-hidden="true" size={18} />
              <span className="micro-label">Repository identity</span>
              <strong>{decision?.upstream_repository ?? "Unavailable"}</strong>
              <StatusBadge value={decision ? "available" : "unavailable"} />
            </div>
            <div className="trust-step">
              <ShieldAlert aria-hidden="true" size={18} />
              <span className="micro-label">Verification decision</span>
              <strong>{decision?.source_verification ?? "Unavailable"}</strong>
              <StatusBadge value={decision?.source_verification ?? "unavailable"} />
            </div>
            <div className="trust-step">
              <FolderX aria-hidden="true" size={18} />
              <span className="micro-label">Payload trust snapshot</span>
              <strong>Unavailable</strong>
              <StatusBadge value="blocked" />
            </div>
          </div>
        </Card>

        <div id="accounting" className="grid grid-three">
          <Card title="Manifest evidence" eyebrow="Producer boundary">
            <MissingEvidence id="sourceManifestSnapshotJson" compact />
          </Card>
          <Card title="Shard accounting" eyebrow="Producer boundary">
            <MissingEvidence id="sourceManifestSnapshotJson" compact />
          </Card>
          <Card title="Configured roots" eyebrow="Redacted configuration">
            <MissingEvidence id="configuredRootsJson" compact />
          </Card>
        </div>
        {detail?.source_status ? (
          <div className="source-catalog-note">
            <span>Target catalog source status</span>
            <StatusBadge value={detail.source_status} />
            <small>
              Catalog status is shown separately from the release decision verification field.
            </small>
          </div>
        ) : null}
      </PageContent>
    </>
  );
}
