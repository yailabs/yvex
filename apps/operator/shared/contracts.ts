/*
 * Owner: apps/operator shared API contract boundary.
 * Owns: versioned Operator transport, availability, capability, producer, settings, job, event, and chat types.
 * Does not own: process execution, persistence, HTTP routing, rendering, provider IO, or native YVEX policy.
 * Invariants: browser and adapter share one schema vocabulary; negative states retain reason and recovery facts.
 * Boundary: these contracts describe observed control-plane truth and never promote native runtime capability.
 */
import { z } from "zod";

export const API_VERSION = "v1" as const;
export const SCHEMA_VERSION = "1.0" as const;

export const availabilityStatusValues = [
  "loading",
  "ready",
  "empty",
  "degraded",
  "blocked",
  "unsupported",
  "unavailable",
  "failed",
  "stale",
] as const;
export type AvailabilityStatus = (typeof availabilityStatusValues)[number];

export const recoveryActionSchema = z.object({
  id: z.string().min(1),
  label: z.string().min(1),
  href: z.string().optional(),
});
export type RecoveryAction = z.infer<typeof recoveryActionSchema>;

export const availabilitySchema = z.object({
  status: z.enum(availabilityStatusValues),
  reasonCode: z.string().min(1),
  message: z.string().min(1),
  detail: z.string().optional(),
  blockingDependency: z.string().optional(),
  recovery: recoveryActionSchema.optional(),
  source: z.string().optional(),
  observedAt: z.string(),
});
export type Availability = z.infer<typeof availabilitySchema>;

/** Constructs one transport-safe availability observation without inferring a higher capability. */
export function availability(
  status: AvailabilityStatus,
  reasonCode: string,
  message: string,
  observedAt: string,
  optional: Partial<
    Pick<Availability, "detail" | "blockingDependency" | "recovery" | "source">
  > = {},
): Availability {
  return { status, reasonCode, message, observedAt, ...optional };
}

export const capabilityDomains = [
  "system",
  "source",
  "compilation",
  "quantization",
  "artifact",
  "backend",
  "runtime",
  "generation",
  "evaluation",
  "benchmark",
  "provider",
  "operator",
] as const;
export type CapabilityDomain = (typeof capabilityDomains)[number];

export const capabilityIds = [
  "system.browser-reachable",
  "system.adapter-transport",
  "system.adapter-process",
  "system.adapter-api-compatible",
  "system.yvex-configured",
  "system.yvex-resolved",
  "system.yvex-executable",
  "system.yvex-identity",
  "system.yvex-version-compatible",
  "source.identity",
  "source.trust",
  "source.accounting",
  "compilation.transformation-ir",
  "compilation.tensor-coverage",
  "compilation.physical-lowering",
  "quantization.policy",
  "quantization.role-support",
  "quantization.reference-evidence",
  "artifact.inventory",
  "artifact.admitted",
  "backend.cpu",
  "backend.cuda",
  "backend.selected",
  "runtime.materialization",
  "runtime.binding",
  "runtime.model-load",
  "runtime.prefill",
  "runtime.kv",
  "runtime.decode",
  "runtime.logits",
  "runtime.sampling",
  "generation.native",
  "generation.streaming",
  "generation.cancellation",
  "generation.tokenizer",
  "evaluation.available",
  "benchmark.available",
  "provider.configured",
  "provider.reachable",
  "provider.models",
  "provider.chat",
  "provider.streaming",
  "operator.producer-run",
  "operator.jobs",
  "operator.events",
] as const;
export type CapabilityId = (typeof capabilityIds)[number];

export const capabilitySchema = z.object({
  schemaVersion: z.literal(SCHEMA_VERSION),
  id: z.enum(capabilityIds),
  domain: z.enum(capabilityDomains),
  status: z.enum(availabilityStatusValues),
  source: z.string().min(1),
  requiredDependencies: z.array(z.enum(capabilityIds)),
  refusalCode: z.string().nullable(),
  reason: z.string().min(1),
  recoveryAction: recoveryActionSchema.nullable(),
  lastObservedAt: z.string(),
});
export type Capability = z.infer<typeof capabilitySchema>;

export const capabilityManifestSchema = z.object({
  apiVersion: z.literal(API_VERSION),
  schemaVersion: z.literal(SCHEMA_VERSION),
  observedAt: z.string(),
  availability: availabilitySchema,
  capabilities: z.array(capabilitySchema),
});
export type CapabilityManifest = z.infer<typeof capabilityManifestSchema>;

export const cliProducerIds = [
  "target-catalog",
  "release-decision",
  "target-detail",
  "artifact-inventory",
] as const;
export type CliProducerId = (typeof cliProducerIds)[number];
export type ProducerId = CliProducerId;

export const producerDomains = [
  "models",
  "sources",
  "compilation",
  "quantization",
  "artifacts",
  "backends",
  "runtime",
  "evidence",
] as const;
export type ProducerDomain = (typeof producerDomains)[number];

export const exitStateValues = [
  "not-run",
  "ok",
  "refused",
  "failed",
  "timeout",
  "cancelled",
  "oversized-output",
  "unavailable-binary",
  "incompatible-binary",
  "malformed-json",
  "invalid-schema",
] as const;
export type ExitState = (typeof exitStateValues)[number];

export const exitRecordSchema = z.object({
  code: z.number().int().nullable(),
  state: z.enum(exitStateValues),
  durationMs: z.number().nonnegative().nullable(),
});
export type ExitRecord = z.infer<typeof exitRecordSchema>;

export const producerDescriptorSchema = z.object({
  id: z.enum(cliProducerIds),
  displayName: z.string().min(1),
  domain: z.enum(producerDomains),
  description: z.string().min(1),
  displayCommand: z.string().min(1),
  requiredCapabilities: z.array(z.enum(capabilityIds)),
  timeoutMs: z.number().int().positive(),
  maxOutputBytes: z.number().int().positive(),
  cachePolicy: z.enum(["immutable", "short-ttl", "none"]),
  ttlMs: z.number().int().nonnegative().nullable(),
  availability: availabilitySchema,
  lastExecutionAt: z.string().nullable(),
  lastExit: exitRecordSchema,
  recovery: recoveryActionSchema.nullable(),
});
export type ProducerDescriptor = z.infer<typeof producerDescriptorSchema>;

export const producerEnvelopeSchema = z.object({
  schemaVersion: z.literal(SCHEMA_VERSION),
  producerId: z.enum(cliProducerIds),
  observedAt: z.string(),
  availability: availabilitySchema,
  cache: z.object({
    policy: z.enum(["immutable", "short-ttl", "none"]),
    hit: z.boolean(),
    expiresAt: z.string().nullable(),
  }),
  durationMs: z.number().nonnegative().nullable(),
  exit: exitRecordSchema,
  data: z.unknown().nullable(),
  refusal: z
    .object({ code: z.string(), message: z.string(), state: z.string().optional() })
    .nullable(),
  error: z
    .object({ code: z.string(), message: z.string(), detail: z.string().optional() })
    .nullable(),
  provenance: z.object({
    executionOwner: z.literal("Operator adapter"),
    command: z.string(),
    binaryIdentity: z.string().nullable(),
  }),
});
export interface ProducerEnvelope<T = unknown> extends Omit<
  z.infer<typeof producerEnvelopeSchema>,
  "data"
> {
  data: T | null;
}

export const producerRunSchema = z.object({
  runId: z.string().min(1),
  producerId: z.enum(cliProducerIds),
  jobId: z.string().min(1),
  startedAt: z.string(),
  completedAt: z.string().nullable(),
  envelope: producerEnvelopeSchema.nullable(),
});
export type ProducerRun = z.infer<typeof producerRunSchema>;

export const producerListResponseSchema = z.object({
  producers: z.array(producerDescriptorSchema),
});
export const producerRunsResponseSchema = z.object({ runs: z.array(producerRunSchema) });

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

export interface ProducerDataMap {
  "target-catalog": TargetCatalog;
  "release-decision": ReleaseDecision;
  "target-detail": TargetDetail;
  "artifact-inventory": ArtifactInventory;
}

export const binaryCandidateSources = [
  "persisted-setting",
  "environment",
  "repository-config",
  "known-build",
  "path-search",
] as const;
export type BinaryCandidateSource = (typeof binaryCandidateSources)[number];

export const binaryIdentitySchema = z.object({
  schemaVersion: z.literal("1"),
  protocolVersion: z.literal("1"),
  yvexVersion: z.string().min(1),
  product: z.literal("yvex"),
});
export type BinaryIdentity = z.infer<typeof binaryIdentitySchema>;

export const binaryCandidateSchema = z.object({
  id: z.string().min(1),
  source: z.enum(binaryCandidateSources),
  label: z.string().min(1),
  displayPath: z.string().min(1),
  exists: z.boolean(),
  regularFile: z.boolean(),
  executable: z.boolean(),
  identityStatus: z.enum(["not-probed", "ready", "failed", "incompatible"]),
  version: z.string().nullable(),
  protocolVersion: z.string().nullable(),
  rejectionReason: z.string().nullable(),
});
export type BinaryCandidate = z.infer<typeof binaryCandidateSchema>;

export const binaryResolutionSchema = z.object({
  apiVersion: z.literal(API_VERSION),
  schemaVersion: z.literal(SCHEMA_VERSION),
  observedAt: z.string(),
  availability: availabilitySchema,
  configured: z.boolean(),
  selectedCandidateId: z.string().nullable(),
  selectedLabel: z.string().nullable(),
  identity: binaryIdentitySchema.nullable(),
  candidates: z.array(binaryCandidateSchema),
});
export type BinaryResolution = z.infer<typeof binaryResolutionSchema>;

export const topologyNodeIds = [
  "browser",
  "adapter-transport",
  "adapter-process",
  "adapter-api",
  "yvex-configured",
  "yvex-resolved",
  "yvex-executable",
  "yvex-identity",
  "yvex-version",
  "cpu-backend",
  "cuda-backend",
  "target",
  "artifact",
  "runtime",
  "native-generation",
  "reference-configured",
  "reference-reachable",
] as const;
export type TopologyNodeId = (typeof topologyNodeIds)[number];

export const topologyNodeSchema = z.object({
  id: z.enum(topologyNodeIds),
  label: z.string(),
  branch: z.enum(["operator", "native", "reference"]),
  status: z.enum(availabilityStatusValues),
  reasonCode: z.string(),
  message: z.string(),
  observedAt: z.string(),
});
export type TopologyNode = z.infer<typeof topologyNodeSchema>;

export const systemHealthSchema = z.object({
  apiVersion: z.literal(API_VERSION),
  schemaVersion: z.literal(SCHEMA_VERSION),
  observedAt: z.string(),
  availability: availabilitySchema,
  adapter: z.object({
    version: z.string(),
    apiVersion: z.literal(API_VERSION),
    uptimeSeconds: z.number().nonnegative(),
    bindAddress: z.string(),
    bindMode: z.enum(["loopback", "remote"]),
    remoteExposure: z.boolean(),
    authenticationRequired: z.boolean(),
    authenticationConfigured: z.boolean(),
  }),
  host: z.object({
    platform: z.string(),
    architecture: z.string(),
    logicalProcessors: z.number().int().nonnegative(),
    totalMemoryBytes: z.number().nonnegative(),
    nodeRuntime: z.string(),
  }),
  topology: z.array(topologyNodeSchema),
});
export type SystemHealth = z.infer<typeof systemHealthSchema>;

export const referenceProviderSettingsSchema = z.object({
  enabled: z.boolean(),
  displayName: z.string().min(1).max(80),
  baseUrl: z.string().max(2_048),
  apiKeyConfigured: z.boolean(),
  defaultModel: z.string().max(256),
  requestTimeoutMs: z.number().int().min(1_000).max(300_000),
});
export type ReferenceProviderSettings = z.infer<typeof referenceProviderSettingsSchema>;

export const settingsResponseSchema = z.object({
  apiVersion: z.literal(API_VERSION),
  schemaVersion: z.literal(SCHEMA_VERSION),
  observedAt: z.string(),
  operator: z.object({
    bindAddress: z.string(),
    bindMode: z.enum(["loopback", "remote"]),
    remoteEnabled: z.boolean(),
    authenticationRequired: z.boolean(),
    eventRetention: z.number().int().positive(),
  }),
  yvex: z.object({
    binaryConfigured: z.boolean(),
    binaryPathLabel: z.string().nullable(),
    environmentCandidateConfigured: z.boolean(),
  }),
  referenceProvider: referenceProviderSettingsSchema,
  cache: z.object({ mutableTtlMs: z.number().int(), binaryTtlMs: z.number().int() }),
  safety: z.object({
    shellEnabled: z.literal(false),
    arbitraryArgvEnabled: z.literal(false),
    remoteProvidersAllowed: z.boolean(),
    providerSecretsReturned: z.literal(false),
  }),
  interface: z.object({
    defaultLane: z.enum(["native-yvex", "reference-provider"]),
    chatDefaultMode: z.enum(["closed", "compact", "docked", "expanded", "fullscreen"]),
  }),
});
export type SettingsResponse = z.infer<typeof settingsResponseSchema>;

export const yvexSettingsPatchSchema = z.object({
  binaryPath: z.string().max(4_096).nullable(),
});
export type YvexSettingsPatch = z.infer<typeof yvexSettingsPatchSchema>;

export const referenceProviderPatchSchema = z.object({
  enabled: z.boolean().optional(),
  displayName: z.string().trim().min(1).max(80).optional(),
  baseUrl: z.string().trim().max(2_048).optional(),
  apiKey: z.string().max(8_192).nullable().optional(),
  defaultModel: z.string().trim().max(256).optional(),
  requestTimeoutMs: z.number().int().min(1_000).max(300_000).optional(),
});
export type ReferenceProviderPatch = z.infer<typeof referenceProviderPatchSchema>;

export const interfaceSettingsPatchSchema = z.object({
  defaultLane: z.enum(["native-yvex", "reference-provider"]).optional(),
  chatDefaultMode: z.enum(["closed", "compact", "docked", "expanded", "fullscreen"]).optional(),
});
export type InterfaceSettingsPatch = z.infer<typeof interfaceSettingsPatchSchema>;

export const executionOwners = ["Operator adapter", "Native YVEX", "Reference provider"] as const;
export type ExecutionOwner = (typeof executionOwners)[number];
export const jobStateValues = [
  "queued",
  "starting",
  "running",
  "cancelling",
  "cancelled",
  "completed",
  "failed",
] as const;
export type JobState = (typeof jobStateValues)[number];

export const structuredErrorSchema = z.object({
  code: z.string(),
  message: z.string(),
  detail: z.string().optional(),
  retryable: z.boolean(),
});
export type StructuredError = z.infer<typeof structuredErrorSchema>;

export const jobEventSchema = z.object({
  id: z.string(),
  jobId: z.string(),
  observedAt: z.string(),
  type: z.string(),
  severity: z.enum(["debug", "info", "warning", "error"]),
  message: z.string(),
  phase: z.string().nullable(),
});
export type JobEvent = z.infer<typeof jobEventSchema>;

export const jobSchema = z.object({
  id: z.string(),
  type: z.enum(["producer-run", "provider-chat", "provider-test", "configuration-reload"]),
  executionOwner: z.enum(executionOwners),
  state: z.enum(jobStateValues),
  createdAt: z.string(),
  startedAt: z.string().nullable(),
  endedAt: z.string().nullable(),
  progress: z.number().min(0).max(1).nullable(),
  phase: z.string().nullable(),
  events: z.array(jobEventSchema),
  cancellable: z.boolean(),
  resultReference: z.string().nullable(),
  error: structuredErrorSchema.nullable(),
});
export type Job = z.infer<typeof jobSchema>;
export const jobsResponseSchema = z.object({ jobs: z.array(jobSchema) });

export const operatorEventSchema = z.object({
  id: z.string(),
  observedAt: z.string(),
  severity: z.enum(["debug", "info", "warning", "error"]),
  type: z.enum([
    "binary-resolution",
    "producer-run",
    "provider-test",
    "chat-generation",
    "cancellation",
    "job-transition",
    "configuration-reload",
  ]),
  message: z.string(),
  correlationId: z.string().nullable(),
});
export type OperatorEvent = z.infer<typeof operatorEventSchema>;
export const eventsResponseSchema = z.object({ events: z.array(operatorEventSchema) });

export const chatLanes = ["native-yvex", "reference-provider"] as const;
export type ChatLane = (typeof chatLanes)[number];
export const chatSessionStates = ["idle", "streaming", "cancelling", "failed"] as const;
export type ChatSessionState = (typeof chatSessionStates)[number];

export const generationParametersSchema = z.object({
  model: z.string().min(1),
  maxOutputTokens: z.number().int().min(1).max(32_768),
  temperature: z.number().min(0).max(2),
  topP: z.number().min(0).max(1),
  seed: z.number().int().nullable(),
  stop: z.array(z.string().max(256)).max(16),
  stream: z.literal(true),
});
export type GenerationParameters = z.infer<typeof generationParametersSchema>;

export const usageSchema = z.object({
  inputTokens: z.number().int().nonnegative().nullable(),
  outputTokens: z.number().int().nonnegative().nullable(),
  totalTokens: z.number().int().nonnegative().nullable(),
});
export type Usage = z.infer<typeof usageSchema>;

export const timingSchema = z.object({
  timeToFirstTokenMs: z.number().nonnegative().nullable(),
  totalDurationMs: z.number().nonnegative().nullable(),
  tokensPerSecond: z.number().nonnegative().nullable(),
});
export type Timing = z.infer<typeof timingSchema>;

export const chatMessageSchema = z.object({
  id: z.string(),
  role: z.enum(["user", "assistant", "system"]),
  content: z.string(),
  state: z.enum(["complete", "streaming", "cancelled", "failed"]),
  createdAt: z.string(),
  completedAt: z.string().nullable(),
  metadata: z
    .object({
      lane: z.enum(chatLanes),
      executionOwner: z.enum(executionOwners),
      provider: z.string().nullable(),
      model: z.string().nullable(),
      requestId: z.string().nullable(),
      usage: usageSchema,
      timings: timingSchema,
      evidence: z.array(z.string()),
    })
    .nullable(),
});
export type ChatMessage = z.infer<typeof chatMessageSchema>;

export const chatSessionSchema = z.object({
  id: z.string(),
  title: z.string(),
  lane: z.enum(chatLanes),
  provider: z.string().nullable(),
  remoteModel: z.string().nullable(),
  nativeTarget: z.string().nullable(),
  state: z.enum(chatSessionStates),
  messages: z.array(chatMessageSchema),
  generationParameters: generationParametersSchema,
  startedAt: z.string(),
  updatedAt: z.string(),
  usage: usageSchema,
  timings: timingSchema,
  associatedEvidence: z.array(z.string()),
  activeRequestId: z.string().nullable(),
});
export type ChatSession = z.infer<typeof chatSessionSchema>;
export const chatSessionsResponseSchema = z.object({ sessions: z.array(chatSessionSchema) });

export const createChatSessionSchema = z.object({
  title: z.string().trim().min(1).max(120).optional(),
  lane: z.enum(chatLanes),
  model: z.string().trim().max(256).optional(),
});

export const sendChatMessageSchema = z.object({
  content: z.string().trim().min(1).max(131_072),
  parameters: generationParametersSchema,
});
export type SendChatMessage = z.infer<typeof sendChatMessageSchema>;

export const renameChatSessionSchema = z.object({ title: z.string().trim().min(1).max(120) });

export const chatStreamEventSchema = z.discriminatedUnion("type", [
  z.object({ type: z.literal("request-started"), requestId: z.string(), jobId: z.string() }),
  z.object({ type: z.literal("token-delta"), requestId: z.string(), delta: z.string() }),
  z.object({ type: z.literal("usage-update"), requestId: z.string(), usage: usageSchema }),
  z.object({ type: z.literal("timing-update"), requestId: z.string(), timings: timingSchema }),
  z.object({ type: z.literal("completion"), requestId: z.string(), session: chatSessionSchema }),
  z.object({ type: z.literal("cancellation"), requestId: z.string(), session: chatSessionSchema }),
  z.object({ type: z.literal("refusal"), requestId: z.string(), error: structuredErrorSchema }),
  z.object({
    type: z.literal("structured-error"),
    requestId: z.string(),
    error: structuredErrorSchema,
  }),
]);
export type ChatStreamEvent = z.infer<typeof chatStreamEventSchema>;

export const providerStatusSchema = z.object({
  apiVersion: z.literal(API_VERSION),
  schemaVersion: z.literal(SCHEMA_VERSION),
  observedAt: z.string(),
  availability: availabilitySchema,
  configured: z.boolean(),
  reachable: z.boolean(),
  streamingCompatible: z.boolean(),
  displayName: z.string(),
  baseUrl: z.string(),
  defaultModel: z.string(),
  lastTestedAt: z.string().nullable(),
  models: z.array(z.string()),
});
export type ProviderStatus = z.infer<typeof providerStatusSchema>;
export const providerModelsResponseSchema = z.object({ models: z.array(z.string()) });
export const providerTestResponseSchema = z.object({
  status: providerStatusSchema,
  jobId: z.string(),
});

export const domainIds = [
  "models",
  "sources",
  "compilation",
  "quantization",
  "artifacts",
  "backends",
  "runtime",
  "evidence",
] as const;
export type DomainId = (typeof domainIds)[number];

export const domainProjectionSchema = z.object({
  apiVersion: z.literal(API_VERSION),
  schemaVersion: z.literal(SCHEMA_VERSION),
  domain: z.enum(domainIds),
  observedAt: z.string(),
  availability: availabilitySchema,
  reports: z.record(z.string(), producerEnvelopeSchema),
  capabilities: z.array(capabilitySchema),
});
export type DomainProjection = z.infer<typeof domainProjectionSchema>;

export const apiErrorResponseSchema = z.object({
  apiVersion: z.literal(API_VERSION),
  schemaVersion: z.literal(SCHEMA_VERSION),
  code: z.string(),
  message: z.string(),
  detail: z.string().optional(),
  requestId: z.string(),
  recovery: recoveryActionSchema.optional(),
});
export type ApiErrorResponse = z.infer<typeof apiErrorResponseSchema>;

/** Returns true only for a compile-time audited CLI producer identifier. */
export function isCliProducerId(value: string): value is CliProducerId {
  return (cliProducerIds as readonly string[]).includes(value);
}

/** Returns true only for a canonical engineering projection identifier. */
export function isDomainId(value: string): value is DomainId {
  return (domainIds as readonly string[]).includes(value);
}

/** Returns one capability by stable ID without guessing from reason prose. */
export function capabilityById(
  manifest: CapabilityManifest | null | undefined,
  id: CapabilityId,
): Capability | null {
  return manifest?.capabilities.find((item) => item.id === id) ?? null;
}
