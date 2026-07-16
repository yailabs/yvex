/*
 * Owner: apps/operator responsive global shell.
 * Owns: navigation rail, context bar, global mode indicator, keyboard shortcuts, command palette, inspector, and chat-dock integration.
 * Does not own: page facts, producer policy, capability calculation, provider secrets, or native execution.
 * Invariants: route selection follows the URL and global surfaces remain keyboard reachable at every breakpoint.
 * Boundary: shell connectivity indicators distinguish adapter, YVEX, and provider layers.
 */
import {
  Activity,
  Archive,
  Boxes,
  Braces,
  Command,
  Cpu,
  Database,
  Gauge,
  Menu,
  MessageSquare,
  Microscope,
  Settings,
  SlidersHorizontal,
  X,
  type LucideIcon,
} from "lucide-react";
import { useEffect, useRef, useState } from "react";
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
import { ChatDock, type ChatDockMode } from "./ChatDock.tsx";
import { CommandPalette } from "./CommandPalette.tsx";
import { InspectorDrawer, type InspectorItem } from "./Inspector.tsx";
import { StatusDot } from "./Status.tsx";

const navIcons: Readonly<Record<RouteId, LucideIcon>> = {
  overview: Gauge,
  models: Boxes,
  sources: Database,
  compilation: Braces,
  quantization: SlidersHorizontal,
  artifacts: Archive,
  runtime: Cpu,
  evidence: Microscope,
  "system-health": Activity,
  settings: Settings,
};

/** Reads one validated local chat layout preference and defaults to closed. */
function initialChatMode(): ChatDockMode {
  const value = window.localStorage.getItem("yvex.operator.chat-mode");
  return ["closed", "compact", "docked", "expanded", "fullscreen"].includes(value ?? "")
    ? (value as ChatDockMode)
    : "closed";
}

/** Returns one stable capability status for a global indicator without parsing reason text. */
function capabilityStatus(state: ReturnType<typeof useOperatorState>, id: CapabilityId) {
  return (
    state.capabilities.data?.capabilities.find((item) => item.id === id)?.status ?? "unavailable"
  );
}

/** Renders one borrowed navigation group and closes only transient mobile state after navigation. */
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

/** Owns all route-independent Operator interaction surfaces and global keyboard bindings. */
export function OperatorShell() {
  const location = useLocation();
  const app = useOperatorState();
  const route = routeFromPath(location.pathname);
  const page = pageMetadata[route];
  const [mobileOpen, setMobileOpen] = useState(false);
  const [paletteOpen, setPaletteOpen] = useState(false);
  const [inspector, setInspector] = useState<InspectorItem | null>(null);
  const [chatMode, setChatMode] = useState<ChatDockMode>(initialChatMode);
  const [newSessionSignal, setNewSessionSignal] = useState(0);
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
        setChatMode((value) => (value === "closed" ? "docked" : "closed"));
      }
    };
    document.addEventListener("keydown", shortcut);
    return () => document.removeEventListener("keydown", shortcut);
  }, []);

  useEffect(() => {
    const openChat = (): void => setChatMode("docked");
    window.addEventListener("yvex:open-chat", openChat);
    return () => window.removeEventListener("yvex:open-chat", openChat);
  }, []);

  const activeJobs =
    app.jobs.data?.jobs.filter((job) => !["cancelled", "completed", "failed"].includes(job.state))
      .length ?? 0;
  const adapterStatus = app.health.error
    ? "failed"
    : app.health.initialLoading
      ? "loading"
      : "ready";
  const yvexStatus = capabilityStatus(app, "system.yvex-version-compatible");
  const providerStatus = capabilityStatus(app, "provider.streaming");

  return (
    <div
      className="operator-shell"
      data-chat-mode={chatMode}
      style={
        chatMode === "docked"
          ? ({
              "--chat-dock-width": `${Number(window.localStorage.getItem("yvex.operator.chat-width")) || 480}px`,
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
            <span>Operator</span>
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
            label="Engineering"
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
            <span>Mode</span>
            <strong>Local control plane</strong>
            <small>Native actions capability-gated</small>
          </div>
          <div className="layer-statuses">
            <span>
              <StatusDot status={adapterStatus} label="Adapter" /> Adapter
            </span>
            <span>
              <StatusDot status={yvexStatus} label="YVEX" /> YVEX
            </span>
            <span>
              <StatusDot status={providerStatus} label="Reference" /> Reference
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
          <span>Operator</span>
          <span aria-hidden="true">/</span>
          <strong>{page.label}</strong>
        </div>
        <div className="context-entities">
          <span>
            Target <strong>{app.selectedTarget ?? "not selected"}</strong>
          </span>
          <span>
            Jobs <strong>{activeJobs}</strong>
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
          aria-label="Toggle chat dock"
          aria-expanded={chatMode !== "closed"}
          onClick={() => setChatMode((value) => (value === "closed" ? "docked" : "closed"))}
        >
          <MessageSquare aria-hidden="true" size={16} />
          <span>Chat</span>
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
        onOpenChat={() => setChatMode("docked")}
        onNewChat={() => {
          setChatMode("docked");
          setNewSessionSignal((value) => value + 1);
        }}
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
      <ChatDock mode={chatMode} setMode={setChatMode} newSessionSignal={newSessionSignal} />
    </div>
  );
}
