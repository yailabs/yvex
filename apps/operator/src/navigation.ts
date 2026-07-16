/*
 * Owner: apps/operator information architecture.
 * Owns: canonical routes, labels, summaries, grouped navigation, and command-palette navigation metadata.
 * Does not own: route rendering, capability status, fetching, layout, or native claims.
 * Invariants: every retained workbench surface has one canonical direct route.
 * Boundary: navigation presence never implies downstream readiness.
 */
export const routeIds = [
  "overview",
  "models",
  "sources",
  "compilation",
  "quantization",
  "artifacts",
  "runtime",
  "evidence",
  "system-health",
  "settings",
] as const;
export type RouteId = (typeof routeIds)[number];

export interface PageMetadata {
  id: RouteId;
  label: string;
  path: string;
  group: "Engineering" | "System";
  eyebrow: string;
  summary: string;
}

export const pageMetadata: Readonly<Record<RouteId, PageMetadata>> = {
  overview: {
    id: "overview",
    label: "Overview",
    path: "/overview",
    group: "Engineering",
    eyebrow: "Operator / Control plane",
    summary: "System readiness, execution lanes, active work, and actionable blockers.",
  },
  models: {
    id: "models",
    label: "Models",
    path: "/models",
    group: "Engineering",
    eyebrow: "Targets / Catalog",
    summary: "Machine-reported targets, release identity, selection, and exact runtime boundaries.",
  },
  sources: {
    id: "sources",
    label: "Sources",
    path: "/sources",
    group: "Engineering",
    eyebrow: "Source / Trust",
    summary:
      "Identity, verification, and accounting as separate machine-readable evidence surfaces.",
  },
  compilation: {
    id: "compilation",
    label: "Compilation",
    path: "/compilation",
    group: "Engineering",
    eyebrow: "Compilation / Plan",
    summary:
      "Logical model, Transformation IR, coverage, and physical lowering without artifact claims.",
  },
  quantization: {
    id: "quantization",
    label: "Quantization",
    path: "/quantization",
    group: "Engineering",
    eyebrow: "Compilation / Qtype",
    summary: "Policy, role support, reference evidence, and exact execution blockers.",
  },
  artifacts: {
    id: "artifacts",
    label: "Artifacts",
    path: "/artifacts",
    group: "Engineering",
    eyebrow: "Artifacts / Inventory",
    summary: "One coherent proof, complete, and supported artifact inventory with provenance.",
  },
  runtime: {
    id: "runtime",
    label: "Runtime",
    path: "/runtime",
    group: "Engineering",
    eyebrow: "Execution / Native lane",
    summary: "Capability stages, backend evidence, and controls gated by real native endpoints.",
  },
  evidence: {
    id: "evidence",
    label: "Evidence",
    path: "/evidence",
    group: "Engineering",
    eyebrow: "Operator / Producers",
    summary:
      "Allowlisted producers, runs, cache policy, command provenance, and structured refusals.",
  },
  "system-health": {
    id: "system-health",
    label: "System health",
    path: "/system-health",
    group: "System",
    eyebrow: "System / Topology",
    summary: "Browser, adapter, YVEX, native execution, and reference provider as distinct layers.",
  },
  settings: {
    id: "settings",
    label: "Settings",
    path: "/settings",
    group: "System",
    eyebrow: "System / Configuration",
    summary:
      "Validated local configuration, binary recovery, provider secrets, cache, and interface defaults.",
  },
};

export const primaryNavigation = routeIds
  .map((id) => pageMetadata[id])
  .filter((item) => item.group === "Engineering");
export const systemNavigation = routeIds
  .map((id) => pageMetadata[id])
  .filter((item) => item.group === "System");

/** Resolves a canonical top-level route and defaults unknown paths to Overview metadata only. */
export function routeFromPath(pathname: string): RouteId {
  const segment = pathname.split("/").filter(Boolean)[0];
  return routeIds.includes(segment as RouteId) ? (segment as RouteId) : "overview";
}
