/*
 * Owner: apps/operator browser API transport.
 * Owns: same-origin versioned fetches, shared-schema validation, mutation bodies, SSE decoding, cancellation, and structured errors.
 * Does not own: server truth, retries, shell commands, provider secrets, fixtures, or capability inference.
 * Invariants: only fixed route builders form URLs and malformed responses never enter application state.
 * Boundary: API connectivity is distinct from binary, provider, backend, and runtime readiness.
 */
import { z, type ZodType } from "zod";

import {
  apiErrorResponseSchema,
  binaryResolutionSchema,
  capabilityManifestSchema,
  chatSessionSchema,
  chatSessionsResponseSchema,
  chatStreamEventSchema,
  domainProjectionSchema,
  eventsResponseSchema,
  jobsResponseSchema,
  producerDescriptorSchema,
  producerListResponseSchema,
  producerRunSchema,
  producerRunsResponseSchema,
  providerModelsResponseSchema,
  providerStatusSchema,
  providerTestResponseSchema,
  settingsResponseSchema,
  systemHealthSchema,
  type ChatStreamEvent,
  type DomainId,
  type InterfaceSettingsPatch,
  type ReferenceProviderPatch,
  type SendChatMessage,
  type YvexSettingsPatch,
} from "../shared/contracts.ts";

export class OperatorApiError extends Error {
  constructor(
    readonly code: string,
    message: string,
    readonly status: number | null = null,
    readonly detail: string | null = null,
    readonly requestId: string | null = null,
  ) {
    super(message);
    this.name = "OperatorApiError";
  }
}

/** Parses one HTTP error through the shared contract while preserving a stable connectivity fallback. */
async function responseError(response: Response): Promise<OperatorApiError> {
  try {
    const parsed = apiErrorResponseSchema.safeParse((await response.json()) as unknown);
    if (parsed.success) {
      return new OperatorApiError(
        parsed.data.code,
        parsed.data.message,
        response.status,
        parsed.data.detail ?? null,
        parsed.data.requestId,
      );
    }
  } catch {
    // The stable HTTP fallback below intentionally hides malformed server detail.
  }
  return new OperatorApiError(
    "adapter-http-error",
    `Operator adapter returned HTTP ${response.status}.`,
    response.status,
  );
}

/** Executes one fixed same-origin request and validates its JSON response with a shared schema. */
async function requestJson<T>(
  path: string,
  schema: ZodType<T>,
  options: {
    method?: "GET" | "POST" | "PATCH" | "DELETE";
    body?: unknown;
    signal?: AbortSignal;
  } = {},
): Promise<T> {
  let response: Response;
  try {
    response = await fetch(path, {
      method: options.method ?? "GET",
      headers: {
        Accept: "application/json",
        ...(options.body === undefined ? {} : { "Content-Type": "application/json" }),
      },
      cache: "no-store",
      credentials: "same-origin",
      ...(options.body === undefined ? {} : { body: JSON.stringify(options.body) }),
      ...(options.signal ? { signal: options.signal } : {}),
    });
  } catch (error) {
    if (error instanceof DOMException && error.name === "AbortError") throw error;
    throw new OperatorApiError("adapter-unreachable", "Local Operator adapter is unreachable.");
  }
  if (!response.ok) throw await responseError(response);
  let payload: unknown;
  try {
    payload = (await response.json()) as unknown;
  } catch {
    throw new OperatorApiError(
      "adapter-malformed-json",
      "Operator adapter returned malformed JSON.",
      response.status,
    );
  }
  const parsed = schema.safeParse(payload);
  if (!parsed.success) {
    throw new OperatorApiError(
      "adapter-contract-mismatch",
      "Operator adapter response failed the shared contract.",
      response.status,
      parsed.error.issues[0]?.message ?? null,
    );
  }
  return parsed.data;
}

export const operatorApi = {
  health: (signal?: AbortSignal) =>
    requestJson("/api/v1/system/health", systemHealthSchema, { signal }),
  binaryResolution: (signal?: AbortSignal) =>
    requestJson("/api/v1/system/binary-resolution", binaryResolutionSchema, { signal }),
  capabilities: (signal?: AbortSignal) =>
    requestJson("/api/v1/capabilities", capabilityManifestSchema, { signal }),
  settings: (signal?: AbortSignal) =>
    requestJson("/api/v1/settings", settingsResponseSchema, { signal }),
  providerStatus: (signal?: AbortSignal) =>
    requestJson("/api/v1/reference-provider/status", providerStatusSchema, { signal }),
  providerModels: (signal?: AbortSignal) =>
    requestJson("/api/v1/reference-provider/models", providerModelsResponseSchema, { signal }),
  domain: (domain: DomainId, signal?: AbortSignal) =>
    requestJson(`/api/v1/${domain}`, domainProjectionSchema, { signal }),
  producers: (signal?: AbortSignal) =>
    requestJson("/api/v1/producers", producerListResponseSchema, { signal }),
  producer: (id: string, signal?: AbortSignal) =>
    requestJson(
      `/api/v1/producers/${encodeURIComponent(id)}`,
      z.object({ producer: producerDescriptorSchema, latestRun: producerRunSchema.nullable() }),
      { signal },
    ),
  runProducer: (id: string, signal?: AbortSignal) =>
    requestJson(`/api/v1/producers/${encodeURIComponent(id)}/run`, producerRunSchema, {
      method: "POST",
      body: {},
      signal,
    }),
  producerRuns: (signal?: AbortSignal) =>
    requestJson("/api/v1/producer-runs", producerRunsResponseSchema, { signal }),
  jobs: (signal?: AbortSignal) => requestJson("/api/v1/jobs", jobsResponseSchema, { signal }),
  cancelJob: (id: string, signal?: AbortSignal) =>
    requestJson(`/api/v1/jobs/${encodeURIComponent(id)}/cancel`, z.unknown(), {
      method: "POST",
      body: {},
      signal,
    }),
  events: (signal?: AbortSignal) =>
    requestJson("/api/v1/events?limit=50", eventsResponseSchema, { signal }),
  sessions: (signal?: AbortSignal) =>
    requestJson("/api/v1/chat/sessions", chatSessionsResponseSchema, { signal }),
  session: (id: string, signal?: AbortSignal) =>
    requestJson(`/api/v1/chat/sessions/${encodeURIComponent(id)}`, chatSessionSchema, { signal }),
  createSession: (
    input: { lane: "native-yvex" | "reference-provider"; title?: string; model?: string },
    signal?: AbortSignal,
  ) =>
    requestJson("/api/v1/chat/sessions", chatSessionSchema, {
      method: "POST",
      body: input,
      signal,
    }),
  renameSession: (id: string, title: string, signal?: AbortSignal) =>
    requestJson(`/api/v1/chat/sessions/${encodeURIComponent(id)}`, chatSessionSchema, {
      method: "PATCH",
      body: { title },
      signal,
    }),
  clearSession: (id: string, signal?: AbortSignal) =>
    requestJson(`/api/v1/chat/sessions/${encodeURIComponent(id)}/clear`, chatSessionSchema, {
      method: "POST",
      body: {},
      signal,
    }),
  deleteSession: async (id: string, signal?: AbortSignal): Promise<void> => {
    const response = await fetch(`/api/v1/chat/sessions/${encodeURIComponent(id)}`, {
      method: "DELETE",
      credentials: "same-origin",
      signal,
    });
    if (!response.ok) throw await responseError(response);
  },
  updateOperator: (patch: InterfaceSettingsPatch, signal?: AbortSignal) =>
    requestJson("/api/v1/settings/operator", settingsResponseSchema, {
      method: "PATCH",
      body: patch,
      signal,
    }),
  updateYvex: (patch: YvexSettingsPatch, signal?: AbortSignal) =>
    requestJson("/api/v1/settings/yvex", settingsResponseSchema, {
      method: "PATCH",
      body: patch,
      signal,
    }),
  updateProvider: (patch: ReferenceProviderPatch, signal?: AbortSignal) =>
    requestJson("/api/v1/settings/reference-provider", settingsResponseSchema, {
      method: "PATCH",
      body: patch,
      signal,
    }),
  testProvider: (signal?: AbortSignal) =>
    requestJson("/api/v1/settings/reference-provider/test", providerTestResponseSchema, {
      method: "POST",
      body: {},
      signal,
    }),
  reload: (signal?: AbortSignal) =>
    requestJson(
      "/api/v1/system/reload",
      z.object({ reloaded: z.boolean(), observedAt: z.string() }),
      { method: "POST", body: {}, signal },
    ),
  clearCache: (signal?: AbortSignal) =>
    requestJson(
      "/api/v1/settings/cache/reset",
      z.object({ cleared: z.boolean(), observedAt: z.string() }),
      { method: "POST", body: {}, signal },
    ),
  cancelChat: (requestId: string, signal?: AbortSignal) =>
    requestJson(
      `/api/v1/chat/requests/${encodeURIComponent(requestId)}/cancel`,
      z.object({ requestId: z.string(), jobId: z.string() }),
      { method: "POST", body: {}, signal },
    ),
};

/** Streams normalized chat events from one fixed session endpoint and renders deltas before completion. */
export async function streamChatMessage(
  sessionId: string,
  input: SendChatMessage,
  onEvent: (event: ChatStreamEvent) => void,
  signal?: AbortSignal,
): Promise<void> {
  let response: Response;
  try {
    response = await fetch(`/api/v1/chat/sessions/${encodeURIComponent(sessionId)}/messages`, {
      method: "POST",
      headers: { Accept: "text/event-stream", "Content-Type": "application/json" },
      body: JSON.stringify(input),
      credentials: "same-origin",
      signal,
    });
  } catch (error) {
    if (error instanceof DOMException && error.name === "AbortError") throw error;
    throw new OperatorApiError("adapter-unreachable", "Local Operator adapter is unreachable.");
  }
  if (!response.ok) throw await responseError(response);
  if (!response.body)
    throw new OperatorApiError(
      "chat-stream-missing",
      "Operator returned no chat event stream.",
      response.status,
    );
  const reader = response.body.getReader();
  const decoder = new TextDecoder();
  let buffer = "";
  try {
    while (true) {
      const result = await reader.read();
      if (result.done) break;
      buffer += decoder.decode(result.value, { stream: true }).replaceAll("\r", "");
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
          const parsed = chatStreamEventSchema.safeParse(JSON.parse(data) as unknown);
          if (!parsed.success)
            throw new OperatorApiError(
              "chat-event-contract",
              "Chat stream event failed validation.",
              response.status,
            );
          onEvent(parsed.data);
        }
        boundary = buffer.indexOf("\n\n");
      }
    }
  } finally {
    reader.releaseLock();
  }
}
