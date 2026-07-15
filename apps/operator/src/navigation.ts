/*
 * Owner: apps/operator information architecture.
 * Owns: YVEX-native route labels, grouped navigation, page summaries, and direct URL mapping.
 * Does not own: CLI catalog labels, evidence status, fetching, layout behavior, or business semantics.
 * Invariants: every required operator surface has exactly one canonical direct route.
 * Boundary: navigation presence is not evidence of implemented downstream capability.
 */
import type { ViewId } from "../shared/contracts.ts";

export interface NavigationItem {
  id: ViewId;
  label: string;
  path: string;
}

export interface PageMetadata extends NavigationItem {
  eyebrow: string;
  summary: string;
}

export const pageMetadata: Readonly<Record<ViewId, PageMetadata>> = {
  overview: {
    id: "overview",
    label: "Overview",
    path: "/overview",
    eyebrow: "Operator / Lifecycle",
    summary: "Current evidence across source, compilation, artifact, and execution boundaries.",
  },
  models: {
    id: "models",
    label: "Models",
    path: "/models",
    eyebrow: "Operator / Target catalog",
    summary: "Release selection and engineering-scope targets at their lowest truthful stage.",
  },
  sources: {
    id: "sources",
    label: "Sources",
    path: "/sources",
    eyebrow: "Compilation / Source trust",
    summary: "Verified identity evidence without browser file access or weight-payload reads.",
  },
  compilation: {
    id: "compilation",
    label: "Compilation",
    path: "/compilation",
    eyebrow: "Compilation / Transformation",
    summary: "Logical-model, Transformation IR, coverage, and physical-lowering gate evidence.",
  },
  quantization: {
    id: "quantization",
    label: "Quantization",
    path: "/quantization",
    eyebrow: "Compilation / Qtype boundary",
    summary: "Baseline qtype policy and refusal evidence with no synthetic execution progress.",
  },
  artifacts: {
    id: "artifacts",
    label: "Artifacts",
    path: "/artifacts",
    eyebrow: "Artifact / Inventory",
    summary: "Proof, complete, and supported artifact classes kept explicitly distinct.",
  },
  runtime: {
    id: "runtime",
    label: "Runtime",
    path: "/runtime",
    eyebrow: "Execution / Runtime boundary",
    summary: "Runtime, generation, evaluation, and benchmark states exactly as reported.",
  },
  evidence: {
    id: "evidence",
    label: "Evidence",
    path: "/evidence",
    eyebrow: "Operator / Provenance",
    summary: "Audited JSON producers, cache behavior, exit status, and missing contracts.",
  },
  "system-health": {
    id: "system-health",
    label: "System health",
    path: "/system-health",
    eyebrow: "System / Connectivity",
    summary: "Adapter, binary, and host topology separated from backend capability evidence.",
  },
  settings: {
    id: "settings",
    label: "Settings",
    path: "/settings",
    eyebrow: "System / Read-only configuration",
    summary: "Redacted startup configuration and immutable safety controls; no mutation surface.",
  },
};

export const primaryNavigation: readonly NavigationItem[] = [
  pageMetadata.overview,
  pageMetadata.models,
  pageMetadata.sources,
  pageMetadata.compilation,
  pageMetadata.quantization,
  pageMetadata.artifacts,
  pageMetadata.runtime,
  pageMetadata.evidence,
];

export const systemNavigation: readonly NavigationItem[] = [
  pageMetadata["system-health"],
  pageMetadata.settings,
];

/** Resolves only canonical single-segment paths and defaults the root to Overview. */
export function viewFromPath(pathname: string): ViewId {
  if (pathname === "/" || pathname === "") return "overview";
  const match = Object.values(pageMetadata).find((page) => page.path === pathname);
  return match?.id ?? "overview";
}
