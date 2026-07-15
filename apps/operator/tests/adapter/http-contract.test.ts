/*
 * Owner: apps/operator HTTP contract validation.
 * Owns: loopback API route, method refusal, forbidden selector, and response redaction assertions.
 * Does not own: browser layout, production listener, real YVEX, model files, or external network traffic.
 * Invariants: the server listens on an ephemeral loopback port and closes after each assertion.
 * Boundary: HTTP contract success is not YVEX runtime evidence.
 */
import { mkdtemp, rm, writeFile } from "node:fs/promises";
import type { AddressInfo } from "node:net";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterEach, describe, expect, it } from "vitest";

import { OperatorAdapter } from "../../server/adapter.ts";
import { createOperatorHttpServer } from "../../server/http.ts";
import { testConfig } from "../helpers.ts";

const directories: string[] = [];

async function withServer(run: (baseUrl: string) => Promise<void>): Promise<void> {
  const staticRoot = await mkdtemp(join(tmpdir(), "yvex-operator-static-"));
  directories.push(staticRoot);
  await writeFile(join(staticRoot, "index.html"), "<!doctype html><title>YVEX operator</title>");
  const server = createOperatorHttpServer(new OperatorAdapter(testConfig()), staticRoot);
  await new Promise<void>((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", resolve);
  });
  const address = server.address() as AddressInfo;
  try {
    await run(`http://127.0.0.1:${address.port}`);
  } finally {
    await new Promise<void>((resolve, reject) =>
      server.close((error) => (error ? reject(error) : resolve())),
    );
  }
}

afterEach(async () => {
  await Promise.all(
    directories.splice(0).map((directory) => rm(directory, { recursive: true, force: true })),
  );
});

describe("read-only HTTP boundary", () => {
  it("serves the SPA entrypoint for the canonical root path", async () => {
    await withServer(async (baseUrl) => {
      const response = await fetch(`${baseUrl}/`);
      expect(response.status).toBe(200);
      expect(await response.text()).toContain("YVEX operator");
    });
  });

  it("rejects mutation methods and arbitrary producer selectors", async () => {
    await withServer(async (baseUrl) => {
      const mutation = await fetch(`${baseUrl}/api/v1/views/overview`, { method: "POST" });
      expect(mutation.status).toBe(405);
      expect(await mutation.json()).toMatchObject({ code: "read-only" });

      const injection = await fetch(
        `${baseUrl}/api/v1/views/${encodeURIComponent("releaseDecision --help")}`,
      );
      expect(injection.status).toBe(404);
      expect(await injection.json()).toMatchObject({ code: "api-route-not-found" });
    });
  });

  it("serves a typed artifact view with local paths redacted", async () => {
    await withServer(async (baseUrl) => {
      const response = await fetch(`${baseUrl}/api/v1/views/artifacts`);
      expect(response.status).toBe(200);
      expect(response.headers.get("content-security-policy")).toContain("default-src 'self'");
      const body = JSON.stringify(await response.json());
      expect(body).toContain("[local]/deepseek-selected.gguf");
      expect(body).not.toContain("/private/model");
    });
  });
});
