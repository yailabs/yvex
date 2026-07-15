/*
 * Owner: apps/operator shared API boundary.
 * Owns: typed availability envelopes, audited producer shapes, and view identifiers.
 * Does not own: process execution, caching, rendering, source inspection, or YVEX policy.
 * Invariants: every domain fact carries producer and last-exit provenance.
 * Boundary: these types describe evidence and never promote runtime or release capability.
 */
import { z } from "zod";

export const availabilityValues = [
  "available",
  "unavailable",
  "blocked",
  "unsupported",
  "not-measured",
] as const;

export type Availability = (typeof availabilityValues)[number];

export const cliProducerIds = [
  "targetCatalog",
  "releaseDecision",
  "targetDetail",
  "artifactInventory",
] as const;

export type CliProducerId = (typeof cliProducerIds)[number];
export type ProbeProducerId = "adapterHealth" | "hostProbe";
export type ProducerId = CliProducerId | ProbeProducerId;

export const viewIds = [
  "overview",
  "models",
  "sources",
  "compilation",
  "quantization",
  "artifacts",
  "runtime",
  "evidence",
  "system-health",
  "settings",
] as const;

export type ViewId = (typeof viewIds)[number];

export type ExitState =
  | "not-run"
  | "ok"
  | "refused"
  | "failed"
  | "timeout"
  | "cancelled"
  | "oversized-output"
  | "unavailable-binary"
  | "malformed-json"
  | "invalid-schema";

export interface ExitRecord {
  code: number | null;
  state: ExitState;
  durationMs: number | null;
}

export interface AvailabilityIssue {
  code: string;
  summary: string;
  refusalState?: string;
}

export type CachePolicy = "immutable" | "short-ttl" | "none";
export type EvidenceClass = "cli-json" | "local-probe";

export interface ProducerDescriptor {
  id: ProducerId;
  label: string;
  evidenceClass: EvidenceClass;
  description: string;
  command: string[];
  displayCommand: string;
  cachePolicy: CachePolicy;
  ttlMs: number | null;
}

export interface EvidenceEnvelope<T> {
  availability: Availability;
  data: T | null;
  producer: ProducerDescriptor;
  observedAt: string;
  fromCache: boolean;
  lastExit: ExitRecord;
  issue?: AvailabilityIssue;
}

export const targetCatalogSchema = z.object({
  status: z.literal("model-target-list"),
  targets: z.array(
    z.object({
      target_id: z.string().min(1),
      family: z.string().min(1),
      class: z.string().min(1),
      release_selected: z.boolean(),
      runtime: z.string().min(1),
      generation: z.string().min(1),
    }),
  ),
});

export type TargetCatalog = z.infer<typeof targetCatalogSchema>;

export const releaseDecisionSchema = z.object({
  status: z.string().min(1),
  release: z.literal("v0.1.0"),
  selected_target_id: z.string().min(1),
  upstream_repository: z.string().min(1),
  source_verification: z.string().min(1),
  architecture_ir: z.string().min(1),
  tensor_coverage: z.string().min(1),
  gguf_mapping: z.string().min(1),
  release_qtype: z.string().nullable(),
  artifact_status: z.string().min(1),
  runtime: z.string().min(1),
  generation: z.string().min(1),
  evaluation: z.string().min(1),
  benchmark: z.string().min(1),
  next: z.string().min(1),
});

export type ReleaseDecision = z.infer<typeof releaseDecisionSchema>;

export const targetDetailSchema = z.object({
  status: z.literal("model-target"),
  target_id: z.string().min(1),
  family: z.string().min(1),
  class: z.string().min(1),
  release_selected: z.boolean(),
  upstream_repository: z.string().nullable(),
  source_status: z.string().min(1),
  artifact_status: z.string().min(1),
  runtime: z.string().min(1),
  generation: z.string().min(1),
  next: z.string(),
});

export type TargetDetail = z.infer<typeof targetDetailSchema>;

export const artifactInventorySchema = z.object({
  status: z.literal("artifacts-list"),
  artifacts: z.array(
    z.object({
      target_id: z.string().min(1),
      family: z.string().min(1),
      artifact_class: z.string().min(1),
      artifact_status: z.string().min(1),
      prepare_status: z.string().min(1),
      top_blocker: z.string(),
      path: z.string(),
    }),
  ),
});

export type ArtifactInventory = z.infer<typeof artifactInventorySchema>;

export interface AdapterHealth {
  adapterVersion: string;
  apiVersion: "v1";
  state: "available" | "degraded";
  bindAddress: string;
  binaryLabel: string;
  binaryResolution: "configured-path" | "path-search" | "not-found";
  binaryExecutable: boolean;
  uptimeSeconds: number;
}

export interface HostProbe {
  platform: string;
  architecture: string;
  logicalProcessors: number;
  totalMemoryBytes: number;
  nodeRuntime: string;
}

export interface ProducerDataMap {
  targetCatalog: TargetCatalog;
  releaseDecision: ReleaseDecision;
  targetDetail: TargetDetail;
  artifactInventory: ArtifactInventory;
  adapterHealth: AdapterHealth;
  hostProbe: HostProbe;
}

export type ReportMap = {
  [K in ProducerId]?: EvidenceEnvelope<ProducerDataMap[K]>;
};

export type MissingProducerId =
  | "buildIdentityJson"
  | "configuredRootsJson"
  | "sourceManifestSnapshotJson"
  | "transformationIdentityJson"
  | "tensorAccountingJson"
  | "qtypePolicyJson"
  | "qtypeRoleSupportJson"
  | "referenceDequantJson"
  | "supportedArtifactJson"
  | "backendCapabilityJson"
  | "validationEvidenceJson";

export interface MissingProducerDescriptor {
  id: MissingProducerId;
  label: string;
  surface: string;
  availability: Exclude<Availability, "available">;
  auditedCommand: string | null;
  reason: string;
}

export interface OperatorViewResponse {
  apiVersion: "v1";
  adapterVersion: string;
  view: ViewId;
  observedAt: string;
  reports: ReportMap;
  missingProducers: MissingProducerDescriptor[];
}

export interface ApiErrorResponse {
  apiVersion: "v1";
  code: string;
  message: string;
}

const producerDescriptorSchema = z.object({
  id: z.enum([
    ...(cliProducerIds as unknown as [CliProducerId, ...CliProducerId[]]),
    "adapterHealth",
    "hostProbe",
  ]),
  label: z.string(),
  evidenceClass: z.enum(["cli-json", "local-probe"]),
  description: z.string(),
  command: z.array(z.string()),
  displayCommand: z.string(),
  cachePolicy: z.enum(["immutable", "short-ttl", "none"]),
  ttlMs: z.number().nullable(),
});

const evidenceEnvelopeSchema = z.object({
  availability: z.enum(availabilityValues),
  data: z.unknown().nullable(),
  producer: producerDescriptorSchema,
  observedAt: z.string(),
  fromCache: z.boolean(),
  lastExit: z.object({
    code: z.number().nullable(),
    state: z.enum([
      "not-run",
      "ok",
      "refused",
      "failed",
      "timeout",
      "cancelled",
      "oversized-output",
      "unavailable-binary",
      "malformed-json",
      "invalid-schema",
    ]),
    durationMs: z.number().nullable(),
  }),
  issue: z
    .object({
      code: z.string(),
      summary: z.string(),
      refusalState: z.string().optional(),
    })
    .optional(),
});

export const operatorViewResponseSchema = z.object({
  apiVersion: z.literal("v1"),
  adapterVersion: z.string(),
  view: z.enum(viewIds),
  observedAt: z.string(),
  reports: z.record(z.string(), evidenceEnvelopeSchema),
  missingProducers: z.array(
    z.object({
      id: z.string(),
      label: z.string(),
      surface: z.string(),
      availability: z.enum(["unavailable", "blocked", "unsupported", "not-measured"]),
      auditedCommand: z.string().nullable(),
      reason: z.string(),
    }),
  ),
});

/** Returns true only for a canonical direct-route view identifier. */
export function isViewId(value: string): value is ViewId {
  return (viewIds as readonly string[]).includes(value);
}

/** Returns true only for a compile-time audited CLI producer identifier. */
export function isCliProducerId(value: string): value is CliProducerId {
  return (cliProducerIds as readonly string[]).includes(value);
}
