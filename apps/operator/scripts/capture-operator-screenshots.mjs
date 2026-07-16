#!/usr/bin/env node
/*
 * Owner: apps/operator manual browser acceptance evidence.
 * Owns: deterministic local server startup, reference-provider configuration, viewport matrix, and named screenshots.
 * Does not own: production secrets, native runtime claims, external providers, test assertions, or application behavior.
 * Invariants: fixtures bind loopback, configuration is isolated, and every captured generation uses real BFF/provider SSE.
 * Boundary: screenshots demonstrate Operator interaction and presentation only; fixture output is not native inference evidence.
 */
import { spawn } from "node:child_process";
import { access, mkdir, rm } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { chromium } from "@playwright/test";

const scriptDirectory = dirname(fileURLToPath(import.meta.url));
const operatorRoot = resolve(scriptDirectory, "..");
const repositoryRoot = resolve(operatorRoot, "../..");
const outputDirectory = resolve(repositoryRoot, "docs/screenshots/operator-e2e");
const configDirectory = resolve(operatorRoot, "test-results/manual/operator-config");
const adapterPort = 15_317;
const providerPort = 15_318;
const adapterOrigin = `http://127.0.0.1:${adapterPort}`;
const providerBaseUrl = `http://127.0.0.1:${providerPort}/v1`;
const fakeYvex = resolve(operatorRoot, "tests/fixtures/fake-yvex.mjs");
const serverBundle = resolve(operatorRoot, "dist/operator-server.js");

/** Waits for one local endpoint without hiding startup failure beyond the fixed deadline. */
async function waitForUrl(url, deadlineMs = 20_000) {
  const deadline = Date.now() + deadlineMs;
  while (Date.now() < deadline) {
    try {
      const response = await fetch(url);
      if (response.ok) return;
    } catch {
      // A bounded retry is expected while the child listener starts.
    }
    await new Promise((resolveWait) => setTimeout(resolveWait, 100));
  }
  throw new Error(`timed out waiting for ${url}`);
}

/** Calls one fixed Operator endpoint and fails with its structured response text. */
async function api(method, path, data) {
  const response = await fetch(`${adapterOrigin}${path}`, {
    method,
    headers: data === undefined ? undefined : { "Content-Type": "application/json" },
    body: data === undefined ? undefined : JSON.stringify(data),
  });
  if (!response.ok)
    throw new Error(`${method} ${path}: ${response.status} ${await response.text()}`);
  return response.json();
}

/** Starts one loopback-only child and retains bounded diagnostics if startup fails. */
function startChild(command, arguments_, environment) {
  const child = spawn(command, arguments_, {
    cwd: operatorRoot,
    env: { ...process.env, ...environment },
    stdio: ["ignore", "pipe", "pipe"],
  });
  let diagnostics = "";
  child.stdout.on("data", (chunk) => {
    diagnostics = `${diagnostics}${chunk}`.slice(-8_192);
  });
  child.stderr.on("data", (chunk) => {
    diagnostics = `${diagnostics}${chunk}`.slice(-8_192);
  });
  child.diagnostics = () => diagnostics;
  return child;
}

/** Stops one fixture child without mutating any external process. */
async function stopChild(child) {
  if (child.exitCode !== null) return;
  child.kill("SIGTERM");
  await Promise.race([
    new Promise((resolveExit) => child.once("exit", resolveExit)),
    new Promise((resolveTimeout) => setTimeout(resolveTimeout, 2_000)),
  ]);
  if (child.exitCode === null) child.kill("SIGKILL");
}

/** Captures one settled route at the current viewport using its visible page title as readiness. */
async function captureRoute(page, route, title, filename) {
  await page.goto(`${adapterOrigin}${route}`);
  await page.getByRole("heading", { name: title, level: 1 }).waitFor();
  await page.waitForTimeout(250);
  await page.screenshot({ path: resolve(outputDirectory, filename), fullPage: true });
}

await access(serverBundle).catch(() => {
  throw new Error("production bundle is missing; run npm run build before npm run screenshots");
});
await rm(configDirectory, { recursive: true, force: true });
await rm(outputDirectory, { recursive: true, force: true });
await mkdir(outputDirectory, { recursive: true });

const provider = startChild(process.execPath, ["tests/fixtures/fake-provider.mjs"], {
  YVEX_FAKE_PROVIDER_PORT: String(providerPort),
});
const adapter = startChild(process.execPath, ["dist/operator-server.js"], {
  YVEX_OPERATOR_CONFIG_DIR: configDirectory,
  YVEX_OPERATOR_PORT: String(adapterPort),
});
let browser;

try {
  await Promise.all([
    waitForUrl(`${providerBaseUrl}/models`),
    waitForUrl(`${adapterOrigin}/api/v1/system/health`),
  ]);
  await api("PATCH", "/api/v1/settings/yvex", { binaryPath: fakeYvex });
  await api("PATCH", "/api/v1/settings/reference-provider", {
    enabled: true,
    displayName: "Fixture reference",
    baseUrl: providerBaseUrl,
    apiKey: "manual-fixture-secret",
    defaultModel: "fixture-reference-model",
    requestTimeoutMs: 30_000,
  });
  await api("POST", "/api/v1/settings/reference-provider/test", {});

  browser = await chromium.launch({ headless: true });
  const context = await browser.newContext({ viewport: { width: 1440, height: 1000 } });
  const page = await context.newPage();

  await captureRoute(page, "/overview", "Overview", "01-overview.png");
  await captureRoute(page, "/models", "Models", "02-models.png");
  await captureRoute(page, "/runtime?tab=stages", "Runtime", "03-runtime-stages.png");
  await captureRoute(page, "/runtime?tab=backend", "Runtime", "04-runtime-backend.png");
  await captureRoute(page, "/runtime?tab=controls", "Runtime", "05-runtime-controls.png");
  await captureRoute(page, "/evidence?tab=producers", "Evidence", "06-evidence.png");
  await captureRoute(page, "/system-health?tab=topology", "System Health", "07-system-health.png");
  await captureRoute(page, "/settings?section=yvex", "Settings", "08-settings-yvex.png");
  await captureRoute(
    page,
    "/settings?section=reference-provider",
    "Settings",
    "09-settings-reference-provider.png",
  );

  await page.goto(`${adapterOrigin}/overview`);
  await page.getByRole("heading", { name: "Overview", level: 1 }).waitFor();
  await page.getByRole("button", { name: "Open command palette" }).click();
  await page.getByRole("dialog", { name: "Command palette" }).waitFor();
  await page.screenshot({ path: resolve(outputDirectory, "10-command-palette.png") });
  await page.keyboard.press("Escape");

  await page.getByRole("button", { name: "Toggle chat dock" }).click();
  const dock = page.getByRole("complementary", { name: "YVEX model chat" });
  await dock.waitFor();
  const composer = dock.getByLabel("Chat message");
  await composer.fill("slow streaming acceptance response");
  await dock.getByRole("button", { name: "Send" }).click();
  const streamingMessage = dock.locator('.chat-message.role-assistant[data-state="streaming"]');
  await streamingMessage.filter({ hasText: "Partial" }).waitFor();
  await page.screenshot({ path: resolve(outputDirectory, "11-chat-streaming.png") });
  await dock.locator('.chat-message.role-assistant[data-state="complete"]').waitFor();
  await page.screenshot({ path: resolve(outputDirectory, "12-chat-completed.png") });

  await composer.fill("cancel this slow acceptance response");
  await dock.getByRole("button", { name: "Send" }).click();
  await dock
    .locator('.chat-message.role-assistant[data-state="streaming"]')
    .filter({ hasText: "Partial" })
    .waitFor();
  await dock.getByRole("button", { name: "Cancel" }).click();
  await dock.locator('.chat-message.role-assistant[data-state="cancelled"]').waitFor();
  await page.screenshot({ path: resolve(outputDirectory, "13-chat-cancelled.png") });

  await dock.getByRole("button", { name: "Native YVEX" }).click();
  await dock.getByText("Native YVEX generation is unavailable").waitFor();
  await page.screenshot({ path: resolve(outputDirectory, "14-native-lane-blocked.png") });
  await dock.getByRole("button", { name: "Close chat" }).click();

  await page.setViewportSize({ width: 1280, height: 800 });
  await captureRoute(page, "/runtime?tab=controls", "Runtime", "15-laptop-runtime-controls.png");
  await page.setViewportSize({ width: 1440, height: 1000 });

  await api("PATCH", "/api/v1/settings/yvex", { binaryPath: null });
  await api("POST", "/api/v1/system/reload", {});
  await captureRoute(
    page,
    "/system-health?tab=binary",
    "System Health",
    "16-system-health-missing-binary.png",
  );

  await page.setViewportSize({ width: 768, height: 900 });
  await captureRoute(page, "/overview", "Overview", "17-narrow-overview.png");
  if (await page.getByRole("button", { name: "Open navigation" }).isVisible()) {
    await page.getByRole("button", { name: "Open navigation" }).click();
    await page.locator(".sidebar.mobile-open").waitFor();
    await page.waitForTimeout(200);
    await page.screenshot({ path: resolve(outputDirectory, "18-narrow-navigation.png") });
  }
  await context.close();
} catch (error) {
  const detail = [provider.diagnostics(), adapter.diagnostics()].filter(Boolean).join("\n");
  if (detail) process.stderr.write(`${detail}\n`);
  throw error;
} finally {
  if (browser) await browser.close();
  await Promise.all([stopChild(adapter), stopChild(provider)]);
}

process.stdout.write(`${outputDirectory}\n`);
