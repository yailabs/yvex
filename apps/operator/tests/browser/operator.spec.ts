/*
 * Owner: apps/operator critical browser journeys.
 * Owns: missing/valid binary recovery, typed producer UI, provider streaming/cancellation, native refusal, route history, responsiveness, and keyboard smoke.
 * Does not own: native inference, external providers, visual fixture fabrication, or production secrets.
 * Invariants: fixture configuration is explicit, provider output is streamed over real SSE, and tests run serially against bounded local state.
 * Boundary: reference-provider behavior never counts as native YVEX execution evidence.
 */
import { resolve } from "node:path";
import { expect, test, type APIRequestContext, type Page } from "@playwright/test";

const fakeYvexPath = resolve(process.cwd(), "tests/fixtures/fake-yvex.mjs");
const providerBaseUrl = "http://127.0.0.1:14318/v1";

test.describe.configure({ mode: "serial" });

/** Clears only the persisted YVEX choice and reloads bounded observations. */
async function clearYvex(request: APIRequestContext): Promise<void> {
  expect((await request.patch("/api/v1/settings/yvex", { data: { binaryPath: null } })).ok()).toBe(
    true,
  );
  expect((await request.post("/api/v1/system/reload", { data: {} })).ok()).toBe(true);
}

/** Configures the executable fixture through the public validation endpoint. */
async function configureYvex(request: APIRequestContext): Promise<void> {
  const response = await request.patch("/api/v1/settings/yvex", {
    data: { binaryPath: fakeYvexPath },
  });
  expect(response.ok()).toBe(true);
}

/** Opens the dock and returns its global complementary landmark after provider capability settles. */
async function openChat(page: Page) {
  await page.keyboard.press("Control+J");
  return page.getByRole("complementary", { name: "YVEX model chat" });
}

test("Flow A: missing binary is one actionable, layered recovery state", async ({
  page,
  request,
}) => {
  await clearYvex(request);
  await page.goto("/overview");
  await expect(page.getByRole("heading", { name: "Overview", level: 1 })).toBeVisible();
  await expect(page.locator(".recovery-state")).toContainText(
    "No compatible YVEX executable was found",
  );
  await expect(page.getByText("Local control plane")).toBeVisible();

  await page.goto("/system-health?tab=topology");
  await expect(page.getByText("Adapter process", { exact: true })).toBeVisible();
  await expect(page.getByText("YVEX resolved", { exact: true })).toBeVisible();
  await page.getByRole("tab", { name: "Binary resolution" }).click();
  await expect(page).toHaveURL(/tab=binary/);
  await expect(page.getByRole("heading", { name: "Deterministic resolution trace" })).toBeVisible();
  await expect(page.getByText("No compatible binary selected")).toBeVisible();

  await page.goto("/settings?section=yvex");
  const candidate = page.getByLabel("Trusted absolute binary path");
  await candidate.fill("../../bin/sh");
  await page.getByRole("button", { name: "Validate and save" }).click();
  await expect(page.getByRole("alert")).toContainText("failed trusted-path or identity validation");
});

test("Flow B: a machine-readable YVEX fixture enables typed Models and Evidence", async ({
  page,
  request,
}) => {
  await configureYvex(request);
  await page.goto("/models");
  await expect(page.getByRole("heading", { name: "Models", level: 1 })).toBeVisible();
  await expect(page.getByRole("cell", { name: "deepseek4-v4-flash" })).toBeVisible();
  await expect(page.getByText("qwen3-8b")).toBeVisible();
  await page.getByRole("button", { name: "Select" }).first().click();
  await expect(page).toHaveURL(/target=deepseek4-v4-flash/);
  await expect(page.getByText("deepseek-ai/DeepSeek-V4-Flash")).toBeVisible();

  await page.goto("/evidence?tab=producers");
  await expect(page.getByRole("heading", { name: "Producer registry" })).toBeVisible();
  await page.getByRole("button", { name: "Target catalog" }).click();
  await expect(page.getByText("yvex model-target list --output json")).toBeVisible();
  await page.getByRole("button", { name: "Run safe producer" }).click();
  await expect(page.locator(".producer-detail").getByText("ok", { exact: true })).toBeVisible();
  const models = await request.get("/api/v1/models");
  expect(models.ok()).toBe(true);
  expect(await models.json()).toMatchObject({
    domain: "models",
    reports: { "target-catalog": { schemaVersion: "1.0" } },
  });
});

test("Flow C: reference-provider chat streams, reports metadata, and cancels with partial output", async ({
  page,
}) => {
  await page.goto("/settings?section=reference-provider");
  await page.getByRole("checkbox", { name: /Enable reference provider/ }).check();
  await page.getByLabel("Display name").fill("Fixture reference");
  await page.getByLabel("Base URL").fill(providerBaseUrl);
  await page.getByLabel("API key").fill("browser-test-secret");
  await page.getByLabel("Default model").fill("fixture-reference-model");
  await page.getByRole("button", { name: "Save and test" }).click();
  await expect(page.getByText(/passed model and streaming compatibility checks/)).toBeVisible();
  await expect(page.getByLabel("API key")).toHaveValue("");

  const dock = await openChat(page);
  await expect(dock).toBeVisible();
  await expect(dock.getByText("Reference provider").first()).toBeVisible();
  const composer = dock.getByLabel("Chat message");
  await expect(composer).toBeEnabled();
  await composer.fill("slow streaming response");
  await dock.getByRole("button", { name: "Send" }).click();
  await expect(dock.getByText("Partial", { exact: false })).toBeVisible();
  await expect(dock.getByText("streaming", { exact: true }).first()).toBeVisible();
  await expect(dock.getByText("Partial reference response continues.")).toBeVisible();
  await expect(dock.locator('.chat-message.role-assistant[data-state="complete"]')).toBeVisible();
  await expect(dock.getByText(/Fixture reference · fixture-reference-model/)).toBeVisible();
  await expect(dock.getByText(/4 in · 4 out · 8 total/)).toBeVisible();
  await expect(dock.getByText(/TTFT \d+ ms/)).toBeVisible();

  await composer.fill("cancel this slow response");
  await dock.getByRole("button", { name: "Send" }).click();
  await expect(dock.locator('.chat-message.role-assistant[data-state="streaming"]')).toContainText(
    "Partial",
  );
  await dock.getByRole("button", { name: "Cancel" }).click();
  const cancelled = dock.locator('.chat-message.role-assistant[data-state="cancelled"]');
  await expect(cancelled).toBeVisible();
  await expect(cancelled).toContainText("Partial");
  await expect(dock).toContainText("partial output preserved");
  await expect(dock.getByText("Reference provider").first()).toBeVisible();
});

test("Flow D: native lane is precisely gated and never falls back", async ({ page }) => {
  let providerMessageRequests = 0;
  page.on("request", (request) => {
    if (request.url().includes("/chat/") && request.url().endsWith("/messages"))
      providerMessageRequests += 1;
  });
  await page.goto("/runtime?tab=controls");
  const dock = await openChat(page);
  await dock.getByRole("button", { name: "Native YVEX" }).click();
  await expect(dock.getByText("Native YVEX generation is unavailable")).toBeVisible();
  await expect(dock.getByText("runtime.binding")).toBeVisible();
  await expect(dock.getByText("generation.tokenizer")).toBeVisible();
  await expect(dock.getByText("generation.streaming")).toBeVisible();
  await expect(dock.getByText(/No request will be redirected/)).toBeVisible();
  await expect(dock.getByLabel("Chat message")).toBeDisabled();
  expect(providerMessageRequests).toBe(0);
});

test("Flow E: deep links, refresh, Back/Forward, keyboard focus, and narrow layout work", async ({
  page,
}) => {
  await page.goto("/runtime?tab=backend");
  await expect(page.getByRole("tab", { name: "Backend" })).toHaveAttribute("aria-selected", "true");
  await page.reload();
  await expect(page.getByRole("tab", { name: "Backend" })).toHaveAttribute("aria-selected", "true");
  await page.getByRole("tab", { name: "Controls" }).click();
  await expect(page).toHaveURL(/tab=controls/);
  await page.goBack();
  await expect(page.getByRole("tab", { name: "Backend" })).toHaveAttribute("aria-selected", "true");
  await page.goForward();
  await expect(page.getByRole("tab", { name: "Controls" })).toHaveAttribute(
    "aria-selected",
    "true",
  );

  await page.keyboard.press("Control+K");
  const palette = page.getByRole("dialog", { name: "Command palette" });
  await expect(palette).toBeVisible();
  await expect(palette.getByRole("textbox", { name: "Search Operator actions" })).toBeFocused();
  await page.keyboard.press("Escape");
  await expect(palette).not.toBeVisible();

  await page.setViewportSize({ width: 768, height: 900 });
  await page.goto("/artifacts");
  await expect(page.getByRole("button", { name: "Open navigation" })).toBeVisible();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(
    true,
  );
  await page.getByRole("button", { name: "Open navigation" }).click();
  await expect(page.getByRole("link", { name: "Artifacts" })).toBeInViewport();
});
