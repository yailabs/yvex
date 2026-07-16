/*
 * Owner: apps/operator local server startup configuration.
 * Owns: loopback-first binding, remote-mode admission, bounded resources, trusted candidate roots, and child environment.
 * Does not own: persisted settings, binary filesystem inspection, producer selection, provider requests, or browser state.
 * Invariants: non-loopback binding requires explicit remote mode, a strong token, and an Origin allowlist.
 * Boundary: startup configuration can locate candidates but never proves YVEX identity or native capability.
 */
import { homedir } from "node:os";
import { isIP } from "node:net";
import { delimiter, resolve } from "node:path";

import type { ProducerRuntimeConfig } from "./producers.ts";

export const ADAPTER_VERSION = "0.2.0";

export interface OperatorConfig extends ProducerRuntimeConfig {
  host: string;
  port: number;
  bindMode: "loopback" | "remote";
  remoteExposure: boolean;
  authToken: string | null;
  allowedOrigins: readonly string[];
  binaryEnvironmentCandidate: string | null;
  binarySearchPath: string;
  binaryLookupTtlMs: number;
  repositoryRoot: string;
  repositoryBuildCandidate: string;
  knownBuildCandidates: readonly string[];
  configDirectory: string;
  childEnvironment: NodeJS.ProcessEnv;
  allowRemoteProviders: boolean;
  eventRetention: number;
  sessionRetention: number;
}

/** Parses one bounded integer without performing IO or silently clipping unsafe values. */
function boundedInteger(
  value: string | undefined,
  fallback: number,
  min: number,
  max: number,
): number {
  if (value === undefined || value === "") return fallback;
  const parsed = Number(value);
  if (!Number.isInteger(parsed) || parsed < min || parsed > max) {
    throw new Error(`operator configuration value must be an integer between ${min} and ${max}`);
  }
  return parsed;
}

/** Parses an explicit boolean vocabulary so misspelled security settings fail closed. */
function configuredBoolean(value: string | undefined, fallback = false): boolean {
  if (value === undefined || value === "") return fallback;
  if (["1", "true", "yes", "enabled"].includes(value.toLowerCase())) return true;
  if (["0", "false", "no", "disabled"].includes(value.toLowerCase())) return false;
  throw new Error("operator boolean configuration must be true or false");
}

/** Returns true only for literal IPv4 or IPv6 loopback addresses. */
export function isLoopbackAddress(host: string): boolean {
  return host === "127.0.0.1" || host === "::1";
}

/** Validates a remote Origin allowlist and strips trailing slashes for exact comparison. */
function parseAllowedOrigins(value: string | undefined): string[] {
  if (!value?.trim()) return [];
  return value.split(",").map((entry) => {
    const candidate = entry.trim().replace(/\/$/, "");
    const parsed = new URL(candidate);
    if (!["http:", "https:"].includes(parsed.protocol) || parsed.pathname !== "/") {
      throw new Error("YVEX operator allowed origins must be HTTP(S) origins without paths");
    }
    return parsed.origin;
  });
}

/** Copies only process settings required by a constrained local YVEX child. */
export function constrainedChildEnvironment(env: NodeJS.ProcessEnv): NodeJS.ProcessEnv {
  const permitted = [
    "PATH",
    "HOME",
    "LANG",
    "LC_ALL",
    "TZ",
    "XDG_CONFIG_HOME",
    "XDG_DATA_HOME",
    "YVEX_HOME",
  ];
  const child: NodeJS.ProcessEnv = {};
  for (const key of permitted) {
    if (env[key] !== undefined) child[key] = env[key];
  }
  child.LC_ALL ??= "C";
  return child;
}

/** Resolves the deterministic Operator configuration directory without creating it. */
function configDirectory(env: NodeJS.ProcessEnv): string {
  if (env.YVEX_OPERATOR_CONFIG_DIR) return resolve(env.YVEX_OPERATOR_CONFIG_DIR);
  const root = env.XDG_CONFIG_HOME
    ? resolve(env.XDG_CONFIG_HOME)
    : resolve(env.HOME || homedir(), ".config");
  return resolve(root, "yvex", "operator");
}

/** Parses trusted startup configuration and refuses insecure remote exposure before listener creation. */
export function loadOperatorConfig(
  env: NodeJS.ProcessEnv = process.env,
  cwd: string = process.cwd(),
): OperatorConfig {
  const host = env.YVEX_OPERATOR_HOST?.trim() || "127.0.0.1";
  if (isIP(host) === 0) {
    throw new Error("YVEX operator bind address must be a literal IPv4 or IPv6 address");
  }
  const remoteExposure = !isLoopbackAddress(host);
  const remoteEnabled = configuredBoolean(env.YVEX_OPERATOR_REMOTE_MODE);
  const authToken = env.YVEX_OPERATOR_AUTH_TOKEN?.trim() || null;
  const allowedOrigins = parseAllowedOrigins(env.YVEX_OPERATOR_ALLOWED_ORIGINS);
  if (remoteExposure && !remoteEnabled) {
    throw new Error("non-loopback binding requires YVEX_OPERATOR_REMOTE_MODE=true");
  }
  if (remoteExposure && (!authToken || authToken.length < 24)) {
    throw new Error(
      "non-loopback binding requires an authentication token of at least 24 characters",
    );
  }
  if (remoteExposure && allowedOrigins.length === 0) {
    throw new Error("non-loopback binding requires an explicit Origin allowlist");
  }

  const repositoryRoot = resolve(
    env.YVEX_OPERATOR_REPOSITORY_ROOT || cwd,
    env.YVEX_OPERATOR_REPOSITORY_ROOT ? "." : "../..",
  );
  const repositoryBuildCandidate = resolve(
    env.YVEX_OPERATOR_REPOSITORY_BIN || repositoryRoot,
    env.YVEX_OPERATOR_REPOSITORY_BIN ? "." : "yvex",
  );
  const knownBuildCandidates = [
    resolve(repositoryRoot, "build", "yvex"),
    resolve(repositoryRoot, "build", "bin", "yvex"),
    resolve(repositoryRoot, "bin", "yvex"),
  ];

  return {
    host,
    port: boundedInteger(env.YVEX_OPERATOR_PORT, 4317, 1, 65_535),
    bindMode: remoteExposure ? "remote" : "loopback",
    remoteExposure,
    authToken,
    allowedOrigins,
    binaryEnvironmentCandidate: env.YVEX_BIN?.trim() || null,
    binarySearchPath: env.PATH || "",
    binaryLookupTtlMs: boundedInteger(env.YVEX_OPERATOR_BINARY_TTL_MS, 5_000, 250, 60_000),
    repositoryRoot,
    repositoryBuildCandidate,
    knownBuildCandidates,
    configDirectory: configDirectory(env),
    modelsRoot: env.YVEX_MODELS_ROOT || undefined,
    immutableTimeoutMs: boundedInteger(env.YVEX_OPERATOR_IMMUTABLE_TIMEOUT_MS, 4_000, 50, 30_000),
    mutableTimeoutMs: boundedInteger(env.YVEX_OPERATOR_MUTABLE_TIMEOUT_MS, 5_000, 50, 30_000),
    maxOutputBytes: boundedInteger(env.YVEX_OPERATOR_MAX_OUTPUT_BYTES, 524_288, 1_024, 4_194_304),
    mutableTtlMs: boundedInteger(env.YVEX_OPERATOR_MUTABLE_TTL_MS, 5_000, 250, 60_000),
    childEnvironment: constrainedChildEnvironment(env),
    allowRemoteProviders: configuredBoolean(env.YVEX_OPERATOR_ALLOW_REMOTE_PROVIDERS),
    eventRetention: boundedInteger(env.YVEX_OPERATOR_EVENT_RETENTION, 200, 20, 2_000),
    sessionRetention: boundedInteger(env.YVEX_OPERATOR_SESSION_RETENTION, 50, 1, 500),
  };
}

/** Splits the trusted PATH once for resolver candidate generation and drops empty entries. */
export function binaryPathEntries(config: OperatorConfig): string[] {
  return config.binarySearchPath.split(delimiter).filter(Boolean);
}
