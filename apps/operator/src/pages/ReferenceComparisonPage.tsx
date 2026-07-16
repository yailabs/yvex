/*
 * Owner: apps/operator optional external-reference comparison workbench.
 * Owns: explicit comparison sessions, streamed deltas, cancellation, parameters, metadata, and session selection.
 * Does not own: YVEX workspace state, YVEX generation, provider secrets, comparison transport, or capability fallback.
 * Invariants: execution owner is always External comparison endpoint and no comparison result enters primary YVEX truth.
 * Boundary: this diagnostics surface is available only by explicit navigation and never represents YVEX runtime evidence.
 */
import { ArrowLeft, Plus, Send, Square, Trash2 } from "lucide-react";
import { useMemo, useRef, useState } from "react";
import { Link } from "react-router-dom";

import type {
  ChatMessage,
  ChatSession,
  ChatStreamEvent,
  GenerationParameters,
} from "../../shared/contracts.ts";
import { operatorApi, streamReferenceComparison } from "../api.ts";
import { Fact, FactGrid, PageHeader, Panel, ResourceBoundary } from "../components/Primitives.tsx";
import { StatusBadge } from "../components/Status.tsx";
import { useApiResource } from "../resource.ts";
import { useOperatorState } from "../state/operator-state.tsx";
import { EmptyState } from "./PageSupport.tsx";

/** Creates one browser-only optimistic comparison message until the server returns its session. */
function optimisticMessage(
  role: "user" | "assistant",
  content: string,
  state: ChatMessage["state"],
): ChatMessage {
  return {
    id: `optimistic-${crypto.randomUUID()}`,
    role,
    content,
    state,
    createdAt: new Date().toISOString(),
    completedAt: state === "complete" ? new Date().toISOString() : null,
    metadata: null,
  };
}

/** Renders the explicitly enabled comparison transport outside the primary YVEX workflow. */
export function ReferenceComparisonPage() {
  const app = useOperatorState();
  const settings = app.settings.data?.comparisonEndpoint;
  const status = useApiResource("reference-comparison-status", operatorApi.comparisonStatus);
  const sessions = useApiResource("reference-comparison-sessions", operatorApi.comparisonSessions);
  const [activeId, setActiveId] = useState<string | null>(null);
  const [sessionOverride, setSessionOverride] = useState<ChatSession | null>(null);
  const [prompt, setPrompt] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [testing, setTesting] = useState(false);
  const streamController = useRef<AbortController | null>(null);
  const session =
    sessionOverride ??
    sessions.data?.sessions.find((item) => item.id === activeId) ??
    sessions.data?.sessions[0] ??
    null;
  const [parameters, setParameters] = useState<GenerationParameters>(() => ({
    model: settings?.defaultModel || "unconfigured",
    maxOutputTokens: 512,
    temperature: 0.7,
    topP: 0.95,
    seed: null,
    stop: [],
    stream: true,
  }));
  const streaming = session?.state === "streaming" || session?.state === "cancelling";
  const endpointReady = status.data?.streamingCompatible === true;
  const effectiveParameters = useMemo(
    () => ({ ...parameters, model: settings?.defaultModel || parameters.model }),
    [parameters, settings?.defaultModel],
  );

  /** Creates one diagnostics-only session through the isolated comparison namespace. */
  const createSession = async (): Promise<ChatSession> => {
    const created = await operatorApi.createComparisonSession({
      model: settings?.defaultModel,
    });
    setActiveId(created.id);
    setSessionOverride(created);
    sessions.refresh();
    return created;
  };

  /** Applies typed SSE events incrementally while retaining partial output on cancellation. */
  const onEvent = (event: ChatStreamEvent): void => {
    if (event.type === "request-started") {
      setSessionOverride((current) =>
        current ? { ...current, activeRequestId: event.requestId, state: "streaming" } : current,
      );
      return;
    }
    if (event.type === "token-delta") {
      setSessionOverride((current) => {
        if (!current) return current;
        const messages = [...current.messages];
        const last = messages.at(-1);
        if (last?.role === "assistant")
          messages[messages.length - 1] = {
            ...last,
            content: last.content + event.delta,
            state: "streaming",
          };
        return { ...current, messages };
      });
      return;
    }
    if (event.type === "usage-update") {
      setSessionOverride((current) => (current ? { ...current, usage: event.usage } : current));
      return;
    }
    if (event.type === "timing-update") {
      setSessionOverride((current) => (current ? { ...current, timings: event.timings } : current));
      return;
    }
    if (event.type === "completion" || event.type === "cancellation") {
      setSessionOverride(event.session);
      sessions.refresh();
      return;
    }
    setError(`${event.error.code}: ${event.error.message}`);
  };

  /** Sends one prompt only to the explicitly configured comparison endpoint. */
  const send = async (): Promise<void> => {
    const content = prompt.trim();
    if (!content || !endpointReady || streaming) return;
    let current = session;
    if (!current) current = await createSession();
    setPrompt("");
    setError(null);
    setSessionOverride({
      ...current,
      state: "streaming",
      messages: [
        ...current.messages,
        optimisticMessage("user", content, "complete"),
        optimisticMessage("assistant", "", "streaming"),
      ],
    });
    const controller = new AbortController();
    streamController.current = controller;
    try {
      await streamReferenceComparison(
        current.id,
        { content, parameters: effectiveParameters },
        onEvent,
        controller.signal,
      );
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : "Comparison transport failed.");
      const refreshed = await operatorApi.comparisonSession(current.id).catch(() => null);
      if (refreshed) setSessionOverride(refreshed);
    } finally {
      streamController.current = null;
    }
  };

  /** Cancels only the active comparison request and preserves streamed output. */
  const cancel = async (): Promise<void> => {
    if (!session?.activeRequestId) return;
    setSessionOverride({ ...session, state: "cancelling" });
    try {
      await operatorApi.cancelComparison(session.activeRequestId);
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : "Comparison cancellation failed.");
    }
  };

  return (
    <div className="page reference-comparison-page">
      <PageHeader
        eyebrow="Diagnostics / Explicit external comparison"
        title="Reference comparison"
        summary="Run a bounded differential prompt against an explicitly enabled external endpoint. This is not YVEX execution."
        actions={
          <Link className="button secondary" to="/settings?section=comparison-endpoint">
            <ArrowLeft aria-hidden="true" size={14} /> Comparison settings
          </Link>
        }
      />
      {!settings?.enabled ? (
        <Panel
          title="Comparison endpoint disabled"
          description="Disabled is normal and does not block YVEX."
        >
          <Link className="button secondary" to="/settings?section=comparison-endpoint">
            Configure optional comparison
          </Link>
        </Panel>
      ) : (
        <ResourceBoundary resource={status}>
          {(connection) => (
            <>
              <div className="comparison-boundary prominent">
                <strong>Execution owner: External comparison endpoint</strong>
                <p>
                  No response, token count, timing, or session state from this page is projected
                  into YVEX workspace or execution evidence.
                </p>
              </div>
              <div className="comparison-layout">
                <Panel title="Sessions" description="Private local diagnostics history only.">
                  <div className="comparison-session-list">
                    {(sessions.data?.sessions ?? []).map((item) => (
                      <button
                        type="button"
                        className={session?.id === item.id ? "active" : ""}
                        onClick={() => {
                          setActiveId(item.id);
                          setSessionOverride(item);
                        }}
                        key={item.id}
                      >
                        <strong>{item.title}</strong>
                        <span>
                          {item.remoteModel} · {item.state}
                        </span>
                      </button>
                    ))}
                    {!sessions.data?.sessions.length ? (
                      <EmptyState
                        title="No comparisons"
                        detail="Create a session to run an explicit differential prompt."
                      />
                    ) : null}
                  </div>
                  <div className="inline-actions">
                    <button
                      type="button"
                      className="button secondary"
                      onClick={() => void createSession()}
                    >
                      <Plus aria-hidden="true" size={14} /> New comparison
                    </button>
                    <button
                      type="button"
                      className="button secondary"
                      disabled={!session || streaming}
                      onClick={() =>
                        session
                          ? void operatorApi.deleteComparisonSession(session.id).then(() => {
                              setSessionOverride(null);
                              setActiveId(null);
                              sessions.refresh();
                            })
                          : undefined
                      }
                    >
                      <Trash2 aria-hidden="true" size={14} /> Delete
                    </button>
                  </div>
                </Panel>
                <Panel
                  title={connection.displayName}
                  description={`${connection.baseUrl} · ${connection.defaultModel}`}
                  actions={<StatusBadge status={connection.availability.status} />}
                >
                  <FactGrid>
                    <Fact label="Configured" value={connection.configured ? "yes" : "no"} />
                    <Fact label="Reachable" value={connection.reachable ? "yes" : "no"} />
                    <Fact
                      label="Streaming"
                      value={connection.streamingCompatible ? "compatible" : "not tested"}
                    />
                    <Fact label="Last tested" value={connection.lastTestedAt ?? "Never"} />
                  </FactGrid>
                  <button
                    type="button"
                    className="button secondary"
                    disabled={testing}
                    onClick={() => {
                      setTesting(true);
                      void operatorApi
                        .testComparisonEndpoint()
                        .then(() => {
                          status.refresh();
                          app.refreshAll();
                        })
                        .catch((reason: unknown) =>
                          setError(
                            reason instanceof Error ? reason.message : "Comparison test failed.",
                          ),
                        )
                        .finally(() => setTesting(false));
                    }}
                  >
                    {testing ? "Testing…" : "Test endpoint"}
                  </button>
                </Panel>
              </div>
              <Panel
                className="comparison-transcript-panel"
                title="Differential prompt"
                description="Responses stream incrementally from the named external endpoint."
              >
                <div className="comparison-transcript" aria-live="polite" aria-busy={streaming}>
                  {session?.messages.length ? (
                    session.messages.map((message) => (
                      <article
                        className={`comparison-message role-${message.role}`}
                        key={message.id}
                      >
                        <header>
                          <strong>
                            {message.role === "assistant" ? "External comparison" : "Prompt"}
                          </strong>
                          <StatusBadge
                            status={
                              message.state === "failed"
                                ? "failed"
                                : message.state === "streaming"
                                  ? "loading"
                                  : message.state === "cancelled"
                                    ? "degraded"
                                    : "ready"
                            }
                            value={message.state}
                          />
                        </header>
                        <p>
                          {message.content}
                          {message.state === "streaming" ? (
                            <span className="stream-cursor" aria-hidden="true" />
                          ) : null}
                        </p>
                        {message.metadata ? (
                          <footer>
                            <span>
                              {message.metadata.executionOwner} · {message.metadata.model}
                            </span>
                            <span>
                              {message.metadata.usage.inputTokens === null
                                ? "usage unavailable"
                                : `${message.metadata.usage.inputTokens} in · ${message.metadata.usage.outputTokens} out`}
                            </span>
                            <span>
                              {message.metadata.timings.totalDurationMs === null
                                ? "timing unavailable"
                                : `${message.metadata.timings.totalDurationMs} ms`}
                            </span>
                            <code>{message.metadata.requestId}</code>
                          </footer>
                        ) : null}
                      </article>
                    ))
                  ) : (
                    <EmptyState
                      title="No comparison output"
                      detail="Test the endpoint, create a session, and submit a prompt."
                    />
                  )}
                </div>
                <div className="comparison-parameters">
                  <label>
                    Max output
                    <input
                      type="number"
                      min={1}
                      max={32768}
                      value={parameters.maxOutputTokens}
                      onChange={(event) =>
                        setParameters({
                          ...parameters,
                          maxOutputTokens: Number(event.target.value),
                        })
                      }
                    />
                  </label>
                  <label>
                    Temperature
                    <input
                      type="number"
                      min={0}
                      max={2}
                      step={0.1}
                      value={parameters.temperature}
                      onChange={(event) =>
                        setParameters({ ...parameters, temperature: Number(event.target.value) })
                      }
                    />
                  </label>
                  <label>
                    Top-p
                    <input
                      type="number"
                      min={0}
                      max={1}
                      step={0.05}
                      value={parameters.topP}
                      onChange={(event) =>
                        setParameters({ ...parameters, topP: Number(event.target.value) })
                      }
                    />
                  </label>
                  <label>
                    Seed
                    <input
                      type="number"
                      value={parameters.seed ?? ""}
                      placeholder="endpoint default"
                      onChange={(event) =>
                        setParameters({
                          ...parameters,
                          seed: event.target.value ? Number(event.target.value) : null,
                        })
                      }
                    />
                  </label>
                </div>
                {error ? (
                  <p className="inline-error" role="alert">
                    {error}
                  </p>
                ) : null}
                <div className="comparison-composer">
                  <textarea
                    aria-label="External comparison prompt"
                    rows={3}
                    value={prompt}
                    disabled={!endpointReady || streaming}
                    placeholder={
                      endpointReady
                        ? "Prompt for external differential comparison…"
                        : connection.availability.message
                    }
                    onChange={(event) => setPrompt(event.target.value)}
                    onKeyDown={(event) => {
                      if (event.key === "Enter" && !event.shiftKey) {
                        event.preventDefault();
                        void send();
                      }
                    }}
                  />
                  {streaming ? (
                    <button type="button" className="button danger" onClick={() => void cancel()}>
                      <Square aria-hidden="true" size={14} /> Cancel comparison
                    </button>
                  ) : (
                    <button
                      type="button"
                      className="button secondary"
                      disabled={!endpointReady || !prompt.trim()}
                      onClick={() => void send()}
                    >
                      <Send aria-hidden="true" size={14} /> Run comparison
                    </button>
                  )}
                </div>
              </Panel>
            </>
          )}
        </ResourceBoundary>
      )}
    </div>
  );
}
