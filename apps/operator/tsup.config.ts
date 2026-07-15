/*
 * Owner: apps/operator server production bundle.
 * Owns: stable operator-server entry name, Node target, source maps, and shared dist location.
 * Does not own: client assets, server behavior, dependency installation, or YVEX execution.
 * Invariants: server output remains below apps/operator/dist beside the client directory.
 * Boundary: bundling the adapter does not build the native YVEX binary.
 */
import { defineConfig } from "tsup";

export default defineConfig({
  entry: { "operator-server": "server/index.ts" },
  format: ["esm"],
  platform: "node",
  target: "node24",
  outDir: "dist",
  clean: false,
  sourcemap: true,
  splitting: false,
  dts: false,
});
