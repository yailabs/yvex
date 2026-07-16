/*
 * Owner: apps/operator backend-for-frontend service assembly.
 * Owns: dependency composition for settings, resolver, producer adapter, workspace, jobs, events, external comparison, and capabilities.
 * Does not own: HTTP routing, listener lifecycle, source facts, browser state, or native YVEX implementation.
 * Invariants: production and tests use the same service graph with only explicit transport/process doubles.
 * Boundary: assembling services does not make any downstream capability ready.
 */
import type { OperatorConfig } from "./config.ts";
import { OperatorAdapter } from "./adapter.ts";
import { CapabilityService } from "./capabilities.ts";
import { ChatService } from "./chat.ts";
import { EventHistory } from "./events.ts";
import { JobManager } from "./jobs.ts";
import { ReferenceProviderService } from "./provider.ts";
import { BinaryResolver } from "./resolver.ts";
import type { RunRequest, RunResult } from "./runner.ts";
import { OperatorSettingsStore } from "./settings.ts";
import { WorkspaceService } from "./workspace.ts";

export interface OperatorServices {
  config: OperatorConfig;
  settings: OperatorSettingsStore;
  resolver: BinaryResolver;
  adapter: OperatorAdapter;
  capabilities: CapabilityService;
  workspace: WorkspaceService;
  events: EventHistory;
  jobs: JobManager;
  provider: ReferenceProviderService;
  comparison: ChatService;
}

export interface ServiceDependencies {
  runner?: (request: RunRequest) => Promise<RunResult>;
  fetcher?: typeof fetch;
  clock?: () => number;
}

/** Creates one coherent local control-plane graph with bounded shared state owners. */
export function createOperatorServices(
  config: OperatorConfig,
  dependencies: ServiceDependencies = {},
): OperatorServices {
  const clock = dependencies.clock ?? Date.now;
  const events = new EventHistory(config.eventRetention, clock);
  const jobs = new JobManager(Math.max(20, Math.min(config.eventRetention, 500)), events, clock);
  const settings = new OperatorSettingsStore(config, clock);
  const resolver = new BinaryResolver(config, settings, events, {
    ...(dependencies.runner ? { runner: dependencies.runner } : {}),
    clock,
  });
  const adapter = new OperatorAdapter(config, resolver, jobs, events, {
    ...(dependencies.runner ? { runner: dependencies.runner } : {}),
    clock,
  });
  const provider = new ReferenceProviderService(
    config,
    settings,
    jobs,
    events,
    dependencies.fetcher ?? fetch,
    clock,
  );
  const capabilities = new CapabilityService(resolver, adapter, clock);
  const workspace = new WorkspaceService(config, adapter, capabilities, jobs, clock);
  const comparison = new ChatService(config, settings, provider, jobs, events, clock);
  return {
    config,
    settings,
    resolver,
    adapter,
    capabilities,
    workspace,
    events,
    jobs,
    provider,
    comparison,
  };
}
