/*
 * Owner: apps/operator typed adapter.
 * Owns: allowlist enforcement, binary lookup, JSON/schema validation, availability mapping, and evidence cache.
 * Does not own: arbitrary commands, shell input, YVEX paths from the browser, rendering, or domain inference.
 * Invariants: only CLI_PRODUCERS entries execute and only validated JSON enters an available envelope.
 * Boundary: refusals remain refusals; fixture data is never selected by production code.
 */
import { arch, cpus, platform, totalmem } from "node:os";

import type {
  AdapterHealth,
  Availability,
  EvidenceEnvelope,
  ExitRecord,
  HostProbe,
  ProducerDataMap,
  ProducerDescriptor,
} from "../shared/contracts.ts";
import { isCliProducerId, type CliProducerId } from "../shared/contracts.ts";
import {
  ADAPTER_VERSION,
  locateBinary,
  type LocatedBinary,
  type OperatorConfig,
} from "./config.ts";
import { CLI_PRODUCERS, producerDescriptor, type CliProducerDefinition } from "./producers.ts";
import { sanitizeText, sanitizeValue } from "./redaction.ts";
import { runConstrainedProcess, type RunRequest, type RunResult } from "./runner.ts";

type Runner = (request: RunRequest) => Promise<RunResult>;
type Locator = (request: string, searchPath: string, cwd?: string) => Promise<LocatedBinary>;

interface CachedEnvelope {
  expiresAt: number;
  envelope: EvidenceEnvelope<unknown>;
}

export interface AdapterDependencies {
  runner?: Runner;
  locator?: Locator;
  clock?: () => number;
  cwd?: string;
}

export class ForbiddenProducerError extends Error {
  constructor(id: string) {
    super(`producer selection is not allowlisted: ${id}`);
    this.name = "ForbiddenProducerError";
  }
}

function exitRecord(
  code: number | null,
  state: ExitRecord["state"],
  durationMs: number | null,
): ExitRecord {
  return { code, state, durationMs };
}

function failureAvailability(code: number | null): Availability {
  if (code === 5) return "blocked";
  if (code === 2) return "unsupported";
  return "unavailable";
}

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

function refusalState(data: unknown): string | undefined {
  if (!data || typeof data !== "object" || !("status" in data)) return undefined;
  const status = Reflect.get(data, "status");
  return typeof status === "string" ? status : undefined;
}

/** Coordinates only immutable allowlisted producers and short-lived local probes. */
export class OperatorAdapter {
  readonly config: OperatorConfig;
  private readonly runner: Runner;
  private readonly locator: Locator;
  private readonly clock: () => number;
  private readonly cwd: string;
  private readonly cache = new Map<CliProducerId, CachedEnvelope>();
  private binaryCache: { expiresAt: number; binary: LocatedBinary } | null = null;

  constructor(config: OperatorConfig, dependencies: AdapterDependencies = {}) {
    this.config = config;
    this.runner = dependencies.runner ?? runConstrainedProcess;
    this.locator = dependencies.locator ?? locateBinary;
    this.clock = dependencies.clock ?? Date.now;
    this.cwd = dependencies.cwd ?? process.cwd();
  }

  /** Resolves the configured executable with a short cache so installation changes become visible. */
  async locate(): Promise<LocatedBinary> {
    const now = this.clock();
    if (this.binaryCache && this.binaryCache.expiresAt > now) return this.binaryCache.binary;
    const binary = await this.locator(
      this.config.binaryRequest,
      this.config.binarySearchPath,
      this.cwd,
    );
    this.binaryCache = { expiresAt: now + this.config.binaryLookupTtlMs, binary };
    return binary;
  }

  /** Clears report and locator caches for tests or explicit server lifecycle resets only. */
  clearCache(): void {
    this.cache.clear();
    this.binaryCache = null;
  }

  async get<K extends CliProducerId>(
    id: K,
    signal?: AbortSignal,
  ): Promise<EvidenceEnvelope<ProducerDataMap[K]>>;
  async get(id: string, signal?: AbortSignal): Promise<EvidenceEnvelope<unknown>>;

  /** Executes one named producer after runtime allowlist validation; browser fragments are rejected. */
  async get(id: string, signal?: AbortSignal): Promise<EvidenceEnvelope<unknown>> {
    if (!isCliProducerId(id)) throw new ForbiddenProducerError(id);
    const now = this.clock();
    const cached = this.cache.get(id);
    if (cached && cached.expiresAt > now) {
      return { ...cached.envelope, fromCache: true };
    }

    const definition = CLI_PRODUCERS[id] as CliProducerDefinition<unknown>;
    const descriptor = producerDescriptor(definition, this.config);
    const binary = await this.locate();
    if (!binary.executablePath) {
      return this.failureEnvelope(
        descriptor,
        "unavailable",
        exitRecord(null, "unavailable-binary", null),
        "unavailable-binary",
        `Configured YVEX executable '${binary.label}' is unavailable.`,
      );
    }

    const result = await this.runner({
      executablePath: binary.executablePath,
      args: definition.buildArgs(this.config),
      timeoutMs: definition.timeoutMs(this.config),
      maxOutputBytes: definition.maxOutputBytes(this.config),
      environment: this.config.childEnvironment,
      ...(signal ? { signal } : {}),
    });

    if (result.failure) {
      const state = failureState(result);
      return this.failureEnvelope(
        descriptor,
        state === "cancelled" ? "unavailable" : "blocked",
        exitRecord(result.code, state, result.durationMs),
        `cli-${result.failure}`,
        `YVEX producer stopped: ${result.failure}.`,
      );
    }

    let parsed: unknown;
    try {
      parsed = JSON.parse(result.stdout) as unknown;
    } catch {
      if (result.code !== 0) {
        return this.failureEnvelope(
          descriptor,
          failureAvailability(result.code),
          exitRecord(result.code, result.code === 5 ? "refused" : "failed", result.durationMs),
          "yvex-cli-exit",
          sanitizeText(result.stderr) || `YVEX producer returned exit ${result.code}.`,
        );
      }
      return this.failureEnvelope(
        descriptor,
        "blocked",
        exitRecord(result.code, "malformed-json", result.durationMs),
        "malformed-json",
        "YVEX producer returned malformed JSON.",
      );
    }

    const validated = definition.schema.safeParse(parsed);
    if (!validated.success) {
      return this.failureEnvelope(
        descriptor,
        "blocked",
        exitRecord(result.code, "invalid-schema", result.durationMs),
        "invalid-json-contract",
        "YVEX JSON did not satisfy the audited producer contract.",
      );
    }
    const data = sanitizeValue(validated.data);

    if (result.code !== 0) {
      return {
        availability: failureAvailability(result.code),
        data,
        producer: descriptor,
        observedAt: new Date(this.clock()).toISOString(),
        fromCache: false,
        lastExit: exitRecord(result.code, "refused", result.durationMs),
        issue: {
          code: "yvex-cli-refusal",
          summary: sanitizeText(result.stderr) || `YVEX producer refused with exit ${result.code}.`,
          ...(refusalState(data) ? { refusalState: refusalState(data) } : {}),
        },
      };
    }

    const envelope: EvidenceEnvelope<unknown> = {
      availability: "available",
      data,
      producer: descriptor,
      observedAt: new Date(this.clock()).toISOString(),
      fromCache: false,
      lastExit: exitRecord(0, "ok", result.durationMs),
    };
    const ttl = definition.ttlMs(this.config);
    this.cache.set(id, {
      expiresAt: ttl === null ? Number.POSITIVE_INFINITY : now + ttl,
      envelope,
    });
    return envelope;
  }

  /** Returns adapter and binary connectivity without exposing the resolved absolute path. */
  async health(): Promise<EvidenceEnvelope<AdapterHealth>> {
    const binary = await this.locate();
    const descriptor: ProducerDescriptor = {
      id: "adapterHealth",
      label: "Adapter health",
      evidenceClass: "local-probe",
      description: "Loopback adapter version and configured YVEX executable availability.",
      command: [],
      displayCommand: "Local typed probe",
      cachePolicy: "short-ttl",
      ttlMs: this.config.binaryLookupTtlMs,
    };
    return {
      availability: "available",
      data: {
        adapterVersion: ADAPTER_VERSION,
        apiVersion: "v1",
        state: binary.executablePath ? "available" : "degraded",
        bindAddress: this.config.host,
        binaryLabel: binary.label,
        binaryResolution: binary.resolution,
        binaryExecutable: binary.executablePath !== null,
        uptimeSeconds: Math.floor(process.uptime()),
      },
      producer: descriptor,
      observedAt: new Date(this.clock()).toISOString(),
      fromCache: false,
      lastExit: exitRecord(null, "not-run", null),
    };
  }

  /** Captures a bounded host topology snapshot and reads no model or artifact file. */
  hostProbe(): EvidenceEnvelope<HostProbe> {
    const descriptor: ProducerDescriptor = {
      id: "hostProbe",
      label: "Host probe",
      evidenceClass: "local-probe",
      description: "Node-reported platform, architecture, processor count, and physical memory.",
      command: [],
      displayCommand: "Local typed probe",
      cachePolicy: "short-ttl",
      ttlMs: this.config.mutableTtlMs,
    };
    return {
      availability: "available",
      data: {
        platform: platform(),
        architecture: arch(),
        logicalProcessors: cpus().length,
        totalMemoryBytes: totalmem(),
        nodeRuntime: process.version,
      },
      producer: descriptor,
      observedAt: new Date(this.clock()).toISOString(),
      fromCache: false,
      lastExit: exitRecord(null, "not-run", null),
    };
  }

  private failureEnvelope(
    producer: ProducerDescriptor,
    availability: Availability,
    lastExit: ExitRecord,
    code: string,
    summary: string,
  ): EvidenceEnvelope<null> {
    return {
      availability,
      data: null,
      producer,
      observedAt: new Date(this.clock()).toISOString(),
      fromCache: false,
      lastExit,
      issue: { code, summary: sanitizeText(summary) },
    };
  }
}
