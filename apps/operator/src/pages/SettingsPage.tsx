/*
 * Owner: apps/operator Settings surface.
 * Owns: redacted startup configuration and immutable read-only policy presentation.
 * Does not own: configuration writes, environment editing, model roots, command mutation, or restarts.
 * Invariants: every displayed setting is non-secret and no form control can change server state.
 * Boundary: this page documents active adapter controls and is not a configuration API.
 */
import { EyeOff, LockKeyhole, Network, TimerReset } from "lucide-react";

import {
  BoundaryNote,
  Card,
  KeyValue,
  MetricStrip,
  PageContent,
  PageHeader,
} from "../components/Primitives.tsx";
import { EnvelopeFailure, StatusBadge } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { reportEnvelope, useOperatorView } from "../view-context.tsx";
import { FactList, MissingEvidence } from "./PageSupport.tsx";

/** Renders redacted active controls and allocates no editable configuration or mutation request. */
export function SettingsPage() {
  const { response } = useOperatorView();
  const healthEnvelope = reportEnvelope(response, "adapterHealth");
  const health = healthEnvelope?.data;
  const page = pageMetadata.settings;

  return (
    <>
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        state="read-only"
        tabs={["Connection", "Cache", "Safety"]}
      />
      <PageContent>
        <MetricStrip
          items={[
            {
              label: "Bind policy",
              value: health?.bindAddress ?? "Unavailable",
              detail: "Loopback only",
              tone: health ? "green" : "amber",
            },
            { label: "API mode", value: "Read-only", detail: "GET / HEAD only", tone: "cyan" },
            {
              label: "Binary path",
              value: "Redacted",
              detail: health?.binaryLabel ?? "Unavailable",
              tone: "neutral",
            },
            { label: "Mutation routes", value: "0", detail: "Enforced by server", tone: "green" },
          ]}
        />
        <div id="connection" className="grid grid-two">
          <Card
            title="Startup connection"
            eyebrow="Redacted active values"
            action={<Network aria-hidden="true" size={19} />}
          >
            {health ? (
              <FactList>
                <KeyValue label="Bind address" value={health.bindAddress} mono />
                <KeyValue label="API version" value={health.apiVersion} mono />
                <KeyValue label="Binary label" value={health.binaryLabel} mono />
                <KeyValue label="Binary lookup" value={health.binaryResolution} />
              </FactList>
            ) : (
              <EnvelopeFailure envelope={healthEnvelope} />
            )}
          </Card>
          <Card
            title="Configured roots"
            eyebrow="No path disclosure"
            action={<EyeOff aria-hidden="true" size={19} />}
          >
            <MissingEvidence id="configuredRootsJson" />
          </Card>
        </div>
        <Card
          id="cache"
          title="Cache policy"
          eyebrow="Bounded evidence lifecycle"
          action={<TimerReset aria-hidden="true" size={19} />}
        >
          <div className="settings-policy-grid">
            <div>
              <strong>Immutable</strong>
              <span>Target catalog, target decision, target detail</span>
            </div>
            <div>
              <strong>Short TTL</strong>
              <span>Artifact inventory, binary lookup, host connectivity</span>
            </div>
            <div>
              <strong>Never success-cache</strong>
              <span>Timeouts, malformed JSON, refusals, missing binary</span>
            </div>
          </div>
        </Card>
        <Card
          id="safety"
          title="Safety controls"
          eyebrow="Non-configurable in browser"
          action={<StatusBadge value="enforced" tone="cyan" />}
        >
          <div className="safety-controls">
            <div>
              <LockKeyhole aria-hidden="true" size={18} />
              <span>Immutable command allowlist</span>
            </div>
            <div>
              <LockKeyhole aria-hidden="true" size={18} />
              <span>Shell disabled</span>
            </div>
            <div>
              <LockKeyhole aria-hidden="true" size={18} />
              <span>Timeout and output ceiling</span>
            </div>
            <div>
              <LockKeyhole aria-hidden="true" size={18} />
              <span>Secret and path redaction</span>
            </div>
          </div>
          <BoundaryNote>
            Startup changes require stopping the local operator and changing trusted environment
            configuration outside the browser.
          </BoundaryNote>
        </Card>
      </PageContent>
    </>
  );
}
