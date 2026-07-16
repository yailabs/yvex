/*
 * Owner: apps/operator deterministic YVEX binary resolution.
 * Owns: trusted candidate ordering, filesystem admission, identity protocol probing, compatibility, redacted trace, and cache.
 * Does not own: persisted settings writes, arbitrary argv, native producer execution, browser paths, or capability wording.
 * Invariants: only absolute trusted candidates are probed and selection requires the exact machine-readable identity contract.
 * Boundary: binary compatibility enables producer adaptation only; it does not establish backend, runtime, or generation support.
 */
import { constants } from "node:fs";
import { access, realpath, stat } from "node:fs/promises";
import { basename, isAbsolute, relative, resolve, sep } from "node:path";

import {
  API_VERSION,
  SCHEMA_VERSION,
  availability,
  binaryIdentitySchema,
  type BinaryCandidate,
  type BinaryCandidateSource,
  type BinaryIdentity,
  type BinaryResolution,
} from "../shared/contracts.ts";
import { binaryPathEntries, type OperatorConfig } from "./config.ts";
import type { EventHistory } from "./events.ts";
import { runConstrainedProcess, type RunRequest, type RunResult } from "./runner.ts";
import type { OperatorSettingsStore } from "./settings.ts";

interface CandidateRequest {
  source: BinaryCandidateSource;
  path: string;
}

export interface ResolvedBinary {
  resolution: BinaryResolution;
  executablePath: string | null;
}

type Runner = (request: RunRequest) => Promise<RunResult>;

export interface ResolverDependencies {
  runner?: Runner;
  clock?: () => number;
}

/** Renders one candidate path using repository-relative or basename-only disclosure. */
function displayPath(path: string, repositoryRoot: string): string {
  const relativePath = relative(repositoryRoot, path);
  if (
    relativePath &&
    relativePath !== ".." &&
    !relativePath.startsWith(`..${sep}`) &&
    !isAbsolute(relativePath)
  ) {
    return `[repository]/${relativePath.replaceAll(sep, "/")}`;
  }
  return `[local]/${basename(path) || "yvex"}`;
}

/** Deduplicates trusted candidate paths while preserving the declared resolution order. */
function candidateRequests(
  config: OperatorConfig,
  persistedPath: string | null,
): CandidateRequest[] {
  const requested: CandidateRequest[] = [];
  if (persistedPath) requested.push({ source: "persisted-setting", path: persistedPath });
  if (config.binaryEnvironmentCandidate) {
    requested.push({
      source: "environment",
      path: isAbsolute(config.binaryEnvironmentCandidate)
        ? config.binaryEnvironmentCandidate
        : resolve(config.repositoryRoot, config.binaryEnvironmentCandidate),
    });
  }
  requested.push({ source: "repository-config", path: config.repositoryBuildCandidate });
  for (const path of config.knownBuildCandidates) requested.push({ source: "known-build", path });
  for (const entry of binaryPathEntries(config))
    requested.push({ source: "path-search", path: resolve(entry, "yvex") });
  const seen = new Set<string>();
  return requested.filter((candidate) => {
    const normalized = resolve(candidate.path);
    if (seen.has(normalized)) return false;
    seen.add(normalized);
    candidate.path = normalized;
    return true;
  });
}

/** Resolves one YVEX executable through a typed identity handshake and bounded short-lived cache. */
export class BinaryResolver {
  private cached: { expiresAt: number; value: ResolvedBinary } | null = null;
  private readonly runner: Runner;
  private readonly clock: () => number;

  constructor(
    private readonly config: OperatorConfig,
    private readonly settings: OperatorSettingsStore,
    private readonly history: EventHistory,
    dependencies: ResolverDependencies = {},
  ) {
    this.runner = dependencies.runner ?? runConstrainedProcess;
    this.clock = dependencies.clock ?? Date.now;
  }

  /** Returns a cached resolution only within the configured diagnostic TTL. */
  async resolve(force = false): Promise<ResolvedBinary> {
    const now = this.clock();
    if (!force && this.cached && this.cached.expiresAt > now)
      return structuredClone(this.cached.value);
    const internal = await this.settings.internal();
    const requests = candidateRequests(this.config, internal.persisted.yvex.binaryPath);
    const candidates: BinaryCandidate[] = [];
    let selectedPath: string | null = null;
    let selectedIdentity: BinaryIdentity | null = null;
    let selectedId: string | null = null;

    for (const [index, request] of requests.entries()) {
      const inspected = await this.inspect(request, index, selectedPath !== null);
      candidates.push(inspected.candidate);
      if (!selectedPath && inspected.path && inspected.identity) {
        selectedPath = inspected.path;
        selectedIdentity = inspected.identity;
        selectedId = inspected.candidate.id;
      }
    }

    const observedAt = new Date(this.clock()).toISOString();
    const configured =
      internal.persisted.yvex.binaryPath !== null ||
      this.config.binaryEnvironmentCandidate !== null;
    const incompatible = candidates.some(
      (candidate) => candidate.identityStatus === "incompatible",
    );
    const state = selectedPath
      ? availability(
          "ready",
          "yvex-binary-ready",
          "A compatible YVEX binary is resolved.",
          observedAt,
          { source: "binary-resolver" },
        )
      : availability(
          incompatible ? "blocked" : "unavailable",
          incompatible ? "yvex-binary-incompatible" : "yvex-binary-unresolved",
          incompatible
            ? "YVEX candidates were found but none satisfied the Operator identity protocol."
            : "No compatible YVEX executable was found in the trusted candidate set.",
          observedAt,
          {
            recovery: {
              id: "configure-yvex",
              label: "Configure YVEX",
              href: "/settings?section=yvex",
            },
            source: "binary-resolver",
          },
        );
    const resolution: BinaryResolution = {
      apiVersion: API_VERSION,
      schemaVersion: SCHEMA_VERSION,
      observedAt,
      availability: state,
      configured,
      selectedCandidateId: selectedId,
      selectedLabel: selectedPath ? basename(selectedPath) : null,
      identity: selectedIdentity,
      candidates,
    };
    const value = { resolution, executablePath: selectedPath };
    this.cached = { expiresAt: now + this.config.binaryLookupTtlMs, value };
    this.history.record(
      "binary-resolution",
      selectedPath ? "info" : incompatible ? "warning" : "info",
      selectedPath ? "Compatible YVEX binary resolved." : state.message,
    );
    return structuredClone(value);
  }

  /** Clears only resolver observations so the next request rechecks disk and identity. */
  clear(): void {
    this.cached = null;
  }

  /** Validates one prospective persisted candidate before settings mutation and returns only its redacted trace. */
  async validateCandidate(path: string): Promise<BinaryCandidate> {
    if (!isAbsolute(path)) throw new Error("binary path must be absolute");
    const inspected = await this.inspect(
      { source: "persisted-setting", path: resolve(path) },
      0,
      false,
    );
    if (!inspected.path || !inspected.identity) {
      throw new Error(inspected.candidate.rejectionReason ?? "binary candidate is incompatible");
    }
    return inspected.candidate;
  }

  /** Inspects one absolute trusted candidate and probes identity only when it can be executed safely. */
  private async inspect(
    request: CandidateRequest,
    index: number,
    selectionAlreadyMade: boolean,
  ): Promise<{ candidate: BinaryCandidate; path: string | null; identity: BinaryIdentity | null }> {
    const id = `candidate-${index + 1}`;
    const base: BinaryCandidate = {
      id,
      source: request.source,
      label: basename(request.path) || "yvex",
      displayPath: displayPath(request.path, this.config.repositoryRoot),
      exists: false,
      regularFile: false,
      executable: false,
      identityStatus: "not-probed",
      version: null,
      protocolVersion: null,
      rejectionReason: null,
    };
    try {
      const info = await stat(request.path);
      base.exists = true;
      base.regularFile = info.isFile();
      if (!base.regularFile) {
        base.rejectionReason = "candidate is not a regular file";
        return { candidate: base, path: null, identity: null };
      }
      await access(request.path, constants.X_OK);
      base.executable = true;
    } catch {
      if (base.exists && base.regularFile) base.rejectionReason = "candidate is not executable";
      else base.rejectionReason = "candidate does not exist";
      return { candidate: base, path: null, identity: null };
    }
    const canonical = await realpath(request.path);
    if (selectionAlreadyMade) {
      base.rejectionReason = "a higher-priority compatible candidate was selected";
      return { candidate: base, path: null, identity: null };
    }
    const result = await this.runner({
      executablePath: canonical,
      args: ["operator-contract", "--output", "json"],
      timeoutMs: Math.min(this.config.immutableTimeoutMs, 2_000),
      maxOutputBytes: Math.min(this.config.maxOutputBytes, 16_384),
      environment: this.config.childEnvironment,
    });
    if (result.failure || result.code !== 0) {
      base.identityStatus = "failed";
      base.rejectionReason = result.failure
        ? `identity probe ${result.failure}`
        : "identity probe was refused";
      return { candidate: base, path: null, identity: null };
    }
    let parsed: unknown;
    try {
      parsed = JSON.parse(result.stdout) as unknown;
    } catch {
      base.identityStatus = "incompatible";
      base.rejectionReason = "identity probe returned malformed JSON";
      return { candidate: base, path: null, identity: null };
    }
    const validated = binaryIdentitySchema.safeParse(parsed);
    if (!validated.success) {
      base.identityStatus = "incompatible";
      base.rejectionReason = "identity protocol or schema is incompatible";
      return { candidate: base, path: null, identity: null };
    }
    base.identityStatus = "ready";
    base.version = validated.data.yvexVersion;
    base.protocolVersion = validated.data.protocolVersion;
    return { candidate: base, path: canonical, identity: validated.data };
  }
}
