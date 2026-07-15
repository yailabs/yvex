/*
 * Owner: apps/operator artifact and secret guard.
 * Owns: operator-tree forbidden artifact, generated-directory, private-path, and credential-pattern scanning.
 * Does not own: repository-wide model fixtures, dependency CVEs, Git ignore policy, or source semantics.
 * Invariants: dependencies/build outputs are excluded from content scan and must remain untracked.
 * Boundary: a clean scan reduces leakage risk but is not a security audit certification.
 */
import { readdir, readFile, stat } from "node:fs/promises";
import { extname, join, relative } from "node:path";

const root = new URL("../", import.meta.url);
const excludedDirectories = new Set([
  "node_modules",
  "dist",
  "coverage",
  "playwright-report",
  "test-results",
  ".vite",
]);
const forbiddenExtensions = new Set([".safetensors", ".bin", ".dat", ".gguf", ".ptx"]);
const textExtensions = new Set([".css", ".html", ".js", ".json", ".mjs", ".ts", ".tsx"]);
const secretPatterns = [
  /-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----/,
  /\bAKIA[0-9A-Z]{16}\b/,
  /\b(?:sk|ghp|github_pat)_[A-Za-z0-9_=-]{20,}\b/,
  /\/(?:home|Users)\/[A-Za-z0-9._-]+\//,
];
const failures = [];

async function walk(directory) {
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    if (entry.isDirectory() && excludedDirectories.has(entry.name)) continue;
    const path = join(directory, entry.name);
    if (entry.isDirectory()) {
      await walk(path);
      continue;
    }
    const extension = extname(entry.name).toLowerCase();
    const label = relative(root.pathname, path);
    if (forbiddenExtensions.has(extension)) failures.push(`${label}: forbidden artifact extension`);
    if (!textExtensions.has(extension)) continue;
    const info = await stat(path);
    if (info.size > 2_000_000) {
      failures.push(`${label}: oversized tracked-source candidate`);
      continue;
    }
    const content = await readFile(path, "utf8");
    for (const pattern of secretPatterns) {
      if (pattern.test(content)) failures.push(`${label}: secret or private-path pattern`);
    }
  }
}

await walk(root.pathname);
if (failures.length) throw new Error(`operator scan refused:\n${failures.join("\n")}`);
process.stdout.write("operator artifact and secret scan: clean\n");
