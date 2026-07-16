#!/usr/bin/env node
/*
 * Owner: apps/operator manual browser acceptance evidence.
 * Owns: deterministic local server startup, explicit comparison diagnostics, viewport matrix, and named screenshots.
 * Does not own: production secrets, YVEX runtime claims, external providers, test assertions, or application behavior.
 * Invariants: fixtures bind loopback, configuration is isolated, and comparison output stays outside the primary YVEX workspace.
 * Boundary: screenshots demonstrate Operator interaction and presentation only; fixture output is not YVEX inference evidence.
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
const comparisonPort = 15_318;
const adapterOrigin = `http://127.0.0.1:${adapterPort}`;
const comparisonBaseUrl = `http://127.0.0.1:${comparisonPort}/v1`;
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
  await page.evaluate(() => window.scrollTo({ top: 0, left: 0 }));
  await page.waitForTimeout(300);
  await page.screenshot({ path: resolve(outputDirectory, filename), fullPage: true });
}

await access(serverBundle).catch(() => {
  throw new Error("production bundle is missing; run npm run build before npm run screenshots");
});
await rm(configDirectory, { recursive: true, force: true });
await rm(outputDirectory, { recursive: true, force: true });
await mkdir(outputDirectory, { recursive: true });

const comparison = startChild(process.execPath, ["tests/fixtures/fake-provider.mjs"], {
  YVEX_FAKE_PROVIDER_PORT: String(comparisonPort),
});
const adapter = startChild(process.execPath, ["dist/operator-server.js"], {
  YVEX_OPERATOR_CONFIG_DIR: configDirectory,
  YVEX_OPERATOR_PORT: String(adapterPort),
  YVEX_OPERATOR_REPOSITORY_ROOT: resolve(configDirectory, "missing-repository"),
  YVEX_OPERATOR_REPOSITORY_BIN: resolve(configDirectory, "missing-repository", "yvex"),
});
let browser;

try {
  await Promise.all([
    waitForUrl(`${comparisonBaseUrl}/models`),
    waitForUrl(`${adapterOrigin}/api/v1/system/health`),
  ]);
  await api("PATCH", "/api/v1/settings/yvex", { binaryPath: fakeYvex });
  await api("PATCH", "/api/v1/settings/comparison-endpoint", {
    enabled: true,
    displayName: "Fixture reference",
    baseUrl: comparisonBaseUrl,
    apiKey: "manual-fixture-secret",
    defaultModel: "fixture-reference-model",
    requestTimeoutMs: 30_000,
  });
  await api("POST", "/api/v1/settings/comparison-endpoint/test", {});

  browser = await chromium.launch({ headless: true });
  const context = await browser.newContext({ viewport: { width: 1600, height: 1000 } });
  const page = await context.newPage();

  await captureRoute(page, "/workspace", "Workspace", "01-workspace.png");
  await captureRoute(
    page,
    "/build?stage=transformation-ir",
    "Build",
    "02-build-transformation-ir.png",
  );
  await captureRoute(page, "/artifacts", "Artifacts", "03-artifacts.png");
  await captureRoute(page, "/runtime?tab=readiness", "Runtime", "04-runtime-readiness.png");
  await captureRoute(page, "/runtime?tab=backend", "Runtime", "05-runtime-backend.png");
  await captureRoute(page, "/runtime?tab=sessions", "Runtime", "06-runtime-sessions.png");
  await captureRoute(page, "/runtime?tab=generation", "Runtime", "07-runtime-generation.png");
  await captureRoute(page, "/evidence?tab=producers", "Evidence", "08-evidence.png");
  await captureRoute(page, "/environment?tab=topology", "Environment", "09-environment.png");
  await captureRoute(page, "/settings?section=yvex", "Settings", "10-settings-yvex.png");
  await captureRoute(
    page,
    "/settings?section=comparison-endpoint",
    "Settings",
    "11-settings-comparison-endpoint.png",
  );

  await page.goto(`${adapterOrigin}/workspace`);
  await page.getByRole("heading", { name: "Workspace", level: 1 }).waitFor();
  await page.getByRole("button", { name: "Open command palette" }).click();
  await page.getByRole("dialog", { name: "Command palette" }).waitFor();
  await page.screenshot({ path: resolve(outputDirectory, "12-command-palette.png") });
  await page.keyboard.press("Escape");

  await page.getByRole("button", { name: "Toggle Generation Console" }).click();
  const console = page.getByRole("complementary", { name: "YVEX Generation Console" });
  await console.waitFor();
  await console.getByText("YVEX generation capability").waitFor();
  await page.screenshot({ path: resolve(outputDirectory, "13-generation-console-blocked.png") });
  await console.getByRole("button", { name: "Close Generation Console" }).click();

  await page.goto(`${adapterOrigin}/settings/reference-comparison`);
  await page.getByRole("heading", { name: "Reference comparison", level: 1 }).waitFor();
  const prompt = page.getByLabel("External comparison prompt");
  await prompt.fill("slow streaming acceptance response");
  await page.getByRole("button", { name: "Run comparison" }).click();
  await page.getByText("Partial", { exact: true }).waitFor();
  await page.evaluate(() => window.scrollTo({ top: 0, left: 0 }));
  await page.screenshot({
    path: resolve(outputDirectory, "14-reference-comparison-streaming.png"),
  });
  await page.getByText("Partial reference response continues.").waitFor();
  await page.evaluate(() => window.scrollTo({ top: 0, left: 0 }));
  await page.screenshot({
    path: resolve(outputDirectory, "15-reference-comparison-completed.png"),
  });

  await page.setViewportSize({ width: 1280, height: 800 });
  await captureRoute(page, "/runtime?tab=readiness", "Runtime", "16-laptop-runtime.png");
  await page.setViewportSize({ width: 1600, height: 1000 });

  await api("PATCH", "/api/v1/settings/yvex", { binaryPath: null });
  await api("POST", "/api/v1/system/reload", {});
  await captureRoute(
    page,
    "/environment?tab=binary",
    "Environment",
    "17-environment-missing-binary.png",
  );

  await api("PATCH", "/api/v1/settings/yvex", { binaryPath: fakeYvex });
  await page.setViewportSize({ width: 768, height: 900 });
  await captureRoute(page, "/workspace", "Workspace", "18-narrow-workspace.png");
  if (await page.getByRole("button", { name: "Open navigation" }).isVisible()) {
    await page.getByRole("button", { name: "Open navigation" }).click();
    await page.locator(".sidebar.mobile-open").waitFor();
    await page.waitForTimeout(200);
    await page.screenshot({ path: resolve(outputDirectory, "19-narrow-navigation.png") });
  }
  await context.close();
} catch (error) {
  const detail = [comparison.diagnostics(), adapter.diagnostics()].filter(Boolean).join("\n");
  if (detail) process.stderr.write(`${detail}\n`);
  throw error;
} finally {
  if (browser) await browser.close();
  await Promise.all([stopChild(adapter), stopChild(comparison)]);
  await rm(configDirectory, { recursive: true, force: true });
}

process.stdout.write(`${outputDirectory}\n`);
