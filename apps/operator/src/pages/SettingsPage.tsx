/*
 * Owner: apps/operator functional Settings workbench.
 * Owns: validated YVEX, interface, cache, safety, and optional external-comparison forms with redacted values.
 * Does not own: secret return, arbitrary paths/URLs, listener restart, shell/environment mutation, YVEX capability, or primary generation.
 * Invariants: comparison keys remain write-only, YVEX paths require server identity validation, and comparison is disabled by default.
 * Boundary: saved settings do not establish YVEX or comparison readiness until their respective typed probes pass.
 */
import { Check, EyeOff, LockKeyhole, RefreshCw, Save, ShieldCheck, Trash2 } from "lucide-react";
import { useState, type FormEvent } from "react";
import { Link } from "react-router-dom";

import type { ComparisonEndpointSettings, SettingsResponse } from "../../shared/contracts.ts";
import { operatorApi } from "../api.ts";
import {
  Fact,
  FactGrid,
  PageHeader,
  Panel,
  ResourceBoundary,
  RouteTabs,
  useRouteTab,
} from "../components/Primitives.tsx";
import { StatusBadge } from "../components/Status.tsx";
import { pageMetadata } from "../navigation.ts";
import { useOperatorState } from "../state/operator-state.tsx";

const sections = [
  { id: "connection", label: "Connection" },
  { id: "yvex", label: "YVEX" },
  { id: "cache", label: "Cache" },
  { id: "safety", label: "Safety" },
  { id: "interface", label: "Interface" },
  { id: "comparison-endpoint", label: "Comparison endpoint" },
] as const;

/** Renders one local save/test result without exposing stack traces or secret detail. */
function FormNotice({ state }: { state: { type: "success" | "error"; message: string } | null }) {
  if (!state) return null;
  return (
    <div className={`form-notice ${state.type}`} role={state.type === "error" ? "alert" : "status"}>
      {state.type === "success" ? <Check aria-hidden="true" size={15} /> : null}
      {state.message}
    </div>
  );
}

/** Persists one binary candidate only after regular-file, execute-bit, identity, and protocol validation. */
function YvexForm({ settings, onSaved }: { settings: SettingsResponse; onSaved: () => void }) {
  const [path, setPath] = useState("");
  const [saving, setSaving] = useState(false);
  const [notice, setNotice] = useState<{ type: "success" | "error"; message: string } | null>(null);

  /** Validates and persists the candidate through the fixed server-side identity probe. */
  const save = async (event: FormEvent): Promise<void> => {
    event.preventDefault();
    setSaving(true);
    setNotice(null);
    try {
      await operatorApi.updateYvex({ binaryPath: path });
      setPath("");
      setNotice({
        type: "success",
        message: "Compatible YVEX binary saved; observations invalidated.",
      });
      onSaved();
    } catch (error) {
      setNotice({
        type: "error",
        message: error instanceof Error ? error.message : "Binary validation failed.",
      });
    } finally {
      setSaving(false);
    }
  };

  /** Clears only the explicit override so deterministic fallback candidates can be re-evaluated. */
  const clear = async (): Promise<void> => {
    setSaving(true);
    try {
      await operatorApi.updateYvex({ binaryPath: null });
      setNotice({
        type: "success",
        message: "Explicit override cleared; auto-discovery will be retried.",
      });
      onSaved();
    } catch (error) {
      setNotice({
        type: "error",
        message: error instanceof Error ? error.message : "Could not clear the binary override.",
      });
    } finally {
      setSaving(false);
    }
  };

  return (
    <form className="settings-form" onSubmit={(event) => void save(event)}>
      <div className="field-row">
        <label>
          <span>Trusted absolute binary path</span>
          <input
            value={path}
            onChange={(event) => setPath(event.target.value)}
            placeholder="/absolute/path/to/yvex"
            autoComplete="off"
            required
          />
        </label>
        <div className="field-help">
          Relative paths and traversal are rejected. No browser value can become argv.
        </div>
      </div>
      <FactGrid>
        <Fact
          label="Explicit binary override"
          value={settings.yvex.binaryPathLabel ?? "Not configured"}
          mono
        />
        <Fact
          label="Environment candidate"
          value={settings.yvex.environmentCandidateConfigured ? "configured" : "not configured"}
        />
      </FactGrid>
      <FormNotice state={notice} />
      <div className="form-actions">
        <button type="submit" className="button primary" disabled={saving || !path.trim()}>
          <Save aria-hidden="true" size={14} /> {saving ? "Validating…" : "Validate and save"}
        </button>
        <button
          type="button"
          className="button secondary"
          disabled={saving || !settings.yvex.binaryConfigured}
          onClick={() => void clear()}
        >
          <Trash2 aria-hidden="true" size={14} /> Clear override
        </button>
        <button
          type="button"
          className="button secondary"
          onClick={() => void operatorApi.reload().then(onSaved)}
        >
          <RefreshCw aria-hidden="true" size={14} /> Retry resolution
        </button>
      </div>
    </form>
  );
}

/** Persists an explicitly optional OpenAI-compatible comparison endpoint with a write-only key. */
function ComparisonEndpointForm({
  endpoint,
  onSaved,
}: {
  endpoint: ComparisonEndpointSettings;
  onSaved: () => void;
}) {
  const [enabled, setEnabled] = useState(endpoint.enabled);
  const [displayName, setDisplayName] = useState(endpoint.displayName);
  const [baseUrl, setBaseUrl] = useState(endpoint.baseUrl);
  const [apiKey, setApiKey] = useState("");
  const [defaultModel, setDefaultModel] = useState(endpoint.defaultModel);
  const [timeout, setTimeoutValue] = useState(endpoint.requestTimeoutMs);
  const [busy, setBusy] = useState<"save" | "test" | null>(null);
  const [notice, setNotice] = useState<{ type: "success" | "error"; message: string } | null>(null);

  /** Saves only the admitted comparison fields and never round-trips the secret. */
  const save = async (event?: FormEvent): Promise<boolean> => {
    event?.preventDefault();
    setBusy("save");
    setNotice(null);
    try {
      await operatorApi.updateComparisonEndpoint({
        enabled,
        displayName,
        baseUrl,
        ...(apiKey ? { apiKey } : {}),
        defaultModel,
        requestTimeoutMs: timeout,
      });
      setApiKey("");
      setNotice({
        type: "success",
        message: "External comparison configuration saved. The key remains server-side.",
      });
      onSaved();
      return true;
    } catch (error) {
      setNotice({
        type: "error",
        message:
          error instanceof Error ? error.message : "Comparison configuration failed validation.",
      });
      return false;
    } finally {
      setBusy(null);
    }
  };

  /** Runs model discovery and a bounded streaming compatibility probe only after explicit enablement. */
  const test = async (): Promise<void> => {
    if (!(await save())) return;
    setBusy("test");
    try {
      const result = await operatorApi.testComparisonEndpoint();
      setNotice({
        type: "success",
        message: `${result.status.displayName} passed comparison model and streaming checks.`,
      });
      onSaved();
    } catch (error) {
      setNotice({
        type: "error",
        message: error instanceof Error ? error.message : "Comparison endpoint test failed.",
      });
    } finally {
      setBusy(null);
    }
  };

  return (
    <form className="settings-form provider-form" onSubmit={(event) => void save(event)}>
      <div className="comparison-boundary">
        <strong>Diagnostics only</strong>
        <p>
          This endpoint may support differential comparison. It is never a YVEX runtime lane,
          fallback, or global readiness dependency.
        </p>
      </div>
      <label className="toggle-field">
        <input
          type="checkbox"
          checked={enabled}
          onChange={(event) => setEnabled(event.target.checked)}
        />
        <span>
          <strong>Enable external comparison endpoint</strong>
          <small>Disabled is the normal default and does not block YVEX.</small>
        </span>
      </label>
      <div className="form-grid">
        <label>
          <span>Display name</span>
          <input
            value={displayName}
            onChange={(event) => setDisplayName(event.target.value)}
            required
          />
        </label>
        <label>
          <span>OpenAI-compatible base URL</span>
          <input
            type="url"
            value={baseUrl}
            onChange={(event) => setBaseUrl(event.target.value)}
            placeholder="http://127.0.0.1:8080/v1"
            required
          />
        </label>
        <label>
          <span>API key</span>
          <input
            type="password"
            value={apiKey}
            onChange={(event) => setApiKey(event.target.value)}
            placeholder={
              endpoint.apiKeyConfigured
                ? "Configured — enter to replace"
                : "Optional for a local endpoint"
            }
            autoComplete="new-password"
          />
          <small>
            <EyeOff aria-hidden="true" size={13} /> Write-only; never returned to this browser.
          </small>
        </label>
        <label>
          <span>Comparison model</span>
          <input
            value={defaultModel}
            onChange={(event) => setDefaultModel(event.target.value)}
            required={enabled}
          />
        </label>
        <label>
          <span>Request timeout (ms)</span>
          <input
            type="number"
            min={1000}
            max={300000}
            value={timeout}
            onChange={(event) => setTimeoutValue(Number(event.target.value))}
            required
          />
        </label>
      </div>
      <FormNotice state={notice} />
      <div className="form-actions">
        <button type="submit" className="button secondary" disabled={busy !== null}>
          <Save aria-hidden="true" size={14} />{" "}
          {busy === "save" ? "Saving…" : "Save comparison configuration"}
        </button>
        <button
          type="button"
          className="button secondary"
          disabled={busy !== null || !enabled || !defaultModel}
          onClick={() => void test()}
        >
          <RefreshCw aria-hidden="true" size={14} />{" "}
          {busy === "test" ? "Testing stream…" : "Save and test comparison"}
        </button>
        {endpoint.enabled ? (
          <Link className="button secondary" to="/settings/reference-comparison">
            Open reference comparison
          </Link>
        ) : null}
      </div>
    </form>
  );
}

/** Persists only the default Generation Console presentation; no execution lane exists. */
function InterfaceForm({ settings, onSaved }: { settings: SettingsResponse; onSaved: () => void }) {
  const [mode, setMode] = useState(settings.interface.generationConsoleDefaultMode);
  const [notice, setNotice] = useState<{ type: "success" | "error"; message: string } | null>(null);

  /** Saves the server-backed console presentation default without mutating workspace truth. */
  const save = async (event: FormEvent): Promise<void> => {
    event.preventDefault();
    try {
      await operatorApi.updateOperator({ generationConsoleDefaultMode: mode });
      setNotice({ type: "success", message: "Generation Console default saved." });
      onSaved();
    } catch (error) {
      setNotice({
        type: "error",
        message: error instanceof Error ? error.message : "Interface settings failed.",
      });
    }
  };

  return (
    <form className="settings-form" onSubmit={(event) => void save(event)}>
      <label>
        <span>Default Generation Console mode</span>
        <select value={mode} onChange={(event) => setMode(event.target.value as typeof mode)}>
          <option value="closed">Closed</option>
          <option value="compact">Compact</option>
          <option value="docked">Docked</option>
          <option value="expanded">Expanded</option>
          <option value="fullscreen">Full screen</option>
        </select>
        <small>
          The console always inherits the active YVEX target, artifact, backend, and runtime
          session.
        </small>
      </label>
      <FormNotice state={notice} />
      <button type="submit" className="button primary">
        <Save aria-hidden="true" size={14} /> Save interface default
      </button>
    </form>
  );
}

/** Renders validated configuration panels and refreshes server truth after every mutation. */
export function SettingsPage() {
  const app = useOperatorState();
  const section = useRouteTab(sections, "connection", "section");
  const page = pageMetadata.settings;
  const refresh = (): void => app.refreshAll();

  return (
    <div className="page">
      <PageHeader eyebrow={page.eyebrow} title={page.label} summary={page.summary} />
      <RouteTabs
        tabs={sections}
        defaultTab="connection"
        parameter="section"
        label="Settings sections"
      />
      <ResourceBoundary resource={app.settings}>
        {(settings) => (
          <>
            {section === "connection" ? (
              <div role="tabpanel" className="detail-layout">
                <Panel
                  title="Adapter connection"
                  description="Listener state is startup-owned and cannot silently switch exposure."
                >
                  <FactGrid>
                    <Fact label="Bind address" value={settings.operator.bindAddress} mono />
                    <Fact label="Bind mode" value={settings.operator.bindMode} />
                    <Fact
                      label="Remote enabled"
                      value={settings.operator.remoteEnabled ? "yes" : "no"}
                    />
                    <Fact
                      label="Authentication required"
                      value={settings.operator.authenticationRequired ? "yes" : "no"}
                    />
                  </FactGrid>
                  {settings.operator.remoteEnabled ? (
                    <div className="security-warning">
                      Remote mode is explicit and secured by bearer token plus Origin allowlist.
                    </div>
                  ) : (
                    <div className="security-ok">
                      <ShieldCheck aria-hidden="true" size={17} /> Loopback-only default is active.
                    </div>
                  )}
                </Panel>
                <Panel title="Remote operation" description="Restart-only security boundary.">
                  <p className="body-copy">
                    Remote binding is configured outside the browser with explicit mode, a strong
                    authentication token, and allowed Origins. Tokens are never returned or logged.
                  </p>
                </Panel>
              </div>
            ) : null}
            {section === "yvex" ? (
              <div role="tabpanel">
                <Panel
                  title="YVEX binary"
                  description="Configure one trusted executable; the adapter owns every subsequent argv."
                >
                  <YvexForm
                    key={`${settings.observedAt}:${settings.yvex.binaryPathLabel}`}
                    settings={settings}
                    onSaved={refresh}
                  />
                </Panel>
              </div>
            ) : null}
            {section === "cache" ? (
              <div role="tabpanel" className="detail-layout">
                <Panel
                  title="Cache policy"
                  description="Bounded observations with explicit invalidation."
                >
                  <FactGrid>
                    <Fact label="Mutable TTL" value={`${settings.cache.mutableTtlMs} ms`} />
                    <Fact label="Binary TTL" value={`${settings.cache.binaryTtlMs} ms`} />
                    <Fact label="Failures" value="Never success-cached" />
                    <Fact label="Comparison success" value="Invalidated after settings change" />
                  </FactGrid>
                  <button
                    type="button"
                    className="button secondary"
                    onClick={() => void operatorApi.clearCache().then(refresh)}
                  >
                    <Trash2 aria-hidden="true" size={14} /> Clear safe Operator caches
                  </button>
                </Panel>
                <Panel title="Persistence boundary" description="What the reset does not touch.">
                  <ul className="plain-list">
                    <li>Does not delete workspace selection.</li>
                    <li>Does not delete model or artifact files.</li>
                    <li>Does not mutate YVEX registries.</li>
                    <li>Does not clear comparison credentials.</li>
                  </ul>
                </Panel>
              </div>
            ) : null}
            {section === "safety" ? (
              <div role="tabpanel">
                <Panel
                  title="Mutation and execution safety"
                  description="Hard server controls, not presentation promises."
                >
                  <div className="safety-grid">
                    {[
                      ["Shell execution", settings.safety.shellEnabled],
                      ["Arbitrary argv", settings.safety.arbitraryArgvEnabled],
                      ["Comparison secrets returned", settings.safety.providerSecretsReturned],
                      ["Remote comparison URLs", settings.safety.remoteProvidersAllowed],
                    ].map(([label, enabled]) => (
                      <article key={String(label)}>
                        <LockKeyhole aria-hidden="true" size={17} />
                        <div>
                          <strong>{label}</strong>
                          <span>{enabled ? "enabled by explicit startup policy" : "disabled"}</span>
                        </div>
                        <StatusBadge status={enabled ? "degraded" : "ready"} />
                      </article>
                    ))}
                  </div>
                  <p className="boundary-copy">
                    YVEX paths are the only browser-configurable executable paths. Model paths,
                    environment maps, commands, pipes, and redirection are never accepted.
                  </p>
                </Panel>
              </div>
            ) : null}
            {section === "interface" ? (
              <div role="tabpanel">
                <Panel
                  title="Interface defaults"
                  description="Presentation only; the active workspace remains server-owned."
                >
                  <InterfaceForm settings={settings} onSaved={refresh} />
                </Panel>
              </div>
            ) : null}
            {section === "comparison-endpoint" ? (
              <div role="tabpanel">
                <Panel
                  title="External comparison endpoint"
                  description="Optional differential diagnostics for an explicitly configured OpenAI-compatible endpoint."
                >
                  <ComparisonEndpointForm
                    endpoint={settings.comparisonEndpoint}
                    onSaved={refresh}
                  />
                </Panel>
              </div>
            ) : null}
          </>
        )}
      </ResourceBoundary>
    </div>
  );
}
