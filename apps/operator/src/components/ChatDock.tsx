/*
 * Owner: apps/operator global LLM chat dock.
 * Owns: dock layout preferences, session interaction, lane selection, composer, incremental SSE rendering, cancellation, and message metadata.
 * Does not own: provider secrets, server session authority, native generation, provider HTTP calls, or silent lane fallback.
 * Invariants: the visible execution owner matches the selected session lane and native refusal never sends a provider request.
 * Boundary: reference-provider output is model interaction evidence only and never native YVEX generation proof.
 */
import {
  Bot,
  ChevronDown,
  Eraser,
  Expand,
  Maximize2,
  MessageSquare,
  Minimize2,
  PanelRight,
  Plus,
  RotateCcw,
  Send,
  Square,
  Trash2,
  X,
} from "lucide-react";
import {
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
  type Dispatch,
  type PointerEvent as ReactPointerEvent,
  type SetStateAction,
} from "react";
import { Link } from "react-router-dom";

import {
  capabilityById,
  type ChatLane,
  type ChatMessage,
  type ChatSession,
  type ChatStreamEvent,
  type GenerationParameters,
} from "../../shared/contracts.ts";
import { operatorApi, streamChatMessage } from "../api.ts";
import { useApiResource } from "../resource.ts";
import { useOperatorState } from "../state/operator-state.tsx";
import { StatusBadge } from "./Status.tsx";

export type ChatDockMode = "closed" | "compact" | "docked" | "expanded" | "fullscreen";

/** Reads one bounded numeric dock preference without exposing server or provider state. */
function initialWidth(): number {
  const stored = window.localStorage.getItem("yvex.operator.chat-width");
  if (stored === null) return 480;
  const value = Number(stored);
  return Number.isFinite(value) ? Math.min(760, Math.max(360, value)) : 480;
}

/** Formats optional latency and throughput metadata without inventing absent measurements. */
function timingLabel(message: ChatMessage): string {
  const timing = message.metadata?.timings;
  if (!timing) return "";
  const values = [
    timing.timeToFirstTokenMs === null ? null : `TTFT ${timing.timeToFirstTokenMs} ms`,
    timing.totalDurationMs === null ? null : `${(timing.totalDurationMs / 1_000).toFixed(2)} s`,
    timing.tokensPerSecond === null ? null : `${timing.tokensPerSecond.toFixed(1)} tok/s`,
  ].filter(Boolean);
  return values.join(" · ");
}

/** Formats optional provider token usage and leaves unavailable counts absent. */
function usageLabel(message: ChatMessage): string {
  const usage = message.metadata?.usage;
  if (!usage) return "";
  return [
    usage.inputTokens === null ? null : `${usage.inputTokens} in`,
    usage.outputTokens === null ? null : `${usage.outputTokens} out`,
    usage.totalTokens === null ? null : `${usage.totalTokens} total`,
  ]
    .filter(Boolean)
    .join(" · ");
}

/** Creates an optimistic browser-only message until the server returns the authoritative session snapshot. */
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

/** Renders and operates the global chat dock against typed session/provider endpoints. */
export function ChatDock({
  mode,
  setMode,
  newSessionSignal,
}: {
  mode: ChatDockMode;
  setMode: Dispatch<SetStateAction<ChatDockMode>>;
  newSessionSignal: number;
}) {
  const app = useOperatorState();
  const sessions = useApiResource("chat-sessions", operatorApi.sessions);
  const [sessionOverride, setSession] = useState<ChatSession | null>(null);
  const [width, setWidth] = useState(initialWidth);
  const [showSessions, setShowSessions] = useState(false);
  const [showParameters, setShowParameters] = useState(
    () => window.localStorage.getItem("yvex.operator.chat-parameters") === "true",
  );
  const [composer, setComposer] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [announcement, setAnnouncement] = useState("");
  const [titleDraft, setTitleDraft] = useState("");
  const composerRef = useRef<HTMLTextAreaElement>(null);
  const streamController = useRef<AbortController | null>(null);
  const lastNewSignal = useRef(newSessionSignal);

  const providerModel =
    app.selectedProviderModel || app.settings.data?.referenceProvider.defaultModel || "";
  const defaultParameters = useMemo<GenerationParameters>(
    () => ({
      model: providerModel || "unconfigured",
      maxOutputTokens: 512,
      temperature: 0.7,
      topP: 0.95,
      seed: null,
      stop: [],
      stream: true,
    }),
    [providerModel],
  );
  const [parameters, setParameters] = useState<GenerationParameters>(defaultParameters);
  const { selectedLane, setActiveSessionId, setSelectedLane } = app;
  const refreshSessions = sessions.refresh;
  const session =
    sessionOverride ??
    sessions.data?.sessions.find((item) => item.id === app.activeSessionId) ??
    sessions.data?.sessions[0] ??
    null;

  useEffect(() => {
    try {
      window.localStorage.setItem("yvex.operator.chat-mode", mode);
      window.localStorage.setItem("yvex.operator.chat-width", String(width));
      window.localStorage.setItem("yvex.operator.chat-parameters", String(showParameters));
      document.documentElement.style.setProperty("--chat-dock-width", `${width}px`);
    } catch {
      // Presentation state remains functional in memory when storage is unavailable.
    }
  }, [mode, showParameters, width]);

  const createSession = useCallback(
    async (lane: ChatLane = selectedLane): Promise<ChatSession> => {
      const created = await operatorApi.createSession({
        lane,
        model: lane === "reference-provider" ? providerModel : undefined,
      });
      setSession(created);
      setTitleDraft(created.title);
      setParameters(created.generationParameters);
      setActiveSessionId(created.id);
      setSelectedLane(lane);
      refreshSessions();
      setError(null);
      requestAnimationFrame(() => composerRef.current?.focus());
      return created;
    },
    [
      providerModel,
      refreshSessions,
      selectedLane,
      setActiveSessionId,
      setError,
      setParameters,
      setSelectedLane,
      setSession,
      setTitleDraft,
    ],
  );

  useEffect(() => {
    if (newSessionSignal === lastNewSignal.current) return;
    lastNewSignal.current = newSessionSignal;
    void createSession();
  }, [createSession, newSessionSignal]);

  const manifest = app.capabilities.data;
  const nativeRequirements = [
    capabilityById(manifest, "runtime.binding"),
    capabilityById(manifest, "generation.tokenizer"),
    capabilityById(manifest, "generation.streaming"),
    capabilityById(manifest, "generation.cancellation"),
  ].filter((item) => item !== null);
  const providerStreaming = capabilityById(manifest, "provider.streaming");
  const laneReady =
    app.selectedLane === "reference-provider"
      ? providerStreaming?.status === "ready"
      : nativeRequirements.every((item) => item?.status === "ready");
  const disabledReason =
    app.selectedLane === "reference-provider"
      ? (providerStreaming?.reason ?? "Configure and test a reference provider.")
      : "Native YVEX generation requires runtime binding, tokenizer, streaming generation, and cancellation.";
  const streaming = session?.state === "streaming" || session?.state === "cancelling";

  const onStreamEvent = (event: ChatStreamEvent): void => {
    if (event.type === "request-started") {
      setSession((current) =>
        current ? { ...current, activeRequestId: event.requestId, state: "streaming" } : current,
      );
      setAnnouncement("Reference provider generation started.");
    } else if (event.type === "token-delta") {
      setSession((current) => {
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
    } else if (event.type === "usage-update") {
      setSession((current) => (current ? { ...current, usage: event.usage } : current));
    } else if (event.type === "timing-update") {
      setSession((current) => (current ? { ...current, timings: event.timings } : current));
    } else if (event.type === "completion" || event.type === "cancellation") {
      setSession(event.session);
      setAnnouncement(
        event.type === "completion"
          ? "Reference provider generation completed."
          : "Generation cancelled; partial output preserved.",
      );
      sessions.refresh();
      app.refreshAll();
    } else if (event.type === "structured-error" || event.type === "refusal") {
      setError(`${event.error.code}: ${event.error.message}`);
      setSession((current) => {
        if (!current) return current;
        const messages = [...current.messages];
        const last = messages.at(-1);
        if (last?.role === "assistant" && last.state === "streaming") {
          messages[messages.length - 1] = { ...last, state: "failed" };
        }
        return { ...current, activeRequestId: null, messages, state: "failed" };
      });
      setAnnouncement("Generation failed.");
      sessions.refresh();
      app.refreshAll();
    }
  };

  const sendContent = async (content: string): Promise<void> => {
    const trimmed = content.trim();
    if (!trimmed || streaming || !laneReady) return;
    let current = session;
    if (!current || current.lane !== app.selectedLane)
      current = await createSession(app.selectedLane);
    if (current.lane === "native-yvex") {
      setError(disabledReason);
      return;
    }
    const nextParameters = { ...parameters, model: providerModel || parameters.model };
    setParameters(nextParameters);
    setComposer("");
    setError(null);
    setSession({
      ...current,
      state: "streaming",
      messages: [
        ...current.messages,
        optimisticMessage("user", trimmed, "complete"),
        optimisticMessage("assistant", "", "streaming"),
      ],
    });
    const controller = new AbortController();
    streamController.current = controller;
    try {
      await streamChatMessage(
        current.id,
        { content: trimmed, parameters: nextParameters },
        onStreamEvent,
        controller.signal,
      );
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : "Chat transport failed.");
      const refreshed = await operatorApi.session(current.id).catch(() => null);
      if (refreshed) setSession(refreshed);
    } finally {
      streamController.current = null;
    }
  };

  const cancel = async (): Promise<void> => {
    if (!session?.activeRequestId) return;
    setSession({ ...session, state: "cancelling" });
    setAnnouncement("Cancelling generation…");
    await operatorApi.cancelChat(session.activeRequestId).catch((reason: unknown) => {
      setError(reason instanceof Error ? reason.message : "Cancellation failed.");
    });
  };

  const resize = (event: ReactPointerEvent<HTMLButtonElement>): void => {
    event.currentTarget.setPointerCapture(event.pointerId);
    const startX = event.clientX;
    const startWidth = width;
    const move = (next: PointerEvent): void =>
      setWidth(Math.min(760, Math.max(360, startWidth + (startX - next.clientX))));
    const stop = (): void => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", stop);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", stop);
  };

  if (mode === "closed") return null;
  const owner = app.selectedLane === "reference-provider" ? "Reference provider" : "Native YVEX";

  return (
    <aside
      className={`chat-dock mode-${mode}`}
      style={mode === "docked" || mode === "expanded" ? { width } : undefined}
      aria-label="YVEX model chat"
    >
      {mode === "docked" || mode === "expanded" ? (
        <button
          type="button"
          className="chat-resize"
          aria-label="Resize chat dock"
          onPointerDown={resize}
        />
      ) : null}
      <header className="chat-header">
        <div className="chat-owner-mark">
          <Bot aria-hidden="true" size={18} />
        </div>
        <div>
          <strong>{owner}</strong>
          <span>
            {app.selectedLane === "reference-provider"
              ? session?.remoteModel || providerModel || "Model not configured"
              : session?.nativeTarget || "deepseek4-v4-flash"}
          </span>
        </div>
        <StatusBadge
          status={streaming ? "loading" : laneReady ? "ready" : "blocked"}
          value={streaming ? "streaming" : laneReady ? "connected" : "blocked"}
        />
        <div className="chat-window-actions">
          <button
            type="button"
            className="icon-button"
            aria-label="Compact chat"
            onClick={() => setMode("compact")}
          >
            <Minimize2 aria-hidden="true" size={16} />
          </button>
          <button
            type="button"
            className="icon-button"
            aria-label="Dock chat"
            onClick={() => setMode("docked")}
          >
            <PanelRight aria-hidden="true" size={16} />
          </button>
          <button
            type="button"
            className="icon-button"
            aria-label="Expand chat"
            onClick={() => setMode("expanded")}
          >
            <Expand aria-hidden="true" size={16} />
          </button>
          <button
            type="button"
            className="icon-button"
            aria-label="Full screen chat"
            onClick={() => setMode("fullscreen")}
          >
            <Maximize2 aria-hidden="true" size={16} />
          </button>
          <button
            type="button"
            className="icon-button"
            aria-label="Close chat"
            onClick={() => setMode("closed")}
          >
            <X aria-hidden="true" size={17} />
          </button>
        </div>
      </header>

      <div className="chat-toolbar">
        <div className="lane-switch" role="group" aria-label="Execution lane">
          <button
            type="button"
            className={app.selectedLane === "reference-provider" ? "active" : ""}
            onClick={() => {
              app.setSelectedLane("reference-provider");
              void createSession("reference-provider");
            }}
          >
            Reference
          </button>
          <button
            type="button"
            className={app.selectedLane === "native-yvex" ? "active" : ""}
            onClick={() => {
              app.setSelectedLane("native-yvex");
              void createSession("native-yvex");
            }}
          >
            Native YVEX
          </button>
        </div>
        <button
          type="button"
          className="toolbar-button"
          aria-expanded={showSessions}
          onClick={() => setShowSessions((value) => !value)}
        >
          <MessageSquare aria-hidden="true" size={15} /> Sessions{" "}
          <ChevronDown aria-hidden="true" size={14} />
        </button>
        <button type="button" className="toolbar-button" onClick={() => void createSession()}>
          <Plus aria-hidden="true" size={15} /> New
        </button>
      </div>

      {showSessions ? (
        <div className="session-panel">
          <div className="session-list">
            {(sessions.data?.sessions ?? []).map((record) => (
              <button
                type="button"
                className={record.id === session?.id ? "active" : ""}
                onClick={() => {
                  setSession(record);
                  setTitleDraft(record.title);
                  setParameters(record.generationParameters);
                  app.setActiveSessionId(record.id);
                  app.setSelectedLane(record.lane);
                  setShowSessions(false);
                }}
                key={record.id}
              >
                <strong>{record.title}</strong>
                <span>
                  {record.lane} · {record.state}
                </span>
              </button>
            ))}
          </div>
          {session ? (
            <div className="session-actions">
              <input
                aria-label="Session title"
                value={titleDraft || session.title}
                onChange={(event) => setTitleDraft(event.target.value)}
              />
              <button
                type="button"
                onClick={() =>
                  void operatorApi
                    .renameSession(session.id, titleDraft || session.title)
                    .then(setSession)
                    .then(() => sessions.refresh())
                }
              >
                Save
              </button>
              <button
                type="button"
                aria-label="Clear current session"
                onClick={() =>
                  void operatorApi
                    .clearSession(session.id)
                    .then(setSession)
                    .then(() => sessions.refresh())
                }
              >
                <Eraser aria-hidden="true" size={15} />
              </button>
              <button
                type="button"
                aria-label="Delete current session"
                onClick={() =>
                  void operatorApi.deleteSession(session.id).then(() => {
                    setSession(null);
                    app.setActiveSessionId(null);
                    sessions.refresh();
                  })
                }
              >
                <Trash2 aria-hidden="true" size={15} />
              </button>
            </div>
          ) : null}
        </div>
      ) : null}

      <div className="chat-transcript" aria-live="polite" aria-busy={streaming}>
        {app.selectedLane === "native-yvex" && !laneReady ? (
          <div className="native-refusal">
            <strong>Native YVEX generation is unavailable</strong>
            <ul>
              {nativeRequirements.map((capability) => (
                <li key={capability?.id}>
                  <span>{capability?.id}</span>
                  <StatusBadge status={capability?.status ?? "unavailable"} />
                </li>
              ))}
            </ul>
            <p>No request will be redirected to the reference provider.</p>
          </div>
        ) : !session?.messages.length ? (
          <div className="chat-empty">
            <Bot aria-hidden="true" size={24} />
            <strong>Start a local engineering session</strong>
            <p>
              Messages are executed only by the lane named above. Reference secrets stay in the
              adapter.
            </p>
          </div>
        ) : (
          session.messages.map((message) => (
            <article
              className={`chat-message role-${message.role}`}
              data-state={message.state}
              key={message.id}
            >
              <header>
                <strong>
                  {message.role === "assistant"
                    ? (message.metadata?.executionOwner ?? owner)
                    : "You"}
                </strong>
                <span>{message.state}</span>
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
                    {message.metadata.provider} · {message.metadata.model}
                  </span>
                  {usageLabel(message) ? <span>{usageLabel(message)}</span> : null}
                  {timingLabel(message) ? <span>{timingLabel(message)}</span> : null}
                  <code>{message.metadata.requestId}</code>
                </footer>
              ) : null}
              {message.role === "assistant" && ["cancelled", "failed"].includes(message.state) ? (
                <button
                  type="button"
                  className="text-button"
                  onClick={() => {
                    const previous = session.messages
                      .slice(0, session.messages.indexOf(message))
                      .reverse()
                      .find((item) => item.role === "user");
                    if (previous) void sendContent(previous.content);
                  }}
                >
                  <RotateCcw aria-hidden="true" size={14} /> Retry last prompt
                </button>
              ) : null}
            </article>
          ))
        )}
      </div>

      {error ? (
        <div className="chat-error" role="alert">
          {error}
        </div>
      ) : null}
      {!laneReady ? (
        <div className="composer-blocker">
          <span>{disabledReason}</span>
          {app.selectedLane === "reference-provider" ? (
            <Link to="/settings?section=reference-provider">Configure provider</Link>
          ) : (
            <Link to="/runtime?tab=stages">Inspect native capabilities</Link>
          )}
        </div>
      ) : null}

      {showParameters ? (
        <div className="generation-panel">
          <label>
            Model
            <input
              value={providerModel || parameters.model}
              disabled={app.selectedLane === "native-yvex"}
              onChange={(event) => {
                app.setSelectedProviderModel(event.target.value);
                setParameters({ ...parameters, model: event.target.value });
              }}
            />
          </label>
          <label>
            Max output
            <input
              type="number"
              min={1}
              max={32768}
              value={parameters.maxOutputTokens}
              onChange={(event) =>
                setParameters({ ...parameters, maxOutputTokens: Number(event.target.value) })
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
              placeholder="Provider default"
              onChange={(event) =>
                setParameters({
                  ...parameters,
                  seed: event.target.value ? Number(event.target.value) : null,
                })
              }
            />
          </label>
          <label className="wide">
            Stop sequences
            <input
              value={parameters.stop.join(", ")}
              placeholder="comma separated"
              onChange={(event) =>
                setParameters({
                  ...parameters,
                  stop: event.target.value
                    .split(",")
                    .map((item) => item.trim())
                    .filter(Boolean)
                    .slice(0, 16),
                })
              }
            />
          </label>
        </div>
      ) : null}

      <footer className="chat-composer">
        <textarea
          ref={composerRef}
          rows={mode === "compact" ? 2 : 3}
          aria-label="Chat message"
          value={composer}
          disabled={!laneReady || streaming}
          placeholder={laneReady ? `Message ${owner}…` : disabledReason}
          onChange={(event) => setComposer(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === "Enter" && !event.shiftKey) {
              event.preventDefault();
              void sendContent(composer);
            }
            if (event.key === "Escape" && streaming) {
              event.preventDefault();
              void cancel();
            }
          }}
        />
        <div>
          <button
            type="button"
            className="text-button"
            aria-expanded={showParameters}
            onClick={() => setShowParameters((value) => !value)}
          >
            Parameters
          </button>
          <span>Enter to send · Shift+Enter newline</span>
          {streaming ? (
            <button type="button" className="button danger" onClick={() => void cancel()}>
              <Square aria-hidden="true" size={14} /> Cancel
            </button>
          ) : (
            <button
              type="button"
              className="button primary"
              disabled={!laneReady || !composer.trim()}
              onClick={() => void sendContent(composer)}
            >
              <Send aria-hidden="true" size={15} /> Send
            </button>
          )}
        </div>
      </footer>
      <span className="sr-only" aria-live="assertive">
        {announcement}
      </span>
    </aside>
  );
}
