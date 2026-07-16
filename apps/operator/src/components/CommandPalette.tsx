/*
 * Owner: apps/operator global command palette.
 * Owns: searchable fixed actions, route/tab navigation, safe producer actions, disabled reasons, keyboard selection, and modal focus.
 * Does not own: arbitrary commands, argv, shell text, provider secrets, producer definitions, or capability calculation.
 * Invariants: every executable action has a stable code-owned ID and only allowlisted producer IDs reach the adapter.
 * Boundary: copying or inspecting a command is not native execution evidence.
 */
import { Command, LoaderCircle, Search, X } from "lucide-react";
import { useEffect, useMemo, useRef, useState, type RefObject } from "react";
import { useNavigate } from "react-router-dom";

import type { Capability, ProducerDescriptor } from "../../shared/contracts.ts";
import { operatorApi } from "../api.ts";
import { capabilityDisplayLabel } from "../capability-labels.ts";
import { pageMetadata, routeIds } from "../navigation.ts";
import { useOperatorState } from "../state/operator-state.tsx";
import type { InspectorItem } from "./Inspector.tsx";

interface PaletteAction {
  id: string;
  label: string;
  category: string;
  detail: string;
  disabledReason: string | null;
  run: () => void | Promise<void>;
}

const tabActions: readonly { id: string; label: string; href: string }[] = [
  { id: "workspace-jobs", label: "Workspace · Jobs", href: "/workspace?panel=jobs" },
  { id: "workspace-events", label: "Workspace · Events", href: "/workspace?panel=events" },
  { id: "build-source", label: "Build · Source", href: "/build?stage=source" },
  { id: "build-architecture", label: "Build · Architecture", href: "/build?stage=architecture" },
  { id: "build-ir", label: "Build · Transformation IR", href: "/build?stage=transformation-ir" },
  { id: "build-lowering", label: "Build · Lowering", href: "/build?stage=physical-lowering" },
  { id: "build-quantization", label: "Build · Quantization", href: "/build?stage=quantization" },
  { id: "build-writer", label: "Build · GGUF Writer", href: "/build?stage=gguf-writer" },
  { id: "runtime-readiness", label: "Runtime · Readiness", href: "/runtime?tab=readiness" },
  { id: "runtime-backend", label: "Runtime · Backend", href: "/runtime?tab=backend" },
  { id: "runtime-sessions", label: "Runtime · Sessions", href: "/runtime?tab=sessions" },
  { id: "runtime-generation", label: "Runtime · Generation", href: "/runtime?tab=generation" },
  { id: "evidence-runs", label: "Evidence · Recent runs", href: "/evidence?tab=runs" },
  {
    id: "environment-binary",
    label: "Environment · Binary resolution",
    href: "/environment?tab=binary",
  },
  { id: "settings-yvex", label: "Settings · YVEX", href: "/settings?section=yvex" },
];

/** Converts one stable capability into a global inspection action. */
function capabilityAction(
  capability: Capability,
  inspect: (item: InspectorItem) => void,
): PaletteAction {
  const label = capabilityDisplayLabel(capability);
  return {
    id: `capability:${capability.id}`,
    label,
    category: "Capabilities",
    detail: `${capability.reason} (${capability.id})`,
    disabledReason: null,
    run: () =>
      inspect({
        kind: "capability",
        title: label,
        subtitle: capability.domain,
        status: capability.status,
        detail: capability.reason,
        rows: [
          { label: "Capability ID", value: capability.id },
          { label: "Source", value: capability.source },
          { label: "Refusal", value: capability.refusalCode ?? "none" },
          { label: "Dependencies", value: capability.requiredDependencies.join(", ") || "none" },
          { label: "Observed", value: capability.lastObservedAt },
        ],
        ...(capability.recoveryAction?.href
          ? {
              recovery: {
                label: capability.recoveryAction.label,
                href: capability.recoveryAction.href,
              },
            }
          : {}),
      }),
  };
}

/** Renders an accessible fixed-action palette and never accepts command text for execution. */
export function CommandPalette({
  open,
  onClose,
  returnFocus,
  onOpenConsole,
  onInspect,
}: {
  open: boolean;
  onClose: () => void;
  returnFocus: RefObject<HTMLElement | null>;
  onOpenConsole: () => void;
  onInspect: (item: InspectorItem) => void;
}) {
  const navigate = useNavigate();
  const state = useOperatorState();
  const dialog = useRef<HTMLDivElement>(null);
  const input = useRef<HTMLInputElement>(null);
  const [query, setQuery] = useState("");
  const [selected, setSelected] = useState(0);
  const [producers, setProducers] = useState<ProducerDescriptor[]>([]);
  const [running, setRunning] = useState<string | null>(null);

  useEffect(() => {
    if (!open) return undefined;
    requestAnimationFrame(() => input.current?.focus());
    const controller = new AbortController();
    void operatorApi
      .producers(controller.signal)
      .then((response) => setProducers(response.producers))
      .catch(() => setProducers([]));
    const onKeyDown = (event: KeyboardEvent): void => {
      if (event.key === "Escape") {
        event.preventDefault();
        onClose();
        returnFocus.current?.focus();
      }
      if (event.key !== "Tab" || !dialog.current) return;
      const nodes = [
        ...dialog.current.querySelectorAll<HTMLElement>(
          "input, button, [href], [tabindex]:not([tabindex='-1'])",
        ),
      ].filter((node) => !node.hasAttribute("disabled"));
      const first = nodes[0];
      const last = nodes.at(-1);
      if (!first || !last) return;
      if (event.shiftKey && document.activeElement === first) {
        event.preventDefault();
        last.focus();
      } else if (!event.shiftKey && document.activeElement === last) {
        event.preventDefault();
        first.focus();
      }
    };
    document.addEventListener("keydown", onKeyDown);
    return () => {
      controller.abort();
      document.removeEventListener("keydown", onKeyDown);
    };
  }, [onClose, open, returnFocus]);

  const actions = useMemo<PaletteAction[]>(() => {
    const go = (href: string): void => {
      void navigate(href);
      onClose();
    };
    const base: PaletteAction[] = routeIds.map((id) => ({
      id: `navigate:${id}`,
      label: `Open ${pageMetadata[id].label}`,
      category: "Navigation",
      detail: pageMetadata[id].summary,
      disabledReason: null,
      run: () => go(pageMetadata[id].path),
    }));
    base.push(
      ...tabActions.map((tab) => ({
        id: `navigate:${tab.id}`,
        label: tab.label,
        category: "Route tabs",
        detail: "Open a persistent secondary surface.",
        disabledReason: null,
        run: () => go(tab.href),
      })),
    );
    base.push(
      {
        id: "generation-console:open",
        label: "Open Generation Console",
        category: "YVEX runtime",
        detail: `Inspect generation for ${state.workspace.data?.activeTarget?.id ?? "the active target"}.`,
        disabledReason: null,
        run: () => {
          onOpenConsole();
          onClose();
        },
      },
      {
        id: "system:reload",
        label: "Retry binary resolution",
        category: "System",
        detail: "Reload settings, invalidate observations, and rerun trusted candidates.",
        disabledReason: null,
        run: async () => {
          await operatorApi.reload();
          state.refreshAll();
          onClose();
        },
      },
    );
    for (const capability of state.workspace.data?.capabilities ??
      state.capabilities.data?.capabilities ??
      [])
      base.push(capabilityAction(capability, onInspect));
    for (const producer of producers) {
      base.push({
        id: `producer:inspect:${producer.id}`,
        label: `Inspect ${producer.displayName}`,
        category: "Producers",
        detail: producer.description,
        disabledReason: null,
        run: () =>
          onInspect({
            kind: "producer",
            title: producer.displayName,
            subtitle: producer.domain,
            status: producer.availability.status,
            detail: producer.description,
            rows: [
              { label: "Producer ID", value: producer.id },
              { label: "Command", value: producer.displayCommand },
              { label: "Cache", value: producer.cachePolicy },
              { label: "Last exit", value: producer.lastExit.state },
            ],
          }),
      });
      base.push({
        id: `producer:run:${producer.id}`,
        label: `Run ${producer.displayName}`,
        category: "Safe producer runs",
        detail: producer.displayCommand,
        disabledReason:
          producer.availability.status === "ready" ? null : producer.availability.message,
        run: async () => {
          setRunning(producer.id);
          try {
            await operatorApi.runProducer(producer.id);
            state.refreshAll();
          } finally {
            setRunning(null);
          }
        },
      });
    }
    return base;
  }, [navigate, onClose, onInspect, onOpenConsole, producers, state]);

  const queryTokens = query
    .toLowerCase()
    .split(/[^a-z0-9.-]+/)
    .filter(Boolean);
  const filtered = actions
    .filter((action) => {
      const searchable = `${action.label} ${action.category} ${action.detail}`.toLowerCase();
      return queryTokens.every((token) => searchable.includes(token));
    })
    .sort((left, right) => {
      const score = (action: PaletteAction): number => {
        const label = action.label.toLowerCase();
        return queryTokens.filter((token) => label.includes(token)).length;
      };
      return score(right) - score(left);
    });
  const activate = async (action: PaletteAction | undefined): Promise<void> => {
    if (!action || action.disabledReason) return;
    await action.run();
  };

  return (
    <>
      <button
        type="button"
        className={`overlay-scrim palette-scrim${open ? " open" : ""}`}
        aria-label="Close command palette"
        aria-hidden={!open}
        tabIndex={open ? 0 : -1}
        onClick={onClose}
      />
      <div
        ref={dialog}
        className={`command-palette${open ? " open" : ""}`}
        role="dialog"
        aria-modal="true"
        aria-labelledby="palette-title"
        aria-hidden={!open}
        inert={!open}
      >
        <header>
          <Command aria-hidden="true" size={18} />
          <h2 id="palette-title">Command palette</h2>
          <kbd>Ctrl K</kbd>
          <button
            type="button"
            className="icon-button"
            aria-label="Close command palette"
            onClick={onClose}
          >
            <X aria-hidden="true" size={18} />
          </button>
        </header>
        <div className="palette-search">
          <Search aria-hidden="true" size={17} />
          <input
            ref={input}
            aria-label="Search Operator actions"
            value={query}
            onChange={(event) => {
              setQuery(event.target.value);
              setSelected(0);
            }}
            onKeyDown={(event) => {
              if (event.key === "ArrowDown") {
                event.preventDefault();
                setSelected((value) => Math.min(value + 1, filtered.length - 1));
              }
              if (event.key === "ArrowUp") {
                event.preventDefault();
                setSelected((value) => Math.max(value - 1, 0));
              }
              if (event.key === "Enter") {
                event.preventDefault();
                void activate(filtered[selected]);
              }
            }}
            placeholder="Navigate, inspect, run safe producers…"
          />
        </div>
        <div className="palette-results" role="listbox" aria-label="Operator actions">
          {filtered.length ? (
            filtered.map((action, index) => (
              <button
                type="button"
                role="option"
                aria-selected={selected === index}
                className={selected === index ? "selected" : ""}
                disabled={Boolean(action.disabledReason) || running !== null}
                onMouseMove={() => setSelected(index)}
                onClick={() => void activate(action)}
                key={action.id}
              >
                <span>{action.category}</span>
                <strong>{action.label}</strong>
                <small>{action.disabledReason ?? action.detail}</small>
                {running && action.id.endsWith(running) ? (
                  <LoaderCircle aria-hidden="true" size={15} />
                ) : null}
              </button>
            ))
          ) : (
            <div className="palette-empty">No fixed action matches this search.</div>
          )}
        </div>
      </div>
    </>
  );
}
