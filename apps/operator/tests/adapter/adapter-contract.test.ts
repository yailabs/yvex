/*
 * Owner: apps/operator adapter and resolver integration validation.
 * Owns: candidate ordering, filesystem admission, identity compatibility, process limits, typed producer parsing, cache, refusal, and allowlist assertions.
 * Does not own: HTTP routing, browser behavior, external providers, or native runtime claims.
 * Invariants: every child process is the explicit test fixture and every temporary path is removed.
 * Boundary: fixture producer success proves adapter behavior only.
 */
import { chmod, copyFile, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterEach, describe, expect, it } from "vitest";

import { ForbiddenProducerError } from "../../server/adapter.ts";
import { createOperatorServices } from "../../server/services.ts";
import { createTestHarness, fakeYvexPath, testConfig, type TestHarness } from "../helpers.ts";

const harnesses: TestHarness[] = [];
const directories: string[] = [];

afterEach(async () => {
  await Promise.all(harnesses.splice(0).map((harness) => harness.close()));
  await Promise.all(
    directories.splice(0).map((directory) => rm(directory, { recursive: true, force: true })),
  );
});

describe("deterministic YVEX resolution", () => {
  it("selects persisted settings before environment and exposes only redacted paths", async () => {
    const root = await mkdtemp(join(tmpdir(), "yvex-resolver-order-"));
    directories.push(root);
    const persisted = join(root, "persisted-yvex");
    await copyFile(fakeYvexPath, persisted);
    await chmod(persisted, 0o755);
    const services = createOperatorServices(
      testConfig("default", { configDirectory: join(root, "config") }),
    );
    await services.settings.updateYvex({ binaryPath: persisted });
    const resolved = await services.resolver.resolve(true);
    expect(resolved.executablePath).toBe(persisted);
    expect(resolved.resolution.candidates.slice(0, 2).map((candidate) => candidate.source)).toEqual(
      ["persisted-setting", "environment"],
    );
    expect(resolved.resolution.candidates[0]).toMatchObject({
      identityStatus: "ready",
      version: "0.1.0",
    });
    expect(JSON.stringify(resolved.resolution)).not.toContain(root);
  });

  it("reports an exact unresolved state when every trusted candidate is missing", async () => {
    const harness = await createTestHarness({
      config: { binaryEnvironmentCandidate: null, binarySearchPath: "", knownBuildCandidates: [] },
    });
    harnesses.push(harness);
    const resolved = await harness.services.resolver.resolve(true);
    expect(resolved.executablePath).toBeNull();
    expect(resolved.resolution.availability).toMatchObject({
      status: "unavailable",
      reasonCode: "yvex-binary-unresolved",
    });
    expect(resolved.resolution.candidates.every((candidate) => candidate.exists === false)).toBe(
      true,
    );
  });

  it("refuses a present non-executable file before identity probing", async () => {
    const root = await mkdtemp(join(tmpdir(), "yvex-resolver-mode-"));
    directories.push(root);
    const candidate = join(root, "yvex");
    await writeFile(candidate, "not executable", { mode: 0o600 });
    const services = createOperatorServices(
      testConfig("default", {
        configDirectory: join(root, "config"),
        binaryEnvironmentCandidate: candidate,
      }),
    );
    const resolved = await services.resolver.resolve(true);
    expect(resolved.executablePath).toBeNull();
    expect(resolved.resolution.candidates[0]).toMatchObject({
      exists: true,
      regularFile: true,
      executable: false,
      identityStatus: "not-probed",
      rejectionReason: "candidate is not executable",
    });
  });

  it("blocks an executable with an incompatible identity protocol", async () => {
    const harness = await createTestHarness({ scenario: "incompatible-identity" });
    harnesses.push(harness);
    const resolved = await harness.services.resolver.resolve(true);
    expect(resolved.executablePath).toBeNull();
    expect(resolved.resolution.availability.status).toBe("blocked");
    expect(resolved.resolution.candidates[0]?.identityStatus).toBe("incompatible");
  });

  it("admits the exact versioned machine-readable identity fixture", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const resolved = await harness.services.resolver.resolve(true);
    expect(resolved.resolution.availability.status).toBe("ready");
    expect(resolved.resolution.identity).toEqual({
      schemaVersion: "1",
      protocolVersion: "1",
      yvexVersion: "0.1.0",
      product: "yvex",
    });
  });
});

describe("typed producer execution", () => {
  it("returns typed data and redacts local artifact paths", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const catalog = await harness.services.adapter.get("target-catalog");
    const artifacts = await harness.services.adapter.get("artifact-inventory");
    expect(catalog.availability.status).toBe("ready");
    expect(catalog.data?.targets[0]?.target_id).toBe("deepseek4-v4-flash");
    expect(artifacts.data?.artifacts[0]?.path).toBe("[local]/deepseek-selected.gguf");
    expect(JSON.stringify(artifacts)).not.toContain("/private/model");
  });

  it("classifies timeout, malformed JSON, oversized output, and typed refusal", async () => {
    const timeout = await createTestHarness({
      scenario: "timeout",
      config: { immutableTimeoutMs: 40 },
    });
    const malformed = await createTestHarness({ scenario: "malformed" });
    const oversized = await createTestHarness({
      scenario: "oversized",
      config: { maxOutputBytes: 1_024 },
    });
    const refusal = await createTestHarness({ scenario: "refusal" });
    harnesses.push(timeout, malformed, oversized, refusal);
    expect((await timeout.services.adapter.get("release-decision")).exit.state).toBe("timeout");
    expect((await malformed.services.adapter.get("release-decision")).exit.state).toBe(
      "malformed-json",
    );
    expect((await oversized.services.adapter.get("release-decision")).exit.state).toBe(
      "oversized-output",
    );
    const refused = await refusal.services.adapter.get("release-decision");
    expect(refused).toMatchObject({
      availability: { status: "blocked" },
      exit: { code: 5, state: "refused" },
      refusal: { code: "yvex-cli-refusal", state: "source-verification-blocked" },
    });
  });

  it("success-caches immutable results and never converts malformed output into success", async () => {
    const root = await mkdtemp(join(tmpdir(), "yvex-adapter-cache-"));
    directories.push(root);
    const count = join(root, "count");
    const config = testConfig("default", {
      configDirectory: join(root, "config"),
      childEnvironment: {
        PATH: process.env.PATH,
        LC_ALL: "C",
        YVEX_FAKE_SCENARIO: "default",
        YVEX_FAKE_COUNT_FILE: count,
      },
    });
    const services = createOperatorServices(config);
    const first = await services.adapter.get("target-catalog");
    const second = await services.adapter.get("target-catalog");
    expect(first.cache.hit).toBe(false);
    expect(second.cache.hit).toBe(true);
    expect(await readFile(count, "utf8")).toBe("1");

    const malformed = await createTestHarness({ scenario: "malformed" });
    harnesses.push(malformed);
    expect((await malformed.services.adapter.get("target-catalog")).availability.status).toBe(
      "failed",
    );
    expect((await malformed.services.adapter.get("target-catalog")).cache.hit).toBe(false);
  });

  it("rejects arbitrary producer selectors before a process can run", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    await expect(
      harness.services.adapter.runExplicit("release-decision; uname -a"),
    ).rejects.toBeInstanceOf(ForbiddenProducerError);
    await expect(harness.services.adapter.get("../../bin/sh")).rejects.toBeInstanceOf(
      ForbiddenProducerError,
    );
    expect(harness.services.adapter.producerRuns()).toEqual([]);
  });

  it("records explicit producer work as a completed indeterminate job", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const run = await harness.services.adapter.runExplicit("release-decision");
    const job = harness.services.jobs.get(run.jobId);
    expect(run.envelope?.availability.status).toBe("ready");
    expect(job).toMatchObject({
      state: "completed",
      progress: null,
      executionOwner: "Operator adapter",
    });
  });
});
