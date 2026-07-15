/*
 * Owner: apps/operator browser smoke validation.
 * Owns: routing, responsive shell, keyboard focus, command drawer, unavailable states, and connectivity-loss smoke.
 * Does not own: native YVEX proof, production data, parallel browser execution, or visual snapshot baselines.
 * Invariants: Playwright runs one worker against the production bundle and explicit test fixture binary.
 * Boundary: browser smoke validates operator behavior only.
 */
import { expect, test } from "@playwright/test";

test("direct routes preserve sidebar state and typed source evidence", async ({ page }) => {
  await page.goto("/sources");
  await expect(page.getByRole("heading", { name: "Sources", level: 1 })).toBeVisible();
  await expect(page.getByRole("link", { name: "Sources" })).toHaveAttribute("aria-current", "page");
  await expect(page.getByText("deepseek-ai/DeepSeek-V4-Flash").first()).toBeVisible();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(
    true,
  );
});

test("quantization renders truthful unavailable states and no progress", async ({ page }) => {
  await page.goto("/quantization");
  await expect(page.getByRole("heading", { name: "Quantization", level: 1 })).toBeVisible();
  await expect(page.locator("#policy").getByText("Release qtype policy")).toBeVisible();
  await expect(page.getByText("Unsupported").first()).toBeVisible();
  await expect(page.getByText("Progress is intentionally absent")).toBeVisible();
  await expect(page.getByText(/\d+%/)).toHaveCount(0);
});

test("responsive navigation collapses without horizontal page overflow", async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await page.goto("/overview");
  const overviewLink = page.getByRole("link", { name: "Overview" });
  await expect(overviewLink).not.toBeInViewport();
  await page.getByRole("button", { name: "Open navigation" }).click();
  await expect(overviewLink).toBeInViewport();
  await page.getByRole("link", { name: "Artifacts" }).click();
  await expect(page).toHaveURL(/\/artifacts$/);
  await expect(page.getByRole("heading", { name: "Artifacts", level: 1 })).toBeVisible();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(
    true,
  );
});

test("keyboard focus reaches content and the producer drawer", async ({ page }) => {
  await page.goto("/overview");
  await page.keyboard.press("Tab");
  await expect(page.getByRole("link", { name: "Skip to operator content" })).toBeFocused();
  await page.keyboard.press("Control+K");
  const drawer = page.getByRole("dialog", { name: "Evidence producers" });
  await expect(drawer).toBeVisible();
  await expect(
    drawer.getByText(/yvex model-target decision --release v0\.1\.0 --output json/),
  ).toBeVisible();
  await expect(drawer.getByRole("textbox")).toHaveCount(0);
  await expect(drawer.getByRole("button", { name: "Close producer drawer" })).toBeFocused();
  await page.keyboard.press("Escape");
  await expect(drawer).not.toBeVisible();
  await expect(page.getByRole("button", { name: "Producers" })).toBeFocused();
});

test("artifact paths are redacted and artifact classes remain distinct", async ({ page }) => {
  await page.goto("/artifacts");
  await expect(page.getByRole("heading", { name: "Tensor proof artifacts" })).toBeVisible();
  await expect(page.getByRole("heading", { name: "Complete model artifacts" })).toBeVisible();
  await expect(page.getByRole("heading", { name: "Supported model artifacts" })).toBeVisible();
  await expect(page.getByText("[local]/deepseek-selected.gguf").first()).toBeVisible();
  await expect(page.getByText(/\/private\/model/)).toHaveCount(0);
});

test("loss of adapter connectivity remains explicit", async ({ page }) => {
  await page.route("**/api/v1/views/runtime", (route) => route.abort("connectionrefused"));
  await page.goto("/runtime");
  await expect(page.getByRole("alert")).toContainText("Adapter connectivity unavailable");
  await expect(page.getByRole("alert")).toContainText("No fixture fallback was used");
  await expect(page.getByTestId("adapter-connectivity")).toContainText("Adapter offline");
});
