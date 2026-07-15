/*
 * Owner: apps/operator Evidence surface.
 * Owns: producer inventory, cache/exit observations, and missing machine-readable contract audit.
 * Does not own: CLI output, test execution, validation aggregation, source facts, or claims.
 * Invariants: producer commands are redacted fixed vectors and missing contracts remain visible.
 * Boundary: producer availability is provenance evidence, not model/runtime readiness.
 */
import { CheckCircle2, CircleSlash2, Clock3, TerminalSquare } from "lucide-react";

import type { EvidenceEnvelope } from "../../shared/contracts.ts";
import { Card, MetricStrip, PageContent, PageHeader } from "../components/Primitives.tsx";
import { AvailabilityBadge, StatusBadge } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { useOperatorView } from "../view-context.tsx";

/** Lists attached/missing producer contracts without executing validation or changing adapter cache state. */
export function EvidencePage() {
  const { response } = useOperatorView();
  const reports = Object.values(response?.reports ?? {}) as EvidenceEnvelope<unknown>[];
  const available = reports.filter((report) => report.availability === "available").length;
  const missing = response?.missingProducers ?? [];
  const page = pageMetadata.evidence;

  return (
    <>
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        state={response ? "report-only" : "Unavailable"}
        tabs={["Producers", "Missing contracts", "Cache"]}
      />
      <PageContent>
        <MetricStrip
          items={[
            {
              label: "Attached producers",
              value: response ? String(reports.length) : "Unavailable",
              detail: "Current evidence route",
              tone: response ? "neutral" : "amber",
            },
            {
              label: "Available",
              value: response ? String(available) : "Unavailable",
              detail: "Validated envelopes",
              tone: available ? "green" : "amber",
            },
            {
              label: "Missing contracts",
              value: response ? String(missing.length) : "Unavailable",
              detail: "Explicit audit records",
              tone: missing.length ? "amber" : "neutral",
            },
            { label: "Arbitrary commands", value: "0", detail: "No API route", tone: "cyan" },
          ]}
        />

        <Card
          id="producers"
          title="Audited evidence producers"
          eyebrow="Allowlist and typed envelopes"
        >
          <div className="evidence-table" role="table" aria-label="Audited evidence producers">
            <div className="evidence-table-head" role="row">
              <span role="columnheader">Producer</span>
              <span role="columnheader">Availability</span>
              <span role="columnheader">Last exit</span>
              <span role="columnheader">Cache</span>
              <span role="columnheader">Command</span>
            </div>
            {reports.map((report) => (
              <div className="evidence-table-row" role="row" key={report.producer.id}>
                <div role="cell">
                  <strong>{report.producer.label}</strong>
                  <small>{report.producer.evidenceClass}</small>
                </div>
                <div role="cell">
                  <AvailabilityBadge value={report.availability} />
                </div>
                <div role="cell" className="exit-cell">
                  {report.lastExit.state === "ok" ? (
                    <CheckCircle2 aria-hidden="true" size={15} />
                  ) : (
                    <CircleSlash2 aria-hidden="true" size={15} />
                  )}
                  <span>
                    {report.lastExit.code ?? "—"} / {report.lastExit.state}
                  </span>
                </div>
                <div role="cell">
                  <Clock3 aria-hidden="true" size={14} /> {report.producer.cachePolicy}
                </div>
                <code role="cell">{report.producer.displayCommand}</code>
              </div>
            ))}
          </div>
        </Card>

        <Card
          id="missing-contracts"
          title="Missing machine-readable contracts"
          eyebrow="Audited, not inferred"
        >
          <div className="missing-contract-list">
            {missing.map((producer) => (
              <article key={producer.id}>
                <div>
                  <span className="micro-label">{producer.surface}</span>
                  <strong>{producer.label}</strong>
                </div>
                <AvailabilityBadge value={producer.availability} />
                <p>{producer.reason}</p>
                <code>{producer.auditedCommand ?? "No stable JSON command"}</code>
              </article>
            ))}
          </div>
        </Card>

        <div id="cache" className="grid grid-two">
          <Card title="Cache contract" eyebrow="Evidence freshness">
            <ul className="check-list">
              <li>Static target reports: immutable for the adapter process</li>
              <li>Artifact and host state: short bounded TTL</li>
              <li>Failures and unavailable binaries: never cached as success</li>
              <li>Browser requests: no fixture or invented fallback</li>
            </ul>
          </Card>
          <Card title="Execution boundary" eyebrow="Process adapter">
            <div className="icon-copy">
              <TerminalSquare aria-hidden="true" size={24} />
              <div>
                <strong>Argument arrays only</strong>
                <p>
                  Shell execution is disabled; timeouts, output ceilings, cancellation, status
                  validation, and schema checks apply to every CLI producer.
                </p>
              </div>
            </div>
            <StatusBadge value="read-only" tone="cyan" />
          </Card>
        </div>
      </PageContent>
    </>
  );
}
