/*
 * Owner: apps/operator functional Settings workbench.
 * Owns: validated YVEX/provider/interface forms, redacted values, provider test, cache reset, connection and safety disclosure.
 * Does not own: secret return, arbitrary paths, arbitrary URLs, listener restart, shell/environment mutation, or native capability.
 * Invariants: API keys remain write-only and a YVEX path is persisted only after server-side identity validation.
 * Boundary: saved settings do not establish binary/provider readiness until their respective probes pass.
 */
import { Check, EyeOff, LockKeyhole, RefreshCw, Save, ShieldCheck, Trash2 } from "lucide-react";
import { useState, type FormEvent } from "react";

import type { ReferenceProviderSettings, SettingsResponse } from "../../shared/contracts.ts";
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
  { id: "reference-provider", label: "Reference provider" },
  { id: "cache", label: "Cache" },
  { id: "safety", label: "Safety" },
  { id: "interface", label: "Interface" },
] as const;

/** Renders one local save/test result without exposing raw stack or secret detail. */
function FormNotice({ state }: { state: { type: "success" | "error"; message: string } | null }) {
  if (!state) return null;
  return (
    <div className={`form-notice ${state.type}`} role={state.type === "error" ? "alert" : "status"}>
      {state.type === "success" ? <Check aria-hidden="true" size={15} /> : null}
      {state.message}
    </div>
  );
}

/** Persists one prospective binary only after the adapter validates regular file, execute bit, identity, and protocol. */
function YvexForm({ settings, onSaved }: { settings: SettingsResponse; onSaved: () => void }) {
  const [path, setPath] = useState("");
  const [saving, setSaving] = useState(false);
  const [notice, setNotice] = useState<{ type: "success" | "error"; message: string } | null>(null);
  const save = async (event: FormEvent): Promise<void> => {
    event.preventDefault();
    setSaving(true);
    setNotice(null);
    try {
      await operatorApi.updateYvex({ binaryPath: path });
      setPath("");
      setNotice({
        type: "success",
        message: "Compatible YVEX binary saved and observations invalidated.",
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
  const clear = async (): Promise<void> => {
    setSaving(true);
    try {
      await operatorApi.updateYvex({ binaryPath: null });
      setNotice({
        type: "success",
        message: "Persisted binary path cleared; fallback candidates will be retried.",
      });
      onSaved();
    } catch (error) {
      setNotice({
        type: "error",
        message: error instanceof Error ? error.message : "Could not clear binary setting.",
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
          Relative paths and traversal are rejected. The adapter executes only the fixed identity
          probe before persistence.
        </div>
      </div>
      <FactGrid>
        <Fact
          label="Persisted candidate"
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
          <Trash2 aria-hidden="true" size={14} /> Clear persisted path
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

/** Persists only the admitted OpenAI-compatible provider fields and keeps the API key write-only. */
function ProviderForm({
  provider,
  onSaved,
}: {
  provider: ReferenceProviderSettings;
  onSaved: () => void;
}) {
  const [enabled, setEnabled] = useState(provider.enabled);
  const [displayName, setDisplayName] = useState(provider.displayName);
  const [baseUrl, setBaseUrl] = useState(provider.baseUrl);
  const [apiKey, setApiKey] = useState("");
  const [defaultModel, setDefaultModel] = useState(provider.defaultModel);
  const [timeout, setTimeoutValue] = useState(provider.requestTimeoutMs);
  const [busy, setBusy] = useState<"save" | "test" | null>(null);
  const [notice, setNotice] = useState<{ type: "success" | "error"; message: string } | null>(null);
  const save = async (event?: FormEvent): Promise<boolean> => {
    event?.preventDefault();
    setBusy("save");
    setNotice(null);
    try {
      await operatorApi.updateProvider({
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
        message: "Reference provider settings saved. Secret value remains server-side.",
      });
      onSaved();
      return true;
    } catch (error) {
      setNotice({
        type: "error",
        message: error instanceof Error ? error.message : "Provider settings failed validation.",
      });
      return false;
    } finally {
      setBusy(null);
    }
  };
  const test = async (): Promise<void> => {
    if (!(await save())) return;
    setBusy("test");
    try {
      const result = await operatorApi.testProvider();
      setNotice({
        type: "success",
        message: `${result.status.displayName} passed model and streaming compatibility checks.`,
      });
      onSaved();
    } catch (error) {
      setNotice({
        type: "error",
        message: error instanceof Error ? error.message : "Provider test failed.",
      });
    } finally {
      setBusy(null);
    }
  };
  return (
    <form className="settings-form provider-form" onSubmit={(event) => void save(event)}>
      <label className="toggle-field">
        <input
          type="checkbox"
          checked={enabled}
          onChange={(event) => setEnabled(event.target.checked)}
        />
        <span>
          <strong>Enable reference provider</strong>
          <small>Execution owner is always shown as Reference provider.</small>
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
          <span>Base URL</span>
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
              provider.apiKeyConfigured
                ? "Configured — enter to replace"
                : "Optional for local provider"
            }
            autoComplete="new-password"
          />
          <small>
            <EyeOff aria-hidden="true" size={13} /> Write-only; never returned to this browser.
          </small>
        </label>
        <label>
          <span>Default model</span>
          <input
            value={defaultModel}
            onChange={(event) => setDefaultModel(event.target.value)}
            required
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
        <button type="submit" className="button primary" disabled={busy !== null}>
          <Save aria-hidden="true" size={14} /> {busy === "save" ? "Saving…" : "Save provider"}
        </button>
        <button
          type="button"
          className="button secondary"
          disabled={busy !== null || !enabled || !defaultModel}
          onClick={() => void test()}
        >
          <RefreshCw aria-hidden="true" size={14} />{" "}
          {busy === "test" ? "Testing stream…" : "Save and test"}
        </button>
      </div>
    </form>
  );
}

/** Persists server-backed default lane and dock mode while sizing remains browser-local. */
function InterfaceForm({ settings, onSaved }: { settings: SettingsResponse; onSaved: () => void }) {
  const [lane, setLane] = useState(settings.interface.defaultLane);
  const [mode, setMode] = useState(settings.interface.chatDefaultMode);
  const [notice, setNotice] = useState<{ type: "success" | "error"; message: string } | null>(null);
  const save = async (event: FormEvent): Promise<void> => {
    event.preventDefault();
    try {
      await operatorApi.updateOperator({ defaultLane: lane, chatDefaultMode: mode });
      setNotice({ type: "success", message: "Interface defaults saved." });
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
      <div className="form-grid">
        <label>
          <span>Default execution lane</span>
          <select value={lane} onChange={(event) => setLane(event.target.value as typeof lane)}>
            <option value="reference-provider">Reference provider</option>
            <option value="native-yvex">Native YVEX (capability-gated)</option>
          </select>
        </label>
        <label>
          <span>Default chat mode</span>
          <select value={mode} onChange={(event) => setMode(event.target.value as typeof mode)}>
            <option value="closed">Closed</option>
            <option value="compact">Compact</option>
            <option value="docked">Docked</option>
            <option value="expanded">Expanded</option>
            <option value="fullscreen">Full screen</option>
          </select>
        </label>
      </div>
      <FormNotice state={notice} />
      <button type="submit" className="button primary">
        <Save aria-hidden="true" size={14} /> Save interface defaults
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
                    Tailscale/LAN guidance is documented in the Operator runbook.
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
            {section === "reference-provider" ? (
              <div role="tabpanel">
                <Panel
                  title="OpenAI-compatible reference provider"
                  description="llama.cpp server, compatible local servers, and explicitly allowed HTTPS providers."
                >
                  <ProviderForm provider={settings.referenceProvider} onSaved={refresh} />
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
                    <Fact label="Provider success" value="Invalidated after settings change" />
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
                    <li>Does not delete chat sessions.</li>
                    <li>Does not delete model or artifact files.</li>
                    <li>Does not mutate YVEX registries.</li>
                    <li>Does not clear provider credentials.</li>
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
                      ["Provider secrets returned", settings.safety.providerSecretsReturned],
                      ["Remote provider URLs", settings.safety.remoteProvidersAllowed],
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
                    YVEX paths are the only browser-configurable executable paths, validated through
                    the fixed identity handshake. Model paths, environment maps, commands, pipes,
                    and redirection are not accepted.
                  </p>
                </Panel>
              </div>
            ) : null}
            {section === "interface" ? (
              <div role="tabpanel">
                <Panel
                  title="Interface defaults"
                  description="Non-secret presentation and lane preferences."
                >
                  <InterfaceForm settings={settings} onSaved={refresh} />
                </Panel>
              </div>
            ) : null}
          </>
        )}
      </ResourceBoundary>
    </div>
  );
}
