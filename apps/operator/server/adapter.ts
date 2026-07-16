/*
 * Owner: apps/operator typed producer adapter.
 * Owns: allowlist enforcement, schema validation, producer envelopes, evidence cache, explicit runs, and exit/refusal mapping.
 * Does not own: binary candidate policy, arbitrary commands, HTTP routing, rendering, provider IO, or native capability inference.
 * Invariants: only registry definitions execute and only validated JSON can enter a ready producer envelope.
 * Boundary: adapter success proves one machine-readable report invocation, not model, runtime, or generation readiness.
 */
import { randomUUID } from "node:crypto";

import {
  SCHEMA_VERSION,
  availability,
  isCliProducerId,
  type AvailabilityStatus,
  type CliProducerId,
  type ExitRecord,
  type ProducerDataMap,
  type ProducerDescriptor,
  type ProducerEnvelope,
  type ProducerRun,
} from "../shared/contracts.ts";
import type { OperatorConfig } from "./config.ts";
import type { EventHistory } from "./events.ts";
import type { JobManager } from "./jobs.ts";
import {
  CLI_PRODUCERS,
  displayCommand,
  producerDefinition,
  type CliProducerDefinition,
} from "./producers.ts";
import { sanitizeText, sanitizeValue } from "./redaction.ts";
import type { BinaryResolver } from "./resolver.ts";
import { runConstrainedProcess, type RunRequest, type RunResult } from "./runner.ts";

type Runner = (request: RunRequest) => Promise<RunResult>;

interface CachedEnvelope {
  expiresAt: number;
  envelope: ProducerEnvelope<unknown>;
}

export interface AdapterDependencies {
  runner?: Runner;
  clock?: () => number;
}

export class ForbiddenProducerError extends Error {
  constructor(id: string) {
    super(`producer selection is not allowlisted: ${id}`);
    this.name = "ForbiddenProducerError";
  }
}

/** Creates one exact exit record without translating native exit codes into capability claims. */
function exitRecord(
  code: number | null,
  state: ExitRecord["state"],
  durationMs: number | null,
): ExitRecord {
  return { code, state, durationMs };
}

/** Maps runner termination classes to the closed producer exit vocabulary. */
function failureState(result: RunResult): ExitRecord["state"] {
  switch (result.failure) {
    case "timeout":
      return "timeout";
    case "cancelled":
      return "cancelled";
    case "oversized-output":
      return "oversized-output";
    default:
      return "failed";
  }
}

/** Maps known CLI refusal exits to presentation state while retaining the original code. */
function refusalAvailability(code: number | null): AvailabilityStatus {
  if (code === 5) return "blocked";
  if (code === 2) return "unsupported";
  return "failed";
}

/** Reads an optional typed refusal status from validated JSON without parsing human output. */
function refusalState(data: unknown): string | undefined {
  if (!data || typeof data !== "object" || !("status" in data)) return undefined;
  const status = Reflect.get(data, "status");
  return typeof status === "string" ? status : undefined;
}

/** Detects only schema-known collection emptiness and never treats missing fields as empty success. */
function resultIsEmpty(id: CliProducerId, data: unknown): boolean {
  if (!data || typeof data !== "object") return false;
  if (id === "target-catalog")
    return (
      Array.isArray(Reflect.get(data, "targets")) &&
      (Reflect.get(data, "targets") as unknown[]).length === 0
    );
  if (id === "artifact-inventory")
    return (
      Array.isArray(Reflect.get(data, "artifacts")) &&
      (Reflect.get(data, "artifacts") as unknown[]).length === 0
    );
  return false;
}

/** Executes and observes only compile-time producer definitions through a resolved compatible YVEX binary. */
export class OperatorAdapter {
  private readonly runner: Runner;
  private readonly clock: () => number;
  private readonly cache = new Map<CliProducerId, CachedEnvelope>();
  private readonly lastEnvelope = new Map<CliProducerId, ProducerEnvelope<unknown>>();
  private readonly runs: ProducerRun[] = [];

  constructor(
    readonly config: OperatorConfig,
    private readonly resolver: BinaryResolver,
    private readonly jobs: JobManager,
    private readonly history: EventHistory,
    dependencies: AdapterDependencies = {},
  ) {
    this.runner = dependencies.runner ?? runConstrainedProcess;
    this.clock = dependencies.clock ?? Date.now;
  }

  /** Clears producer success caches and optionally preserves last observations for the inspector. */
  clearCache(preserveObservations = true): void {
    this.cache.clear();
    if (!preserveObservations) this.lastEnvelope.clear();
  }

  async get<K extends CliProducerId>(
    id: K,
    options?: { signal?: AbortSignal; force?: boolean },
  ): Promise<ProducerEnvelope<ProducerDataMap[K]>>;
  async get(
    id: string,
    options?: { signal?: AbortSignal; force?: boolean },
  ): Promise<ProducerEnvelope<unknown>>;

  /** Executes one named producer after runtime allowlist validation; no caller value contributes argv. */
  async get(
    id: string,
    options: { signal?: AbortSignal; force?: boolean } = {},
  ): Promise<ProducerEnvelope<unknown>> {
    if (!isCliProducerId(id)) throw new ForbiddenProducerError(id);
    const now = this.clock();
    const cached = this.cache.get(id);
    if (!options.force && cached && cached.expiresAt > now) {
      return {
        ...structuredClone(cached.envelope),
        cache: { ...cached.envelope.cache, hit: true },
      };
    }
    if (options.force) this.cache.delete(id);

    const definition = CLI_PRODUCERS[id] as CliProducerDefinition<unknown>;
    const resolved = await this.resolver.resolve();
    const binaryIdentity = resolved.resolution.identity?.yvexVersion ?? null;
    if (!resolved.executablePath) {
      const state =
        resolved.resolution.availability.status === "blocked"
          ? "incompatible-binary"
          : "unavailable-binary";
      const envelope = this.failureEnvelope(
        definition,
        resolved.resolution.availability.status,
        exitRecord(null, state, null),
        resolved.resolution.availability.reasonCode,
        resolved.resolution.availability.message,
        binaryIdentity,
      );
      this.lastEnvelope.set(id, envelope);
      return envelope;
    }

    const result = await this.runner({
      executablePath: resolved.executablePath,
      args: definition.buildArgs(this.config),
      timeoutMs: definition.timeoutMs(this.config),
      maxOutputBytes: definition.maxOutputBytes(this.config),
      environment: this.config.childEnvironment,
      ...(options.signal ? { signal: options.signal } : {}),
    });

    if (result.failure) {
      const state = failureState(result);
      const envelope = this.failureEnvelope(
        definition,
        state === "cancelled" ? "degraded" : "failed",
        exitRecord(result.code, state, result.durationMs),
        `producer-${result.failure}`,
        `YVEX producer stopped: ${result.failure}.`,
        binaryIdentity,
      );
      this.lastEnvelope.set(id, envelope);
      return envelope;
    }

    let parsed: unknown;
    try {
      parsed = JSON.parse(result.stdout) as unknown;
    } catch {
      const envelope =
        result.code !== 0
          ? this.failureEnvelope(
              definition,
              refusalAvailability(result.code),
              exitRecord(result.code, result.code === 5 ? "refused" : "failed", result.durationMs),
              "yvex-cli-exit",
              sanitizeText(result.stderr) || `YVEX producer returned exit ${result.code}.`,
              binaryIdentity,
            )
          : this.failureEnvelope(
              definition,
              "failed",
              exitRecord(result.code, "malformed-json", result.durationMs),
              "malformed-json",
              "YVEX producer returned malformed JSON.",
              binaryIdentity,
            );
      this.lastEnvelope.set(id, envelope);
      return envelope;
    }

    const validated = definition.schema.safeParse(parsed);
    if (!validated.success) {
      const envelope = this.failureEnvelope(
        definition,
        "failed",
        exitRecord(result.code, "invalid-schema", result.durationMs),
        "invalid-producer-contract",
        "YVEX JSON did not satisfy the audited producer contract.",
        binaryIdentity,
      );
      this.lastEnvelope.set(id, envelope);
      return envelope;
    }
    const data = sanitizeValue(validated.data);
    const observedAt = new Date(this.clock()).toISOString();
    if (result.code !== 0) {
      const summary =
        sanitizeText(result.stderr) || `YVEX producer refused with exit ${result.code}.`;
      const envelope: ProducerEnvelope<unknown> = {
        schemaVersion: SCHEMA_VERSION,
        producerId: id,
        observedAt,
        availability: availability(
          refusalAvailability(result.code),
          "yvex-cli-refusal",
          summary,
          observedAt,
          {
            source: id,
            recovery: {
              id: "inspect-producer",
              label: "Inspect refusal",
              href: `/evidence?producer=${id}`,
            },
          },
        ),
        cache: { policy: definition.cachePolicy, hit: false, expiresAt: null },
        durationMs: result.durationMs,
        exit: exitRecord(result.code, "refused", result.durationMs),
        data,
        refusal: {
          code: "yvex-cli-refusal",
          message: summary,
          ...(refusalState(data) ? { state: refusalState(data) } : {}),
        },
        error: null,
        provenance: {
          executionOwner: "Operator adapter",
          command: displayCommand(definition.buildSafeCommand(this.config)),
          binaryIdentity,
        },
      };
      this.lastEnvelope.set(id, envelope);
      return envelope;
    }

    const empty = resultIsEmpty(id, data);
    const ttl = definition.ttlMs(this.config);
    const expiresAtMs = ttl === null ? Number.POSITIVE_INFINITY : now + ttl;
    const envelope: ProducerEnvelope<unknown> = {
      schemaVersion: SCHEMA_VERSION,
      producerId: id,
      observedAt,
      availability: availability(
        empty ? "empty" : "ready",
        empty ? "producer-empty" : "producer-ready",
        empty
          ? "The producer returned a valid empty collection."
          : "The producer returned validated machine-readable data.",
        observedAt,
        { source: id },
      ),
      cache: {
        policy: definition.cachePolicy,
        hit: false,
        expiresAt: Number.isFinite(expiresAtMs) ? new Date(expiresAtMs).toISOString() : null,
      },
      durationMs: result.durationMs,
      exit: exitRecord(0, "ok", result.durationMs),
      data,
      refusal: null,
      error: null,
      provenance: {
        executionOwner: "Operator adapter",
        command: displayCommand(definition.buildSafeCommand(this.config)),
        binaryIdentity,
      },
    };
    this.cache.set(id, { expiresAt: expiresAtMs, envelope });
    this.lastEnvelope.set(id, envelope);
    return structuredClone(envelope);
  }

  /** Returns the typed producer registry with current binary gating and latest observations. */
  async listProducers(): Promise<ProducerDescriptor[]> {
    const resolved = await this.resolver.resolve();
    const now = new Date(this.clock()).toISOString();
    return Object.values(CLI_PRODUCERS).map((definition) => {
      const last = this.lastEnvelope.get(definition.id);
      const ready = resolved.executablePath !== null;
      return {
        id: definition.id,
        displayName: definition.displayName,
        domain: definition.domain,
        description: definition.description,
        displayCommand: displayCommand(definition.buildSafeCommand(this.config)),
        requiredCapabilities: [...definition.requiredCapabilities],
        timeoutMs: definition.timeoutMs(this.config),
        maxOutputBytes: definition.maxOutputBytes(this.config),
        cachePolicy: definition.cachePolicy,
        ttlMs: definition.ttlMs(this.config),
        availability: ready
          ? availability(
              "ready",
              "producer-admitted",
              "Producer is admitted for controlled execution.",
              now,
              { source: "producer-registry" },
            )
          : resolved.resolution.availability,
        lastExecutionAt: last?.observedAt ?? null,
        lastExit: last?.exit ?? exitRecord(null, "not-run", null),
        recovery: ready
          ? null
          : { id: "configure-yvex", label: "Configure YVEX", href: "/settings?section=yvex" },
      };
    });
  }

  /** Runs one allowlisted producer as a cancellable job and records a bounded explicit run. */
  async runExplicit(id: string): Promise<ProducerRun> {
    const definition = producerDefinition(id);
    if (!definition) throw new ForbiddenProducerError(id);
    const runId = randomUUID();
    const job = this.jobs.create("producer-run", "Operator adapter", definition.displayName, true);
    const controller = new AbortController();
    this.jobs.registerCancellation(job.id, () => controller.abort());
    this.jobs.transition(job.id, "starting", "binary-resolution");
    const startedAt = new Date(this.clock()).toISOString();
    const run: ProducerRun = {
      runId,
      producerId: definition.id,
      jobId: job.id,
      startedAt,
      completedAt: null,
      envelope: null,
    };
    this.runs.unshift(run);
    if (this.runs.length > 100) this.runs.length = 100;
    try {
      this.jobs.transition(job.id, "running", "producer-execution");
      const envelope = await this.get(definition.id, { force: true, signal: controller.signal });
      run.envelope = envelope;
      run.completedAt = new Date(this.clock()).toISOString();
      if (envelope.exit.state === "cancelled") {
        this.jobs.transition(job.id, "cancelled", "cancelled", { resultReference: runId });
      } else if (
        ["ready", "empty", "degraded", "blocked", "unsupported"].includes(
          envelope.availability.status,
        )
      ) {
        this.jobs.transition(job.id, "completed", "completed", { resultReference: runId });
      } else {
        this.jobs.transition(job.id, "failed", "failed", {
          resultReference: runId,
          error: {
            code: envelope.error?.code ?? envelope.availability.reasonCode,
            message: envelope.error?.message ?? envelope.availability.message,
            retryable: true,
          },
        });
      }
      this.history.record(
        "producer-run",
        envelope.availability.status === "failed" ? "error" : "info",
        `${definition.displayName}: ${envelope.availability.status}.`,
        runId,
      );
      return structuredClone(run);
    } catch (error) {
      run.completedAt = new Date(this.clock()).toISOString();
      this.jobs.transition(job.id, "failed", "adapter-failure", {
        resultReference: runId,
        error: {
          code: "producer-adapter-failure",
          message: "The producer adapter could not complete the run.",
          retryable: true,
        },
      });
      throw error;
    }
  }

  /** Returns newest-first explicit producer runs without live cancellation or cache handles. */
  producerRuns(): ProducerRun[] {
    return structuredClone(this.runs);
  }

  /** Returns one explicit producer run or null without accepting alternate selectors. */
  producerRun(runId: string): ProducerRun | null {
    const run = this.runs.find((candidate) => candidate.runId === runId);
    return run ? structuredClone(run) : null;
  }

  /** Creates one structured failed envelope and never success-caches it. */
  private failureEnvelope(
    definition: CliProducerDefinition<unknown>,
    status: AvailabilityStatus,
    exit: ExitRecord,
    code: string,
    message: string,
    binaryIdentity: string | null,
  ): ProducerEnvelope<null> {
    const observedAt = new Date(this.clock()).toISOString();
    const summary = sanitizeText(message);
    return {
      schemaVersion: SCHEMA_VERSION,
      producerId: definition.id,
      observedAt,
      availability: availability(status, code, summary, observedAt, {
        source: definition.id,
        recovery: {
          id: "inspect-producer",
          label: "Inspect producer",
          href: `/evidence?producer=${definition.id}`,
        },
      }),
      cache: { policy: definition.cachePolicy, hit: false, expiresAt: null },
      durationMs: exit.durationMs,
      exit,
      data: null,
      refusal: exit.state === "refused" ? { code, message: summary } : null,
      error: exit.state === "refused" ? null : { code, message: summary },
      provenance: {
        executionOwner: "Operator adapter",
        command: displayCommand(definition.buildSafeCommand(this.config)),
        binaryIdentity,
      },
    };
  }
}
