/*
 * Owner: apps/operator optional OpenAI-compatible comparison transport.
 * Owns: endpoint validation, secret-bearing requests, model discovery, compatibility tests, SSE parsing, usage, and timings.
 * Does not own: browser secrets, primary workspace state, YVEX execution, arbitrary URL fetches, or fallback.
 * Invariants: every request targets the explicitly enabled comparison URL and only supported OpenAI parameters are sent.
 * Boundary: comparison readiness never enters YVEX capability, health, workspace, runtime, or generation state.
 */
import { performance } from "node:perf_hooks";

import {
  API_VERSION,
  SCHEMA_VERSION,
  availability,
  type GenerationParameters,
  type ProviderStatus,
  type Timing,
  type Usage,
} from "../shared/contracts.ts";
import type { OperatorConfig } from "./config.ts";
import type { EventHistory } from "./events.ts";
import type { JobManager } from "./jobs.ts";
import type { OperatorSettingsStore } from "./settings.ts";

export class ProviderTransportError extends Error {
  constructor(
    readonly code: string,
    message: string,
    readonly retryable: boolean,
    readonly status: number | null = null,
  ) {
    super(message);
    this.name = "ProviderTransportError";
  }
}

export interface ProviderStreamCallbacks {
  delta: (value: string) => void | Promise<void>;
  usage: (value: Usage) => void | Promise<void>;
  timing: (value: Timing) => void | Promise<void>;
}

export interface ProviderStreamResult {
  usage: Usage;
  timings: Timing;
  finishReason: string | null;
}

interface OpenAiChunk {
  choices?: Array<{
    delta?: { content?: string | null };
    finish_reason?: string | null;
  }>;
  usage?: { prompt_tokens?: number; completion_tokens?: number; total_tokens?: number };
}

/** Narrows untrusted provider JSON to a property-readable object without assigning a provider schema. */
function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

type Fetcher = typeof fetch;

const emptyUsage: Usage = { inputTokens: null, outputTokens: null, totalTokens: null };
const maxProviderJsonBytes = 1_048_576;
const maxProviderStreamBytes = 16_777_216;
const maxProviderModels = 4_096;

/** Converts optional OpenAI usage fields without inventing unavailable counts. */
function normalizedUsage(value: OpenAiChunk["usage"]): Usage {
  return {
    inputTokens: Number.isInteger(value?.prompt_tokens) ? (value?.prompt_tokens as number) : null,
    outputTokens: Number.isInteger(value?.completion_tokens)
      ? (value?.completion_tokens as number)
      : null,
    totalTokens: Number.isInteger(value?.total_tokens) ? (value?.total_tokens as number) : null,
  };
}

/** Reads one provider body through an explicit byte ceiling instead of trusting Content-Length. */
async function readBoundedText(response: Response, maxBytes: number): Promise<string> {
  const declared = Number(response.headers.get("content-length"));
  if (Number.isFinite(declared) && declared > maxBytes) {
    throw new ProviderTransportError(
      "comparison-response-oversized",
      "External comparison response exceeded the Operator limit.",
      false,
      response.status,
    );
  }
  if (!response.body) return "";
  const reader: ReadableStreamDefaultReader<Uint8Array> = response.body.getReader();
  const chunks: Uint8Array[] = [];
  let bytes = 0;
  try {
    while (true) {
      const chunk = await reader.read();
      if (chunk.done) break;
      bytes += chunk.value.byteLength;
      if (bytes > maxBytes) {
        await reader.cancel();
        throw new ProviderTransportError(
          "comparison-response-oversized",
          "External comparison response exceeded the Operator limit.",
          false,
          response.status,
        );
      }
      chunks.push(chunk.value);
    }
  } finally {
    reader.releaseLock();
  }
  return Buffer.concat(chunks.map((chunk) => Buffer.from(chunk))).toString("utf8");
}

/** Locates the earliest complete SSE frame for either LF or CRLF wire formatting. */
function sseBoundary(buffer: string): { index: number; length: number } | null {
  const lf = buffer.indexOf("\n\n");
  const crlf = buffer.indexOf("\r\n\r\n");
  if (lf < 0 && crlf < 0) return null;
  if (crlf >= 0 && (lf < 0 || crlf < lf)) return { index: crlf, length: 4 };
  return { index: lf, length: 2 };
}

/** Validates one configured base URL and blocks unrestricted remote fetching by default. */
export function validateProviderBaseUrl(value: string, allowRemote: boolean): URL {
  let parsed: URL;
  try {
    parsed = new URL(value);
  } catch {
    throw new ProviderTransportError(
      "comparison-invalid-url",
      "External comparison URL is invalid.",
      false,
    );
  }
  if (!["http:", "https:"].includes(parsed.protocol)) {
    throw new ProviderTransportError(
      "comparison-invalid-protocol",
      "External comparison URL must use HTTP or HTTPS.",
      false,
    );
  }
  if (parsed.username || parsed.password || parsed.search || parsed.hash) {
    throw new ProviderTransportError(
      "comparison-unsafe-url",
      "External comparison URL cannot contain credentials, query, or fragment.",
      false,
    );
  }
  const localHosts = new Set(["127.0.0.1", "localhost", "[::1]", "::1"]);
  if (!localHosts.has(parsed.hostname) && !allowRemote) {
    throw new ProviderTransportError(
      "comparison-remote-disabled",
      "Remote comparison URLs are disabled by Operator safety policy.",
      false,
    );
  }
  if (!localHosts.has(parsed.hostname) && parsed.protocol !== "https:") {
    throw new ProviderTransportError(
      "comparison-remote-requires-tls",
      "Remote comparison URLs require HTTPS.",
      false,
    );
  }
  parsed.pathname = parsed.pathname.replace(/\/$/, "");
  return parsed;
}

/** Reads normalized SSE data frames from a fetch response with bounded line buffering. */
async function readSse(
  response: Response,
  signal: AbortSignal,
  onData: (data: string) => void | Promise<void>,
): Promise<void> {
  if (!response.body)
    throw new ProviderTransportError(
      "comparison-stream-missing",
      "External comparison response has no streaming body.",
      true,
      response.status,
    );
  const reader: ReadableStreamDefaultReader<Uint8Array> = response.body.getReader();
  const decoder = new TextDecoder();
  let buffer = "";
  let receivedBytes = 0;
  try {
    while (true) {
      if (signal.aborted) throw new DOMException("cancelled", "AbortError");
      const chunk = await reader.read();
      if (chunk.done) break;
      receivedBytes += chunk.value.byteLength;
      if (receivedBytes > maxProviderStreamBytes)
        throw new ProviderTransportError(
          "comparison-stream-oversized",
          "External comparison SSE response exceeded the Operator limit.",
          false,
        );
      buffer += decoder.decode(chunk.value, { stream: true });
      if (buffer.length > 1_048_576)
        throw new ProviderTransportError(
          "comparison-stream-oversized",
          "External comparison SSE frame exceeded the Operator limit.",
          false,
        );
      let boundary = sseBoundary(buffer);
      while (boundary) {
        const frame = buffer.slice(0, boundary.index).replaceAll("\r", "");
        buffer = buffer.slice(boundary.index + boundary.length);
        const data = frame
          .split("\n")
          .filter((line) => line.startsWith("data:"))
          .map((line) => line.slice(5).trimStart())
          .join("\n");
        if (data) await onData(data);
        boundary = sseBoundary(buffer);
      }
    }
  } finally {
    reader.releaseLock();
  }
}

/** Owns server-side OpenAI-compatible requests and a bounded last-tested status observation. */
export class ReferenceProviderService {
  private tested: ProviderStatus | null = null;

  constructor(
    private readonly config: OperatorConfig,
    private readonly settings: OperatorSettingsStore,
    private readonly jobs: JobManager,
    private readonly history: EventHistory,
    private readonly fetcher: Fetcher = fetch,
    private readonly clock: () => number = Date.now,
  ) {}

  /** Returns configured/reference test state without performing background network IO. */
  async status(): Promise<ProviderStatus> {
    const settings = await this.settings.publicSnapshot();
    if (
      this.tested &&
      this.tested.baseUrl === settings.comparisonEndpoint.baseUrl &&
      settings.comparisonEndpoint.enabled
    ) {
      return structuredClone(this.tested);
    }
    const observedAt = new Date(this.clock()).toISOString();
    const configured =
      settings.comparisonEndpoint.enabled &&
      Boolean(settings.comparisonEndpoint.baseUrl && settings.comparisonEndpoint.defaultModel);
    return {
      apiVersion: API_VERSION,
      schemaVersion: SCHEMA_VERSION,
      observedAt,
      availability: configured
        ? availability(
            "stale",
            "comparison-not-tested",
            "The external comparison endpoint is configured but has not passed a current test.",
            observedAt,
            {
              recovery: {
                id: "test-comparison-endpoint",
                label: "Test comparison endpoint",
                href: "/settings?section=comparison-endpoint",
              },
              source: "external-comparison",
            },
          )
        : availability(
            "unavailable",
            "comparison-not-configured",
            "The optional external comparison endpoint is not configured.",
            observedAt,
            {
              recovery: {
                id: "configure-comparison-endpoint",
                label: "Configure comparison endpoint",
                href: "/settings?section=comparison-endpoint",
              },
              source: "external-comparison",
            },
          ),
      configured,
      reachable: false,
      streamingCompatible: false,
      displayName: settings.comparisonEndpoint.displayName,
      baseUrl: settings.comparisonEndpoint.baseUrl,
      defaultModel: settings.comparisonEndpoint.defaultModel,
      lastTestedAt: null,
      models: [],
    };
  }

  /** Tests model discovery and a bounded visible-token stream through a cancellable comparison job. */
  async testConnection(): Promise<{ status: ProviderStatus; jobId: string }> {
    const job = this.jobs.create(
      "comparison-test",
      "External comparison endpoint",
      "validating-configuration",
      true,
    );
    const controller = new AbortController();
    this.jobs.registerCancellation(job.id, () => controller.abort());
    this.jobs.transition(job.id, "starting", "model-discovery");
    try {
      const context = await this.context();
      this.jobs.transition(job.id, "running", "model-discovery");
      const models = await this.fetchModels(context, controller.signal);
      const model = context.settings.defaultModel || models[0];
      if (!model)
        throw new ProviderTransportError(
          "comparison-model-missing",
          "External comparison returned no model and no default model is configured.",
          false,
        );
      let streamed = false;
      await this.streamRequest(
        context,
        [{ role: "user", content: "Reply with OK only. Do not reason or explain." }],
        {
          model,
          maxOutputTokens: 16,
          temperature: 0,
          topP: 1,
          seed: null,
          stop: [],
          stream: true,
        },
        controller.signal,
        {
          delta: (delta) => {
            if (delta) streamed = true;
          },
          usage: () => undefined,
          timing: () => undefined,
        },
      );
      if (!streamed)
        throw new ProviderTransportError(
          "comparison-stream-empty",
          "External comparison stream completed without a token delta.",
          true,
        );
      const observedAt = new Date(this.clock()).toISOString();
      this.tested = {
        apiVersion: API_VERSION,
        schemaVersion: SCHEMA_VERSION,
        observedAt,
        availability: availability(
          "ready",
          "comparison-ready",
          "External comparison model and streaming APIs are compatible.",
          observedAt,
          { source: "external-comparison-test" },
        ),
        configured: true,
        reachable: true,
        streamingCompatible: true,
        displayName: context.settings.displayName,
        baseUrl: context.settings.baseUrl,
        defaultModel: model,
        lastTestedAt: observedAt,
        models,
      };
      this.jobs.transition(job.id, "completed", "completed", {
        resultReference: "external-comparison-status",
      });
      this.history.record(
        "comparison-test",
        "info",
        "External comparison connection and streaming test passed.",
        job.id,
      );
      return { status: structuredClone(this.tested), jobId: job.id };
    } catch (error) {
      const failure =
        error instanceof ProviderTransportError
          ? error
          : new ProviderTransportError(
              "comparison-test-failed",
              "External comparison test failed.",
              true,
            );
      if (controller.signal.aborted) {
        this.jobs.transition(job.id, "cancelled", "cancelled", { resultReference: null });
      } else {
        this.jobs.transition(job.id, "failed", "failed", {
          error: { code: failure.code, message: failure.message, retryable: failure.retryable },
        });
      }
      this.history.record(
        "comparison-test",
        controller.signal.aborted ? "warning" : "error",
        controller.signal.aborted ? "External comparison test cancelled." : failure.message,
        job.id,
      );
      throw failure;
    }
  }

  /** Lists provider model IDs through the single configured endpoint and returns no provider payload extras. */
  async models(signal: AbortSignal): Promise<string[]> {
    return this.fetchModels(await this.context(), signal);
  }

  /** Streams one external comparison with normalized deltas, optional usage, and measurable timings. */
  async stream(
    messages: readonly { role: "system" | "user" | "assistant"; content: string }[],
    parameters: GenerationParameters,
    signal: AbortSignal,
    callbacks: ProviderStreamCallbacks,
  ): Promise<ProviderStreamResult> {
    return this.streamRequest(await this.context(), messages, parameters, signal, callbacks);
  }

  /** Invalidates reachability after settings mutation so stale success cannot conceal a changed provider. */
  clearStatus(): void {
    this.tested = null;
  }

  /** Loads secret-bearing internal provider context and validates the configured URL once per request. */
  private async context(): Promise<{
    base: URL;
    apiKey: string | null;
    settings: {
      enabled: boolean;
      displayName: string;
      baseUrl: string;
      defaultModel: string;
      requestTimeoutMs: number;
    };
  }> {
    const internal = await this.settings.internal();
    const settings = internal.persisted.comparisonEndpoint;
    if (!settings.enabled)
      throw new ProviderTransportError(
        "comparison-disabled",
        "The external comparison endpoint is disabled.",
        false,
      );
    if (!settings.defaultModel)
      throw new ProviderTransportError(
        "comparison-model-missing",
        "A default reference model is required.",
        false,
      );
    return {
      base: validateProviderBaseUrl(settings.baseUrl, this.config.allowRemoteProviders),
      apiKey: internal.comparisonEndpointApiKey,
      settings,
    };
  }

  /** Builds provider headers server-side so API keys never cross into browser transport. */
  private headers(apiKey: string | null): Headers {
    const headers = new Headers({ Accept: "application/json", "Content-Type": "application/json" });
    if (apiKey) headers.set("Authorization", `Bearer ${apiKey}`);
    return headers;
  }

  /** Fetches and validates only model identifier strings with timeout and structured failures. */
  private async fetchModels(
    context: Awaited<ReturnType<ReferenceProviderService["context"]>>,
    outerSignal: AbortSignal,
  ): Promise<string[]> {
    const timeout = AbortSignal.timeout(context.settings.requestTimeoutMs);
    const signal = AbortSignal.any([outerSignal, timeout]);
    let response: Response;
    try {
      response = await this.fetcher(new URL(`${context.base.pathname}/models`, context.base), {
        method: "GET",
        headers: this.headers(context.apiKey),
        signal,
      });
    } catch {
      if (outerSignal.aborted) throw new DOMException("cancelled", "AbortError");
      throw new ProviderTransportError(
        "comparison-unreachable",
        "External comparison endpoint is unreachable.",
        true,
      );
    }
    if (!response.ok)
      throw new ProviderTransportError(
        response.status === 401 || response.status === 403
          ? "comparison-authentication"
          : "comparison-models-failed",
        `External comparison model endpoint returned HTTP ${response.status}.`,
        response.status >= 500,
        response.status,
      );
    let payload: unknown;
    try {
      payload = JSON.parse(await readBoundedText(response, maxProviderJsonBytes)) as unknown;
    } catch (error) {
      if (error instanceof ProviderTransportError) throw error;
      throw new ProviderTransportError(
        "comparison-models-malformed",
        "External comparison model endpoint returned malformed JSON.",
        true,
        response.status,
      );
    }
    if (!isRecord(payload) || !Array.isArray(payload.data)) {
      throw new ProviderTransportError(
        "comparison-models-contract",
        "External comparison model response is incompatible.",
        false,
        response.status,
      );
    }
    if (payload.data.length > maxProviderModels) {
      throw new ProviderTransportError(
        "comparison-models-oversized",
        "External comparison endpoint returned too many model records.",
        false,
        response.status,
      );
    }
    return payload.data
      .map((entry) => (isRecord(entry) ? entry.id : null))
      .filter((id): id is string => typeof id === "string" && id.length > 0);
  }

  /** Sends one supported OpenAI chat-completion shape and normalizes its SSE stream. */
  private async streamRequest(
    context: Awaited<ReturnType<ReferenceProviderService["context"]>>,
    messages: readonly { role: "system" | "user" | "assistant"; content: string }[],
    parameters: GenerationParameters,
    outerSignal: AbortSignal,
    callbacks: ProviderStreamCallbacks,
  ): Promise<ProviderStreamResult> {
    const timeout = AbortSignal.timeout(context.settings.requestTimeoutMs);
    const signal = AbortSignal.any([outerSignal, timeout]);
    const body: Record<string, unknown> = {
      model: parameters.model,
      messages,
      max_tokens: parameters.maxOutputTokens,
      temperature: parameters.temperature,
      top_p: parameters.topP,
      stream: true,
      stream_options: { include_usage: true },
    };
    if (parameters.seed !== null) body.seed = parameters.seed;
    if (parameters.stop.length) body.stop = parameters.stop;
    const started = performance.now();
    let response: Response;
    try {
      response = await this.fetcher(
        new URL(`${context.base.pathname}/chat/completions`, context.base),
        {
          method: "POST",
          headers: this.headers(context.apiKey),
          body: JSON.stringify(body),
          signal,
        },
      );
    } catch {
      if (outerSignal.aborted) throw new DOMException("cancelled", "AbortError");
      throw new ProviderTransportError(
        "comparison-unreachable",
        "External comparison endpoint is unreachable.",
        true,
      );
    }
    if (!response.ok) {
      throw new ProviderTransportError(
        response.status === 401 || response.status === 403
          ? "comparison-authentication"
          : "comparison-generation-failed",
        `External comparison chat endpoint returned HTTP ${response.status}.`,
        response.status >= 500,
        response.status,
      );
    }
    const contentType = response.headers.get("content-type") ?? "";
    if (!contentType.includes("text/event-stream")) {
      throw new ProviderTransportError(
        "comparison-stream-incompatible",
        "External comparison endpoint did not return an SSE stream.",
        false,
        response.status,
      );
    }
    let usage = { ...emptyUsage };
    let firstTokenAt: number | null = null;
    let finishReason: string | null = null;
    await readSse(response, signal, async (frame) => {
      if (frame === "[DONE]") return;
      let chunk: OpenAiChunk;
      try {
        chunk = JSON.parse(frame) as OpenAiChunk;
      } catch {
        throw new ProviderTransportError(
          "comparison-stream-malformed",
          "External comparison endpoint returned malformed SSE JSON.",
          true,
          response.status,
        );
      }
      const delta = chunk.choices?.[0]?.delta?.content;
      if (typeof delta === "string" && delta) {
        firstTokenAt ??= performance.now();
        await callbacks.delta(delta);
      }
      const nextUsage = normalizedUsage(chunk.usage);
      if (
        nextUsage.inputTokens !== null ||
        nextUsage.outputTokens !== null ||
        nextUsage.totalTokens !== null
      ) {
        usage = nextUsage;
        await callbacks.usage(usage);
      }
      if (chunk.choices?.[0]?.finish_reason) finishReason = chunk.choices[0].finish_reason;
    });
    const completed = performance.now();
    const totalDurationMs = Math.max(0, Math.round(completed - started));
    const timeToFirstTokenMs =
      firstTokenAt === null ? null : Math.max(0, Math.round(firstTokenAt - started));
    const tokensPerSecond =
      usage.outputTokens && totalDurationMs > 0
        ? usage.outputTokens / (totalDurationMs / 1_000)
        : null;
    const timings: Timing = { timeToFirstTokenMs, totalDurationMs, tokensPerSecond };
    await callbacks.timing(timings);
    return { usage, timings, finishReason };
  }
}
