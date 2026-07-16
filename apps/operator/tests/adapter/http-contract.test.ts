/*
 * Owner: apps/operator versioned API integration validation.
 * Owns: health, workspace, capabilities, settings, allowlisted producers, remote authorization, comparison testing/SSE/cancellation, and structured failures.
 * Does not own: browser layout, external network traffic, native runtime execution, or production credentials.
 * Invariants: all listeners are ephemeral loopback servers and every comparison response is a deterministic stream double.
 * Boundary: API success proves control-plane integration, not native inference.
 */
import { afterEach, describe, expect, it } from "vitest";

import {
  chatStreamEventSchema,
  operatorWorkspaceSchema,
  type ChatSession,
  type ChatStreamEvent,
} from "../../shared/contracts.ts";
import {
  createTestHarness,
  fakeProviderModel,
  fakeYvexPath,
  providerFetcher,
  readyComparisonEndpoint,
  type TestHarness,
} from "../helpers.ts";

const harnesses: TestHarness[] = [];

afterEach(async () => {
  await Promise.all(harnesses.splice(0).map((harness) => harness.close()));
});

/** Sends one bounded JSON mutation to the ephemeral same-origin API. */
function jsonRequest(
  baseUrl: string,
  path: string,
  method: "POST" | "PATCH",
  body: unknown,
): Promise<Response> {
  return fetch(`${baseUrl}${path}`, {
    method,
    headers: { Accept: "application/json", "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
}

/** Parses typed Operator SSE frames incrementally and optionally performs work between frames. */
async function readChatEvents(
  response: Response,
  onEvent?: (event: ChatStreamEvent) => void | Promise<void>,
): Promise<ChatStreamEvent[]> {
  if (!response.body) throw new Error("test response has no stream");
  const reader: ReadableStreamDefaultReader<Uint8Array> = response.body.getReader();
  const decoder = new TextDecoder();
  const events: ChatStreamEvent[] = [];
  let buffer = "";
  while (true) {
    const chunk = await reader.read();
    if (chunk.done) break;
    buffer += decoder.decode(chunk.value, { stream: true }).replaceAll("\r", "");
    let boundary = buffer.indexOf("\n\n");
    while (boundary >= 0) {
      const frame = buffer.slice(0, boundary);
      buffer = buffer.slice(boundary + 2);
      const data = frame
        .split("\n")
        .filter((line) => line.startsWith("data:"))
        .map((line) => line.slice(5).trimStart())
        .join("\n");
      if (data) {
        const event = chatStreamEventSchema.parse(JSON.parse(data) as unknown);
        events.push(event);
        await onEvent?.(event);
      }
      boundary = buffer.indexOf("\n\n");
    }
  }
  return events;
}

/** Creates one reference-comparison session through the isolated diagnostics API. */
async function createReferenceSession(harness: TestHarness): Promise<ChatSession> {
  await readyComparisonEndpoint(harness.services);
  const response = await jsonRequest(
    harness.baseUrl,
    "/api/v1/comparison/reference/sessions",
    "POST",
    {
      model: fakeProviderModel,
    },
  );
  expect(response.status).toBe(201);
  return response.json() as Promise<ChatSession>;
}

const parameters = {
  model: fakeProviderModel,
  maxOutputTokens: 32,
  temperature: 0.2,
  topP: 0.9,
  seed: 7,
  stop: [],
  stream: true,
} as const;

describe("system, settings, and producer API", () => {
  it("serves the SPA plus a layered health topology and complete manifest", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    expect((await fetch(`${harness.baseUrl}/`)).status).toBe(200);
    const response = await fetch(`${harness.baseUrl}/api/v1/system/health`);
    expect(response.status).toBe(200);
    expect(response.headers.get("content-security-policy")).toContain("default-src 'self'");
    const health = (await response.json()) as {
      topology: Array<{ id: string; status: string }>;
      adapter: { bindMode: string };
    };
    expect(health.adapter.bindMode).toBe("loopback");
    expect(health.topology.find((node) => node.id === "adapter-process")?.status).toBe("ready");
    expect(health.topology.find((node) => node.id === "yvex-generation")?.status).toBe(
      "unsupported",
    );
    expect(health.topology.find((node) => node.id === "reference-reachable")).toBeUndefined();
    const manifest = (await (await fetch(`${harness.baseUrl}/api/v1/capabilities`)).json()) as {
      capabilities: Array<{ id: string }>;
    };
    expect(new Set(manifest.capabilities.map((capability) => capability.id)).size).toBe(
      manifest.capabilities.length,
    );
    expect(manifest.capabilities.map((capability) => capability.id)).not.toContain(
      "provider.streaming",
    );
  });

  it("serves one authoritative workspace and validates typed target selection", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const initial = await fetch(`${harness.baseUrl}/api/v1/workspace`);
    expect(initial.status).toBe(200);
    expect(await initial.json()).toMatchObject({
      workspaceIdentity: { id: "local-default", authority: "operator-workspace" },
      activeTarget: { id: "deepseek4-v4-flash", kind: "release-target" },
      targetSelectionSource: "release-default",
      activeBuild: {
        targetId: "deepseek4-v4-flash",
        currentStage: "architecture",
      },
      activeArtifact: null,
      activeBackend: null,
      activeRuntimeSession: null,
    });
    const targets = (await (await fetch(`${harness.baseUrl}/api/v1/targets`)).json()) as {
      targets: Array<{ id: string; kind: string }>;
    };
    expect(targets.targets).toEqual(
      expect.arrayContaining([
        expect.objectContaining({ id: "deepseek4-v4-flash", kind: "release-target" }),
        expect.objectContaining({ id: "qwen3-8b", kind: "source-candidate" }),
      ]),
    );
    const artifactInventory = (await (
      await fetch(`${harness.baseUrl}/api/v1/workspace/artifacts`)
    ).json()) as {
      artifacts: Array<{ id: string; classification: string }>;
    };
    expect(artifactInventory.artifacts).toEqual(
      expect.arrayContaining([
        expect.objectContaining({ classification: "proof" }),
        expect.objectContaining({ classification: "unclassified" }),
      ]),
    );
    expect(JSON.stringify(artifactInventory)).not.toContain("/private/model");
    const inadmissibleArtifact = await jsonRequest(
      harness.baseUrl,
      "/api/v1/workspace/artifact",
      "POST",
      { artifactId: "artifact:deepseek4-v4-flash:planned-full-gguf" },
    );
    expect(inadmissibleArtifact.status).toBe(409);
    expect(await inadmissibleArtifact.json()).toMatchObject({ code: "artifact-not-admissible" });
    const selected = await jsonRequest(harness.baseUrl, "/api/v1/workspace/target", "POST", {
      targetId: "qwen3-8b",
    });
    expect(selected.status).toBe(200);
    expect(await selected.json()).toMatchObject({
      activeTarget: { id: "qwen3-8b", kind: "source-candidate" },
      targetSelectionSource: "operator-selection",
      activeBuild: null,
    });
    const persisted = operatorWorkspaceSchema.parse(
      await (await fetch(`${harness.baseUrl}/api/v1/workspace`)).json(),
    );
    expect(persisted).toMatchObject({ activeTarget: { id: "qwen3-8b" } });
    const rejected = await jsonRequest(harness.baseUrl, "/api/v1/workspace/target", "POST", {
      targetId: "deepseek4-v4-flash --output json",
    });
    expect(rejected.status).toBe(409);
    expect(await rejected.json()).toMatchObject({ code: "target-not-catalogued" });
  });

  it("validates YVEX settings server-side and never returns absolute paths", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const invalid = await jsonRequest(harness.baseUrl, "/api/v1/settings/yvex", "PATCH", {
      binaryPath: "../../bin/sh",
    });
    expect(invalid.status).toBe(400);
    expect(await invalid.json()).toMatchObject({ code: "invalid-yvex-binary" });
    const saved = await jsonRequest(harness.baseUrl, "/api/v1/settings/yvex", "PATCH", {
      binaryPath: fakeYvexPath,
    });
    expect(saved.status).toBe(200);
    const body = JSON.stringify(await saved.json());
    expect(body).toContain("[local]/fake-yvex.mjs");
    expect(body).not.toContain(fakeYvexPath);
  });

  it("persists write-only provider credentials, tests models and streaming, and returns redacted state", async () => {
    let compatibilityRequest: Record<string, unknown> | null = null;
    const harness = await createTestHarness({
      dependencies: {
        fetcher: providerFetcher({
          lineEnding: "\r\n",
          onRequest: (url, init) => {
            if (url.pathname.endsWith("/chat/completions") && typeof init?.body === "string") {
              compatibilityRequest = JSON.parse(init.body) as Record<string, unknown>;
            }
          },
        }),
      },
    });
    harnesses.push(harness);
    const saved = await jsonRequest(
      harness.baseUrl,
      "/api/v1/settings/comparison-endpoint",
      "PATCH",
      {
        enabled: true,
        displayName: "Fixture reference",
        baseUrl: "http://127.0.0.1:14318/v1",
        apiKey: "private-provider-key",
        defaultModel: fakeProviderModel,
        requestTimeoutMs: 5_000,
      },
    );
    expect(saved.status).toBe(200);
    const savedBody = JSON.stringify(await saved.json());
    expect(savedBody).toContain('"apiKeyConfigured":true');
    expect(savedBody).not.toContain("private-provider-key");
    const tested = await jsonRequest(
      harness.baseUrl,
      "/api/v1/settings/comparison-endpoint/test",
      "POST",
      {},
    );
    expect(tested.status).toBe(200);
    expect(await tested.json()).toMatchObject({
      status: { reachable: true, streamingCompatible: true, models: [fakeProviderModel] },
    });
    expect(compatibilityRequest).toMatchObject({
      max_tokens: 16,
      messages: [{ content: "Reply with OK only. Do not reason or explain." }],
      stream: true,
    });
    expect(
      await (await fetch(`${harness.baseUrl}/api/v1/comparison/reference/models`)).json(),
    ).toEqual({ models: [fakeProviderModel] });
  });

  it("refuses an oversized provider model response before JSON parsing", async () => {
    const oversizedFetcher: typeof fetch = () =>
      Promise.resolve(
        new Response("x".repeat(1_048_577), {
          status: 200,
          headers: { "Content-Type": "application/json" },
        }),
      );
    const harness = await createTestHarness({ dependencies: { fetcher: oversizedFetcher } });
    harnesses.push(harness);
    await harness.services.settings.updateComparisonEndpoint({
      enabled: true,
      displayName: "Bounded fixture",
      baseUrl: "http://127.0.0.1:14318/v1",
      defaultModel: fakeProviderModel,
      requestTimeoutMs: 5_000,
    });

    const response = await fetch(`${harness.baseUrl}/api/v1/comparison/reference/models`);
    expect(response.status).toBe(400);
    expect(await response.json()).toMatchObject({ code: "comparison-response-oversized" });
  });

  it("runs only registry-owned producers and records typed jobs/runs", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const registry = (await (await fetch(`${harness.baseUrl}/api/v1/producers`)).json()) as {
      producers: Array<{ id: string; availability: { status: string } }>;
    };
    expect(registry.producers.map((producer) => producer.id)).toEqual([
      "target-catalog",
      "release-decision",
      "target-detail",
      "artifact-inventory",
    ]);
    expect(registry.producers.every((producer) => producer.availability.status === "ready")).toBe(
      true,
    );
    const run = await jsonRequest(
      harness.baseUrl,
      "/api/v1/producers/release-decision/run",
      "POST",
      {},
    );
    expect(run.status).toBe(200);
    expect(await run.json()).toMatchObject({
      producerId: "release-decision",
      envelope: { exit: { state: "ok" } },
    });
    const injection = await jsonRequest(
      harness.baseUrl,
      "/api/v1/producers/release-decision%3Buname/run",
      "POST",
      {},
    );
    expect(injection.status).toBe(404);
    expect(await injection.json()).toMatchObject({ code: "producer-not-found" });
    const jobs = (await (await fetch(`${harness.baseUrl}/api/v1/jobs`)).json()) as {
      jobs: Array<{ state: string; progress: number | null }>;
    };
    expect(jobs.jobs[0]).toMatchObject({ state: "completed", progress: null });
  });

  it("requires exact Origin and bearer authentication only in explicit remote mode", async () => {
    const token = "remote-test-token-longer-than-24-chars";
    const origin = "https://operator.example";
    const harness = await createTestHarness({
      config: {
        host: "0.0.0.0",
        bindMode: "remote",
        remoteExposure: true,
        authToken: token,
        allowedOrigins: [origin],
      },
    });
    harnesses.push(harness);
    expect((await fetch(`${harness.baseUrl}/api/v1/version`)).status).toBe(403);
    const preflight = await fetch(`${harness.baseUrl}/api/v1/version`, {
      method: "OPTIONS",
      headers: { Origin: origin },
    });
    expect(preflight.status).toBe(204);
    const admitted = await fetch(`${harness.baseUrl}/api/v1/version`, {
      headers: { Origin: origin, Authorization: `Bearer ${token}` },
    });
    expect(admitted.status).toBe(200);
    expect(admitted.headers.get("access-control-allow-origin")).toBe(origin);
  });
});

describe("isolated external reference-comparison API", () => {
  it("streams deltas, usage, timings, completion metadata, and a completed job", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const session = await createReferenceSession(harness);
    const response = await jsonRequest(
      harness.baseUrl,
      `/api/v1/comparison/reference/sessions/${session.id}/messages`,
      "POST",
      {
        content: "hello reference",
        parameters,
      },
    );
    expect(response.status).toBe(200);
    expect(response.headers.get("content-type")).toContain("text/event-stream");
    const events = await readChatEvents(response);
    expect(events.map((event) => event.type)).toEqual([
      "request-started",
      "token-delta",
      "token-delta",
      "usage-update",
      "timing-update",
      "completion",
    ]);
    const completed = events.find(
      (event): event is Extract<ChatStreamEvent, { type: "completion" }> =>
        event.type === "completion",
    );
    expect(completed?.session.messages.at(-1)?.metadata).toMatchObject({
      executionOwner: "External comparison endpoint",
      provider: "Fixture reference",
      model: fakeProviderModel,
      usage: { inputTokens: 4, outputTokens: 2, totalTokens: 6 },
    });
    expect(completed?.session.timings.totalDurationMs).not.toBeNull();
    const jobs = harness.services.jobs.list();
    expect(jobs.find((job) => job.type === "reference-comparison")).toMatchObject({
      state: "completed",
      progress: null,
    });
  });

  it("cancels an active stream and preserves partial provider output", async () => {
    const harness = await createTestHarness({
      dependencies: { fetcher: providerFetcher({ delayMs: 60 }) },
    });
    harnesses.push(harness);
    const session = await createReferenceSession(harness);
    let requestId: string | null = null;
    let cancellationSent = false;
    const response = await jsonRequest(
      harness.baseUrl,
      `/api/v1/comparison/reference/sessions/${session.id}/messages`,
      "POST",
      {
        content: "slow cancel request",
        parameters,
      },
    );
    const events = await readChatEvents(response, async (event) => {
      if (event.type === "request-started") requestId = event.requestId;
      if (event.type === "token-delta" && requestId && !cancellationSent) {
        cancellationSent = true;
        const cancelled = await jsonRequest(
          harness.baseUrl,
          `/api/v1/comparison/reference/requests/${requestId}/cancel`,
          "POST",
          {},
        );
        expect(cancelled.status).toBe(202);
      }
    });
    const cancelled = events.find(
      (event): event is Extract<ChatStreamEvent, { type: "cancellation" }> =>
        event.type === "cancellation",
    );
    const assistant = cancelled?.session.messages.at(-1);
    expect(assistant?.state).toBe("cancelled");
    expect(assistant?.content).toBe("Partial ");
    expect(
      harness.services.jobs.list().find((job) => job.type === "reference-comparison")?.state,
    ).toBe("cancelled");
  });

  it("returns structured provider failure events and never relabels them native", async () => {
    const harness = await createTestHarness({
      dependencies: { fetcher: providerFetcher({ failChat: true }) },
    });
    harnesses.push(harness);
    await harness.services.settings.updateComparisonEndpoint({
      enabled: true,
      displayName: "Fixture reference",
      baseUrl: "http://127.0.0.1:14318/v1",
      defaultModel: fakeProviderModel,
      requestTimeoutMs: 5_000,
    });
    const sessionResponse = await jsonRequest(
      harness.baseUrl,
      "/api/v1/comparison/reference/sessions",
      "POST",
      { model: fakeProviderModel },
    );
    const session = (await sessionResponse.json()) as ChatSession;
    const response = await jsonRequest(
      harness.baseUrl,
      `/api/v1/comparison/reference/sessions/${session.id}/messages`,
      "POST",
      { content: "fail", parameters },
    );
    const events = await readChatEvents(response);
    expect(events.at(-1)).toMatchObject({
      type: "structured-error",
      error: { code: "comparison-generation-failed" },
    });
    expect(JSON.stringify(events)).not.toContain('"executionOwner":"YVEX"');
  });

  it("has no primary chat route or selectable YVEX/provider lane", async () => {
    let providerCalls = 0;
    const harness = await createTestHarness({
      dependencies: {
        fetcher: providerFetcher({
          onRequest: () => {
            providerCalls += 1;
          },
        }),
      },
    });
    harnesses.push(harness);
    const primaryRoute = await jsonRequest(harness.baseUrl, "/api/v1/chat/sessions", "POST", {});
    expect(primaryRoute.status).toBe(404);
    const injectedLane = await jsonRequest(
      harness.baseUrl,
      "/api/v1/comparison/reference/sessions",
      "POST",
      { lane: "native-yvex", model: fakeProviderModel },
    );
    expect(injectedLane.status).toBe(400);
    expect(await injectedLane.json()).toMatchObject({ code: "invalid-request" });
    expect(providerCalls).toBe(0);
  });
});
