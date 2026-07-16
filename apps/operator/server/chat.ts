/*
 * Owner: apps/operator external reference-comparison sessions.
 * Owns: private comparison-session persistence, external streaming lifecycle, partial output, cancellation, and metadata.
 * Does not own: comparison HTTP details, primary workspace state, YVEX generation, tokenizer behavior, or fallback.
 * Invariants: every session is explicitly reference-comparison; active output survives cancellation and failure.
 * Boundary: completed messages remain owned by the External comparison endpoint and never become YVEX evidence.
 */
import { randomUUID } from "node:crypto";
import { chmod, mkdir, readFile, rename, writeFile } from "node:fs/promises";
import { join } from "node:path";

import {
  chatSessionsResponseSchema,
  createChatSessionSchema,
  renameChatSessionSchema,
  sendChatMessageSchema,
  type ChatSession,
  type ChatStreamEvent,
  type Timing,
  type Usage,
} from "../shared/contracts.ts";
import type { OperatorConfig } from "./config.ts";
import type { EventHistory } from "./events.ts";
import type { JobManager } from "./jobs.ts";
import { ProviderTransportError, type ReferenceProviderService } from "./provider.ts";
import type { OperatorSettingsStore } from "./settings.ts";

const emptyUsage: Usage = { inputTokens: null, outputTokens: null, totalTokens: null };
const emptyTiming: Timing = {
  timeToFirstTokenMs: null,
  totalDurationMs: null,
  tokensPerSecond: null,
};
type StreamEmitter = (event: ChatStreamEvent) => void | Promise<void>;

interface ActiveRequest {
  controller: AbortController;
  jobId: string;
  sessionId: string;
}

/** Owns bounded, mode-0600 local session storage and serialized persistence. */
class ChatSessionStore {
  private readonly path: string;
  private sessions: ChatSession[] | null = null;
  private mutation: Promise<void> = Promise.resolve();

  constructor(
    private readonly config: OperatorConfig,
    private readonly clock: () => number,
  ) {
    this.path = join(config.configDirectory, "comparison-sessions.json");
  }

  /** Loads schema-valid session records defensively; corrupt local state fails empty. */
  async list(): Promise<ChatSession[]> {
    if (!this.sessions) {
      try {
        const parsed = JSON.parse(await readFile(this.path, "utf8")) as unknown;
        const result = chatSessionsResponseSchema.safeParse({ sessions: parsed });
        this.sessions = result.success ? result.data.sessions : [];
      } catch {
        this.sessions = [];
      }
    }
    return structuredClone(this.sessions).sort((left, right) =>
      right.updatedAt.localeCompare(left.updatedAt),
    );
  }

  /** Returns one defensive session or null for an unknown ID. */
  async get(id: string): Promise<ChatSession | null> {
    await this.list();
    const session = this.sessions?.find((candidate) => candidate.id === id);
    return session ? structuredClone(session) : null;
  }

  /** Inserts one bounded session and persists prompts only in the private local file. */
  async insert(session: ChatSession): Promise<ChatSession> {
    await this.list();
    this.sessions?.unshift(session);
    if ((this.sessions?.length ?? 0) > this.config.sessionRetention)
      this.sessions?.splice(this.config.sessionRetention);
    await this.persist();
    return structuredClone(session);
  }

  /** Mutates one owned session under the persistence queue and returns its defensive after-state. */
  async update(
    id: string,
    mutate: (session: ChatSession) => void,
    persist = true,
  ): Promise<ChatSession> {
    await this.list();
    const session = this.sessions?.find((candidate) => candidate.id === id);
    if (!session) throw new Error("chat session not found");
    mutate(session);
    session.updatedAt = new Date(this.clock()).toISOString();
    if (persist) await this.persist();
    return structuredClone(session);
  }

  /** Deletes one inactive session and preserves all other session identity. */
  async delete(id: string): Promise<boolean> {
    await this.list();
    const index = this.sessions?.findIndex((candidate) => candidate.id === id) ?? -1;
    if (index < 0) return false;
    if (this.sessions?.[index]?.activeRequestId)
      throw new Error("active chat session cannot be deleted");
    this.sessions?.splice(index, 1);
    await this.persist();
    return true;
  }

  /** Flushes current in-memory deltas to the private local persistence boundary. */
  async flush(): Promise<void> {
    await this.persist();
  }

  /** Atomically replaces the bounded session file with restrictive permissions. */
  private async persist(): Promise<void> {
    this.mutation = this.mutation.then(async () => {
      await mkdir(this.config.configDirectory, { recursive: true, mode: 0o700 });
      await chmod(this.config.configDirectory, 0o700);
      const temporary = `${this.path}.${process.pid}-${this.clock()}.tmp`;
      await writeFile(temporary, `${JSON.stringify(this.sessions ?? [], null, 2)}\n`, {
        mode: 0o600,
      });
      await rename(temporary, this.path);
      await chmod(this.path, 0o600);
    });
    await this.mutation;
  }
}

/** Coordinates explicitly requested external comparisons and keeps execution ownership visible. */
export class ChatService {
  private readonly store: ChatSessionStore;
  private readonly active = new Map<string, ActiveRequest>();

  constructor(
    config: OperatorConfig,
    private readonly settings: OperatorSettingsStore,
    private readonly provider: ReferenceProviderService,
    private readonly jobs: JobManager,
    private readonly history: EventHistory,
    private readonly clock: () => number = Date.now,
  ) {
    this.store = new ChatSessionStore(config, clock);
  }

  /** Creates one local comparison session without probing or touching YVEX workspace state. */
  async create(input: unknown): Promise<ChatSession> {
    const value = createChatSessionSchema.parse(input);
    const publicSettings = await this.settings.publicSnapshot();
    const model = value.model || publicSettings.comparisonEndpoint.defaultModel || "unconfigured";
    const now = new Date(this.clock()).toISOString();
    const session: ChatSession = {
      id: randomUUID(),
      title: value.title ?? "New comparison",
      lane: "reference-comparison",
      provider: publicSettings.comparisonEndpoint.displayName,
      remoteModel: model,
      nativeTarget: null,
      state: "idle",
      messages: [],
      generationParameters: {
        model,
        maxOutputTokens: 512,
        temperature: 0.7,
        topP: 0.95,
        seed: null,
        stop: [],
        stream: true,
      },
      startedAt: now,
      updatedAt: now,
      usage: { ...emptyUsage },
      timings: { ...emptyTiming },
      associatedEvidence: [],
      activeRequestId: null,
    };
    return this.store.insert(session);
  }

  /** Returns newest-first local session snapshots. */
  list(): Promise<ChatSession[]> {
    return this.store.list();
  }

  /** Returns one session without starting comparison or YVEX work. */
  get(id: string): Promise<ChatSession | null> {
    return this.store.get(id);
  }

  /** Renames one session through a bounded validated title mutation. */
  async rename(id: string, input: unknown): Promise<ChatSession> {
    const value = renameChatSessionSchema.parse(input);
    return this.store.update(id, (session) => {
      session.title = value.title;
    });
  }

  /** Clears messages from an inactive session while preserving its identity and parameters. */
  clear(id: string): Promise<ChatSession> {
    return this.store.update(id, (session) => {
      if (session.activeRequestId) throw new Error("active chat session cannot be cleared");
      session.messages = [];
      session.usage = { ...emptyUsage };
      session.timings = { ...emptyTiming };
      session.associatedEvidence = [];
    });
  }

  /** Deletes one inactive local session. */
  delete(id: string): Promise<boolean> {
    return this.store.delete(id);
  }

  /** Streams one explicitly requested external comparison and never touches YVEX execution. */
  async streamMessage(sessionId: string, input: unknown, emit: StreamEmitter): Promise<void> {
    const value = sendChatMessageSchema.parse(input);
    const existing = await this.store.get(sessionId);
    if (!existing) throw new Error("chat session not found");
    if (existing.activeRequestId) throw new Error("chat session already has an active request");
    const requestId = randomUUID();
    const assistantId = randomUUID();
    const userId = randomUUID();
    const job = this.jobs.create(
      "reference-comparison",
      "External comparison endpoint",
      "request-start",
      true,
    );
    const controller = new AbortController();
    this.jobs.registerCancellation(job.id, () => controller.abort());
    this.active.set(requestId, { controller, jobId: job.id, sessionId });
    const startedAt = new Date(this.clock()).toISOString();
    const session = await this.store.update(sessionId, (current) => {
      current.state = "streaming";
      current.activeRequestId = requestId;
      current.generationParameters = value.parameters;
      current.remoteModel = value.parameters.model;
      current.messages.push({
        id: userId,
        role: "user",
        content: value.content,
        state: "complete",
        createdAt: startedAt,
        completedAt: startedAt,
        metadata: null,
      });
      current.messages.push({
        id: assistantId,
        role: "assistant",
        content: "",
        state: "streaming",
        createdAt: startedAt,
        completedAt: null,
        metadata: {
          lane: "reference-comparison",
          executionOwner: "External comparison endpoint",
          provider: current.provider,
          model: value.parameters.model,
          requestId,
          usage: { ...emptyUsage },
          timings: { ...emptyTiming },
          evidence: [],
        },
      });
      if (current.title === "New comparison") current.title = value.content.slice(0, 60);
    });
    await emit({ type: "request-started", requestId, jobId: job.id });
    this.jobs.transition(job.id, "starting", "external-comparison-connect");
    try {
      this.jobs.transition(job.id, "running", "streaming");
      const providerMessages = session.messages
        .filter((message) => message.id !== assistantId)
        .map((message) => ({ role: message.role, content: message.content }));
      const result = await this.provider.stream(
        providerMessages,
        value.parameters,
        controller.signal,
        {
          delta: async (delta) => {
            await this.store.update(
              sessionId,
              (current) => {
                const message = current.messages.find((candidate) => candidate.id === assistantId);
                if (message) message.content += delta;
              },
              false,
            );
            await emit({ type: "token-delta", requestId, delta });
          },
          usage: async (usage) => {
            await this.store.update(
              sessionId,
              (current) => {
                current.usage = usage;
                const message = current.messages.find((candidate) => candidate.id === assistantId);
                if (message?.metadata) message.metadata.usage = usage;
              },
              false,
            );
            await emit({ type: "usage-update", requestId, usage });
          },
          timing: async (timings) => {
            await emit({ type: "timing-update", requestId, timings });
          },
        },
      );
      const completed = await this.store.update(sessionId, (current) => {
        current.state = "idle";
        current.activeRequestId = null;
        current.usage = result.usage;
        current.timings = result.timings;
        const message = current.messages.find((candidate) => candidate.id === assistantId);
        if (message) {
          message.state = "complete";
          message.completedAt = new Date(this.clock()).toISOString();
          if (message.metadata) {
            message.metadata.usage = result.usage;
            message.metadata.timings = result.timings;
          }
        }
      });
      this.jobs.transition(job.id, "completed", "completed", { resultReference: sessionId });
      this.history.record(
        "comparison-generation",
        "info",
        "External reference comparison completed.",
        requestId,
      );
      await emit({ type: "completion", requestId, session: completed });
    } catch (error) {
      if (controller.signal.aborted) {
        const cancelled = await this.finishInterrupted(
          sessionId,
          assistantId,
          requestId,
          "cancelled",
        );
        const currentJob = this.jobs.get(job.id);
        if (currentJob?.state === "running")
          this.jobs.transition(job.id, "cancelling", "cancelling");
        if (this.jobs.get(job.id)?.state === "cancelling")
          this.jobs.transition(job.id, "cancelled", "cancelled", { resultReference: sessionId });
        this.history.record(
          "comparison-cancellation",
          "warning",
          "External reference comparison cancelled; partial output preserved.",
          requestId,
        );
        await emit({ type: "cancellation", requestId, session: cancelled });
      } else {
        const failure =
          error instanceof ProviderTransportError
            ? error
            : new ProviderTransportError(
                "comparison-stream-failed",
                "External comparison stream failed.",
                true,
              );
        await this.finishInterrupted(sessionId, assistantId, requestId, "failed");
        this.jobs.transition(job.id, "failed", "failed", {
          resultReference: sessionId,
          error: { code: failure.code, message: failure.message, retryable: failure.retryable },
        });
        this.history.record("comparison-generation", "error", failure.message, requestId);
        await emit({
          type: "structured-error",
          requestId,
          error: { code: failure.code, message: failure.message, retryable: failure.retryable },
        });
      }
    } finally {
      this.active.delete(requestId);
      await this.store.flush();
    }
  }

  /** Requests cancellation of one active comparison and never touches YVEX or another session. */
  cancel(requestId: string): { requestId: string; jobId: string } {
    const active = this.active.get(requestId);
    if (!active) throw new Error("chat request not found");
    const job = this.jobs.get(active.jobId);
    if (job?.state === "running" || job?.state === "starting")
      this.jobs.transition(active.jobId, "cancelling", "cancelling");
    active.controller.abort();
    return { requestId, jobId: active.jobId };
  }

  /** Marks the active assistant message cancelled/failed while retaining any streamed content. */
  private finishInterrupted(
    sessionId: string,
    assistantId: string,
    requestId: string,
    state: "cancelled" | "failed",
  ): Promise<ChatSession> {
    return this.store.update(sessionId, (current) => {
      current.state = state === "failed" ? "failed" : "idle";
      current.activeRequestId = null;
      const message = current.messages.find((candidate) => candidate.id === assistantId);
      if (message) {
        message.state = state;
        message.completedAt = new Date(this.clock()).toISOString();
        if (message.metadata) message.metadata.requestId = requestId;
      }
    });
  }
}
