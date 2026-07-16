/*
 * Owner: apps/operator automated test harness.
 * Owns: isolated configuration, deterministic YVEX/provider doubles, ephemeral BFF listeners, and cleanup.
 * Does not own: production fallback, native runtime facts, external network traffic, or browser fixtures outside tests.
 * Invariants: every harness uses a private temporary configuration directory and loopback-only listener.
 * Boundary: fixture readiness validates Operator behavior only and never promotes native YVEX capability.
 */
import { mkdir, mkdtemp, rm, writeFile } from "node:fs/promises";
import type { AddressInfo } from "node:net";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";

import type { OperatorConfig } from "../server/config.ts";
import { createOperatorHttpServer } from "../server/http.ts";
import {
  createOperatorServices,
  type OperatorServices,
  type ServiceDependencies,
} from "../server/services.ts";

export const fakeYvexPath = resolve(process.cwd(), "tests/fixtures/fake-yvex.mjs");
export const fakeProviderModel = "fixture-reference-model";

/** Builds one fully explicit loopback test configuration with only the selected fake YVEX candidate. */
export function testConfig(
  scenario = "default",
  overrides: Partial<OperatorConfig> = {},
): OperatorConfig {
  const unavailableRoot = resolve(tmpdir(), "yvex-operator-no-repository-binary");
  return {
    host: "127.0.0.1",
    port: 4317,
    bindMode: "loopback",
    remoteExposure: false,
    authToken: null,
    allowedOrigins: [],
    binaryEnvironmentCandidate: fakeYvexPath,
    binarySearchPath: "",
    binaryLookupTtlMs: 250,
    repositoryRoot: unavailableRoot,
    repositoryBuildCandidate: resolve(unavailableRoot, "yvex"),
    knownBuildCandidates: [],
    configDirectory: resolve(tmpdir(), "yvex-operator-test-config"),
    childEnvironment: {
      PATH: process.env.PATH,
      LC_ALL: "C",
      YVEX_FAKE_SCENARIO: scenario,
    },
    allowRemoteProviders: false,
    eventRetention: 100,
    sessionRetention: 20,
    modelsRoot: undefined,
    immutableTimeoutMs: 500,
    mutableTimeoutMs: 500,
    maxOutputBytes: 32_768,
    mutableTtlMs: 250,
    ...overrides,
  };
}

export interface ProviderDoubleOptions {
  delayMs?: number;
  failModels?: boolean;
  failChat?: boolean;
  lineEnding?: "\n" | "\r\n";
  onRequest?: (url: URL, init: RequestInit | undefined) => void;
}

/** Creates a deterministic OpenAI-compatible fetch double with real incremental ReadableStream frames. */
export function providerFetcher(options: ProviderDoubleOptions = {}): typeof fetch {
  const fetcher: typeof fetch = (input, init) => {
    const url = new URL(input instanceof Request ? input.url : input.toString());
    options.onRequest?.(url, init);
    if (url.pathname.endsWith("/models")) {
      if (options.failModels)
        return Promise.resolve(new Response("provider unavailable", { status: 503 }));
      return Promise.resolve(
        Response.json({ object: "list", data: [{ id: fakeProviderModel, object: "model" }] }),
      );
    }
    if (!url.pathname.endsWith("/chat/completions"))
      return Promise.resolve(new Response(null, { status: 404 }));
    if (options.failChat) return Promise.resolve(new Response("provider refused", { status: 503 }));
    const body =
      typeof init?.body === "string"
        ? (JSON.parse(init.body) as { messages?: Array<{ content?: string }> })
        : {};
    const prompt = body.messages?.at(-1)?.content ?? "";
    const pieces =
      prompt === "Reply with OK."
        ? ["OK"]
        : /slow|cancel/i.test(prompt)
          ? ["Partial ", "reference ", "response ", "continues."]
          : ["Reference ", "response."];
    const encoder = new TextEncoder();
    const lineEnding = options.lineEnding ?? "\n";
    const separator = `${lineEnding}${lineEnding}`;
    let timer: ReturnType<typeof setTimeout> | null = null;
    let index = 0;
    const stream = new ReadableStream<Uint8Array>({
      start(controller) {
        const closeForAbort = (): void => {
          if (timer) clearTimeout(timer);
          try {
            controller.error(new DOMException("cancelled", "AbortError"));
          } catch {
            /* Stream is already closed. */
          }
        };
        init?.signal?.addEventListener("abort", closeForAbort, { once: true });
        const push = (): void => {
          if (init?.signal?.aborted) return closeForAbort();
          if (index < pieces.length) {
            const chunk = { choices: [{ delta: { content: pieces[index] }, finish_reason: null }] };
            controller.enqueue(encoder.encode(`data: ${JSON.stringify(chunk)}${separator}`));
            index += 1;
            timer = setTimeout(push, options.delayMs ?? 5);
            return;
          }
          const usage = {
            choices: [{ delta: {}, finish_reason: "stop" }],
            usage: {
              prompt_tokens: 4,
              completion_tokens: pieces.length,
              total_tokens: 4 + pieces.length,
            },
          };
          controller.enqueue(
            encoder.encode(`data: ${JSON.stringify(usage)}${separator}data: [DONE]${separator}`),
          );
          controller.close();
        };
        timer = setTimeout(push, options.delayMs ?? 5);
      },
      cancel() {
        if (timer) clearTimeout(timer);
      },
    });
    return Promise.resolve(
      new Response(stream, {
        status: 200,
        headers: { "Content-Type": "text/event-stream; charset=utf-8" },
      }),
    );
  };
  return fetcher;
}

export interface TestHarness {
  config: OperatorConfig;
  services: OperatorServices;
  baseUrl: string;
  close: () => Promise<void>;
}

/** Starts the production BFF graph on an ephemeral loopback port with isolated local persistence. */
export async function createTestHarness(
  options: {
    scenario?: string;
    config?: Partial<OperatorConfig>;
    dependencies?: ServiceDependencies;
  } = {},
): Promise<TestHarness> {
  const root = await mkdtemp(join(tmpdir(), "yvex-operator-test-"));
  const staticRoot = join(root, "client");
  const configDirectory = join(root, "config");
  await mkdir(staticRoot, { recursive: true });
  await writeFile(
    join(staticRoot, "index.html"),
    "<!doctype html><title>YVEX Operator test</title>",
  );
  const config = testConfig(options.scenario, { configDirectory, ...options.config });
  const services = createOperatorServices(config, {
    fetcher: providerFetcher(),
    ...options.dependencies,
  });
  const server = createOperatorHttpServer(services, staticRoot);
  await new Promise<void>((resolveListen, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", resolveListen);
  });
  const address = server.address() as AddressInfo;
  return {
    config,
    services,
    baseUrl: `http://127.0.0.1:${address.port}`,
    close: async () => {
      await new Promise<void>((resolveClose, reject) =>
        server.close((error) => (error ? reject(error) : resolveClose())),
      );
      await rm(root, { recursive: true, force: true });
    },
  };
}

/** Configures and verifies the deterministic reference provider through the same service APIs as HTTP. */
export async function readyReferenceProvider(services: OperatorServices): Promise<void> {
  await services.settings.updateReferenceProvider({
    enabled: true,
    displayName: "Fixture reference",
    baseUrl: "http://127.0.0.1:14318/v1",
    apiKey: "fixture-secret-never-returned",
    defaultModel: fakeProviderModel,
    requestTimeoutMs: 5_000,
  });
  await services.provider.testConnection();
}

/** Prefixes same-origin browser API requests with an ephemeral test server while retaining stream semantics. */
export function browserFetch(baseUrl: string, nativeFetch: typeof fetch): typeof fetch {
  const fetcher: typeof fetch = (input, init) => {
    if (typeof input === "string" && input.startsWith("/"))
      return nativeFetch(`${baseUrl}${input}`, init);
    if (input instanceof URL && input.origin === "null")
      return nativeFetch(`${baseUrl}${input.pathname}${input.search}`, init);
    return nativeFetch(input, init);
  };
  return fetcher;
}
