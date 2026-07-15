/*
 * Owner: apps/operator build boundary.
 * Owns: browser development proxy, production bundle location, and test runtime.
 * Does not own: adapter execution, YVEX facts, UI semantics, or release claims.
 * Invariants: browser assets are emitted only below apps/operator/dist.
 * Boundary: bundling the operator does not build or execute the YVEX engine.
 */
import react from "@vitejs/plugin-react";
import { defineConfig } from "vitest/config";

export default defineConfig({
  plugins: [react()],
  server: {
    host: "127.0.0.1",
    port: 4173,
    proxy: {
      "/api": "http://127.0.0.1:4317",
    },
  },
  build: {
    outDir: "dist/client",
    emptyOutDir: true,
    sourcemap: true,
  },
  test: {
    environment: "jsdom",
    setupFiles: ["./src/test/setup.ts"],
    exclude: ["tests/browser/**", "node_modules/**", "dist/**"],
    restoreMocks: true,
    clearMocks: true,
    coverage: {
      reporter: ["text", "json-summary"],
    },
  },
});
