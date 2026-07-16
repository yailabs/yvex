/*
 * Owner: apps/operator responsive lifecycle shell.
 * Owns: workbench navigation, authoritative context bar, global mode indicator, shortcuts, command palette, inspector, and Generation Console integration.
 * Does not own: workspace facts, producer policy, capability calculation, comparison secrets, or native execution.
 * Invariants: every context field derives from OperatorWorkspaceState and YVEX is the sole primary execution owner.
 * Boundary: shell connectivity and selection indicators do not promote backend, runtime, or generation readiness.
 */
import {
  Activity,
  Archive,
  Braces,
  Command,
  Cpu,
  Menu,
  Microscope,
  Settings,
  TerminalSquare,
  Workflow,
  X,
  type LucideIcon,
} from "lucide-react";
import { useCallback, useEffect, useRef, useState, type SetStateAction } from "react";
import { NavLink, Outlet, useLocation } from "react-router-dom";

import type { CapabilityId } from "../../shared/contracts.ts";
import {
  pageMetadata,
  primaryNavigation,
  routeFromPath,
  systemNavigation,
  type RouteId,
} from "../navigation.ts";
import { useOperatorState } from "../state/operator-state.tsx";
import { CommandPalette } from "./CommandPalette.tsx";
import { GenerationConsole, type GenerationConsoleMode } from "./GenerationConsole.tsx";
import { InspectorDrawer, type InspectorItem } from "./Inspector.tsx";
import { StatusDot } from "./Status.tsx";

const navIcons: Readonly<Record<RouteId, LucideIcon>> = {
  workspace: Workflow,
  build: Braces,
  artifacts: Archive,
  runtime: Cpu,
  evidence: Microscope,
  environment: Activity,
  settings: Settings,
};

/** Reads one validated local Generation Console layout preference or reports that no override exists. */
function storedConsoleMode(): GenerationConsoleMode | null {
  try {
    const value = window.localStorage.getItem("yvex.operator.generation-console-mode");
    return ["closed", "compact", "docked", "expanded", "fullscreen"].includes(value ?? "")
      ? (value as GenerationConsoleMode)
      : null;
  } catch {
    return null;
  }
}

/** Reads and bounds the stored dock width before it can influence global shell geometry. */
function initialConsoleWidth(): number {
  try {
    const stored = window.localStorage.getItem("yvex.operator.generation-console-width");
    const value = Number(stored);
    return stored !== null && Number.isFinite(value) ? Math.min(820, Math.max(400, value)) : 520;
  } catch {
    return 520;
  }
}

/** Returns one stable capability status for a shell indicator without parsing reason text. */
function capabilityStatus(state: ReturnType<typeof useOperatorState>, id: CapabilityId) {
  return (
    state.workspace.data?.capabilities.find((item) => item.id === id)?.status ??
    state.capabilities.data?.capabilities.find((item) => item.id === id)?.status ??
    "unavailable"
  );
}

/** Renders one fixed navigation group and closes only transient mobile state after navigation. */
function NavigationGroup({
  label,
  items,
  onNavigate,
}: {
  label: string;
  items: typeof primaryNavigation;
  onNavigate: () => void;
}) {
  return (
    <div className="nav-group">
      <span className="nav-group-label">{label}</span>
      {items.map((item) => {
        const Icon = navIcons[item.id];
        return (
          <NavLink
            key={item.id}
            to={item.path}
            onClick={onNavigate}
            className={({ isActive }) => `nav-link${isActive ? " active" : ""}`}
          >
            <Icon aria-hidden="true" size={17} strokeWidth={1.7} />
            <span>{item.label}</span>
          </NavLink>
        );
      })}
    </div>
  );
}

/** Owns route-independent YVEX workbench interaction surfaces and global keyboard bindings. */
export function OperatorShell() {
  const location = useLocation();
  const app = useOperatorState();
  const route = routeFromPath(location.pathname);
  const page = pageMetadata[route];
  const [mobileOpen, setMobileOpen] = useState(false);
  const [paletteOpen, setPaletteOpen] = useState(false);
  const [inspector, setInspector] = useState<InspectorItem | null>(null);
  const [consoleModeOverride, setConsoleModeOverride] = useState<GenerationConsoleMode | null>(
    storedConsoleMode,
  );
  const consoleDefault = app.settings.data?.interface.generationConsoleDefaultMode ?? "closed";
  const consoleMode = consoleModeOverride ?? consoleDefault;
  const setConsoleMode = useCallback(
    (next: SetStateAction<GenerationConsoleMode>) =>
      setConsoleModeOverride((current) => {
        const effective = current ?? consoleDefault;
        return typeof next === "function" ? next(effective) : next;
      }),
    [consoleDefault],
  );
  const mobileTrigger = useRef<HTMLButtonElement>(null);
  const mobileClose = useRef<HTMLButtonElement>(null);
  const paletteTrigger = useRef<HTMLButtonElement>(null);
  const inspectorReturn = useRef<HTMLElement | null>(null);

  useEffect(() => {
    if (!mobileOpen) return undefined;
    mobileClose.current?.focus();
    const escape = (event: KeyboardEvent): void => {
      if (event.key === "Escape") {
        setMobileOpen(false);
        mobileTrigger.current?.focus();
      }
    };
    document.addEventListener("keydown", escape);
    return () => document.removeEventListener("keydown", escape);
  }, [mobileOpen]);

  useEffect(() => {
    const shortcut = (event: KeyboardEvent): void => {
      if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "k") {
        event.preventDefault();
        setPaletteOpen(true);
      }
      if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "j") {
        event.preventDefault();
        setConsoleMode((value) => (value === "closed" ? "docked" : "closed"));
      }
    };
    document.addEventListener("keydown", shortcut);
    return () => document.removeEventListener("keydown", shortcut);
  }, [setConsoleMode]);

  useEffect(() => {
    const openConsole = (): void => setConsoleMode("docked");
    window.addEventListener("yvex:open-generation-console", openConsole);
    return () => window.removeEventListener("yvex:open-generation-console", openConsole);
  }, [setConsoleMode]);

  const workspace = app.workspace.data;
  const adapterStatus = app.health.error
    ? "failed"
    : app.health.initialLoading
      ? "loading"
      : "ready";
  const yvexStatus = capabilityStatus(app, "system.yvex-version-compatible");
  const runtimeStatus = capabilityStatus(app, "runtime.binding");

  return (
    <div
      className="operator-shell"
      data-console-mode={consoleMode}
      style={
        consoleMode === "docked"
          ? ({
              "--generation-console-width": `${initialConsoleWidth()}px`,
            } as React.CSSProperties)
          : undefined
      }
    >
      <a className="skip-link" href="#main-content">
        Skip to Operator content
      </a>
      <button
        type="button"
        className={`mobile-scrim${mobileOpen ? " open" : ""}`}
        tabIndex={mobileOpen ? 0 : -1}
        aria-hidden={!mobileOpen}
        aria-label="Close navigation"
        onClick={() => setMobileOpen(false)}
      />
      <aside
        id="operator-sidebar"
        className={`sidebar${mobileOpen ? " mobile-open" : ""}`}
        aria-label="YVEX Operator navigation"
      >
        <div className="brand-block">
          <div className="brand-mark" aria-hidden="true">
            <span>Y</span>
          </div>
          <div>
            <strong>YVEX</strong>
            <span>Engineering Operator</span>
          </div>
          <button
            ref={mobileClose}
            className="mobile-close icon-button"
            type="button"
            tabIndex={mobileOpen ? 0 : -1}
            aria-hidden={!mobileOpen}
            onClick={() => setMobileOpen(false)}
            aria-label="Close navigation"
          >
            <X aria-hidden="true" size={18} />
          </button>
        </div>
        <nav className="sidebar-nav" aria-label="Primary">
          <NavigationGroup
            label="Workspace"
            items={primaryNavigation}
            onNavigate={() => setMobileOpen(false)}
          />
          <NavigationGroup
            label="System"
            items={systemNavigation}
            onNavigate={() => setMobileOpen(false)}
          />
        </nav>
        <div className="sidebar-footer">
          <div className="global-mode">
            <span>Execution authority</span>
            <strong>YVEX</strong>
            <small>Build and runtime capability-gated</small>
          </div>
          <div className="layer-statuses">
            <span>
              <StatusDot status={adapterStatus} label="Adapter" /> Adapter
            </span>
            <span>
              <StatusDot status={yvexStatus} label="YVEX" /> YVEX
            </span>
            <span>
              <StatusDot status={runtimeStatus} label="Runtime" /> Runtime
            </span>
          </div>
          <code>adapter {app.health.data?.adapter.version ?? "0.2.0"}</code>
        </div>
      </aside>

      <header className="context-bar">
        <button
          ref={mobileTrigger}
          className="mobile-menu icon-button"
          type="button"
          aria-expanded={mobileOpen}
          aria-controls="operator-sidebar"
          onClick={() => setMobileOpen(true)}
        >
          <Menu aria-hidden="true" size={19} />
          <span className="sr-only">Open navigation</span>
        </button>
        <div className="context-path">
          <span>YVEX</span>
          <span aria-hidden="true">/</span>
          <strong>{page.label}</strong>
        </div>
        <div className="context-entities" aria-label="Active YVEX workspace context">
          <span>
            Target <strong>{workspace?.activeTarget?.id ?? "not selected"}</strong>
          </span>
          <span>
            Build <strong>{workspace?.activeBuild?.currentStage ?? "none"}</strong>
          </span>
          <span>
            Artifact <strong>{workspace?.activeArtifact?.artifactClass ?? "none"}</strong>
          </span>
          <span>
            Backend <strong>{workspace?.activeBackend?.label ?? "none"}</strong>
          </span>
          <span>
            Session <strong>{workspace?.activeRuntimeSession?.state ?? "unloaded"}</strong>
          </span>
        </div>
        <button
          ref={paletteTrigger}
          type="button"
          className="context-command"
          aria-label="Open command palette"
          aria-haspopup="dialog"
          aria-expanded={paletteOpen}
          onClick={() => setPaletteOpen(true)}
        >
          <Command aria-hidden="true" size={15} />
          <span>Commands</span>
          <kbd>Ctrl K</kbd>
        </button>
        <button
          type="button"
          className="context-chat"
          aria-label="Toggle Generation Console"
          aria-expanded={consoleMode !== "closed"}
          onClick={() => setConsoleMode((value) => (value === "closed" ? "docked" : "closed"))}
        >
          <TerminalSquare aria-hidden="true" size={16} />
          <span>Generate</span>
          <kbd>Ctrl J</kbd>
        </button>
      </header>

      <main id="main-content" className="workspace" tabIndex={-1}>
        <Outlet />
      </main>

      <CommandPalette
        open={paletteOpen}
        onClose={() => setPaletteOpen(false)}
        returnFocus={paletteTrigger}
        onOpenConsole={() => setConsoleMode("docked")}
        onInspect={(item) => {
          inspectorReturn.current = document.activeElement as HTMLElement | null;
          setInspector(item);
          setPaletteOpen(false);
        }}
      />
      <InspectorDrawer
        item={inspector}
        onClose={() => setInspector(null)}
        returnFocus={inspectorReturn}
      />
      <GenerationConsole mode={consoleMode} setMode={setConsoleMode} />
    </div>
  );
}
