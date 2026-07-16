/*
 * Owner: apps/operator OpenAI-compatible reference-provider transport.
 * Owns: endpoint validation, secret-bearing requests, model discovery, compatibility tests, SSE parsing, usage, and timings.
 * Does not own: browser secrets, session persistence, chat UI, native YVEX execution, arbitrary URL fetches, or silent fallback.
 * Invariants: every request targets the single validated server-side base URL and only supported OpenAI parameters are sent.
 * Boundary: provider readiness is always reference-provider capability and never native YVEX capability.
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
      "provider-response-oversized",
      "Reference provider response exceeded the Operator limit.",
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
          "provider-response-oversized",
          "Reference provider response exceeded the Operator limit.",
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
      "provider-invalid-url",
      "Reference provider URL is invalid.",
      false,
    );
  }
  if (!["http:", "https:"].includes(parsed.protocol)) {
    throw new ProviderTransportError(
      "provider-invalid-protocol",
      "Reference provider URL must use HTTP or HTTPS.",
      false,
    );
  }
  if (parsed.username || parsed.password || parsed.search || parsed.hash) {
    throw new ProviderTransportError(
      "provider-unsafe-url",
      "Provider URL cannot contain credentials, query, or fragment.",
      false,
    );
  }
  const localHosts = new Set(["127.0.0.1", "localhost", "[::1]", "::1"]);
  if (!localHosts.has(parsed.hostname) && !allowRemote) {
    throw new ProviderTransportError(
      "provider-remote-disabled",
      "Remote provider URLs are disabled by Operator safety policy.",
      false,
    );
  }
  if (!localHosts.has(parsed.hostname) && parsed.protocol !== "https:") {
    throw new ProviderTransportError(
      "provider-remote-requires-tls",
      "Remote provider URLs require HTTPS.",
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
      "provider-stream-missing",
      "Provider response has no streaming body.",
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
          "provider-stream-oversized",
          "Provider SSE response exceeded the Operator limit.",
          false,
        );
      buffer += decoder.decode(chunk.value, { stream: true });
      if (buffer.length > 1_048_576)
        throw new ProviderTransportError(
          "provider-stream-oversized",
          "Provider SSE frame exceeded the Operator limit.",
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
      this.tested.baseUrl === settings.referenceProvider.baseUrl &&
      settings.referenceProvider.enabled
    ) {
      return structuredClone(this.tested);
    }
    const observedAt = new Date(this.clock()).toISOString();
    const configured =
      settings.referenceProvider.enabled &&
      Boolean(settings.referenceProvider.baseUrl && settings.referenceProvider.defaultModel);
    return {
      apiVersion: API_VERSION,
      schemaVersion: SCHEMA_VERSION,
      observedAt,
      availability: configured
        ? availability(
            "stale",
            "provider-not-tested",
            "Reference provider is configured but has not passed a current connection test.",
            observedAt,
            {
              recovery: {
                id: "test-provider",
                label: "Test provider",
                href: "/settings?section=reference-provider",
              },
              source: "reference-provider",
            },
          )
        : availability(
            "unavailable",
            "provider-not-configured",
            "Reference provider configuration is incomplete.",
            observedAt,
            {
              recovery: {
                id: "configure-provider",
                label: "Configure provider",
                href: "/settings?section=reference-provider",
              },
              source: "reference-provider",
            },
          ),
      configured,
      reachable: false,
      streamingCompatible: false,
      displayName: settings.referenceProvider.displayName,
      baseUrl: settings.referenceProvider.baseUrl,
      defaultModel: settings.referenceProvider.defaultModel,
      lastTestedAt: null,
      models: [],
    };
  }

  /** Tests model discovery and one-token streaming through a cancellable provider-test job. */
  async testConnection(): Promise<{ status: ProviderStatus; jobId: string }> {
    const job = this.jobs.create(
      "provider-test",
      "Reference provider",
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
          "provider-model-missing",
          "Provider returned no model and no default model is configured.",
          false,
        );
      let streamed = false;
      await this.streamRequest(
        context,
        [{ role: "user", content: "Reply with OK." }],
        { model, maxOutputTokens: 1, temperature: 0, topP: 1, seed: null, stop: [], stream: true },
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
          "provider-stream-empty",
          "Provider stream completed without a token delta.",
          true,
        );
      const observedAt = new Date(this.clock()).toISOString();
      this.tested = {
        apiVersion: API_VERSION,
        schemaVersion: SCHEMA_VERSION,
        observedAt,
        availability: availability(
          "ready",
          "provider-ready",
          "Reference provider model and streaming APIs are compatible.",
          observedAt,
          { source: "reference-provider-test" },
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
        resultReference: "reference-provider-status",
      });
      this.history.record(
        "provider-test",
        "info",
        "Reference provider connection and streaming test passed.",
        job.id,
      );
      return { status: structuredClone(this.tested), jobId: job.id };
    } catch (error) {
      const failure =
        error instanceof ProviderTransportError
          ? error
          : new ProviderTransportError(
              "provider-test-failed",
              "Reference provider test failed.",
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
        "provider-test",
        controller.signal.aborted ? "warning" : "error",
        controller.signal.aborted ? "Reference provider test cancelled." : failure.message,
        job.id,
      );
      throw failure;
    }
  }

  /** Lists provider model IDs through the single configured endpoint and returns no provider payload extras. */
  async models(signal: AbortSignal): Promise<string[]> {
    return this.fetchModels(await this.context(), signal);
  }

  /** Streams one reference-provider generation with normalized deltas, optional usage, and measurable timings. */
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
    const settings = internal.persisted.referenceProvider;
    if (!settings.enabled)
      throw new ProviderTransportError(
        "provider-disabled",
        "Reference provider is disabled.",
        false,
      );
    if (!settings.defaultModel)
      throw new ProviderTransportError(
        "provider-model-missing",
        "A default reference model is required.",
        false,
      );
    return {
      base: validateProviderBaseUrl(settings.baseUrl, this.config.allowRemoteProviders),
      apiKey: internal.referenceProviderApiKey,
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
        "provider-unreachable",
        "Reference provider is unreachable.",
        true,
      );
    }
    if (!response.ok)
      throw new ProviderTransportError(
        response.status === 401 || response.status === 403
          ? "provider-authentication"
          : "provider-models-failed",
        `Reference provider model endpoint returned HTTP ${response.status}.`,
        response.status >= 500,
        response.status,
      );
    let payload: unknown;
    try {
      payload = JSON.parse(await readBoundedText(response, maxProviderJsonBytes)) as unknown;
    } catch (error) {
      if (error instanceof ProviderTransportError) throw error;
      throw new ProviderTransportError(
        "provider-models-malformed",
        "Reference provider model endpoint returned malformed JSON.",
        true,
        response.status,
      );
    }
    if (!isRecord(payload) || !Array.isArray(payload.data)) {
      throw new ProviderTransportError(
        "provider-models-contract",
        "Reference provider model response is incompatible.",
        false,
        response.status,
      );
    }
    if (payload.data.length > maxProviderModels) {
      throw new ProviderTransportError(
        "provider-models-oversized",
        "Reference provider returned too many model records.",
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
        "provider-unreachable",
        "Reference provider is unreachable.",
        true,
      );
    }
    if (!response.ok) {
      throw new ProviderTransportError(
        response.status === 401 || response.status === 403
          ? "provider-authentication"
          : "provider-chat-failed",
        `Reference provider chat endpoint returned HTTP ${response.status}.`,
        response.status >= 500,
        response.status,
      );
    }
    const contentType = response.headers.get("content-type") ?? "";
    if (!contentType.includes("text/event-stream")) {
      throw new ProviderTransportError(
        "provider-stream-incompatible",
        "Reference provider did not return an SSE stream.",
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
          "provider-stream-malformed",
          "Reference provider returned malformed SSE JSON.",
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
