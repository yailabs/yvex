/*
 * Owner: apps/operator read-only HTTP boundary.
 * Owns: fixed read-only API routes, security headers, static asset serving, and SPA fallback.
 * Does not own: command selection, adapter facts, arbitrary file access, CORS, or mutation endpoints.
 * Invariants: only GET/HEAD are accepted and resolved static paths remain inside dist/client.
 * Boundary: HTTP availability is operator connectivity, not YVEX runtime availability.
 */
import { createReadStream } from "node:fs";
import { stat } from "node:fs/promises";
import { createServer, type Server, type ServerResponse } from "node:http";
import { extname, resolve, sep } from "node:path";

import { isViewId, type ApiErrorResponse } from "../shared/contracts.ts";
import type { OperatorAdapter } from "./adapter.ts";
import { ADAPTER_VERSION } from "./config.ts";
import { buildOperatorView } from "./views.ts";

const contentTypes: Readonly<Record<string, string>> = {
  ".css": "text/css; charset=utf-8",
  ".html": "text/html; charset=utf-8",
  ".ico": "image/x-icon",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".map": "application/json; charset=utf-8",
  ".svg": "image/svg+xml",
  ".woff2": "font/woff2",
};

function commonHeaders(response: ServerResponse): void {
  response.setHeader(
    "Content-Security-Policy",
    "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; font-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'none'",
  );
  response.setHeader("Cross-Origin-Opener-Policy", "same-origin");
  response.setHeader("Referrer-Policy", "no-referrer");
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.setHeader("X-Frame-Options", "DENY");
}

function json(response: ServerResponse, status: number, value: unknown): void {
  const body = JSON.stringify(value);
  commonHeaders(response);
  response.statusCode = status;
  response.setHeader("Cache-Control", "no-store");
  response.setHeader("Content-Type", "application/json; charset=utf-8");
  response.setHeader("Content-Length", Buffer.byteLength(body));
  response.end(body);
}

function apiError(response: ServerResponse, status: number, code: string, message: string): void {
  const payload: ApiErrorResponse = { apiVersion: "v1", code, message };
  json(response, status, payload);
}

function safeStaticPath(staticRoot: string, pathname: string): string | null {
  let decoded: string;
  try {
    decoded = decodeURIComponent(pathname);
  } catch {
    return null;
  }
  if (decoded.includes("\0")) return null;
  const normalizedRoot = resolve(staticRoot);
  const candidate = resolve(normalizedRoot, `.${decoded}`);
  const prefix = `${normalizedRoot}${sep}`;
  return candidate === normalizedRoot || candidate.startsWith(prefix) ? candidate : null;
}

async function serveStatic(
  response: ServerResponse,
  staticRoot: string,
  pathname: string,
  headOnly: boolean,
): Promise<void> {
  let candidate = safeStaticPath(staticRoot, pathname);
  if (!candidate) {
    apiError(response, 400, "invalid-path", "Invalid request path.");
    return;
  }
  try {
    const info = await stat(candidate);
    if (info.isDirectory()) candidate = resolve(candidate, "index.html");
  } catch {
    if (extname(pathname)) {
      apiError(response, 404, "asset-not-found", "Static asset is unavailable.");
      return;
    }
    candidate = resolve(staticRoot, "index.html");
  }
  try {
    const info = await stat(candidate);
    if (!info.isFile()) throw new Error("not a file");
    commonHeaders(response);
    response.statusCode = 200;
    response.setHeader(
      "Cache-Control",
      extname(candidate) === ".html" ? "no-cache" : "public, max-age=31536000, immutable",
    );
    response.setHeader(
      "Content-Type",
      contentTypes[extname(candidate)] ?? "application/octet-stream",
    );
    response.setHeader("Content-Length", info.size);
    if (headOnly) {
      response.end();
      return;
    }
    createReadStream(candidate).pipe(response);
  } catch {
    apiError(
      response,
      503,
      "operator-not-built",
      "Operator client assets are unavailable; run the production build.",
    );
  }
}

/** Creates a read-only HTTP server with no command or mutation endpoint. */
export function createOperatorHttpServer(adapter: OperatorAdapter, staticRoot: string): Server {
  return createServer((request, response) => {
    void (async () => {
      commonHeaders(response);
      const method = request.method ?? "GET";
      if (method !== "GET" && method !== "HEAD") {
        response.setHeader("Allow", "GET, HEAD");
        apiError(response, 405, "read-only", "The YVEX operator exposes read-only routes only.");
        return;
      }

      const url = new URL(request.url ?? "/", "http://127.0.0.1");
      if (url.pathname === "/api/v1/health") {
        json(response, 200, await adapter.health());
        return;
      }
      if (url.pathname.startsWith("/api/")) {
        const match = /^\/api\/v1\/views\/([^/]+)$/.exec(url.pathname);
        if (!match?.[1] || !isViewId(match[1])) {
          apiError(response, 404, "api-route-not-found", "Unknown read-only operator API route.");
          return;
        }
        const controller = new AbortController();
        request.once("aborted", () => controller.abort());
        const view = await buildOperatorView(adapter, match[1], controller.signal);
        if (!response.destroyed) json(response, 200, view);
        return;
      }
      await serveStatic(response, staticRoot, url.pathname, method === "HEAD");
    })().catch(() => {
      if (!response.headersSent) {
        apiError(
          response,
          500,
          "adapter-internal",
          `YVEX operator adapter ${ADAPTER_VERSION} could not complete the request.`,
        );
      } else {
        response.destroy();
      }
    });
  });
}
