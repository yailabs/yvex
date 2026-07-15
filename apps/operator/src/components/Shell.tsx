/*
 * Owner: apps/operator responsive navigation shell.
 * Owns: fixed desktop rail, mobile drawer, context bar, safety state, main landmark, and producer action placement.
 * Does not own: page facts, command execution, route data, capability status, or YVEX navigation labels.
 * Invariants: selection follows the direct URL and all navigation remains keyboard reachable.
 * Boundary: the persistent read-only state is an operator control, not a YVEX readiness claim.
 */
import {
  Activity,
  Archive,
  Boxes,
  Braces,
  Command,
  Cpu,
  Database,
  FileCheck2,
  Gauge,
  Menu,
  Microscope,
  Settings,
  ShieldCheck,
  SlidersHorizontal,
  X,
  type LucideIcon,
} from "lucide-react";
import { useEffect, useRef, useState } from "react";
import { NavLink, Outlet, useLocation } from "react-router-dom";

import type { ViewId } from "../../shared/contracts.ts";
import { pageMetadata, primaryNavigation, systemNavigation, viewFromPath } from "../navigation.ts";
import { OperatorViewProvider, useOperatorView } from "../view-context.tsx";
import { CommandDrawer } from "./CommandDrawer.tsx";

const navIcons: Readonly<Record<ViewId, LucideIcon>> = {
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

/** Renders one borrowed navigation group; only the supplied close callback mutates shell state. */
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
            <Icon aria-hidden="true" size={16} strokeWidth={1.7} />
            <span>{item.label}</span>
          </NavLink>
        );
      })}
    </div>
  );
}

/** Owns transient drawer state and focus listeners; it performs no data or process IO. */
function ShellFrame({ view }: { view: ViewId }) {
  const { response, loading, error } = useOperatorView();
  const [mobileOpen, setMobileOpen] = useState(false);
  const [commandOpen, setCommandOpen] = useState(false);
  const mobileTrigger = useRef<HTMLButtonElement>(null);
  const commandTrigger = useRef<HTMLButtonElement>(null);
  const mobileClose = useRef<HTMLButtonElement>(null);
  const page = pageMetadata[view];

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
        setCommandOpen(true);
      }
    };
    document.addEventListener("keydown", shortcut);
    return () => document.removeEventListener("keydown", shortcut);
  }, []);

  const connectionLabel = error
    ? "Adapter offline"
    : loading
      ? "Checking adapter"
      : response
        ? "Adapter connected"
        : "Adapter unavailable";

  return (
    <div className="operator-shell">
      <a className="skip-link" href="#main-content">
        Skip to operator content
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
        aria-label="YVEX operator navigation"
      >
        <div className="brand-block">
          <div className="brand-mark" aria-hidden="true">
            <span>Y</span>
          </div>
          <div>
            <strong>YVEX</strong>
            <span>local operator</span>
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
            onNavigate={() => {
              setMobileOpen(false);
              setCommandOpen(false);
            }}
          />
          <NavigationGroup
            label="System"
            items={systemNavigation}
            onNavigate={() => {
              setMobileOpen(false);
              setCommandOpen(false);
            }}
          />
        </nav>
        <div className="sidebar-footer">
          <div className="safety-state">
            <ShieldCheck aria-hidden="true" size={17} />
            <div>
              <strong>Read-only enforced</strong>
              <span>No execution control</span>
            </div>
          </div>
          <div className="sidebar-version">
            <span>ADAPTER</span>
            <code>{response?.adapterVersion ?? "0.1.0"}</code>
          </div>
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
          <Menu aria-hidden="true" size={18} />
          <span className="sr-only">Open navigation</span>
        </button>
        <div className="context-path">
          <span>YVEX</span>
          <span aria-hidden="true">/</span>
          <strong>{page.label}</strong>
        </div>
        <div className="context-status" data-testid="adapter-connectivity">
          <span
            className={`connection-dot${error ? " offline" : loading ? " pending" : ""}`}
            aria-hidden="true"
          />
          <span>{connectionLabel}</span>
          <span className="context-separator" aria-hidden="true" />
          <FileCheck2 aria-hidden="true" size={14} />
          <span>v0.1.0 target</span>
        </div>
      </header>

      <main id="main-content" className="workspace" tabIndex={-1}>
        <Outlet />
      </main>

      <button
        ref={commandTrigger}
        type="button"
        className="floating-command"
        aria-label="Producers"
        aria-haspopup="dialog"
        aria-expanded={commandOpen}
        onClick={() => setCommandOpen(true)}
      >
        <Command aria-hidden="true" size={17} />
        <span>Producers</span>
        <kbd>⌘ K</kbd>
      </button>
      <CommandDrawer
        open={commandOpen}
        onClose={() => setCommandOpen(false)}
        returnFocus={commandTrigger}
      />
    </div>
  );
}

/** Binds the canonical URL to a fresh route-scoped evidence provider and shared shell. */
export function OperatorShell() {
  const location = useLocation();
  const view = viewFromPath(location.pathname);
  return (
    <OperatorViewProvider key={view} view={view}>
      <ShellFrame view={view} />
    </OperatorViewProvider>
  );
}
