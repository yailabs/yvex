/*
 * Owner: apps/operator System health surface.
 * Owns: adapter connectivity, executable availability, and typed host topology projection.
 * Does not own: build identity, backend/CUDA facts, health mutation, process control, or model inspection.
 * Invariants: resolved absolute binary paths and secret-bearing environment values never render.
 * Boundary: executable availability and host architecture are not backend or runtime capability.
 */
import { Binary, Cpu, HeartPulse, MemoryStick, ServerCog } from "lucide-react";

import { Card, KeyValue, MetricStrip, PageContent, PageHeader } from "../components/Primitives.tsx";
import { EnvelopeFailure, StatusBadge, statusTone } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { reportEnvelope, useOperatorView } from "../view-context.tsx";
import { FactList, formatBytes, MissingEvidence } from "./PageSupport.tsx";

/** Projects bounded local probes without exposing paths, environment secrets, or backend capability claims. */
export function SystemHealthPage() {
  const { response } = useOperatorView();
  const healthEnvelope = reportEnvelope(response, "adapterHealth");
  const hostEnvelope = reportEnvelope(response, "hostProbe");
  const health = healthEnvelope?.data;
  const host = hostEnvelope?.data;
  const page = pageMetadata["system-health"];

  return (
    <>
      <PageHeader
        eyebrow={page.eyebrow}
        title={page.label}
        summary={page.summary}
        state={health?.state ?? "Unavailable"}
        tabs={["Connectivity", "Host", "Backend"]}
      />
      <PageContent>
        <MetricStrip
          items={[
            {
              label: "Adapter",
              value: health?.state ?? "Unavailable",
              detail: health ? `API ${health.apiVersion}` : "No typed health",
              tone: statusTone(health?.state),
            },
            {
              label: "YVEX binary",
              value: health
                ? health.binaryExecutable
                  ? "Available"
                  : "Unavailable"
                : "Unavailable",
              detail: health?.binaryLabel ?? "Redacted label",
              tone: health?.binaryExecutable ? "green" : "amber",
            },
            {
              label: "Host architecture",
              value: host?.architecture ?? "Unavailable",
              detail: host?.platform ?? "No local probe",
              tone: host ? "neutral" : "amber",
            },
            {
              label: "Backend evidence",
              value: "Unavailable",
              detail: "No stable JSON",
              tone: "amber",
            },
          ]}
        />

        <div id="connectivity" className="grid grid-two">
          <Card
            title="Local adapter"
            eyebrow="Configured listener"
            action={health ? <StatusBadge value={health.state} /> : null}
          >
            {health ? (
              <FactList>
                <KeyValue label="Adapter version" value={health.adapterVersion} mono />
                <KeyValue label="API" value={health.apiVersion} mono />
                <KeyValue label="Bind" value={health.bindAddress} mono />
                <KeyValue label="Uptime" value={`${health.uptimeSeconds}s`} mono />
              </FactList>
            ) : (
              <EnvelopeFailure envelope={healthEnvelope} />
            )}
          </Card>
          <Card
            title="YVEX executable"
            eyebrow="Existing binary only"
            action={<Binary aria-hidden="true" size={19} />}
          >
            {health ? (
              <FactList>
                <KeyValue label="Label" value={health.binaryLabel} mono />
                <KeyValue label="Resolution" value={health.binaryResolution} />
                <KeyValue
                  label="Executable"
                  value={health.binaryExecutable ? "true" : "false"}
                  mono
                />
                <KeyValue label="Absolute path" value="Redacted" />
              </FactList>
            ) : (
              <EnvelopeFailure envelope={healthEnvelope} />
            )}
          </Card>
        </div>

        <Card id="host" title="Host topology" eyebrow="Typed local-system probe">
          {host ? (
            <div className="host-metrics">
              <div>
                <ServerCog aria-hidden="true" size={19} />
                <span>Platform</span>
                <strong>{host.platform}</strong>
              </div>
              <div>
                <Cpu aria-hidden="true" size={19} />
                <span>Architecture</span>
                <strong>{host.architecture}</strong>
              </div>
              <div>
                <HeartPulse aria-hidden="true" size={19} />
                <span>Logical processors</span>
                <strong>{host.logicalProcessors}</strong>
              </div>
              <div>
                <MemoryStick aria-hidden="true" size={19} />
                <span>Physical memory</span>
                <strong>{formatBytes(host.totalMemoryBytes)}</strong>
              </div>
            </div>
          ) : (
            <EnvelopeFailure envelope={hostEnvelope} />
          )}
        </Card>

        <div id="backend" className="grid grid-two">
          <Card title="Build identity" eyebrow="Machine-readable gap">
            <MissingEvidence id="buildIdentityJson" />
          </Card>
          <Card title="Backend and CUDA" eyebrow="Machine-readable gap">
            <MissingEvidence id="backendCapabilityJson" />
          </Card>
        </div>
      </PageContent>
    </>
  );
}
