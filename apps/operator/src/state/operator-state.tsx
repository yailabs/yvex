/*
 * Owner: apps/operator coherent browser application state.
 * Owns: bootstrap server resources, authoritative workspace projection, comparison presentation state, and global invalidation.
 * Does not own: provider secrets, server capabilities, job mutation, page projections, URL tab selection, or chat streaming buffers.
 * Invariants: target, artifact, backend, build, and runtime selection come only from the server workspace authority.
 * Boundary: browser-local comparison presentation never makes a YVEX capability available.
 */
import { createContext, useCallback, useContext, useMemo, useState, type ReactNode } from "react";

import type {
  CapabilityManifest,
  Job,
  OperatorWorkspace,
  OperatorEvent,
  ProducerRun,
  SettingsResponse,
  SystemHealth,
} from "../../shared/contracts.ts";
import { operatorApi } from "../api.ts";
import { useApiResource, type ResourceState } from "../resource.ts";

interface OperatorState {
  health: ResourceState<SystemHealth>;
  workspace: ResourceState<OperatorWorkspace>;
  capabilities: ResourceState<CapabilityManifest>;
  settings: ResourceState<SettingsResponse>;
  jobs: ResourceState<{ jobs: Job[] }>;
  runs: ResourceState<{ runs: ProducerRun[] }>;
  events: ResourceState<{ events: OperatorEvent[] }>;
  refreshAll: () => void;
}

const OperatorStateContext = createContext<OperatorState | null>(null);

/** Provides validated server resources and authoritative workspace state to every route and global surface. */
export function OperatorStateProvider({ children }: { children: ReactNode }) {
  const [revision, setRevision] = useState(0);
  const key = String(revision);
  const health = useApiResource(`health:${key}`, operatorApi.health);
  const workspace = useApiResource(`workspace:${key}`, operatorApi.workspace);
  const capabilities = useApiResource(`capabilities:${key}`, operatorApi.capabilities);
  const settings = useApiResource(`settings:${key}`, operatorApi.settings);
  const jobs = useApiResource(`jobs:${key}`, operatorApi.jobs);
  const runs = useApiResource(`runs:${key}`, operatorApi.producerRuns);
  const events = useApiResource(`events:${key}`, operatorApi.events);
  const refreshAll = useCallback(() => setRevision((value) => value + 1), []);

  const state = useMemo<OperatorState>(
    () => ({
      health,
      workspace,
      capabilities,
      settings,
      jobs,
      runs,
      events,
      refreshAll,
    }),
    [health, workspace, capabilities, settings, jobs, runs, events, refreshAll],
  );
  return <OperatorStateContext.Provider value={state}>{children}</OperatorStateContext.Provider>;
}

/** Returns the single Operator state owner and refuses use outside its provider. */
export function useOperatorState(): OperatorState {
  const state = useContext(OperatorStateContext);
  if (!state) throw new Error("useOperatorState requires OperatorStateProvider");
  return state;
}
