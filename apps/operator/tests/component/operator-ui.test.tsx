/*
 * Owner: apps/operator component validation.
 * Owns: route selection, unavailable rendering, provenance drawer, copy, and connectivity-loss assertions.
 * Does not own: real HTTP, native YVEX execution, CSS geometry, browser process behavior, or production fixtures.
 * Invariants: every response is explicitly installed through a test-local fetch mock.
 * Boundary: component success validates presentation contracts only.
 */
import { render, screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { MemoryRouter } from "react-router-dom";
import { afterEach, describe, expect, it, vi } from "vitest";

import type { ViewId } from "../../shared/contracts.ts";
import { App } from "../../src/App.tsx";
import { viewFixture } from "../helpers.ts";

function installViewFetch(): void {
  vi.stubGlobal(
    "fetch",
    vi.fn((input: string | URL | Request) => {
      const url =
        typeof input === "string" ? input : input instanceof URL ? input.toString() : input.url;
      const view = url.split("/").at(-1) as ViewId;
      return Promise.resolve(
        new Response(JSON.stringify(viewFixture(view)), {
          status: 200,
          headers: { "Content-Type": "application/json" },
        }),
      );
    }),
  );
}

afterEach(() => {
  vi.unstubAllGlobals();
});

describe("operator UI", () => {
  it("preserves direct route selection in the sidebar", async () => {
    installViewFetch();
    render(
      <MemoryRouter initialEntries={["/sources"]}>
        <App />
      </MemoryRouter>,
    );
    expect(await screen.findByRole("heading", { name: "Sources", level: 1 })).toBeVisible();
    expect(screen.getByRole("link", { name: "Sources" })).toHaveAttribute("aria-current", "page");
    expect(screen.getAllByText("deepseek-ai/DeepSeek-V4-Flash")[0]).toBeVisible();
  });

  it("renders qtype refusal states instead of progress data", async () => {
    installViewFetch();
    render(
      <MemoryRouter initialEntries={["/quantization"]}>
        <App />
      </MemoryRouter>,
    );
    expect(await screen.findByRole("heading", { name: "Quantization", level: 1 })).toBeVisible();
    const policy = screen
      .getByRole("heading", { name: "Qtype policy producer" })
      .closest("section");
    expect(policy).not.toBeNull();
    expect(within(policy!).getByText("Release qtype policy")).toBeVisible();
    expect(within(policy!).getByText("Unsupported")).toBeVisible();
    expect(screen.getByText("Progress is intentionally absent")).toBeVisible();
    expect(screen.queryByText(/\d+%/)).not.toBeInTheDocument();
  });

  it("opens the contextual drawer and copies only an audited command", async () => {
    installViewFetch();
    const user = userEvent.setup();
    const writeText = vi.spyOn(navigator.clipboard, "writeText").mockResolvedValue();
    render(
      <MemoryRouter initialEntries={["/overview"]}>
        <App />
      </MemoryRouter>,
    );
    await screen.findByRole("heading", { name: "Overview", level: 1 });
    await user.click(screen.getByRole("button", { name: "Producers" }));
    const drawer = screen.getByRole("dialog", { name: "Evidence producers" });
    expect(drawer).toBeVisible();
    expect(
      within(drawer).getByText(/yvex model-target decision --release v0\.1\.0 --output json/),
    ).toBeVisible();
    expect(within(drawer).queryByRole("textbox")).not.toBeInTheDocument();
    await user.click(within(drawer).getByRole("button", { name: "Copy releaseDecision command" }));
    expect(writeText).toHaveBeenCalledWith(
      "yvex model-target decision --release v0.1.0 --output json",
    );
  });

  it("renders adapter loss with no fixture fallback", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn(() => Promise.reject(new TypeError("connection refused"))),
    );
    render(
      <MemoryRouter initialEntries={["/runtime"]}>
        <App />
      </MemoryRouter>,
    );
    expect(await screen.findByRole("alert")).toHaveTextContent("Adapter connectivity unavailable");
    expect(screen.getByRole("alert")).toHaveTextContent("No fixture fallback was used");
    await waitFor(() =>
      expect(screen.getByTestId("adapter-connectivity")).toHaveTextContent("Adapter offline"),
    );
  });
});
