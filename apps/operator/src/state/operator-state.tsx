/*
 * Owner: apps/operator coherent browser application state.
 * Owns: bootstrap server resources, durable non-secret selections, active session identity, and global invalidation.
 * Does not own: provider secrets, server capabilities, job mutation, page projections, URL tab selection, or chat streaming buffers.
 * Invariants: server truth remains in validated resource responses and only presentation/domain selections use local storage.
 * Boundary: persisted selection never makes the selected target, artifact, backend, provider, or lane available.
 */
import {
  createContext,
  useCallback,
  useContext,
  useMemo,
  useState,
  type Dispatch,
  type ReactNode,
  type SetStateAction,
} from "react";

import type {
  CapabilityManifest,
  ChatLane,
  Job,
  OperatorEvent,
  ProducerRun,
  ProviderStatus,
  SettingsResponse,
  SystemHealth,
} from "../../shared/contracts.ts";
import { operatorApi } from "../api.ts";
import { useApiResource, type ResourceState } from "../resource.ts";

interface OperatorState {
  health: ResourceState<SystemHealth>;
  capabilities: ResourceState<CapabilityManifest>;
  settings: ResourceState<SettingsResponse>;
  provider: ResourceState<ProviderStatus>;
  jobs: ResourceState<{ jobs: Job[] }>;
  runs: ResourceState<{ runs: ProducerRun[] }>;
  events: ResourceState<{ events: OperatorEvent[] }>;
  selectedTarget: string | null;
  setSelectedTarget: Dispatch<SetStateAction<string | null>>;
  selectedArtifact: string | null;
  setSelectedArtifact: Dispatch<SetStateAction<string | null>>;
  selectedBackend: string | null;
  setSelectedBackend: Dispatch<SetStateAction<string | null>>;
  selectedLane: ChatLane;
  setSelectedLane: Dispatch<SetStateAction<ChatLane>>;
  selectedProviderModel: string;
  setSelectedProviderModel: Dispatch<SetStateAction<string>>;
  activeSessionId: string | null;
  setActiveSessionId: Dispatch<SetStateAction<string | null>>;
  refreshAll: () => void;
}

const OperatorStateContext = createContext<OperatorState | null>(null);

/** Reads one non-secret local preference and validates it through a caller-owned parser. */
function storedValue<T>(key: string, fallback: T, parse: (value: string) => T | null): T {
  try {
    const value = window.localStorage.getItem(key);
    return value === null ? fallback : (parse(value) ?? fallback);
  } catch {
    return fallback;
  }
}

/** Couples one local non-secret preference with deterministic storage updates. */
function useStoredState<T>(
  key: string,
  fallback: T,
  parse: (value: string) => T | null,
): [T, Dispatch<SetStateAction<T>>] {
  const [value, setValue] = useState<T>(() => storedValue(key, fallback, parse));
  const setStored: Dispatch<SetStateAction<T>> = useCallback(
    (next) => {
      setValue((current) => {
        const resolved = typeof next === "function" ? (next as (value: T) => T)(current) : next;
        try {
          if (resolved === null || resolved === "") window.localStorage.removeItem(key);
          else window.localStorage.setItem(key, String(resolved));
        } catch {
          // Private browsing storage refusal leaves the in-memory preference usable.
        }
        return resolved;
      });
    },
    [key],
  );
  return [value, setStored];
}

/** Provides validated server resources and non-secret durable selection state to every route and global surface. */
export function OperatorStateProvider({ children }: { children: ReactNode }) {
  const [revision, setRevision] = useState(0);
  const key = String(revision);
  const health = useApiResource(`health:${key}`, operatorApi.health);
  const capabilities = useApiResource(`capabilities:${key}`, operatorApi.capabilities);
  const settings = useApiResource(`settings:${key}`, operatorApi.settings);
  const provider = useApiResource(`provider:${key}`, operatorApi.providerStatus);
  const jobs = useApiResource(`jobs:${key}`, operatorApi.jobs);
  const runs = useApiResource(`runs:${key}`, operatorApi.producerRuns);
  const events = useApiResource(`events:${key}`, operatorApi.events);
  const [selectedTarget, setSelectedTarget] = useStoredState<string | null>(
    "yvex.operator.target",
    null,
    (value) => value || null,
  );
  const [selectedArtifact, setSelectedArtifact] = useStoredState<string | null>(
    "yvex.operator.artifact",
    null,
    (value) => value || null,
  );
  const [selectedBackend, setSelectedBackend] = useStoredState<string | null>(
    "yvex.operator.backend",
    null,
    (value) => value || null,
  );
  const [selectedLane, setSelectedLane] = useStoredState<ChatLane>(
    "yvex.operator.lane",
    "reference-provider",
    (value) => (value === "native-yvex" || value === "reference-provider" ? value : null),
  );
  const [selectedProviderModel, setSelectedProviderModel] = useStoredState(
    "yvex.operator.provider-model",
    "",
    (value) => value,
  );
  const [activeSessionId, setActiveSessionId] = useStoredState<string | null>(
    "yvex.operator.session",
    null,
    (value) => value || null,
  );
  const refreshAll = useCallback(() => setRevision((value) => value + 1), []);

  const state = useMemo<OperatorState>(
    () => ({
      health,
      capabilities,
      settings,
      provider,
      jobs,
      runs,
      events,
      selectedTarget,
      setSelectedTarget,
      selectedArtifact,
      setSelectedArtifact,
      selectedBackend,
      setSelectedBackend,
      selectedLane,
      setSelectedLane,
      selectedProviderModel,
      setSelectedProviderModel,
      activeSessionId,
      setActiveSessionId,
      refreshAll,
    }),
    [
      health,
      capabilities,
      settings,
      provider,
      jobs,
      runs,
      events,
      selectedTarget,
      setSelectedTarget,
      selectedArtifact,
      setSelectedArtifact,
      selectedBackend,
      setSelectedBackend,
      selectedLane,
      setSelectedLane,
      selectedProviderModel,
      setSelectedProviderModel,
      activeSessionId,
      setActiveSessionId,
      refreshAll,
    ],
  );
  return <OperatorStateContext.Provider value={state}>{children}</OperatorStateContext.Provider>;
}

/** Returns the single Operator state owner and refuses use outside its provider. */
export function useOperatorState(): OperatorState {
  const state = useContext(OperatorStateContext);
  if (!state) throw new Error("useOperatorState requires OperatorStateProvider");
  return state;
}
