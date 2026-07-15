/*
 * Owner: apps/operator local server configuration.
 * Owns: default-loopback binding, explicit IP binding, bounded resource settings, trusted startup paths, and child environment.
 * Does not own: frontend values, command selection, model discovery, rebuilding, or registry mutation.
 * Invariants: the default is loopback and configured listeners must be literal IPv4 or IPv6 addresses.
 * Boundary: configuration locates an existing executable and never builds YVEX.
 */
import { constants } from "node:fs";
import { access } from "node:fs/promises";
import { isIP } from "node:net";
import { basename, delimiter, isAbsolute, resolve } from "node:path";

import type { ProducerRuntimeConfig } from "./producers.ts";

export const ADAPTER_VERSION = "0.1.0";

export interface OperatorConfig extends ProducerRuntimeConfig {
  host: string;
  port: number;
  binaryRequest: string;
  binarySearchPath: string;
  binaryLookupTtlMs: number;
  childEnvironment: NodeJS.ProcessEnv;
}

export interface LocatedBinary {
  executablePath: string | null;
  label: string;
  resolution: "configured-path" | "path-search" | "not-found";
}

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

/** Selects loopback by default and admits only an explicit literal address for network exposure. */
function bindHost(value: string | undefined): string {
  const host = value?.trim() || "127.0.0.1";
  if (isIP(host) === 0) {
    throw new Error("YVEX operator bind address must be a literal IPv4 or IPv6 address");
  }
  return host;
}

/** Copies only non-secret process settings required by a local read-only YVEX child. */
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

/** Parses trusted startup configuration and performs no filesystem or process IO. */
export function loadOperatorConfig(env: NodeJS.ProcessEnv = process.env): OperatorConfig {
  return {
    host: bindHost(env.YVEX_OPERATOR_HOST),
    port: boundedInteger(env.YVEX_OPERATOR_PORT, 4317, 1, 65_535),
    binaryRequest: env.YVEX_BIN || "yvex",
    binarySearchPath: env.PATH || "",
    modelsRoot: env.YVEX_MODELS_ROOT || undefined,
    immutableTimeoutMs: boundedInteger(env.YVEX_OPERATOR_IMMUTABLE_TIMEOUT_MS, 4_000, 50, 30_000),
    mutableTimeoutMs: boundedInteger(env.YVEX_OPERATOR_MUTABLE_TIMEOUT_MS, 5_000, 50, 30_000),
    maxOutputBytes: boundedInteger(env.YVEX_OPERATOR_MAX_OUTPUT_BYTES, 524_288, 1_024, 4_194_304),
    mutableTtlMs: boundedInteger(env.YVEX_OPERATOR_MUTABLE_TTL_MS, 5_000, 250, 60_000),
    binaryLookupTtlMs: boundedInteger(env.YVEX_OPERATOR_BINARY_TTL_MS, 5_000, 250, 60_000),
    childEnvironment: constrainedChildEnvironment(env),
  };
}

async function executable(path: string): Promise<boolean> {
  try {
    await access(path, constants.X_OK);
    return true;
  } catch {
    return false;
  }
}

/** Locates only the configured existing binary or PATH entry and never invokes a build. */
export async function locateBinary(
  request: string,
  searchPath: string,
  cwd: string = process.cwd(),
): Promise<LocatedBinary> {
  const label = basename(request) || "yvex";
  if (request.includes("/") || isAbsolute(request)) {
    const candidate = isAbsolute(request) ? request : resolve(cwd, request);
    return (await executable(candidate))
      ? { executablePath: candidate, label, resolution: "configured-path" }
      : { executablePath: null, label, resolution: "not-found" };
  }
  for (const entry of searchPath.split(delimiter)) {
    if (!entry) continue;
    const candidate = resolve(entry, request);
    if (await executable(candidate)) {
      return { executablePath: candidate, label, resolution: "path-search" };
    }
  }
  return { executablePath: null, label, resolution: "not-found" };
}
