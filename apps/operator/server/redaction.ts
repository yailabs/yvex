/*
 * Owner: apps/operator response safety boundary.
 * Owns: secret-key suppression, absolute-path minimization, and bounded diagnostic summaries.
 * Does not own: command schemas, process limits, source identity, or frontend formatting.
 * Invariants: returned objects contain no raw secret fields or unnecessary local absolute paths.
 * Boundary: redaction preserves status semantics but is not domain validation.
 */
import { basename } from "node:path";

const sensitiveKey = /(authorization|credential|password|secret|token(?:_value)?|api[_-]?key)/i;
const secretAssignment =
  /\b(?:authorization|password|secret|token|api[_-]?key)\s*[:=]\s*[^\s,;]+/gi;
const unixAbsolutePath = /(^|[\s"'=:])\/(?:[^\s"'\\/]+\/)*[^\s"'\\/]*/g;
const windowsAbsolutePath = /[A-Za-z]:\\(?:[^\s"'\\]+\\)*[^\s"'\\]*/g;

function redactPath(path: string): string {
  const leaf = basename(path.replaceAll("\\", "/"));
  return leaf ? `[local]/${leaf}` : "[local-path]";
}

/** Removes credential-like assignments and replaces absolute paths with basename-only labels. */
export function sanitizeText(value: string, maxLength = 320): string {
  const sanitized = value
    .replace(secretAssignment, "[redacted]")
    .replace(windowsAbsolutePath, (match) => redactPath(match))
    .replace(
      unixAbsolutePath,
      (match, prefix: string) => `${prefix}${redactPath(match.slice(prefix.length))}`,
    )
    .replace(/[\r\n\t]+/g, " ")
    .trim();
  return sanitized.length <= maxLength ? sanitized : `${sanitized.slice(0, maxLength - 1)}…`;
}

/** Recursively sanitizes parsed JSON without mutating the validated producer result. */
export function sanitizeValue<T>(value: T, key = ""): T {
  if (sensitiveKey.test(key)) return "[redacted]" as T;
  if (typeof value === "string") return sanitizeText(value, 1_024) as T;
  if (Array.isArray(value)) {
    const items: readonly unknown[] = value;
    return items.map((item) => sanitizeValue(item)) as T;
  }
  if (value && typeof value === "object") {
    const clean: Record<string, unknown> = {};
    for (const [childKey, childValue] of Object.entries(value)) {
      clean[childKey] = sanitizeValue(childValue, childKey);
    }
    return clean as T;
  }
  return value;
}
