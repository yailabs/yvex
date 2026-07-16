/*
 * Owner: apps/operator authoritative engineering workspace.
 * Owns: active target/artifact selection, target taxonomy, build-stage projection, and coherent workspace snapshots.
 * Does not own: YVEX producers, native build execution, backend selection, runtime sessions, provider comparison, or browser state.
 * Invariants: one server-owned selection feeds every surface; only catalogued targets and artifacts can be selected.
 * Boundary: workspace selection and report projection never promote artifact, backend, runtime, or generation capability.
 */
import { chmod, mkdir, readFile, rename, writeFile } from "node:fs/promises";
import { join } from "node:path";

import {
  API_VERSION,
  SCHEMA_VERSION,
  availability,
  type Availability,
  type BuildProjection,
  type BuildResponse,
  type BuildStage,
  type BuildStageId,
  type BuildStagesResponse,
  type Capability,
  type CapabilityId,
  type OperatorWorkspace,
  type ReleaseDecision,
  type TargetsResponse,
  type WorkspaceArtifact,
  type WorkspaceArtifactsResponse,
  type WorkspaceTarget,
  type WorkspaceTargetKind,
} from "../shared/contracts.ts";
import type { OperatorAdapter } from "./adapter.ts";
import type { CapabilityService } from "./capabilities.ts";
import type { OperatorConfig } from "./config.ts";
import type { JobManager } from "./jobs.ts";

interface PersistedWorkspaceSelection {
  schemaVersion: "1";
  activeTargetId: string | null;
  activeArtifactId: string | null;
}

const emptySelection: PersistedWorkspaceSelection = {
  schemaVersion: "1",
  activeTargetId: null,
  activeArtifactId: null,
};

export class WorkspaceSelectionError extends Error {
  constructor(
    readonly code:
      | "target-not-catalogued"
      | "artifact-not-catalogued"
      | "artifact-not-admissible"
      | "target-unavailable",
    message: string,
  ) {
    super(message);
    this.name = "WorkspaceSelectionError";
  }
}

/** Maps only audited producer classes to Operator taxonomy and leaves every unknown class explicit. */
export function classifyTarget(producerClass: string): {
  kind: WorkspaceTargetKind;
  kindLabel: string;
} {
  switch (producerClass) {
    case "release-source-target":
      return { kind: "release-target", kindLabel: "Release target" };
    case "selected-runtime-slice":
      return { kind: "engineering-slice", kindLabel: "Engineering slice" };
    case "source-model-candidate":
    case "official-source-huge-model":
      return { kind: "source-candidate", kindLabel: "Source candidate" };
    case "proof-target":
      return { kind: "proof-target", kindLabel: "Proof target" };
    case "runtime-target":
      return { kind: "runtime-target", kindLabel: "Runtime target" };
    default:
      return { kind: "unclassified", kindLabel: "Unclassified target" };
  }
}

/** Converts one validated CLI catalog row into a typed workspace target without changing its status. */
function workspaceTarget(row: {
  target_id: string;
  family: string;
  class: string;
  release_selected: boolean;
  runtime: string;
  generation: string;
}): WorkspaceTarget {
  return {
    id: row.target_id,
    family: row.family,
    producerClass: row.class,
    ...classifyTarget(row.class),
    releaseSelected: row.release_selected,
    runtimeStatus: row.runtime,
    generationStatus: row.generation,
    provenance: "target-catalog",
  };
}

/** Builds a stable browser-safe artifact identity without leaking the producer path field. */
function artifactIdentity(targetId: string, artifactClass: string): string {
  return `artifact:${encodeURIComponent(targetId)}:${encodeURIComponent(artifactClass)}`;
}

/** Maps only explicit artifact classes into canonical terminology and never infers completeness from an unknown name. */
export function classifyArtifactClass(artifactClass: string): WorkspaceArtifact["classification"] {
  switch (artifactClass) {
    case "yvex-selected-gguf":
      return "proof";
    case "complete-model-gguf":
      return "complete";
    case "supported-model-gguf":
      return "supported";
    default:
      return "unclassified";
  }
}

/** Converts one validated inventory row into workspace context and intentionally drops its local path. */
function workspaceArtifact(row: {
  target_id: string;
  family: string;
  artifact_class: string;
  artifact_status: string;
  prepare_status: string;
  top_blocker: string;
}): WorkspaceArtifact {
  return {
    id: artifactIdentity(row.target_id, row.artifact_class),
    targetId: row.target_id,
    family: row.family,
    artifactClass: row.artifact_class,
    classification: classifyArtifactClass(row.artifact_class),
    artifactStatus: row.artifact_status,
    prepareStatus: row.prepare_status,
    topBlocker: row.top_blocker,
    provenance: "artifact-inventory",
  };
}

/** Projects one normalized capability into a build-stage observation without changing its semantics. */
function stageAvailability(
  item: Capability | undefined,
  observedAt: string,
  fallback: { code: string; message: string; source: string },
): Availability {
  if (!item)
    return availability("unavailable", fallback.code, fallback.message, observedAt, {
      source: fallback.source,
    });
  return availability(
    item.status,
    item.refusalCode ?? `${item.id}-ready`,
    item.reason,
    observedAt,
    {
      source: item.source,
      ...(item.recoveryAction ? { recovery: item.recoveryAction } : {}),
    },
  );
}

/** Builds a structured refusal only for a non-ready build stage. */
function stageRefusal(value: Availability): BuildStage["refusal"] {
  return value.status === "ready" || value.status === "empty"
    ? null
    : { code: value.reasonCode, message: value.message };
}

/** Looks up one stable capability without using human-readable status text. */
function capabilityById(
  capabilities: readonly Capability[],
  id: CapabilityId,
): Capability | undefined {
  return capabilities.find((item) => item.id === id);
}

/** Constructs the truthful build chain for the exact release-decision target. */
function buildProjection(
  decision: ReleaseDecision,
  target: WorkspaceTarget,
  capabilities: readonly Capability[],
  observedAt: string,
): BuildProjection | null {
  if (decision.selected_target_id !== target.id) return null;

  const source = stageAvailability(capabilityById(capabilities, "source.trust"), observedAt, {
    code: "source-trust-unavailable",
    message: "Source trust evidence is unavailable.",
    source: "release-decision",
  });
  const architecture = availability(
    "unavailable",
    "logical-model-producer-missing",
    "No separate machine-readable logical-model identity producer is admitted.",
    observedAt,
    { source: "producer-registry" },
  );
  const transformationIr = stageAvailability(
    capabilityById(capabilities, "compilation.transformation-ir"),
    observedAt,
    {
      code: "transformation-ir-unavailable",
      message: "Transformation IR evidence is unavailable.",
      source: "release-decision",
    },
  );
  const lowering = stageAvailability(
    capabilityById(capabilities, "compilation.physical-lowering"),
    observedAt,
    {
      code: "physical-lowering-unavailable",
      message: "Physical-lowering evidence is unavailable.",
      source: "release-decision",
    },
  );
  const quantization = stageAvailability(
    capabilityById(capabilities, "quantization.policy"),
    observedAt,
    {
      code: "quantization-policy-unavailable",
      message: "Quantization policy evidence is unavailable.",
      source: "release-decision",
    },
  );
  const writer = availability(
    "unavailable",
    "gguf-writer-producer-missing",
    "No stable machine-readable GGUF writer-state producer is admitted.",
    observedAt,
    { source: "producer-registry" },
  );

  const stage = (
    id: BuildStageId,
    label: string,
    state: Availability,
    optional: Omit<BuildStage, "id" | "label" | "availability" | "refusal">,
  ): BuildStage => ({ id, label, availability: state, refusal: stageRefusal(state), ...optional });

  const stages: BuildStage[] = [
    stage("source", "Source", source, {
      producerId: "release-decision",
      requiredCapability: "source.trust",
      inputIdentity: decision.upstream_repository,
      outputIdentity: decision.source_verification,
      descriptorCount: null,
      tensorCoverage: null,
      dependencies: [],
      evidence: ["release-decision:upstream_repository", "release-decision:source_verification"],
    }),
    stage("architecture", "Logical model", architecture, {
      producerId: null,
      requiredCapability: null,
      inputIdentity: decision.source_verification,
      outputIdentity: null,
      descriptorCount: null,
      tensorCoverage: null,
      dependencies: ["source"],
      evidence: [],
    }),
    stage("transformation-ir", "Transformation IR", transformationIr, {
      producerId: "release-decision",
      requiredCapability: "compilation.transformation-ir",
      inputIdentity: decision.source_verification,
      outputIdentity: decision.architecture_ir,
      descriptorCount: null,
      tensorCoverage: decision.tensor_coverage,
      dependencies: ["source"],
      evidence: ["release-decision:architecture_ir", "release-decision:tensor_coverage"],
    }),
    stage("physical-lowering", "Physical lowering", lowering, {
      producerId: "release-decision",
      requiredCapability: "compilation.physical-lowering",
      inputIdentity: decision.architecture_ir,
      outputIdentity: decision.gguf_mapping,
      descriptorCount: null,
      tensorCoverage: decision.tensor_coverage,
      dependencies: ["transformation-ir"],
      evidence: ["release-decision:gguf_mapping", "release-decision:tensor_coverage"],
    }),
    stage("quantization", "Quantization plan", quantization, {
      producerId: "release-decision",
      requiredCapability: "quantization.policy",
      inputIdentity: decision.gguf_mapping,
      outputIdentity: decision.release_qtype,
      descriptorCount: null,
      tensorCoverage: null,
      dependencies: ["physical-lowering"],
      evidence: ["release-decision:release_qtype"],
    }),
    stage("gguf-writer", "GGUF writer", writer, {
      producerId: null,
      requiredCapability: null,
      inputIdentity: decision.release_qtype,
      outputIdentity: null,
      descriptorCount: null,
      tensorCoverage: null,
      dependencies: ["quantization"],
      evidence: [],
    }),
  ];
  const current = stages.find(
    (item) => item.availability.status !== "ready" && item.availability.status !== "empty",
  );
  return {
    id: `build:${encodeURIComponent(target.id)}:${decision.release}`,
    targetId: target.id,
    release: decision.release,
    status: current?.availability.status ?? "ready",
    currentStage: current?.id ?? null,
    source: "release-decision",
    stages,
  };
}

/** Owns serialized workspace mutations and composes coherent snapshots from typed producers. */
export class WorkspaceService {
  private readonly statePath: string;
  private selection: PersistedWorkspaceSelection | null = null;
  private mutation: Promise<void> = Promise.resolve();

  constructor(
    private readonly config: OperatorConfig,
    private readonly adapter: OperatorAdapter,
    private readonly capabilities: CapabilityService,
    private readonly jobs: JobManager,
    private readonly clock: () => number = Date.now,
  ) {
    this.statePath = join(config.configDirectory, "workspace.json");
  }

  /** Returns the validated target taxonomy directly from the allowlisted target catalog. */
  async targets(): Promise<TargetsResponse> {
    const envelope = await this.adapter.get("target-catalog");
    return {
      apiVersion: API_VERSION,
      schemaVersion: SCHEMA_VERSION,
      observedAt: envelope.observedAt,
      availability: envelope.availability,
      targets: envelope.data?.targets.map(workspaceTarget) ?? [],
    };
  }

  /** Returns the typed artifact inventory without producer filesystem paths. */
  async artifacts(): Promise<WorkspaceArtifactsResponse> {
    const envelope = await this.adapter.get("artifact-inventory");
    return {
      apiVersion: API_VERSION,
      schemaVersion: SCHEMA_VERSION,
      observedAt: envelope.observedAt,
      availability: envelope.availability,
      producerId: "artifact-inventory",
      artifacts: envelope.data?.artifacts.map(workspaceArtifact) ?? [],
    };
  }

  /** Returns one authoritative workspace snapshot; downstream absence remains nullable and explicit. */
  async snapshot(): Promise<OperatorWorkspace> {
    const [manifest, targets] = await Promise.all([this.capabilities.manifest(), this.targets()]);
    const [decisionEnvelope, artifactEnvelope, selection] = await Promise.all([
      this.adapter.get("release-decision"),
      this.adapter.get("artifact-inventory"),
      this.loadSelection(),
    ]);
    const observedAt = new Date(this.clock()).toISOString();
    const explicit = selection.activeTargetId
      ? targets.targets.find((target) => target.id === selection.activeTargetId)
      : undefined;
    const releaseDefault = targets.targets.find((target) => target.releaseSelected);
    const activeTarget = explicit ?? releaseDefault ?? null;
    const targetSelectionSource = explicit
      ? "operator-selection"
      : releaseDefault
        ? "release-default"
        : "none";
    const staleSelection = selection.activeTargetId !== null && explicit === undefined;
    const artifacts = artifactEnvelope.data?.artifacts.map(workspaceArtifact) ?? [];
    const activeArtifact = selection.activeArtifactId
      ? (artifacts.find(
          (artifact) =>
            artifact.id === selection.activeArtifactId && artifact.targetId === activeTarget?.id,
        ) ?? null)
      : null;
    const decision = decisionEnvelope.data;
    const activeBuild =
      decision && activeTarget
        ? buildProjection(decision, activeTarget, manifest.capabilities, observedAt)
        : null;
    const snapshotAvailability = staleSelection
      ? availability(
          "degraded",
          "workspace-selection-stale",
          "The persisted target is no longer catalogued; the release target is active.",
          observedAt,
          { source: "operator-workspace" },
        )
      : activeTarget
        ? availability(
            "ready",
            "workspace-ready",
            "The authoritative Operator workspace snapshot is ready.",
            observedAt,
            { source: "operator-workspace" },
          )
        : availability(
            targets.availability.status,
            "workspace-target-unavailable",
            "No active target can be resolved from the machine-readable target catalog.",
            observedAt,
            { source: "operator-workspace", recovery: targets.availability.recovery },
          );
    return {
      apiVersion: API_VERSION,
      schemaVersion: SCHEMA_VERSION,
      observedAt,
      availability: snapshotAvailability,
      workspaceIdentity: { id: "local-default", authority: "operator-workspace" },
      activeTarget,
      targetSelectionSource,
      activeBuild,
      activeArtifact,
      activeBackend: null,
      activeRuntimeSession: null,
      capabilities: manifest.capabilities,
      activeJobs: this.jobs
        .list()
        .filter(
          (job) =>
            !["cancelled", "completed", "failed"].includes(job.state) &&
            job.type !== "reference-comparison" &&
            job.type !== "comparison-test",
        ),
      recentEvidence: this.adapter.producerRuns().slice(0, 12),
    };
  }

  /** Persists one catalog-validated active target and clears every downstream selection. */
  async selectTarget(targetId: string): Promise<OperatorWorkspace> {
    const targets = await this.targets();
    if (!targets.targets.length && targets.availability.status !== "empty")
      throw new WorkspaceSelectionError(
        "target-unavailable",
        "The target catalog is unavailable; target selection cannot be validated.",
      );
    if (!targets.targets.some((target) => target.id === targetId))
      throw new WorkspaceSelectionError(
        "target-not-catalogued",
        "The requested target is not present in the audited YVEX target catalog.",
      );
    await this.persistSelection({
      schemaVersion: "1",
      activeTargetId: targetId,
      activeArtifactId: null,
    });
    return this.snapshot();
  }

  /** Persists one inventory-validated artifact for the active target or clears the selection. */
  async selectArtifact(artifactId: string | null): Promise<OperatorWorkspace> {
    const current = await this.snapshot();
    if (!current.activeTarget)
      throw new WorkspaceSelectionError(
        "target-unavailable",
        "An active target is required before selecting an artifact.",
      );
    if (artifactId === null) {
      await this.persistSelection({
        schemaVersion: "1",
        activeTargetId: current.activeTarget.id,
        activeArtifactId: null,
      });
      return this.snapshot();
    }
    const inventory = await this.adapter.get("artifact-inventory");
    const artifacts = inventory.data?.artifacts.map(workspaceArtifact) ?? [];
    const selected = artifacts.find(
      (artifact) => artifact.id === artifactId && artifact.targetId === current.activeTarget?.id,
    );
    if (!selected)
      throw new WorkspaceSelectionError(
        "artifact-not-catalogued",
        "The requested artifact is not present for the active target.",
      );
    if (selected.artifactStatus !== "present")
      throw new WorkspaceSelectionError(
        "artifact-not-admissible",
        "Only a present machine-reported artifact can become active workspace context.",
      );
    await this.persistSelection({
      schemaVersion: "1",
      activeTargetId: current.activeTarget.id,
      activeArtifactId: artifactId,
    });
    return this.snapshot();
  }

  /** Returns the build projection for the active target with an explicit absent-state envelope. */
  async build(): Promise<BuildResponse> {
    const workspace = await this.snapshot();
    return {
      apiVersion: API_VERSION,
      schemaVersion: SCHEMA_VERSION,
      observedAt: workspace.observedAt,
      availability: workspace.activeBuild
        ? availability(
            "ready",
            "build-projection-ready",
            "The active target build projection is available.",
            workspace.observedAt,
            { source: "operator-workspace" },
          )
        : availability(
            "unavailable",
            "build-producer-unavailable-for-target",
            "No machine-readable build projection is available for the active target.",
            workspace.observedAt,
            { source: "operator-workspace" },
          ),
      build: workspace.activeBuild,
    };
  }

  /** Returns build stages as a direct resource for stage-specific panels. */
  async stages(): Promise<BuildStagesResponse> {
    const response = await this.build();
    return {
      apiVersion: API_VERSION,
      schemaVersion: SCHEMA_VERSION,
      observedAt: response.observedAt,
      availability: response.availability,
      targetId: response.build?.targetId ?? null,
      stages: response.build?.stages ?? [],
    };
  }

  /** Loads only the two admitted selection identifiers and rejects every malformed persisted shape. */
  private async loadSelection(): Promise<PersistedWorkspaceSelection> {
    if (this.selection) return structuredClone(this.selection);
    try {
      const parsed = JSON.parse(
        await readFile(this.statePath, "utf8"),
      ) as Partial<PersistedWorkspaceSelection>;
      this.selection = {
        schemaVersion: "1",
        activeTargetId:
          typeof parsed.activeTargetId === "string" && parsed.activeTargetId
            ? parsed.activeTargetId
            : null,
        activeArtifactId:
          typeof parsed.activeArtifactId === "string" && parsed.activeArtifactId
            ? parsed.activeArtifactId
            : null,
      };
    } catch {
      this.selection = structuredClone(emptySelection);
    }
    return structuredClone(this.selection);
  }

  /** Serializes one atomic private workspace-state replacement without exposing filesystem mutation to HTTP. */
  private async persistSelection(next: PersistedWorkspaceSelection): Promise<void> {
    this.mutation = this.mutation.then(async () => {
      await mkdir(this.config.configDirectory, { recursive: true, mode: 0o700 });
      await chmod(this.config.configDirectory, 0o700);
      const temporary = `${this.statePath}.${process.pid}-${this.clock()}.tmp`;
      await writeFile(temporary, `${JSON.stringify(next, null, 2)}\n`, { mode: 0o600 });
      await rename(temporary, this.statePath);
      await chmod(this.statePath, 0o600);
      this.selection = structuredClone(next);
    });
    await this.mutation;
  }
}
