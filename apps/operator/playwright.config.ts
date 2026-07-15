/*
 * Owner: apps/operator browser validation configuration.
 * Owns: single-worker Chromium smoke, loopback production server, and resource-bounded test settings.
 * Does not own: production fixtures, adapter policy, frontend behavior, or parallel execution.
 * Invariants: browser validation uses one worker and an explicitly configured test-only YVEX fixture.
 * Boundary: browser smoke validates the operator, not the native YVEX runtime.
 */
import { defineConfig, devices } from "@playwright/test";

export default defineConfig({
  testDir: "./tests/browser",
  fullyParallel: false,
  workers: 1,
  retries: 0,
  timeout: 20_000,
  expect: { timeout: 5_000 },
  reporter: [["line"]],
  use: {
    baseURL: "http://127.0.0.1:4317",
    trace: "retain-on-failure",
    screenshot: "only-on-failure",
    ...devices["Desktop Chrome"],
  },
  webServer: {
    command:
      "YVEX_BIN=./tests/fixtures/fake-yvex.mjs YVEX_OPERATOR_PORT=4317 node dist/operator-server.js",
    url: "http://127.0.0.1:4317/api/v1/health",
    reuseExistingServer: false,
    timeout: 20_000,
  },
});
