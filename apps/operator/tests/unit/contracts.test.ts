/*
 * Owner: apps/operator unit contract validation.
 * Owns: identifier guards, security configuration, URL/path admission, settings redaction, jobs, and visual status mapping assertions.
 * Does not own: process integration, HTTP routing, browser geometry, or native YVEX behavior.
 * Invariants: every assertion is deterministic and uses no external network or model file.
 * Boundary: unit success validates control-plane contracts only.
 */
import { readFile, stat } from "node:fs/promises";
import { afterEach, describe, expect, it } from "vitest";

import { isCliProducerId, isDomainId } from "../../shared/contracts.ts";
import { nativeStatus } from "../../server/capabilities.ts";
import { loadOperatorConfig } from "../../server/config.ts";
import { EventHistory } from "../../server/events.ts";
import { JobManager } from "../../server/jobs.ts";
import { producerDefinition } from "../../server/producers.ts";
import { validateProviderBaseUrl } from "../../server/provider.ts";
import { validateTrustedBinaryPath } from "../../server/settings.ts";
import { classifyArtifactClass, classifyTarget } from "../../server/workspace.ts";
import { capabilityDisplayLabel } from "../../src/capability-labels.ts";
import { availabilityTone, machineTone } from "../../src/components/Status.tsx";
import { createTestHarness, type TestHarness } from "../helpers.ts";

const harnesses: TestHarness[] = [];

afterEach(async () => {
  await Promise.all(harnesses.splice(0).map((harness) => harness.close()));
});

describe("closed shared contracts", () => {
  it("admits only canonical producer and domain identifiers", () => {
    expect(isCliProducerId("release-decision")).toBe(true);
    expect(isCliProducerId("release-decision --help")).toBe(false);
    expect(isDomainId("runtime")).toBe(true);
    expect(isDomainId("shell")).toBe(false);
    expect(
      producerDefinition("artifact-inventory")?.buildArgs({
        immutableTimeoutMs: 100,
        mutableTimeoutMs: 100,
        maxOutputBytes: 1_024,
        mutableTtlMs: 250,
      }),
    ).toEqual(["models", "artifacts", "list", "--output", "json"]);
    expect(producerDefinition("../../bin/sh")).toBeNull();
  });

  it("keeps visual status semantics exact and restrained", () => {
    expect(availabilityTone("ready")).toBe("ready");
    expect(availabilityTone("unsupported")).toBe("neutral");
    expect(availabilityTone("failed")).toBe("danger");
    expect(machineTone("unsupported")).toBe("neutral");
    expect(machineTone("complete")).toBe("ready");
  });

  it("classifies only audited target classes and keeps unknown classes explicit", () => {
    expect(classifyTarget("release-source-target")).toEqual({
      kind: "release-target",
      kindLabel: "Release target",
    });
    expect(classifyTarget("selected-runtime-slice").kind).toBe("engineering-slice");
    expect(classifyTarget("source-model-candidate").kind).toBe("source-candidate");
    expect(classifyTarget("future-unreviewed-class").kind).toBe("unclassified");
  });

  it("never promotes an unknown or merely planned artifact class to complete", () => {
    expect(classifyArtifactClass("yvex-selected-gguf")).toBe("proof");
    expect(classifyArtifactClass("complete-model-gguf")).toBe("complete");
    expect(classifyArtifactClass("supported-model-gguf")).toBe("supported");
    expect(classifyArtifactClass("planned-full-gguf")).toBe("unclassified");
    expect(classifyArtifactClass("future-unreviewed-class")).toBe("unclassified");
  });

  it("keeps source intake below verified capability readiness", () => {
    expect(nativeStatus("complete")).toBe("ready");
    expect(nativeStatus("source-intake")).toBe("degraded");
    expect(nativeStatus("future-unreviewed-stage")).toBe("degraded");
  });

  it("uses human capability labels while preserving technical acronyms", () => {
    expect(capabilityDisplayLabel({ id: "backend.cuda", domain: "backend" })).toBe(
      "Backend · CUDA",
    );
    expect(capabilityDisplayLabel({ id: "system.yvex-version-compatible", domain: "system" })).toBe(
      "System · YVEX version compatible",
    );
    expect(capabilityDisplayLabel({ id: "generation.native", domain: "generation" })).toBe(
      "YVEX generation",
    );
  });
});

describe("configuration safety", () => {
  it("defaults to literal loopback with remote exposure disabled", () => {
    const config = loadOperatorConfig(
      { HOME: "/tmp", PATH: "/bin" },
      "/tmp/repository/apps/operator",
    );
    expect(config.host).toBe("127.0.0.1");
    expect(config.remoteExposure).toBe(false);
    expect(config.authToken).toBeNull();
  });

  it("refuses non-loopback binding without every security prerequisite", () => {
    expect(() =>
      loadOperatorConfig({ YVEX_OPERATOR_HOST: "0.0.0.0" }, "/tmp/repository/apps/operator"),
    ).toThrow("YVEX_OPERATOR_REMOTE_MODE=true");
    expect(() =>
      loadOperatorConfig(
        {
          YVEX_OPERATOR_HOST: "0.0.0.0",
          YVEX_OPERATOR_REMOTE_MODE: "true",
          YVEX_OPERATOR_AUTH_TOKEN: "short",
        },
        "/tmp/repository/apps/operator",
      ),
    ).toThrow("at least 24");
    const config = loadOperatorConfig(
      {
        HOME: "/tmp",
        YVEX_OPERATOR_HOST: "0.0.0.0",
        YVEX_OPERATOR_REMOTE_MODE: "true",
        YVEX_OPERATOR_AUTH_TOKEN: "a-secure-test-token-with-24-chars",
        YVEX_OPERATOR_ALLOWED_ORIGINS: "https://operator.example",
      },
      "/tmp/repository/apps/operator",
    );
    expect(config.bindMode).toBe("remote");
    expect(config.allowedOrigins).toEqual(["https://operator.example"]);
  });

  it("admits only absolute trusted binary paths and local provider URLs by default", () => {
    expect(validateTrustedBinaryPath("/opt/yvex/bin/yvex")).toBe("/opt/yvex/bin/yvex");
    expect(() => validateTrustedBinaryPath("../../bin/yvex")).toThrow("absolute");
    expect(() => validateTrustedBinaryPath("/opt/yvex/../bin/yvex")).toThrow("traversal");
    expect(validateProviderBaseUrl("http://127.0.0.1:8080/v1", false).href).toBe(
      "http://127.0.0.1:8080/v1",
    );
    expect(() => validateProviderBaseUrl("http://provider.example/v1", false)).toThrow("disabled");
    expect(() => validateProviderBaseUrl("http://provider.example/v1", true)).toThrow("HTTPS");
  });

  it("persists provider secrets mode-0600 and returns presence only", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    await harness.services.settings.updateComparisonEndpoint({
      enabled: true,
      displayName: "Local fixture",
      baseUrl: "http://127.0.0.1:14318/v1",
      apiKey: "provider-secret-value",
      defaultModel: "fixture-reference-model",
    });
    const snapshot = await harness.services.settings.publicSnapshot();
    expect(snapshot.comparisonEndpoint.apiKeyConfigured).toBe(true);
    expect(JSON.stringify(snapshot)).not.toContain("provider-secret-value");
    const secretPath = `${harness.config.configDirectory}/secrets.json`;
    expect((await stat(secretPath)).mode & 0o777).toBe(0o600);
    expect(await readFile(secretPath, "utf8")).toContain("provider-secret-value");
  });
});

describe("job and event foundations", () => {
  it("uses legal transitions and leaves progress indeterminate", () => {
    let now = Date.parse("2026-07-16T00:00:00.000Z");
    const history = new EventHistory(20, () => now++);
    const jobs = new JobManager(20, history, () => now++);
    const job = jobs.create("producer-run", "Operator adapter", "queued", true);
    expect(job.progress).toBeNull();
    jobs.transition(job.id, "starting", "resolution");
    jobs.transition(job.id, "running", "producer");
    const completed = jobs.transition(job.id, "completed", "complete", {
      resultReference: "run-1",
    });
    expect(completed).toMatchObject({
      state: "completed",
      progress: null,
      cancellable: false,
      resultReference: "run-1",
    });
    expect(() => jobs.transition(job.id, "running", "illegal")).toThrow("invalid job transition");
  });

  it("bounds and redacts local event history", () => {
    const history = new EventHistory(2, () => Date.parse("2026-07-16T00:00:00.000Z"));
    history.record("configuration-reload", "info", "token=secret-one");
    history.record("binary-resolution", "info", "candidate /private/build/yvex");
    history.record("comparison-test", "warning", "comparison unavailable");
    const events = history.recent(10);
    expect(events).toHaveLength(2);
    expect(JSON.stringify(events)).not.toContain("secret-one");
    expect(JSON.stringify(events)).not.toContain("/private/build");
  });
});
