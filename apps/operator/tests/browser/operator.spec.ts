/*
 * Owner: apps/operator critical browser journeys.
 * Owns: missing/valid binary recovery, authoritative workspace state, typed producers, isolated comparison streaming/cancellation, YVEX console refusal, route history, responsiveness, and keyboard smoke.
 * Does not own: YVEX inference, external providers, visual fixture fabrication, or production secrets.
 * Invariants: fixture configuration is explicit, comparison output streams over real SSE, and tests run serially against bounded local state.
 * Boundary: external comparison behavior never counts as YVEX execution evidence or a primary readiness dependency.
 */
import { resolve } from "node:path";
import { expect, test, type APIRequestContext, type Page } from "@playwright/test";

const fakeYvexPath = resolve(process.cwd(), "tests/fixtures/fake-yvex.mjs");
const comparisonBaseUrl = "http://127.0.0.1:14318/v1";

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

/** Opens the sole primary generation surface and returns its complementary landmark. */
async function openGenerationConsole(page: Page) {
  await page.keyboard.press("Control+J");
  return page.getByRole("complementary", { name: "YVEX Generation Console" });
}

test("Flow A: missing binary is one actionable, layered recovery state", async ({
  page,
  request,
}) => {
  await clearYvex(request);
  await page.goto("/environment?tab=topology");
  await expect(page.getByRole("heading", { name: "Environment", level: 1 })).toBeVisible();
  await expect(page.getByText("Adapter process", { exact: true })).toBeVisible();
  await expect(page.getByText("Active binary", { exact: true })).toBeVisible();
  await expect(page.getByText("Explicit binary override", { exact: true })).toBeVisible();
  await expect(page.getByText("Reference provider", { exact: true })).toHaveCount(0);

  await page.getByRole("tab", { name: "Binary resolution" }).click();
  await expect(page).toHaveURL(/tab=binary/);
  await expect(page.getByRole("heading", { name: "Deterministic resolution trace" })).toBeVisible();
  await expect(page.getByText("No compatible binary selected")).toBeVisible();
  await expect(
    page.getByText("No compatible YVEX executable was found in the trusted candidate set.").first(),
  ).toBeVisible();

  await page.goto("/settings?section=yvex");
  const candidate = page.getByLabel("Trusted absolute binary path");
  await candidate.fill("../../bin/sh");
  await page.getByRole("button", { name: "Validate and save" }).click();
  await expect(page.getByRole("alert")).toContainText("failed trusted-path or identity validation");
});

test("Flow B: machine-readable YVEX establishes one coherent target and typed evidence", async ({
  page,
  request,
}) => {
  await configureYvex(request);
  await page.goto("/workspace");
  await expect(page.getByRole("heading", { name: "Workspace", level: 1 })).toBeVisible();
  const context = page.getByLabel("Active YVEX workspace context");
  await expect(context.getByText("deepseek4-v4-flash")).toBeVisible();
  const navigator = page.getByRole("complementary", { name: "Target navigator" });
  await expect(navigator.getByRole("heading", { name: /Release target/ })).toBeVisible();
  await expect(navigator.getByRole("heading", { name: /Source candidate/ })).toBeVisible();
  await expect(page.getByRole("list", { name: "YVEX lifecycle stages" })).toBeVisible();

  await navigator.getByRole("button", { name: /qwen3-8b/ }).click();
  await expect(context.getByText("qwen3-8b")).toBeVisible();
  await expect(navigator.getByRole("button", { name: /qwen3-8b/ })).toHaveClass(/active/);
  await page.reload();
  await expect(context.getByText("qwen3-8b")).toBeVisible();
  await navigator.getByRole("button", { name: /deepseek4-v4-flash/ }).click();
  await expect(context.getByText("deepseek4-v4-flash")).toBeVisible();

  await page.goto("/evidence?tab=producers");
  await expect(page.getByRole("heading", { name: "Producer registry" })).toBeVisible();
  await page.getByRole("button", { name: "Target catalog" }).click();
  await expect(page.getByText("yvex model-target list --output json")).toBeVisible();
  await page.getByRole("button", { name: "Run safe producer" }).click();
  await expect(page.locator(".producer-detail").getByText("ok", { exact: true })).toBeVisible();

  const workspace = await request.get("/api/v1/workspace");
  expect(workspace.ok()).toBe(true);
  expect(await workspace.json()).toMatchObject({
    activeTarget: { id: "deepseek4-v4-flash", kind: "release-target" },
    workspaceIdentity: { authority: "operator-workspace" },
  });
  const targets = await request.get("/api/v1/targets");
  expect(targets.ok()).toBe(true);
  expect(await targets.json()).toMatchObject({
    targets: expect.arrayContaining([
      expect.objectContaining({ id: "deepseek4-v4-flash", kind: "release-target" }),
      expect.objectContaining({ id: "qwen3-8b", kind: "source-candidate" }),
    ]),
  });
});

test("Flow C: optional comparison streams and cancels only in diagnostics", async ({ page }) => {
  await page.goto("/settings?section=comparison-endpoint");
  await page.getByRole("checkbox", { name: /Enable external comparison endpoint/ }).check();
  await page.getByLabel("Display name").fill("Fixture reference");
  await page.getByLabel("OpenAI-compatible base URL").fill(comparisonBaseUrl);
  await page.getByLabel("API key").fill("browser-test-secret");
  await page.getByLabel("Comparison model").fill("fixture-reference-model");
  await page.getByRole("button", { name: "Save and test comparison" }).click();
  await expect(page.getByText(/passed comparison model and streaming checks/)).toBeVisible();
  await expect(page.getByLabel("API key")).toHaveValue("");

  await page.goto("/settings/reference-comparison");
  await expect(page.getByRole("heading", { name: "Reference comparison", level: 1 })).toBeVisible();
  await expect(page.getByText("Execution owner: External comparison endpoint")).toBeVisible();
  const prompt = page.getByLabel("External comparison prompt");
  await expect(prompt).toBeEnabled();
  await prompt.fill("slow streaming response");
  await page.getByRole("button", { name: "Run comparison" }).click();
  await expect(page.getByText("Partial", { exact: true })).toBeVisible();
  await expect(page.getByText("Partial reference response continues.")).toBeVisible();
  await expect(page.getByText(/4 in · 4 out/)).toBeVisible();
  await expect(page.locator(".comparison-message.role-assistant").last()).toContainText(
    "External comparison",
  );

  await prompt.fill("cancel this slow response");
  await page.getByRole("button", { name: "Run comparison" }).click();
  const active = page.locator(".comparison-message.role-assistant").last();
  await expect(active).toContainText("Partial");
  await page.getByRole("button", { name: "Cancel comparison" }).click();
  await expect(active).toContainText("cancelled");
  await expect(active).toContainText("Partial");
});

test("Flow D: Generation Console is YVEX-only and never falls back", async ({ page }) => {
  let comparisonMessageRequests = 0;
  page.on("request", (request) => {
    if (request.url().includes("/comparison/reference/") && request.url().endsWith("/messages"))
      comparisonMessageRequests += 1;
  });
  await page.goto("/runtime?tab=generation");
  await expect(page.getByRole("tab", { name: "Generation" })).toHaveAttribute(
    "aria-selected",
    "true",
  );
  const console = await openGenerationConsole(page);
  await expect(console).toBeVisible();
  await expect(console.getByText("deepseek4-v4-flash").first()).toBeVisible();
  await expect(console.getByText("Runtime binding", { exact: true })).toBeVisible();
  await expect(console.getByText("Tokenizer", { exact: true })).toBeVisible();
  await expect(console.getByText("Streaming", { exact: true })).toBeVisible();
  await expect(console.getByText(/No external endpoint is selected or used/)).toBeVisible();
  await expect(console.getByLabel("YVEX generation prompt")).toBeDisabled();
  await expect(console.getByText("Select provider")).toHaveCount(0);
  await expect(console.getByText("Reference provider")).toHaveCount(0);
  expect(comparisonMessageRequests).toBe(0);
});

test("Flow E: deep links, history, keyboard focus, and narrow layout work", async ({ page }) => {
  await page.goto("/build?stage=transformation-ir");
  await expect(page.getByRole("tab", { name: "Transformation IR" })).toHaveAttribute(
    "aria-selected",
    "true",
  );
  await page.reload();
  await expect(page.getByRole("tab", { name: "Transformation IR" })).toHaveAttribute(
    "aria-selected",
    "true",
  );
  await page.getByRole("tab", { name: "GGUF Writer" }).click();
  await expect(page).toHaveURL(/stage=gguf-writer/);
  await page.goBack();
  await expect(page.getByRole("tab", { name: "Transformation IR" })).toHaveAttribute(
    "aria-selected",
    "true",
  );
  await page.goForward();
  await expect(page.getByRole("tab", { name: "GGUF Writer" })).toHaveAttribute(
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
  await page.goto("/workspace");
  await expect(page.getByRole("button", { name: "Open navigation" })).toBeVisible();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(
    true,
  );
  await page.getByRole("button", { name: "Open navigation" }).click();
  await expect(page.getByRole("link", { name: "Artifacts" })).toBeInViewport();
});
