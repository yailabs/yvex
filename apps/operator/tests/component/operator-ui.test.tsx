/*
 * Owner: apps/operator browser-component integration validation.
 * Owns: route/tab history, capability recovery, settings validation, command keyboard behavior, chat streaming/cancellation, provider failure, and native refusal assertions.
 * Does not own: CSS pixel baselines, external network traffic, native inference, or production credentials.
 * Invariants: components use the real BFF contract through an ephemeral same-origin fetch bridge.
 * Boundary: rendered provider output remains explicitly reference-provider evidence.
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
  readyReferenceProvider,
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

describe("functional routes and recovery", () => {
  it("persists a deep-linked tab and restores prior tab through browser history", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const user = userEvent.setup();
    mount(harness, "/runtime?tab=backend");
    expect(await screen.findByRole("heading", { name: "Runtime", level: 1 })).toBeVisible();
    expect(screen.getByRole("tab", { name: "Backend" })).toHaveAttribute("aria-selected", "true");
    await user.click(screen.getByRole("tab", { name: "Controls" }));
    expect(window.location.search).toBe("?tab=controls");
    expect(screen.getByRole("tab", { name: "Controls" })).toHaveAttribute("aria-selected", "true");
    act(() => window.history.back());
    await waitFor(() =>
      expect(screen.getByRole("tab", { name: "Backend" })).toHaveAttribute("aria-selected", "true"),
    );
    expect(window.location.search).toBe("?tab=backend");
  });

  it("shows one missing-binary recovery surface and rejects an unsafe Settings candidate", async () => {
    const harness = await createTestHarness({ config: { binaryEnvironmentCandidate: null } });
    harnesses.push(harness);
    const user = userEvent.setup();
    mount(harness, "/overview");
    expect(
      (
        await screen.findAllByText(
          "No compatible YVEX executable was found in the trusted candidate set.",
        )
      )[0],
    ).toBeVisible();
    const recovery = screen.getByRole("link", { name: /Configure YVEX/ });
    await user.click(recovery);
    expect(window.location.pathname).toBe("/settings");
    expect(window.location.search).toBe("?section=yvex");
    const input = await screen.findByLabelText("Trusted absolute binary path");
    await user.type(input, "../../bin/sh");
    await user.click(screen.getByRole("button", { name: "Validate and save" }));
    expect(await screen.findByRole("alert")).toHaveTextContent(
      "failed trusted-path or identity validation",
    );
  });

  it("opens a searchable keyboard command palette and navigates fixed tab actions", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    const user = userEvent.setup();
    mount(harness, "/overview");
    await screen.findByRole("heading", { name: "Overview", level: 1 });
    await user.keyboard("{Control>}k{/Control}");
    const palette = screen.getByRole("dialog", { name: "Command palette" });
    expect(palette).toBeVisible();
    const search = within(palette).getByRole("textbox", { name: "Search Operator actions" });
    await waitFor(() => expect(search).toHaveFocus());
    await user.type(search, "Runtime Backend");
    await user.keyboard("{Enter}");
    expect(window.location.pathname).toBe("/runtime");
    expect(window.location.search).toBe("?tab=backend");
  });
});

describe("global chat dock", () => {
  it("streams real reference deltas and renders owner, model, usage, and timing metadata", async () => {
    const harness = await createTestHarness();
    harnesses.push(harness);
    await readyReferenceProvider(harness.services);
    const user = userEvent.setup();
    mount(harness, "/models");
    await screen.findByRole("heading", { name: "Models", level: 1 });
    await user.keyboard("{Control>}j{/Control}");
    const dock = await screen.findByRole("complementary", { name: "YVEX model chat" });
    const composer = within(dock).getByRole("textbox", { name: "Chat message" });
    await waitFor(() => expect(composer).toBeEnabled());
    await user.type(composer, "hello reference");
    await user.click(within(dock).getByRole("button", { name: "Send" }));
    expect(await within(dock).findByText("Reference response.")).toBeVisible();
    await waitFor(() =>
      expect(
        dock.querySelector('.chat-message.role-assistant[data-state="complete"]'),
      ).not.toBeNull(),
    );
    expect(within(dock).getAllByText("Reference provider").length).toBeGreaterThan(0);
    expect(within(dock).getByText(/Fixture reference · fixture-reference-model/)).toBeVisible();
    expect(within(dock).getByText(/4 in · 2 out · 6 total/)).toBeVisible();
    expect(within(dock).getByText(/TTFT \d+ ms/)).toBeVisible();
  });

  it("cancels streaming and preserves partial output", async () => {
    const harness = await createTestHarness({
      dependencies: { fetcher: providerFetcher({ delayMs: 80 }) },
    });
    harnesses.push(harness);
    await readyReferenceProvider(harness.services);
    const user = userEvent.setup();
    mount(harness, "/overview");
    await screen.findByRole("heading", { name: "Overview", level: 1 });
    await user.keyboard("{Control>}j{/Control}");
    const dock = await screen.findByRole("complementary", { name: "YVEX model chat" });
    const composer = within(dock).getByRole("textbox", { name: "Chat message" });
    await waitFor(() => expect(composer).toBeEnabled());
    await user.type(composer, "slow cancel request");
    await user.click(within(dock).getByRole("button", { name: "Send" }));
    await within(dock).findByText("Partial");
    await user.click(within(dock).getByRole("button", { name: "Cancel" }));
    await waitFor(() =>
      expect(
        dock.querySelector('.chat-message.role-assistant[data-state="cancelled"]'),
      ).not.toBeNull(),
    );
    expect(within(dock).getByText("Partial")).toBeVisible();
    expect(within(dock).getByText(/partial output preserved/i)).toBeInTheDocument();
  });

  it("gates native composition with exact dependencies and sends no provider request", async () => {
    let messageRequests = 0;
    const harness = await createTestHarness();
    harnesses.push(harness);
    const user = userEvent.setup();
    mount(harness, "/runtime?tab=controls", (path) => {
      if (path.includes("/messages")) messageRequests += 1;
    });
    await screen.findByRole("heading", { name: "Runtime", level: 1 });
    await user.keyboard("{Control>}j{/Control}");
    const dock = await screen.findByRole("complementary", { name: "YVEX model chat" });
    await user.click(within(dock).getByRole("button", { name: "Native YVEX" }));
    expect(await within(dock).findByText("Native YVEX generation is unavailable")).toBeVisible();
    expect(within(dock).getByText("runtime.binding")).toBeVisible();
    expect(within(dock).getByText("generation.tokenizer")).toBeVisible();
    expect(within(dock).getByText("generation.streaming")).toBeVisible();
    expect(within(dock).getByText(/No request will be redirected/)).toBeVisible();
    expect(within(dock).getByRole("textbox", { name: "Chat message" })).toBeDisabled();
    expect(messageRequests).toBe(0);
  });

  it("renders a structured provider failure without replacing the lane", async () => {
    let fail = false;
    const readyFetcher = providerFetcher();
    const failedFetcher = providerFetcher({ failChat: true });
    const mutableFetcher: typeof fetch = (input, init) =>
      fail ? failedFetcher(input, init) : readyFetcher(input, init);
    const harness = await createTestHarness({ dependencies: { fetcher: mutableFetcher } });
    harnesses.push(harness);
    await readyReferenceProvider(harness.services);
    fail = true;
    const user = userEvent.setup();
    mount(harness, "/overview");
    await screen.findByRole("heading", { name: "Overview", level: 1 });
    await user.keyboard("{Control>}j{/Control}");
    const dock = await screen.findByRole("complementary", { name: "YVEX model chat" });
    const composer = within(dock).getByRole("textbox", { name: "Chat message" });
    await waitFor(() => expect(composer).toBeEnabled());
    await user.type(composer, "provider should fail");
    await user.click(within(dock).getByRole("button", { name: "Send" }));
    expect(await within(dock).findByRole("alert")).toHaveTextContent("provider-chat-failed");
    expect(within(dock).getAllByText("Reference provider").length).toBeGreaterThan(0);
    expect(
      within(dock).queryByText("Native YVEX", { selector: ".chat-message *" }),
    ).not.toBeInTheDocument();
  });
});
