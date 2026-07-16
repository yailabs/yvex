/*
 * Owner: apps/operator end-to-end browser validation configuration.
 * Owns: single-worker Chromium, production BFF startup, isolated persistence, deterministic provider fixture, and artifacts.
 * Does not own: production provider configuration, native runtime claims, test behavior, or external network traffic.
 * Invariants: both servers bind loopback and no YVEX fixture is configured implicitly at startup.
 * Boundary: browser fixture success validates Operator vertical slices only.
 */
import { defineConfig, devices } from "@playwright/test";

const browserTestPort = 14_317;
const providerTestPort = 14_318;
const browserTestOrigin = `http://127.0.0.1:${browserTestPort}`;

export default defineConfig({
  testDir: "./tests/browser",
  outputDir: "test-results/playwright",
  fullyParallel: false,
  workers: 1,
  retries: 0,
  timeout: 30_000,
  expect: { timeout: 7_000 },
  reporter: [["line"]],
  use: {
    baseURL: browserTestOrigin,
    trace: "retain-on-failure",
    screenshot: "only-on-failure",
    ...devices["Desktop Chrome"],
  },
  webServer: [
    {
      command: `YVEX_FAKE_PROVIDER_PORT=${providerTestPort} node tests/fixtures/fake-provider.mjs`,
      url: `http://127.0.0.1:${providerTestPort}/v1/models`,
      reuseExistingServer: false,
      timeout: 20_000,
    },
    {
      command: `YVEX_OPERATOR_CONFIG_DIR=./test-results/playwright/operator-config YVEX_OPERATOR_PORT=${browserTestPort} node dist/operator-server.js`,
      url: `${browserTestOrigin}/api/v1/system/health`,
      reuseExistingServer: false,
      timeout: 20_000,
    },
  ],
});
