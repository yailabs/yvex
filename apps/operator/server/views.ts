/*
 * Owner: apps/operator typed view API.
 * Owns: fixed route-to-producer composition and missing-evidence projection.
 * Does not own: process arguments, JSON parsing, frontend layout, or YVEX domain state.
 * Invariants: each route uses a compile-time producer set and includes provenance envelopes.
 * Boundary: composing evidence does not infer unsupported readiness or live progress.
 */
import type { OperatorViewResponse, ReportMap, ViewId } from "../shared/contracts.ts";
import { ADAPTER_VERSION } from "./config.ts";
import { missingProducersForView, VIEW_PRODUCERS } from "./producers.ts";
import type { OperatorAdapter } from "./adapter.ts";

/** Builds one route response by executing only that route's immutable producer identifiers. */
export async function buildOperatorView(
  adapter: OperatorAdapter,
  view: ViewId,
  signal?: AbortSignal,
): Promise<OperatorViewResponse> {
  const entries = await Promise.all(
    VIEW_PRODUCERS[view].map(async (id) => [id, await adapter.get(id, signal)] as const),
  );
  const reports = Object.fromEntries(entries) as ReportMap;

  if (view === "system-health" || view === "settings" || view === "evidence") {
    reports.adapterHealth = await adapter.health();
    reports.hostProbe = adapter.hostProbe();
  }

  return {
    apiVersion: "v1",
    adapterVersion: ADAPTER_VERSION,
    view,
    observedAt: new Date().toISOString(),
    reports,
    missingProducers: missingProducersForView(view),
  };
}
