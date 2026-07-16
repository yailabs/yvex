/*
 * Owner: apps/operator capability presentation labels.
 * Owns: deterministic human-readable labels derived from stable capability identity.
 * Does not own: capability status, reason, grouping, localization, or native capability inference.
 * Invariants: the stable ID remains available as secondary provenance and acronyms retain technical casing.
 * Boundary: changing a label never changes the capability contract or its readiness.
 */
import type { Capability } from "../shared/contracts.ts";

/** Converts one stable capability identity into a compact primary UI label. */
export function capabilityDisplayLabel(capability: Pick<Capability, "id" | "domain">): string {
  if (capability.id === "generation.native") return "YVEX generation";
  const title = capability.id
    .split(".")
    .at(-1)
    ?.replaceAll("-", " ")
    .replace(/\b(yvex|cpu|cuda|kv|api|ir)\b/gi, (value) => value.toUpperCase());
  const domain = `${capability.domain[0]?.toUpperCase()}${capability.domain.slice(1)}`;
  const humanTitle = title ? `${title[0]?.toUpperCase()}${title.slice(1)}` : "Capability";
  return `${domain} · ${humanTitle}`;
}
