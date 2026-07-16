/*
 * Owner: apps/operator normalized capability manifest.
 * Owns: stable capability IDs, exact dependency graph, source/refusal projection, native/reference separation, and recovery actions.
 * Does not own: producer execution, provider transport, native implementation, UI gating behavior, or capability fabrication.
 * Invariants: native generation remains gated by every required native dependency and never inherits provider readiness.
 * Boundary: this manifest normalizes observed reports; it cannot create a lower native capability.
 */
import {
  API_VERSION,
  SCHEMA_VERSION,
  availability,
  type Capability,
  type CapabilityDomain,
  type CapabilityId,
  type CapabilityManifest,
  type AvailabilityStatus,
  type ProducerEnvelope,
} from "../shared/contracts.ts";
import type { OperatorAdapter } from "./adapter.ts";
import type { ReferenceProviderService } from "./provider.ts";
import type { BinaryResolver } from "./resolver.ts";

/** Maps only exact machine-readable YVEX status values and leaves unknown values degraded. */
function nativeStatus(value: string | null | undefined): AvailabilityStatus {
  if (!value) return "unavailable";
  if (
    [
      "complete",
      "verified",
      "present",
      "ready",
      "available",
      "admitted",
      "mapping-specified",
      "source-intake",
    ].includes(value)
  )
    return "ready";
  if (["unsupported"].includes(value)) return "unsupported";
  if (["blocked", "not-produced", "unselected", "verification-required"].includes(value))
    return "blocked";
  if (["not-measured", "not-run"].includes(value)) return "unavailable";
  return "degraded";
}

/** Creates one capability with a stable reason and optional contextual recovery. */
function capability(
  id: CapabilityId,
  domain: CapabilityDomain,
  status: AvailabilityStatus,
  source: string,
  requiredDependencies: readonly CapabilityId[],
  reason: string,
  observedAt: string,
  optional: {
    refusalCode?: string | null;
    recovery?: { id: string; label: string; href?: string } | null;
  } = {},
): Capability {
  return {
    schemaVersion: SCHEMA_VERSION,
    id,
    domain,
    status,
    source,
    requiredDependencies: [...requiredDependencies],
    refusalCode:
      optional.refusalCode ?? (status === "ready" ? null : `${id.replaceAll(".", "-")}-${status}`),
    reason,
    recoveryAction: optional.recovery ?? null,
    lastObservedAt: observedAt,
  };
}

/** Extracts validated typed data from a producer envelope without inspecting its error prose. */
function dataOf<T>(envelope: ProducerEnvelope<T>): T | null {
  return envelope.availability.status === "ready" || envelope.availability.status === "empty"
    ? envelope.data
    : null;
}

/** Builds one complete capability graph from resolver, typed producers, and independent provider state. */
export class CapabilityService {
  constructor(
    private readonly resolver: BinaryResolver,
    private readonly adapter: OperatorAdapter,
    private readonly provider: ReferenceProviderService,
    private readonly clock: () => number = Date.now,
  ) {}

  /** Returns every stable capability exactly once and performs no native action beyond safe report producers. */
  async manifest(): Promise<CapabilityManifest> {
    const observedAt = new Date(this.clock()).toISOString();
    const resolved = await this.resolver.resolve();
    const [decisionEnvelope, artifactEnvelope, providerStatus] = await Promise.all([
      this.adapter.get("release-decision"),
      this.adapter.get("artifact-inventory"),
      this.provider.status(),
    ]);
    const decision = dataOf(decisionEnvelope);
    const inventory = dataOf(artifactEnvelope);
    const hasArtifact =
      inventory?.artifacts.some((row) => row.artifact_status === "present") ?? false;
    const binaryReady = resolved.executablePath !== null;
    const producerRecovery = {
      id: "configure-yvex",
      label: "Configure YVEX",
      href: "/settings?section=yvex",
    };
    const providerRecovery = {
      id: "configure-provider",
      label: "Configure provider",
      href: "/settings?section=reference-provider",
    };
    const runtime = nativeStatus(decision?.runtime);
    const generation = nativeStatus(decision?.generation);

    const capabilities: Capability[] = [
      capability(
        "system.browser-reachable",
        "system",
        "ready",
        "request-context",
        [],
        "The browser completed a same-origin API request.",
        observedAt,
      ),
      capability(
        "system.adapter-transport",
        "system",
        "ready",
        "adapter-health",
        ["system.browser-reachable"],
        "The browser-to-adapter transport is reachable.",
        observedAt,
      ),
      capability(
        "system.adapter-process",
        "system",
        "ready",
        "adapter-health",
        ["system.adapter-transport"],
        "The local adapter process is healthy.",
        observedAt,
      ),
      capability(
        "system.adapter-api-compatible",
        "system",
        "ready",
        "adapter-version",
        ["system.adapter-process"],
        "Adapter API v1 and schema 1.0 are compatible.",
        observedAt,
      ),
      capability(
        "system.yvex-configured",
        "system",
        resolved.resolution.configured ? "ready" : "empty",
        "binary-resolver",
        ["system.adapter-api-compatible"],
        resolved.resolution.configured
          ? "A trusted explicit YVEX candidate is configured."
          : "No persisted or environment YVEX candidate is configured; repository and PATH candidates are still inspected.",
        observedAt,
        { recovery: producerRecovery },
      ),
      capability(
        "system.yvex-resolved",
        "system",
        binaryReady ? "ready" : resolved.resolution.availability.status,
        "binary-resolver",
        ["system.adapter-api-compatible"],
        resolved.resolution.availability.message,
        observedAt,
        { recovery: binaryReady ? null : producerRecovery },
      ),
      capability(
        "system.yvex-executable",
        "system",
        binaryReady ? "ready" : "unavailable",
        "binary-resolver",
        ["system.yvex-resolved"],
        binaryReady
          ? "The selected candidate is a regular executable file."
          : "No executable candidate passed resolution.",
        observedAt,
        { recovery: binaryReady ? null : producerRecovery },
      ),
      capability(
        "system.yvex-identity",
        "system",
        resolved.resolution.identity ? "ready" : resolved.resolution.availability.status,
        "binary-identity-probe",
        ["system.yvex-executable"],
        resolved.resolution.identity
          ? "The YVEX machine-readable identity contract was verified."
          : resolved.resolution.availability.message,
        observedAt,
        { recovery: resolved.resolution.identity ? null : producerRecovery },
      ),
      capability(
        "system.yvex-version-compatible",
        "system",
        resolved.resolution.identity ? "ready" : resolved.resolution.availability.status,
        "binary-identity-probe",
        ["system.yvex-identity"],
        resolved.resolution.identity
          ? `YVEX ${resolved.resolution.identity.yvexVersion} supports Operator protocol 1.`
          : resolved.resolution.availability.message,
        observedAt,
        { recovery: resolved.resolution.identity ? null : producerRecovery },
      ),
      capability(
        "source.identity",
        "source",
        nativeStatus(decision?.source_verification),
        "release-decision",
        ["system.yvex-version-compatible"],
        decision
          ? `Release source identity is ${decision.source_verification}.`
          : "Source identity report is unavailable.",
        observedAt,
      ),
      capability(
        "source.trust",
        "source",
        nativeStatus(decision?.source_verification),
        "release-decision",
        ["source.identity"],
        decision
          ? `Source verification is ${decision.source_verification}.`
          : "Source trust report is unavailable.",
        observedAt,
      ),
      capability(
        "source.accounting",
        "source",
        "unavailable",
        "producer-registry",
        ["source.trust"],
        "No non-scanning machine-readable source accounting producer is admitted.",
        observedAt,
        { refusalCode: "source-accounting-contract-missing" },
      ),
      capability(
        "compilation.transformation-ir",
        "compilation",
        nativeStatus(decision?.architecture_ir),
        "release-decision",
        ["source.trust"],
        decision
          ? `Transformation IR gate is ${decision.architecture_ir}.`
          : "Transformation IR report is unavailable.",
        observedAt,
      ),
      capability(
        "compilation.tensor-coverage",
        "compilation",
        nativeStatus(decision?.tensor_coverage),
        "release-decision",
        ["compilation.transformation-ir"],
        decision
          ? `Tensor coverage gate is ${decision.tensor_coverage}.`
          : "Tensor coverage report is unavailable.",
        observedAt,
      ),
      capability(
        "compilation.physical-lowering",
        "compilation",
        nativeStatus(decision?.gguf_mapping),
        "release-decision",
        ["compilation.tensor-coverage"],
        decision
          ? `Physical lowering gate is ${decision.gguf_mapping}.`
          : "Physical lowering report is unavailable.",
        observedAt,
      ),
      capability(
        "quantization.policy",
        "quantization",
        decision?.release_qtype ? "ready" : "blocked",
        "release-decision",
        ["compilation.physical-lowering"],
        decision?.release_qtype
          ? `Release qtype is ${decision.release_qtype}.`
          : "Release qtype remains unselected in the target decision.",
        observedAt,
      ),
      capability(
        "quantization.role-support",
        "quantization",
        "unavailable",
        "producer-registry",
        ["quantization.policy"],
        "No stable machine-readable role/qtype support producer is admitted.",
        observedAt,
      ),
      capability(
        "quantization.reference-evidence",
        "quantization",
        "unavailable",
        "producer-registry",
        ["quantization.role-support"],
        "Reference dequantization evidence has no stable Operator producer.",
        observedAt,
      ),
      capability(
        "artifact.inventory",
        "artifact",
        artifactEnvelope.availability.status,
        "artifact-inventory",
        ["system.yvex-version-compatible"],
        artifactEnvelope.availability.message,
        observedAt,
      ),
      capability(
        "artifact.admitted",
        "artifact",
        "blocked",
        "artifact-inventory",
        ["artifact.inventory", "quantization.reference-evidence"],
        hasArtifact
          ? "Only bounded proof-class artifacts are reported; no complete supported artifact is admitted."
          : "No present artifact is reported.",
        observedAt,
      ),
      capability(
        "backend.cpu",
        "backend",
        "unavailable",
        "producer-registry",
        ["system.yvex-version-compatible"],
        "CPU host architecture is not machine-readable backend capability evidence.",
        observedAt,
      ),
      capability(
        "backend.cuda",
        "backend",
        "unavailable",
        "producer-registry",
        ["system.yvex-version-compatible"],
        "No stable machine-readable CUDA capability producer is admitted.",
        observedAt,
      ),
      capability(
        "backend.selected",
        "backend",
        "blocked",
        "operator-selection",
        ["backend.cpu", "backend.cuda"],
        "No execution-capable backend can be selected.",
        observedAt,
      ),
      capability(
        "runtime.materialization",
        "runtime",
        "unsupported",
        "native-contract",
        ["artifact.admitted"],
        "Complete-model materialization is unsupported.",
        observedAt,
      ),
      capability(
        "runtime.binding",
        "runtime",
        runtime,
        "release-decision",
        ["runtime.materialization", "backend.selected"],
        decision
          ? `Native runtime binding is ${decision.runtime}.`
          : "Native runtime binding report is unavailable.",
        observedAt,
      ),
      capability(
        "runtime.model-load",
        "runtime",
        "unsupported",
        "native-contract",
        ["runtime.binding"],
        "No native full-model load endpoint exists.",
        observedAt,
      ),
      capability(
        "runtime.prefill",
        "runtime",
        "unsupported",
        "native-contract",
        ["runtime.model-load"],
        "Native model-backed prefill is unsupported.",
        observedAt,
      ),
      capability(
        "runtime.kv",
        "runtime",
        "unsupported",
        "native-contract",
        ["runtime.prefill"],
        "Native model-backed KV state is unsupported.",
        observedAt,
      ),
      capability(
        "runtime.decode",
        "runtime",
        "unsupported",
        "native-contract",
        ["runtime.kv"],
        "Native model-backed decode is unsupported.",
        observedAt,
      ),
      capability(
        "runtime.logits",
        "runtime",
        "unsupported",
        "native-contract",
        ["runtime.decode"],
        "Native model-backed logits are unsupported.",
        observedAt,
      ),
      capability(
        "runtime.sampling",
        "runtime",
        "unsupported",
        "native-contract",
        ["runtime.logits"],
        "Native sampling over real model logits is unsupported.",
        observedAt,
      ),
      capability(
        "generation.tokenizer",
        "generation",
        "unsupported",
        "native-contract",
        ["runtime.model-load"],
        "Exact native tokenizer loading is unsupported.",
        observedAt,
      ),
      capability(
        "generation.native",
        "generation",
        generation,
        "release-decision",
        ["runtime.sampling", "generation.tokenizer"],
        decision
          ? `Native generation is ${decision.generation}.`
          : "Native generation report is unavailable.",
        observedAt,
      ),
      capability(
        "generation.streaming",
        "generation",
        "unsupported",
        "native-contract",
        ["generation.native"],
        "Native streamed generation is unsupported.",
        observedAt,
      ),
      capability(
        "generation.cancellation",
        "generation",
        "unsupported",
        "native-contract",
        ["generation.streaming"],
        "Native generation cancellation is unsupported.",
        observedAt,
      ),
      capability(
        "evaluation.available",
        "evaluation",
        nativeStatus(decision?.evaluation),
        "release-decision",
        ["generation.native"],
        decision ? `Evaluation is ${decision.evaluation}.` : "Evaluation report is unavailable.",
        observedAt,
      ),
      capability(
        "benchmark.available",
        "benchmark",
        nativeStatus(decision?.benchmark),
        "release-decision",
        ["evaluation.available"],
        decision ? `Benchmark is ${decision.benchmark}.` : "Benchmark report is unavailable.",
        observedAt,
      ),
      capability(
        "provider.configured",
        "provider",
        providerStatus.configured ? "ready" : "unavailable",
        "reference-provider-settings",
        [],
        providerStatus.configured
          ? "Reference provider configuration is complete."
          : "Reference provider configuration is incomplete.",
        observedAt,
        { recovery: providerStatus.configured ? null : providerRecovery },
      ),
      capability(
        "provider.reachable",
        "provider",
        providerStatus.reachable ? "ready" : providerStatus.availability.status,
        "reference-provider-test",
        ["provider.configured"],
        providerStatus.availability.message,
        observedAt,
        { recovery: providerStatus.reachable ? null : providerRecovery },
      ),
      capability(
        "provider.models",
        "provider",
        providerStatus.models.length
          ? "ready"
          : providerStatus.configured
            ? "stale"
            : "unavailable",
        "reference-provider-models",
        ["provider.reachable"],
        providerStatus.models.length
          ? `${providerStatus.models.length} reference model(s) reported.`
          : "Reference provider models have not been observed.",
        observedAt,
        { recovery: providerStatus.models.length ? null : providerRecovery },
      ),
      capability(
        "provider.chat",
        "provider",
        providerStatus.streamingCompatible ? "ready" : providerStatus.availability.status,
        "reference-provider-test",
        ["provider.reachable", "provider.models"],
        providerStatus.streamingCompatible
          ? "Reference chat completion is compatible."
          : "Reference chat compatibility has not passed testing.",
        observedAt,
        { recovery: providerStatus.streamingCompatible ? null : providerRecovery },
      ),
      capability(
        "provider.streaming",
        "provider",
        providerStatus.streamingCompatible ? "ready" : providerStatus.availability.status,
        "reference-provider-test",
        ["provider.chat"],
        providerStatus.streamingCompatible
          ? "Reference SSE token streaming is compatible."
          : "Reference token streaming has not passed testing.",
        observedAt,
        { recovery: providerStatus.streamingCompatible ? null : providerRecovery },
      ),
      capability(
        "operator.producer-run",
        "operator",
        binaryReady ? "ready" : "blocked",
        "producer-registry",
        ["system.yvex-version-compatible"],
        binaryReady
          ? "Allowlisted producers can run as controlled jobs."
          : "Producer execution is blocked by binary resolution.",
        observedAt,
        { recovery: binaryReady ? null : producerRecovery },
      ),
      capability(
        "operator.jobs",
        "operator",
        "ready",
        "job-manager",
        ["system.adapter-process"],
        "Bounded job state, events, and cancellation are available.",
        observedAt,
      ),
      capability(
        "operator.events",
        "operator",
        "ready",
        "event-history",
        ["system.adapter-process"],
        "Bounded redacted Operator event history is available.",
        observedAt,
      ),
    ];

    const degraded = capabilities.some((item) =>
      ["failed", "degraded", "blocked"].includes(item.status),
    );
    return {
      apiVersion: API_VERSION,
      schemaVersion: SCHEMA_VERSION,
      observedAt,
      availability: availability(
        degraded ? "degraded" : "ready",
        degraded ? "capabilities-degraded" : "capabilities-ready",
        degraded
          ? "Capability manifest is available with explicit blockers."
          : "Capability manifest is ready.",
        observedAt,
        { source: "capability-service" },
      ),
      capabilities,
    };
  }
}
