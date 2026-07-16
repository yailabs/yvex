/*
 * Owner: apps/operator global YVEX generation console.
 * Owns: console layout preferences, inherited workspace context, exact native capability gating, composer presentation, and window controls.
 * Does not own: runtime sessions, native generation, provider comparison, model selection, token streaming, or fabricated output.
 * Invariants: YVEX is the only execution owner; the console inherits target/artifact/backend/session and never asks for a provider.
 * Boundary: until a real YVEX session and generation endpoint exist, the composer remains blocked and sends no request.
 */
import { Expand, Maximize2, Minimize2, PanelRight, Send, TerminalSquare, X } from "lucide-react";
import {
  useEffect,
  useState,
  type Dispatch,
  type PointerEvent as ReactPointerEvent,
  type SetStateAction,
} from "react";
import { Link } from "react-router-dom";

import { capabilityById, type CapabilityId } from "../../shared/contracts.ts";
import { useOperatorState } from "../state/operator-state.tsx";
import { StatusBadge } from "./Status.tsx";

export type GenerationConsoleMode = "closed" | "compact" | "docked" | "expanded" | "fullscreen";

const requiredCapabilities: readonly { id: CapabilityId; label: string }[] = [
  { id: "artifact.admitted", label: "Artifact admission" },
  { id: "backend.selected", label: "Backend binding" },
  { id: "runtime.binding", label: "Runtime binding" },
  { id: "runtime.model-load", label: "Model load" },
  { id: "generation.tokenizer", label: "Tokenizer" },
  { id: "generation.native", label: "Generation" },
  { id: "generation.streaming", label: "Streaming" },
  { id: "generation.cancellation", label: "Cancellation" },
];

/** Reads one bounded local width used only for console presentation. */
function initialWidth(): number {
  try {
    const stored = window.localStorage.getItem("yvex.operator.generation-console-width");
    const value = Number(stored);
    return Number.isFinite(value) && stored !== null ? Math.min(820, Math.max(400, value)) : 520;
  } catch {
    return 520;
  }
}

/** Renders the global console against YVEX workspace truth with no comparison-provider path. */
export function GenerationConsole({
  mode,
  setMode,
}: {
  mode: GenerationConsoleMode;
  setMode: Dispatch<SetStateAction<GenerationConsoleMode>>;
}) {
  const app = useOperatorState();
  const [width, setWidth] = useState(initialWidth);
  const [composer, setComposer] = useState("");
  const workspace = app.workspace.data;
  const manifest = app.capabilities.data;
  const requirements = requiredCapabilities.map((definition) => ({
    ...definition,
    capability:
      workspace?.capabilities.find((item) => item.id === definition.id) ??
      capabilityById(manifest, definition.id),
  }));
  const sessionReady = workspace?.activeRuntimeSession?.state === "ready";
  const generationEndpointAvailable = false;
  const generationReady =
    generationEndpointAvailable &&
    sessionReady &&
    requirements.every((item) => item.capability?.status === "ready");
  const firstCapabilityBlocker = requirements.find(
    (item) => item.capability?.status !== "ready",
  )?.capability;
  const primaryBlocker = !workspace?.activeTarget
    ? "Select an active YVEX target."
    : !workspace.activeArtifact
      ? "Select and admit a complete YVEX artifact."
      : firstCapabilityBlocker
        ? firstCapabilityBlocker.reason
        : !workspace.activeBackend
          ? "Bind a reported YVEX backend."
          : !workspace.activeRuntimeSession
            ? "Create and load a YVEX runtime session."
            : "No YVEX generation endpoint is admitted by the current adapter contract.";

  useEffect(() => {
    try {
      window.localStorage.setItem("yvex.operator.generation-console-mode", mode);
      window.localStorage.setItem("yvex.operator.generation-console-width", String(width));
      document.documentElement.style.setProperty("--generation-console-width", `${width}px`);
    } catch {
      // The in-memory presentation remains usable when local storage is unavailable.
    }
  }, [mode, width]);

  /** Resizes docked console presentation only and never mutates server workspace state. */
  const resize = (event: ReactPointerEvent<HTMLButtonElement>): void => {
    event.currentTarget.setPointerCapture(event.pointerId);
    const startX = event.clientX;
    const startWidth = width;
    const move = (next: PointerEvent): void =>
      setWidth(Math.min(820, Math.max(400, startWidth + (startX - next.clientX))));
    const stop = (): void => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", stop);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", stop);
  };

  if (mode === "closed") return null;

  return (
    <aside
      className={`generation-console chat-dock mode-${mode}`}
      style={mode === "docked" || mode === "expanded" ? { width } : undefined}
      aria-label="YVEX Generation Console"
    >
      {mode === "docked" || mode === "expanded" ? (
        <button
          type="button"
          className="chat-resize"
          aria-label="Resize Generation Console"
          onPointerDown={resize}
        />
      ) : null}
      <header className="chat-header generation-console-header">
        <div className="chat-owner-mark">
          <TerminalSquare aria-hidden="true" size={18} />
        </div>
        <div>
          <strong>YVEX Generation Console</strong>
          <span>{workspace?.activeTarget?.id ?? "No active target"}</span>
        </div>
        <StatusBadge
          status={generationReady ? "ready" : "blocked"}
          value={generationReady ? "session ready" : "capability gated"}
        />
        <div className="chat-window-actions">
          <button
            type="button"
            className="icon-button"
            aria-label="Compact Generation Console"
            onClick={() => setMode("compact")}
          >
            <Minimize2 aria-hidden="true" size={16} />
          </button>
          <button
            type="button"
            className="icon-button"
            aria-label="Dock Generation Console"
            onClick={() => setMode("docked")}
          >
            <PanelRight aria-hidden="true" size={16} />
          </button>
          <button
            type="button"
            className="icon-button"
            aria-label="Expand Generation Console"
            onClick={() => setMode("expanded")}
          >
            <Expand aria-hidden="true" size={16} />
          </button>
          <button
            type="button"
            className="icon-button"
            aria-label="Full-screen Generation Console"
            onClick={() => setMode("fullscreen")}
          >
            <Maximize2 aria-hidden="true" size={16} />
          </button>
          <button
            type="button"
            className="icon-button"
            aria-label="Close Generation Console"
            onClick={() => setMode("closed")}
          >
            <X aria-hidden="true" size={17} />
          </button>
        </div>
      </header>

      <div className="generation-context-strip" aria-label="Inherited runtime context">
        <div>
          <span>Target</span>
          <strong>{workspace?.activeTarget?.id ?? "Not selected"}</strong>
        </div>
        <div>
          <span>Artifact</span>
          <strong>{workspace?.activeArtifact?.artifactClass ?? "Not selected"}</strong>
        </div>
        <div>
          <span>Backend</span>
          <strong>{workspace?.activeBackend?.label ?? "Not bound"}</strong>
        </div>
        <div>
          <span>Session</span>
          <strong>{workspace?.activeRuntimeSession?.state ?? "Unloaded"}</strong>
        </div>
      </div>

      <div className="chat-transcript generation-console-body" aria-live="polite">
        <div className="native-refusal">
          <strong>YVEX generation capability</strong>
          <p>{primaryBlocker}</p>
          <ul>
            {requirements.map((item) => (
              <li key={item.id}>
                <span>{item.label}</span>
                <StatusBadge status={item.capability?.status ?? "unavailable"} />
                <small>{item.capability?.reason ?? "No capability observation."}</small>
              </li>
            ))}
          </ul>
          <p className="console-boundary">
            No external endpoint is selected or used. The console will execute only through the
            active YVEX runtime session.
          </p>
          <Link className="button secondary" to="/runtime?tab=readiness">
            Inspect Runtime workbench
          </Link>
        </div>
      </div>

      <footer className="chat-composer generation-composer">
        <textarea
          rows={mode === "compact" ? 2 : 3}
          aria-label="YVEX generation prompt"
          value={composer}
          disabled={!generationReady}
          placeholder={generationReady ? "Prompt the active YVEX runtime session…" : primaryBlocker}
          onChange={(event) => setComposer(event.target.value)}
        />
        <div>
          <span>Execution owner: YVEX</span>
          <button
            type="button"
            className="button primary"
            disabled={!generationReady || !composer.trim()}
          >
            <Send aria-hidden="true" size={14} /> Generate
          </button>
        </div>
      </footer>
    </aside>
  );
}
