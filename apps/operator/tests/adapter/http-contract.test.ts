/*
 * Owner: apps/operator versioned API integration validation.
 * Owns: health, capabilities, settings, allowlisted producers, remote authorization, provider testing, chat SSE, cancellation, and structured failures.
 * Does not own: browser layout, external network traffic, native runtime execution, or production credentials.
 * Invariants: all listeners are ephemeral loopback servers and every provider response is a deterministic stream double.
 * Boundary: API success proves control-plane integration, not native inference.
 */
import { afterEach, describe, expect, it } from "vitest";

import {
  chatStreamEventSchema,
  type ChatSession,
  type ChatStreamEvent,
} from "../../shared/contracts.ts";
import {
  createTestHarness,
  fakeProviderModel,
  fakeYvexPath,
  providerFetcher,
  readyReferenceProvider,
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

/** Creates one reference session through the public API after configuring the private provider service. */
async function createReferenceSession(harness: TestHarness): Promise<ChatSession> {
  await readyReferenceProvider(harness.services);
  const response = await jsonRequest(harness.baseUrl, "/api/v1/chat/sessions", "POST", {
    lane: "reference-provider",
    model: fakeProviderModel,
  });
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
    expect(health.topology.find((node) => node.id === "native-generation")?.status).toBe(
      "unsupported",
    );
    expect(health.topology.find((node) => node.id === "reference-reachable")?.status).toBe(
      "unavailable",
    );
    const manifest = (await (await fetch(`${harness.baseUrl}/api/v1/capabilities`)).json()) as {
      capabilities: Array<{ id: string }>;
    };
    expect(new Set(manifest.capabilities.map((capability) => capability.id)).size).toBe(
      manifest.capabilities.length,
    );
    expect(manifest.capabilities.map((capability) => capability.id)).toContain(
      "provider.streaming",
    );
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
    const harness = await createTestHarness({
      dependencies: { fetcher: providerFetcher({ lineEnding: "\r\n" }) },
    });
    harnesses.push(harness);
    const saved = await jsonRequest(
      harness.baseUrl,
      "/api/v1/settings/reference-provider",
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
      "/api/v1/settings/reference-provider/test",
      "POST",
      {},
    );
    expect(tested.status).toBe(200);
    expect(await tested.json()).toMatchObject({
      status: { reachable: true, streamingCompatible: true, models: [fakeProviderModel] },
    });
    expect(
      await (await fetch(`${harness.baseUrl}/api/v1/reference-provider/models`)).json(),
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
    await harness.services.settings.updateReferenceProvider({
      enabled: true,
      displayName: "Bounded fixture",
      baseUrl: "http://127.0.0.1:14318/v1",
      defaultModel: fakeProviderModel,
      requestTimeoutMs: 5_000,
    });

    const response = await fetch(`${harness.baseUrl}/api/v1/reference-provider/models`);
    expect(response.status).toBe(400);
    expect(await response.json()).toMatchObject({ code: "provider-response-oversized" });
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

describe("reference-provider chat API", () => {
  it("streams deltas, usage, timings, completion metadata, and a completed job", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const session = await createReferenceSession(harness);
    const response = await jsonRequest(
      harness.baseUrl,
      `/api/v1/chat/sessions/${session.id}/messages`,
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
      executionOwner: "Reference provider",
      provider: "Fixture reference",
      model: fakeProviderModel,
      usage: { inputTokens: 4, outputTokens: 2, totalTokens: 6 },
    });
    expect(completed?.session.timings.totalDurationMs).not.toBeNull();
    const jobs = harness.services.jobs.list();
    expect(jobs.find((job) => job.type === "provider-chat")).toMatchObject({
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
      `/api/v1/chat/sessions/${session.id}/messages`,
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
          `/api/v1/chat/requests/${requestId}/cancel`,
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
    expect(harness.services.jobs.list().find((job) => job.type === "provider-chat")?.state).toBe(
      "cancelled",
    );
  });

  it("returns structured provider failure events and never relabels them native", async () => {
    const harness = await createTestHarness({
      dependencies: { fetcher: providerFetcher({ failChat: true }) },
    });
    harnesses.push(harness);
    await harness.services.settings.updateReferenceProvider({
      enabled: true,
      displayName: "Fixture reference",
      baseUrl: "http://127.0.0.1:14318/v1",
      defaultModel: fakeProviderModel,
      requestTimeoutMs: 5_000,
    });
    const sessionResponse = await jsonRequest(harness.baseUrl, "/api/v1/chat/sessions", "POST", {
      lane: "reference-provider",
      model: fakeProviderModel,
    });
    const session = (await sessionResponse.json()) as ChatSession;
    const response = await jsonRequest(
      harness.baseUrl,
      `/api/v1/chat/sessions/${session.id}/messages`,
      "POST",
      { content: "fail", parameters },
    );
    const events = await readChatEvents(response);
    expect(events.at(-1)).toMatchObject({
      type: "structured-error",
      error: { code: "provider-chat-failed" },
    });
    expect(JSON.stringify(events)).not.toContain("Native YVEX");
  });

  it("refuses the native lane before the provider transport is called", async () => {
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
    const sessionResponse = await jsonRequest(harness.baseUrl, "/api/v1/chat/sessions", "POST", {
      lane: "native-yvex",
    });
    const session = (await sessionResponse.json()) as ChatSession;
    const response = await jsonRequest(
      harness.baseUrl,
      `/api/v1/chat/sessions/${session.id}/messages`,
      "POST",
      { content: "must not fall back", parameters },
    );
    expect(response.status).toBe(409);
    expect(await response.json()).toMatchObject({ code: "native-generation-unavailable" });
    expect(providerCalls).toBe(0);
  });
});
