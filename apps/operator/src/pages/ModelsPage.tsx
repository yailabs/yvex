/*
 * Owner: apps/operator Models surface.
 * Owns: navigable target-catalog projection and release-selection distinction.
 * Does not own: model support, source discovery, artifact creation, runtime execution, or catalog mutation.
 * Invariants: target rows and stages are displayed exactly from model-target list JSON.
 * Boundary: engineering-scope catalog presence is not a model support claim.
 */
import { CheckCircle2, Circle } from "lucide-react";

import { Card, MetricStrip, PageContent, PageHeader } from "../components/Primitives.tsx";
import { EnvelopeFailure, StatusBadge, statusTone } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { reportEnvelope, useOperatorView } from "../view-context.tsx";
import { EmptyCollection, ProducerStamp } from "./PageSupport.tsx";

/** Renders the borrowed target catalog without mutation, source access, or model-support inference. */
export function ModelsPage() {
  const { response } = useOperatorView();
  const catalogEnvelope = reportEnvelope(response, "targetCatalog");
  const detailEnvelope = reportEnvelope(response, "targetDetail");
  const catalog = catalogEnvelope?.data;
  const selected = catalog?.targets.find((target) => target.release_selected);
  const page = pageMetadata.models;

  return (
    <>
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        state={catalog?.status ?? "Unavailable"}
        tabs={["Catalog", "Selection"]}
      />
      <PageContent>
        <MetricStrip
          items={[
            {
              label: "Catalog targets",
              value: catalog ? String(catalog.targets.length) : "Unavailable",
              detail: "Static typed catalog",
              tone: catalog ? "neutral" : "amber",
            },
            {
              label: "Release selected",
              value: selected?.target_id ?? "Unavailable",
              detail: "v0.1.0",
              tone: selected ? "cyan" : "amber",
            },
            {
              label: "Selected runtime",
              value: selected?.runtime ?? "Unavailable",
              detail: "Exact catalog state",
              tone: statusTone(selected?.runtime),
            },
            {
              label: "Selected generation",
              value: selected?.generation ?? "Unavailable",
              detail: "Exact catalog state",
              tone: statusTone(selected?.generation),
            },
          ]}
        />
        <Card id="catalog" title="Target catalog" eyebrow="Machine-readable model scope">
          {!catalog ? (
            <EnvelopeFailure envelope={catalogEnvelope} />
          ) : catalog.targets.length === 0 ? (
            <EmptyCollection detail="The target catalog producer returned no rows." />
          ) : (
            <div className="target-grid">
              {catalog.targets.map((target) => (
                <article
                  className={`target-row${target.release_selected ? " selected" : ""}`}
                  key={target.target_id}
                >
                  <div className="target-selection-icon">
                    {target.release_selected ? (
                      <CheckCircle2 aria-hidden="true" size={18} />
                    ) : (
                      <Circle aria-hidden="true" size={18} />
                    )}
                  </div>
                  <div className="target-main">
                    <span className="micro-label">
                      {target.family} / {target.class}
                    </span>
                    <strong>{target.target_id}</strong>
                  </div>
                  <div className="target-states">
                    <StatusBadge value={target.runtime} />
                    <StatusBadge value={target.generation} />
                  </div>
                </article>
              ))}
            </div>
          )}
          {catalogEnvelope ? (
            <ProducerStamp
              command={catalogEnvelope.producer.displayCommand}
              exit={catalogEnvelope.lastExit.code}
            />
          ) : null}
        </Card>
        <div id="selection" className="grid grid-two">
          <Card
            title="Release selection"
            eyebrow="Canonical target"
            action={detailEnvelope?.data ? <StatusBadge value="selected" tone="cyan" /> : null}
          >
            {detailEnvelope?.data ? (
              <div className="selection-summary">
                <strong>{detailEnvelope.data.target_id}</strong>
                <p>
                  {detailEnvelope.data.family} · {detailEnvelope.data.class}
                </p>
                <span>{detailEnvelope.data.upstream_repository ?? "Upstream unavailable"}</span>
              </div>
            ) : (
              <EnvelopeFailure envelope={detailEnvelope} />
            )}
          </Card>
          <Card title="Claim boundary" eyebrow="Catalog semantics">
            <p className="body-copy">
              Target catalog entries preserve release and engineering scope. Their presence does not
              assert complete artifacts, runtime execution, or generation.
            </p>
          </Card>
        </div>
      </PageContent>
    </>
  );
}
