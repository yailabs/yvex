/*
 * Owner: apps/operator lifecycle-centered information architecture.
 * Owns: canonical workbench routes, labels, summaries, grouped navigation, and legacy route mapping.
 * Does not own: route rendering, capability status, fetching, layout, or native claims.
 * Invariants: build stages are one process, YVEX is the execution authority, and every retained surface has one canonical route.
 * Boundary: navigation presence and route selection never imply downstream readiness.
 */
export const routeIds = [
  "workspace",
  "build",
  "artifacts",
  "runtime",
  "evidence",
  "environment",
  "settings",
] as const;
export type RouteId = (typeof routeIds)[number];

export interface PageMetadata {
  id: RouteId;
  label: string;
  path: string;
  group: "Workspace" | "System";
  eyebrow: string;
  summary: string;
}

export const pageMetadata: Readonly<Record<RouteId, PageMetadata>> = {
  workspace: {
    id: "workspace",
    label: "Workspace",
    path: "/workspace",
    group: "Workspace",
    eyebrow: "YVEX / Active target",
    summary: "One target, one transformation chain, and one authoritative execution context.",
  },
  build: {
    id: "build",
    label: "Build",
    path: "/build",
    group: "Workspace",
    eyebrow: "Build / Transformation chain",
    summary:
      "Source, logical model, Transformation IR, lowering, quantization, and GGUF writer as inspectable stages.",
  },
  artifacts: {
    id: "artifacts",
    label: "Artifacts",
    path: "/artifacts",
    group: "Workspace",
    eyebrow: "Artifacts / Inventory",
    summary: "Proof, complete, and supported artifacts with admission boundaries and provenance.",
  },
  runtime: {
    id: "runtime",
    label: "Runtime",
    path: "/runtime",
    group: "Workspace",
    eyebrow: "YVEX / Runtime workbench",
    summary: "Backend binding, sessions, generation stages, active operations, and exact blockers.",
  },
  evidence: {
    id: "evidence",
    label: "Evidence",
    path: "/evidence",
    group: "Workspace",
    eyebrow: "Evidence / Producer trace",
    summary:
      "Allowlisted producers, runs, cache state, command provenance, and structured refusals.",
  },
  environment: {
    id: "environment",
    label: "Environment",
    path: "/environment",
    group: "System",
    eyebrow: "Environment / Topology",
    summary:
      "Browser, adapter, active YVEX binary, backend evidence, exposure, and host boundaries.",
  },
  settings: {
    id: "settings",
    label: "Settings",
    path: "/settings",
    group: "System",
    eyebrow: "System / Configuration",
    summary:
      "Trusted YVEX configuration, safety, cache, interface, and optional comparison diagnostics.",
  },
};

export const primaryNavigation = routeIds
  .map((id) => pageMetadata[id])
  .filter((item) => item.group === "Workspace");
export const systemNavigation = routeIds
  .map((id) => pageMetadata[id])
  .filter((item) => item.group === "System");

const legacyRouteMap: Readonly<Record<string, RouteId>> = {
  overview: "workspace",
  models: "workspace",
  sources: "build",
  compilation: "build",
  quantization: "build",
  "system-health": "environment",
};

/** Resolves canonical and legacy top-level routes without changing the browser URL itself. */
export function routeFromPath(pathname: string): RouteId {
  const segment = pathname.split("/").filter(Boolean)[0] ?? "workspace";
  if (routeIds.includes(segment as RouteId)) return segment as RouteId;
  return legacyRouteMap[segment] ?? "workspace";
}
