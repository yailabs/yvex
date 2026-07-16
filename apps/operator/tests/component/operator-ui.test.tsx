/*
 * Owner: apps/operator browser-component integration validation.
 * Owns: authoritative workspace state, lifecycle routes, recovery, command keyboard behavior, YVEX console gating, and explicit comparison diagnostics assertions.
 * Does not own: CSS pixel baselines, external network traffic, YVEX inference, or production credentials.
 * Invariants: components use the real BFF contract through an ephemeral same-origin fetch bridge.
 * Boundary: external comparison output remains isolated diagnostics and never becomes YVEX execution evidence.
 */
import { act, render, screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { BrowserRouter } from "react-router-dom";
import { afterEach, describe, expect, it, vi } from "vitest";

import { App } from "../../src/App.tsx";
import { OperatorStateProvider } from "../../src/state/operator-state.tsx";
import {
  browserFetch,
  createTestHarness,
  providerFetcher,
  readyComparisonEndpoint,
  type TestHarness,
} from "../helpers.ts";

const nativeFetch = globalThis.fetch.bind(globalThis);
const harnesses: TestHarness[] = [];

afterEach(async () => {
  vi.unstubAllGlobals();
  window.localStorage.clear();
  await Promise.all(harnesses.splice(0).map((harness) => harness.close()));
});

/** Mounts the production route tree against one real ephemeral BFF and direct browser URL. */
function mount(
  harness: TestHarness,
  path: string,
  observe?: (path: string, init: RequestInit | undefined) => void,
): void {
  window.localStorage.clear();
  window.history.replaceState({}, "", path);
  const bridged = browserFetch(harness.baseUrl, nativeFetch);
  const fetcher: typeof fetch = (input, init) => {
    const value =
      typeof input === "string"
        ? input
        : input instanceof URL
          ? input.pathname
          : new URL(input.url).pathname;
    observe?.(value, init);
    return bridged(input, init);
  };
  vi.stubGlobal("fetch", fetcher);
  render(
    <BrowserRouter>
      <OperatorStateProvider>
        <App />
      </OperatorStateProvider>
    </BrowserRouter>,
  );
}

describe("authoritative YVEX workspace", () => {
  it("keeps shell, target taxonomy, pipeline, and inspector on one server-owned target", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const user = userEvent.setup();
    mount(harness, "/workspace");

    expect(await screen.findByRole("heading", { name: "Workspace", level: 1 })).toBeVisible();
    const context = screen.getByLabelText("Active YVEX workspace context");
    await waitFor(() => expect(within(context).getByText("deepseek4-v4-flash")).toBeVisible());
    const navigator = screen.getByRole("complementary", { name: "Target navigator" });
    expect(
      await within(navigator).findByRole("heading", { name: /Release target/ }, { timeout: 5_000 }),
    ).toBeVisible();
    expect(within(navigator).getByRole("heading", { name: /Source candidate/ })).toBeVisible();
    expect(within(navigator).getByRole("button", { name: /deepseek4-v4-flash/ })).toHaveClass(
      "active",
    );
    expect(screen.getByRole("list", { name: "YVEX lifecycle stages" })).toBeVisible();

    await user.click(within(navigator).getByRole("button", { name: /qwen3-8b/ }));
    await waitFor(() => expect(within(context).getByText("qwen3-8b")).toBeVisible());
    await waitFor(() =>
      expect(within(navigator).getByRole("button", { name: /qwen3-8b/ })).toHaveClass("active"),
    );
    expect(screen.getAllByText("No projection").length).toBeGreaterThan(0);
    expect(screen.queryByText("Target not selected")).not.toBeInTheDocument();
    expect(screen.queryByText("Select provider")).not.toBeInTheDocument();
    expect(screen.queryByText("Reference provider")).not.toBeInTheDocument();
  });

  it("persists a deep-linked build stage and restores it through browser history", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const user = userEvent.setup();
    mount(harness, "/build?stage=transformation-ir");

    expect(await screen.findByRole("heading", { name: "Build", level: 1 })).toBeVisible();
    expect(screen.getByRole("tab", { name: "Transformation IR" })).toHaveAttribute(
      "aria-selected",
      "true",
    );
    expect(
      await screen.findByRole("heading", { name: "Transformation IR inspector" }),
    ).toBeVisible();
    await user.click(screen.getByRole("tab", { name: "GGUF Writer" }));
    expect(window.location.search).toBe("?stage=gguf-writer");
    expect(screen.getByRole("tab", { name: "GGUF Writer" })).toHaveAttribute(
      "aria-selected",
      "true",
    );
    act(() => window.history.back());
    await waitFor(() =>
      expect(screen.getByRole("tab", { name: "Transformation IR" })).toHaveAttribute(
        "aria-selected",
        "true",
      ),
    );
    expect(window.location.search).toBe("?stage=transformation-ir");
  });
});

describe("recovery and global engineering controls", () => {
  it("shows deterministic missing-binary truth and rejects an unsafe Settings candidate", async () => {
    const harness = await createTestHarness({ config: { binaryEnvironmentCandidate: null } });
    harnesses.push(harness);
    const user = userEvent.setup();
    mount(harness, "/environment?tab=binary");

    expect(await screen.findByRole("heading", { name: "Environment", level: 1 })).toBeVisible();
    expect(screen.getByRole("tab", { name: "Binary resolution" })).toHaveAttribute(
      "aria-selected",
      "true",
    );
    expect(
      (
        await screen.findAllByText(
          "No compatible YVEX executable was found in the trusted candidate set.",
        )
      )[0],
    ).toBeVisible();
    await user.click(screen.getByRole("link", { name: "Settings" }));
    await user.click(await screen.findByRole("tab", { name: "YVEX" }));
    const input = await screen.findByLabelText("Trusted absolute binary path");
    await user.type(input, "../../bin/sh");
    await user.click(screen.getByRole("button", { name: "Validate and save" }));
    expect(await screen.findByRole("alert")).toHaveTextContent(
      "failed trusted-path or identity validation",
    );
  });

  it("opens the keyboard command palette and navigates lifecycle actions", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const user = userEvent.setup();
    mount(harness, "/workspace");
    await screen.findByRole("heading", { name: "Workspace", level: 1 });
    await user.keyboard("{Control>}k{/Control}");
    const palette = screen.getByRole("dialog", { name: "Command palette" });
    const search = within(palette).getByRole("textbox", { name: "Search Operator actions" });
    await waitFor(() => expect(search).toHaveFocus());
    await user.type(search, "Runtime Backend");
    await user.keyboard("{Enter}");
    expect(window.location.pathname).toBe("/runtime");
    expect(window.location.search).toBe("?tab=backend");
  });
});

describe("YVEX Generation Console", () => {
  it("inherits workspace context, exposes exact blockers, and never selects a provider", async () => {
    let comparisonMessageRequests = 0;
    const harness = await createTestHarness();
    harnesses.push(harness);
    const user = userEvent.setup();
    mount(harness, "/runtime?tab=generation", (path) => {
      if (path.includes("/comparison/reference") && path.includes("/messages"))
        comparisonMessageRequests += 1;
    });

    expect(await screen.findByRole("heading", { name: "Runtime", level: 1 })).toBeVisible();
    await user.keyboard("{Control>}j{/Control}");
    const console = await screen.findByRole("complementary", {
      name: "YVEX Generation Console",
    });
    await waitFor(() =>
      expect(within(console).getAllByText("deepseek4-v4-flash").length).toBeGreaterThan(0),
    );
    expect(within(console).getByText("Artifact admission")).toBeVisible();
    expect(within(console).getByText("Runtime binding")).toBeVisible();
    expect(within(console).getByText("Tokenizer")).toBeVisible();
    expect(within(console).getByText("Streaming")).toBeVisible();
    expect(within(console).getByText(/No external endpoint is selected or used/)).toBeVisible();
    expect(within(console).getByRole("textbox", { name: "YVEX generation prompt" })).toBeDisabled();
    expect(within(console).queryByText("Select provider")).not.toBeInTheDocument();
    expect(within(console).queryByText("Reference provider")).not.toBeInTheDocument();
    expect(comparisonMessageRequests).toBe(0);
  });

  it("opens from the command palette as the sole primary generation surface", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const user = userEvent.setup();
    mount(harness, "/workspace");
    await screen.findByRole("heading", { name: "Workspace", level: 1 });
    await user.keyboard("{Control>}k{/Control}");
    const palette = screen.getByRole("dialog", { name: "Command palette" });
    await user.type(
      within(palette).getByRole("textbox", { name: "Search Operator actions" }),
      "Generation Console",
    );
    await user.keyboard("{Enter}");
    expect(
      await screen.findByRole("complementary", { name: "YVEX Generation Console" }),
    ).toBeVisible();
  });
});

describe("optional reference comparison diagnostics", () => {
  it("keeps comparison disabled by default and out of global YVEX readiness", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const user = userEvent.setup();
    mount(harness, "/settings");

    expect(await screen.findByRole("heading", { name: "Settings", level: 1 })).toBeVisible();
    expect(screen.queryByText("provider not configured", { exact: false })).not.toBeInTheDocument();
    await user.click(screen.getByRole("tab", { name: "Comparison endpoint" }));
    expect(await screen.findByText(/Disabled is the normal default/)).toBeVisible();
    expect(screen.getByText(/never a YVEX runtime lane, fallback/)).toBeVisible();
  });

  it("streams real deltas only inside the explicitly enabled comparison namespace", async () => {
    const requestedPaths: string[] = [];
    const harness = await createTestHarness();
    harnesses.push(harness);
    await readyComparisonEndpoint(harness.services);
    const user = userEvent.setup();
    mount(harness, "/settings/reference-comparison", (path) => requestedPaths.push(path));

    expect(
      await screen.findByRole("heading", { name: "Reference comparison", level: 1 }),
    ).toBeVisible();
    expect(await screen.findByText("Execution owner: External comparison endpoint")).toBeVisible();
    const prompt = await screen.findByRole("textbox", { name: "External comparison prompt" });
    await waitFor(() => expect(prompt).toBeEnabled());
    await user.type(prompt, "hello reference");
    await user.click(screen.getByRole("button", { name: "Run comparison" }));
    expect(await screen.findByText("Reference response.")).toBeVisible();
    expect(await screen.findByText(/4 in · 2 out/)).toBeVisible();
    expect(requestedPaths.some((path) => path.includes("/comparison/reference/"))).toBe(true);
    expect(requestedPaths.some((path) => path.startsWith("/api/v1/chat/"))).toBe(false);
  });

  it("cancels a comparison and preserves partial external output", async () => {
    const harness = await createTestHarness({
      dependencies: { fetcher: providerFetcher({ delayMs: 80 }) },
    });
    harnesses.push(harness);
    await readyComparisonEndpoint(harness.services);
    const user = userEvent.setup();
    mount(harness, "/settings/reference-comparison");

    const prompt = await screen.findByRole("textbox", { name: "External comparison prompt" });
    await waitFor(() => expect(prompt).toBeEnabled());
    await user.type(prompt, "slow cancel request");
    await user.click(screen.getByRole("button", { name: "Run comparison" }));
    expect(await screen.findByText("Partial")).toBeVisible();
    await user.click(screen.getByRole("button", { name: "Cancel comparison" }));
    expect(await screen.findByText("cancelled")).toBeVisible();
    expect(screen.getByText("Partial")).toBeVisible();
  });
});
