/*
 * Owner: apps/operator unit validation.
 * Owns: route/producer guards, command allowlist, redaction, config refusal, and status-tone assertions.
 * Does not own: process integration, browser smoke, YVEX facts, or production configuration.
 * Invariants: tests execute no native YVEX command and use no model files.
 * Boundary: unit success validates operator contracts only.
 */
import { describe, expect, it } from "vitest";

import { isCliProducerId, isViewId } from "../../shared/contracts.ts";
import { constrainedChildEnvironment, loadOperatorConfig } from "../../server/config.ts";
import { CLI_PRODUCERS, missingProducersForView } from "../../server/producers.ts";
import { sanitizeText, sanitizeValue } from "../../server/redaction.ts";
import { statusTone } from "../../src/components/Status.tsx";

describe("operator guard contracts", () => {
  it("accepts only canonical views and producer identifiers", () => {
    expect(isViewId("quantization")).toBe(true);
    expect(isViewId("quantization/../../shell")).toBe(false);
    expect(isCliProducerId("releaseDecision")).toBe(true);
    expect(isCliProducerId("releaseDecision --help")).toBe(false);
  });

  it("keeps the CLI allowlist immutable and read-only", () => {
    expect(Object.isFrozen(CLI_PRODUCERS)).toBe(true);
    const commands = Object.values(CLI_PRODUCERS).map((producer) =>
      producer.buildArgs({
        immutableTimeoutMs: 1_000,
        mutableTimeoutMs: 1_000,
        maxOutputBytes: 4_096,
        mutableTtlMs: 500,
      }),
    );
    expect(commands).toHaveLength(4);
    expect(commands.flat()).not.toContain("source-manifest");
    expect(commands.flat()).not.toContain("generate");
    expect(commands.flat()).not.toContain("quant-job");
    expect(commands.every((args) => args.includes("json"))).toBe(true);
  });

  it("records unavailable quantization producers explicitly", () => {
    const missing = missingProducersForView("quantization");
    expect(missing.map((producer) => producer.id)).toEqual([
      "qtypePolicyJson",
      "qtypeRoleSupportJson",
      "referenceDequantJson",
    ]);
    expect(missing[0]?.availability).toBe("unsupported");
  });

  it("redacts secrets and absolute paths without changing repository identities", () => {
    expect(sanitizeText("deepseek-ai/DeepSeek-V4-Flash")).toBe("deepseek-ai/DeepSeek-V4-Flash");
    expect(sanitizeText("path=/srv/models/model.gguf")).toBe("path=[local]/model.gguf");
    expect(sanitizeText("token=not-a-real-value")).toBe("[redacted]");
    expect(
      sanitizeValue({ repository: "deepseek-ai/DeepSeek-V4-Flash", secret: "fixture-value" }),
    ).toEqual({ repository: "deepseek-ai/DeepSeek-V4-Flash", secret: "[redacted]" });
  });

  it("refuses external bind configuration and strips secret child environment", () => {
    expect(() => loadOperatorConfig({ YVEX_OPERATOR_HOST: "0.0.0.0" })).toThrow(
      "refuses non-loopback",
    );
    expect(
      constrainedChildEnvironment({
        PATH: "/bin",
        HOME: "/tmp/test-home",
        PROVIDER_TOKEN: "fixture-value",
      }),
    ).toEqual({ PATH: "/bin", HOME: "/tmp/test-home", LC_ALL: "C" });
  });

  it("uses semantic colors without changing raw labels", () => {
    expect(statusTone("complete")).toBe("green");
    expect(statusTone("blocked")).toBe("amber");
    expect(statusTone("unsupported")).toBe("red");
    expect(statusTone("not-measured")).toBe("neutral");
  });
});
