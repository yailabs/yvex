/*
 * Owner: apps/operator constrained process boundary.
 * Owns: shell-free child execution, timeout, cancellation, bounded capture, and exit observations.
 * Does not own: command selection, schema validation, cache policy, rendering, or YVEX facts.
 * Invariants: executable and argument arrays are passed directly to spawn with shell disabled.
 * Boundary: this runner cannot accept browser command text or execute write-capable commands.
 */
import { spawn } from "node:child_process";

export type RunFailure = "timeout" | "cancelled" | "oversized-output" | "spawn-failure";

export interface RunRequest {
  executablePath: string;
  args: readonly string[];
  timeoutMs: number;
  maxOutputBytes: number;
  environment: NodeJS.ProcessEnv;
  signal?: AbortSignal;
}

export interface RunResult {
  code: number | null;
  stdout: string;
  stderr: string;
  durationMs: number;
  failure?: RunFailure;
}

function appendBounded(chunks: Buffer[], chunk: Buffer, used: number, limit: number): number {
  const remaining = Math.max(0, limit - used);
  if (remaining > 0) chunks.push(chunk.subarray(0, remaining));
  return used + chunk.byteLength;
}

/** Executes one already-audited argument vector with bounded IO and deterministic cleanup. */
export function runConstrainedProcess(request: RunRequest): Promise<RunResult> {
  const started = performance.now();
  if (request.signal?.aborted) {
    return Promise.resolve({
      code: null,
      stdout: "",
      stderr: "",
      durationMs: 0,
      failure: "cancelled",
    });
  }

  return new Promise((resolve) => {
    const stdoutChunks: Buffer[] = [];
    const stderrChunks: Buffer[] = [];
    let outputBytes = 0;
    let settled = false;
    let failure: RunFailure | undefined;
    let forceKillTimer: NodeJS.Timeout | undefined;

    const child = spawn(request.executablePath, [...request.args], {
      env: request.environment,
      shell: false,
      stdio: ["ignore", "pipe", "pipe"],
      windowsHide: true,
    });

    const terminate = (reason: RunFailure): void => {
      if (settled || failure) return;
      failure = reason;
      child.kill("SIGTERM");
      forceKillTimer = setTimeout(() => child.kill("SIGKILL"), 250);
      forceKillTimer.unref();
    };

    const timeout = setTimeout(() => terminate("timeout"), request.timeoutMs);
    timeout.unref();
    const abort = (): void => terminate("cancelled");
    request.signal?.addEventListener("abort", abort, { once: true });

    child.stdout.on("data", (chunk: Buffer) => {
      outputBytes = appendBounded(stdoutChunks, chunk, outputBytes, request.maxOutputBytes);
      if (outputBytes > request.maxOutputBytes) terminate("oversized-output");
    });
    child.stderr.on("data", (chunk: Buffer) => {
      outputBytes = appendBounded(stderrChunks, chunk, outputBytes, request.maxOutputBytes);
      if (outputBytes > request.maxOutputBytes) terminate("oversized-output");
    });

    child.once("error", () => {
      failure ??= "spawn-failure";
    });
    child.once("close", (code) => {
      if (settled) return;
      settled = true;
      clearTimeout(timeout);
      if (forceKillTimer) clearTimeout(forceKillTimer);
      request.signal?.removeEventListener("abort", abort);
      resolve({
        code,
        stdout: Buffer.concat(stdoutChunks).toString("utf8"),
        stderr: Buffer.concat(stderrChunks).toString("utf8"),
        durationMs: Math.max(0, Math.round(performance.now() - started)),
        ...(failure ? { failure } : {}),
      });
    });
  });
}
