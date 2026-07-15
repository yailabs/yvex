/*
 * Owner: apps/operator Artifacts surface.
 * Owns: inventory projection and explicit separation of proof, complete, and supported artifact terms.
 * Does not own: filesystem discovery, artifact admission, integrity, materialization, writes, or publication.
 * Invariants: every row retains its CLI artifact_class/status and local paths arrive redacted.
 * Boundary: a present proof artifact is not a complete or supported model artifact.
 */
import { Archive, Box, Boxes, ShieldQuestion } from "lucide-react";

import type { ArtifactInventory } from "../../shared/contracts.ts";
import {
  BoundaryNote,
  Card,
  MetricStrip,
  PageContent,
  PageHeader,
} from "../components/Primitives.tsx";
import { EnvelopeFailure, StatusBadge } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { reportEnvelope, useOperatorView } from "../view-context.tsx";
import { EmptyCollection, MissingEvidence, ProducerStamp } from "./PageSupport.tsx";

type ArtifactRow = ArtifactInventory["artifacts"][number];

/** Renders borrowed inventory rows only; it performs no artifact IO and preserves exact row states. */
function ArtifactRows({ rows, empty }: { rows: readonly ArtifactRow[]; empty: string }) {
  if (rows.length === 0) return <EmptyCollection detail={empty} />;
  return (
    <div className="artifact-rows">
      {rows.map((row) => (
        <article className="artifact-row" key={`${row.target_id}:${row.artifact_class}`}>
          <div className="artifact-row-main">
            <span className="micro-label">
              {row.family} / {row.artifact_class}
            </span>
            <strong>{row.target_id}</strong>
            <code>{row.path || "Path unavailable"}</code>
          </div>
          <div className="artifact-row-state">
            <StatusBadge value={row.artifact_status} />
            <StatusBadge value={row.prepare_status} />
          </div>
          {row.top_blocker ? <p>{row.top_blocker}</p> : null}
        </article>
      ))}
    </div>
  );
}

/** Classifies validated inventory fields for display without opening, hashing, or admitting artifacts. */
export function ArtifactsPage() {
  const { response } = useOperatorView();
  const inventoryEnvelope = reportEnvelope(response, "artifactInventory");
  const inventory = inventoryEnvelope?.data;
  const rows = inventory?.artifacts ?? [];
  const proofRows = rows.filter((row) => /selected|proof/i.test(row.artifact_class));
  const completeRows = rows.filter(
    (row) => /full|complete/i.test(row.artifact_class) && row.artifact_status === "present",
  );
  const page = pageMetadata.artifacts;

  return (
    <>
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        state={inventory?.status ?? "Unavailable"}
        tabs={["Inventory", "Artifact classes", "Admission"]}
      />
      <PageContent>
        <MetricStrip
          items={[
            {
              label: "Inventory rows",
              value: inventory ? String(rows.length) : "Unavailable",
              detail: "Known catalog rows",
              tone: inventory ? "neutral" : "amber",
            },
            {
              label: "Proof-class rows",
              value: inventory ? String(proofRows.length) : "Unavailable",
              detail: "Selected or proof class",
              tone: proofRows.length ? "cyan" : "neutral",
            },
            {
              label: "Complete + present",
              value: inventory ? String(completeRows.length) : "Unavailable",
              detail: "Full-class inventory test",
              tone: completeRows.length ? "green" : "amber",
            },
            {
              label: "Supported admission",
              value: "Unavailable",
              detail: "No aggregate gate producer",
              tone: "amber",
            },
          ]}
        />

        <Card
          id="inventory"
          title="Artifact inventory"
          eyebrow="Read-only CLI evidence"
          action={inventory ? <StatusBadge value={inventory.status} tone="cyan" /> : null}
        >
          {inventory ? (
            <>
              <ArtifactRows
                rows={rows}
                empty="The artifact inventory producer returned no catalog rows."
              />
              {inventoryEnvelope ? (
                <ProducerStamp
                  command={inventoryEnvelope.producer.displayCommand}
                  exit={inventoryEnvelope.lastExit.code}
                />
              ) : null}
            </>
          ) : (
            <EnvelopeFailure envelope={inventoryEnvelope} />
          )}
        </Card>

        <div id="artifact-classes" className="grid grid-three artifact-class-grid">
          <Card
            title="Tensor proof artifacts"
            eyebrow="Bounded subset"
            action={<Archive aria-hidden="true" size={19} />}
          >
            <ArtifactRows
              rows={proofRows}
              empty="No selected/proof-class row is reported by the current inventory."
            />
          </Card>
          <Card
            title="Complete model artifacts"
            eyebrow="All required tensors + metadata"
            action={<Boxes aria-hidden="true" size={19} />}
          >
            <ArtifactRows
              rows={completeRows}
              empty="No full-class row is both present and validated by this inventory producer."
            />
          </Card>
          <Card
            title="Supported model artifacts"
            eyebrow="All lifecycle gates"
            action={<ShieldQuestion aria-hidden="true" size={19} />}
          >
            <MissingEvidence id="supportedArtifactJson" compact />
          </Card>
        </div>

        <Card id="admission" title="Admission boundary" eyebrow="Artifact terminology">
          <div className="artifact-terms">
            <div>
              <Box aria-hidden="true" size={17} />
              <strong>Proof</strong>
              <span>One tensor or bounded subset</span>
            </div>
            <div>
              <Boxes aria-hidden="true" size={17} />
              <strong>Complete</strong>
              <span>Every required tensor and metadata item</span>
            </div>
            <div>
              <ShieldQuestion aria-hidden="true" size={17} />
              <strong>Supported</strong>
              <span>Complete plus every downstream release gate</span>
            </div>
          </div>
          <BoundaryNote>
            Inventory presence and prepare status do not prove integrity, materialization, runtime
            generation, evaluation, benchmark, or release admission.
          </BoundaryNote>
        </Card>
      </PageContent>
    </>
  );
}
