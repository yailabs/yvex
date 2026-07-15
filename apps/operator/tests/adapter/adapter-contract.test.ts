/*
 * Owner: apps/operator adapter contract validation.
 * Owns: malformed JSON, exit, timeout, output cap, binary, injection, cache, cancellation, and refusal tests.
 * Does not own: real YVEX execution, source scans, model files, frontend rendering, or production fixtures.
 * Invariants: every child process is the explicit tests/fixtures/fake-yvex.mjs executable.
 * Boundary: passing tests proves constrained adaptation only.
 */
import { access, mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterEach, describe, expect, it } from "vitest";

import { ForbiddenProducerError, OperatorAdapter } from "../../server/adapter.ts";
import { testConfig } from "../helpers.ts";

const temporaryDirectories: string[] = [];

async function temporaryDirectory(): Promise<string> {
  const directory = await mkdtemp(join(tmpdir(), "yvex-operator-test-"));
  temporaryDirectories.push(directory);
  return directory;
}

async function exists(path: string): Promise<boolean> {
  try {
    await access(path);
    return true;
  } catch {
    return false;
  }
}

afterEach(async () => {
  await Promise.all(
    temporaryDirectories
      .splice(0)
      .map((directory) => rm(directory, { recursive: true, force: true })),
  );
});

describe("constrained adapter", () => {
  it("returns schema-validated JSON with exit provenance", async () => {
    const result = await new OperatorAdapter(testConfig()).get("releaseDecision");
    expect(result.availability).toBe("available");
    expect(result.lastExit).toMatchObject({ code: 0, state: "ok" });
    expect(result.data?.selected_target_id).toBe("deepseek4-v4-flash");
  });

  it("blocks malformed JSON", async () => {
    const result = await new OperatorAdapter(testConfig("malformed")).get("releaseDecision");
    expect(result.availability).toBe("blocked");
    expect(result.data).toBeNull();
    expect(result.lastExit.state).toBe("malformed-json");
  });

  it("preserves non-zero CLI exit codes", async () => {
    const result = await new OperatorAdapter(testConfig("nonzero")).get("releaseDecision");
    expect(result.availability).toBe("unavailable");
    expect(result.lastExit).toMatchObject({ code: 3, state: "failed" });
    expect(result.issue?.summary).toContain("typed producer refusal");
  });

  it("terminates a timed-out producer", async () => {
    const result = await new OperatorAdapter(testConfig("timeout", { immutableTimeoutMs: 50 })).get(
      "releaseDecision",
    );
    expect(result.availability).toBe("blocked");
    expect(result.lastExit.state).toBe("timeout");
  });

  it("terminates oversized output", async () => {
    const result = await new OperatorAdapter(
      testConfig("oversized", { maxOutputBytes: 1_024 }),
    ).get("releaseDecision");
    expect(result.availability).toBe("blocked");
    expect(result.lastExit.state).toBe("oversized-output");
  });

  it("reports an unavailable configured binary without fallback", async () => {
    const directory = await temporaryDirectory();
    const result = await new OperatorAdapter(
      testConfig("default", { binaryRequest: join(directory, "missing-yvex") }),
    ).get("targetCatalog");
    expect(result.availability).toBe("unavailable");
    expect(result.lastExit).toEqual({ code: null, state: "unavailable-binary", durationMs: null });
  });

  it("rejects forbidden command selection", async () => {
    const adapter = new OperatorAdapter(testConfig());
    await expect(adapter.get("releaseDecision --help")).rejects.toBeInstanceOf(
      ForbiddenProducerError,
    );
  });

  it("passes trusted path configuration as a literal argument with shell disabled", async () => {
    const directory = await temporaryDirectory();
    const marker = join(directory, "injected-marker");
    const adapter = new OperatorAdapter(
      testConfig("default", { modelsRoot: `unused; touch ${marker}` }),
    );
    const result = await adapter.get("artifactInventory");
    expect(result.availability).toBe("available");
    expect(await exists(marker)).toBe(false);
    expect(result.data?.artifacts[0]?.path).toBe("[local]/deepseek-selected.gguf");
  });

  it("caches immutable evidence and expires mutable evidence", async () => {
    const directory = await temporaryDirectory();
    const counter = join(directory, "count");
    let now = 1_000;
    const config = testConfig("default", {
      mutableTtlMs: 100,
      childEnvironment: {
        PATH: process.env.PATH,
        LC_ALL: "C",
        YVEX_FAKE_COUNT_FILE: counter,
      },
    });
    const adapter = new OperatorAdapter(config, { clock: () => now });
    await adapter.get("artifactInventory");
    const cached = await adapter.get("artifactInventory");
    expect(cached.fromCache).toBe(true);
    expect(await readFile(counter, "utf8")).toBe("1");
    now += 101;
    const refreshed = await adapter.get("artifactInventory");
    expect(refreshed.fromCache).toBe(false);
    expect(await readFile(counter, "utf8")).toBe("2");
  });

  it("propagates typed refusal state and exit five", async () => {
    const result = await new OperatorAdapter(testConfig("refusal")).get("releaseDecision");
    expect(result.availability).toBe("blocked");
    expect(result.lastExit).toMatchObject({ code: 5, state: "refused" });
    expect(result.issue?.refusalState).toBe("source-verification-blocked");
    expect(result.data?.source_verification).toBe("blocked");
  });

  it("cancels a producer when the client request is abandoned", async () => {
    const adapter = new OperatorAdapter(testConfig("timeout", { immutableTimeoutMs: 1_000 }));
    const controller = new AbortController();
    const pending = adapter.get("releaseDecision", controller.signal);
    setTimeout(() => controller.abort(), 20);
    const result = await pending;
    expect(result.lastExit.state).toBe("cancelled");
  });
});
