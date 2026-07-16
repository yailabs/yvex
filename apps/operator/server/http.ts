/*
 * Owner: apps/operator versioned HTTP backend-for-frontend.
 * Owns: fixed API routing, JSON bounds, same-origin/remote authorization, SSE framing, security headers, and static SPA delivery.
 * Does not own: command selection, provider secrets, business state, arbitrary files, native inference, or browser rendering.
 * Invariants: every mutation is validated, every executable action is service-owned, and remote API access requires token plus Origin admission.
 * Boundary: HTTP reachability is Operator connectivity and never native backend/runtime readiness.
 */
import { randomUUID, timingSafeEqual } from "node:crypto";
import { createReadStream } from "node:fs";
import { stat } from "node:fs/promises";
import { createServer, type IncomingMessage, type Server, type ServerResponse } from "node:http";
import { extname, resolve, sep } from "node:path";
import { ZodError } from "zod";

import {
  API_VERSION,
  SCHEMA_VERSION,
  isCliProducerId,
  isDomainId,
  comparisonEndpointPatchSchema,
  sendChatMessageSchema,
  interfaceSettingsPatchSchema,
  workspaceArtifactSelectionSchema,
  workspaceTargetSelectionSchema,
  yvexSettingsPatchSchema,
  type ApiErrorResponse,
  type ChatStreamEvent,
  type TargetCatalog,
} from "../shared/contracts.ts";
import { ForbiddenProducerError } from "./adapter.ts";
import { buildDomainProjection } from "./projections.ts";
import { ProviderTransportError } from "./provider.ts";
import { sanitizeText } from "./redaction.ts";
import type { OperatorServices } from "./services.ts";
import { validateTrustedBinaryPath } from "./settings.ts";
import { buildSystemHealth } from "./system.ts";
import { WorkspaceSelectionError } from "./workspace.ts";
import { ADAPTER_VERSION, isLoopbackAddress } from "./config.ts";

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

class HttpBoundaryError extends Error {
  constructor(
    readonly status: number,
    readonly code: string,
    message: string,
    readonly detail?: string,
  ) {
    super(message);
    this.name = "HttpBoundaryError";
  }
}

/** Applies browser hardening headers to API, SSE, error, and static responses. */
function commonHeaders(response: ServerResponse): void {
  response.setHeader(
    "Content-Security-Policy",
    "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; font-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'self'",
  );
  response.setHeader("Cross-Origin-Opener-Policy", "same-origin");
  response.setHeader("Cross-Origin-Resource-Policy", "same-origin");
  response.setHeader("Permissions-Policy", "camera=(), microphone=(), geolocation=(), payment=()");
  response.setHeader("Referrer-Policy", "no-referrer");
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.setHeader("X-Frame-Options", "DENY");
}

/** Writes one no-store JSON response after applying the common security policy. */
function json(response: ServerResponse, status: number, value: unknown): void {
  const body = JSON.stringify(value);
  commonHeaders(response);
  response.statusCode = status;
  response.setHeader("Cache-Control", "no-store");
  response.setHeader("Content-Type", "application/json; charset=utf-8");
  response.setHeader("Content-Length", Buffer.byteLength(body));
  response.end(body);
}

/** Writes one structured API error with a correlation ID and no stack trace. */
function apiError(
  response: ServerResponse,
  status: number,
  code: string,
  message: string,
  requestId: string,
  detail?: string,
): void {
  const payload: ApiErrorResponse = {
    apiVersion: API_VERSION,
    schemaVersion: SCHEMA_VERSION,
    code,
    message,
    requestId,
    ...(detail ? { detail: sanitizeText(detail, 500) } : {}),
  };
  json(response, status, payload);
}

/** Compares authentication tokens without data-dependent prefix timing. */
function tokenMatches(expected: string, authorization: string | undefined): boolean {
  if (!authorization?.startsWith("Bearer ")) return false;
  const supplied = authorization.slice(7);
  const expectedBuffer = Buffer.from(expected);
  const suppliedBuffer = Buffer.from(supplied);
  return (
    expectedBuffer.length === suppliedBuffer.length &&
    timingSafeEqual(expectedBuffer, suppliedBuffer)
  );
}

/** Validates Origin/CORS/auth policy before an API route can observe request data. */
function authorizeApi(
  services: OperatorServices,
  request: IncomingMessage,
  response: ServerResponse,
  method: string,
): void {
  const origin = request.headers.origin;
  if (services.config.remoteExposure) {
    if (!origin || !services.config.allowedOrigins.includes(origin.replace(/\/$/, ""))) {
      throw new HttpBoundaryError(
        403,
        "origin-forbidden",
        "Request Origin is not admitted by remote-mode policy.",
      );
    }
    if (
      method !== "OPTIONS" &&
      (!services.config.authToken ||
        !tokenMatches(services.config.authToken, request.headers.authorization))
    ) {
      throw new HttpBoundaryError(
        401,
        "authentication-required",
        "Remote Operator API access requires a valid bearer token.",
      );
    }
    response.setHeader("Access-Control-Allow-Origin", origin);
    response.setHeader("Access-Control-Allow-Credentials", "false");
    response.setHeader("Vary", "Origin");
    return;
  }
  if (!origin) return;
  let parsed: URL;
  try {
    parsed = new URL(origin);
  } catch {
    throw new HttpBoundaryError(403, "origin-invalid", "Request Origin is invalid.");
  }
  const requestHost = request.headers.host;
  if (!isLoopbackAddress(parsed.hostname) || parsed.host !== requestHost) {
    throw new HttpBoundaryError(
      403,
      "origin-forbidden",
      "Only same-origin loopback browser requests are admitted.",
    );
  }
  if (!["GET", "HEAD", "POST", "PATCH", "DELETE", "OPTIONS"].includes(method)) {
    throw new HttpBoundaryError(405, "method-not-allowed", "HTTP method is not admitted.");
  }
}

/** Reads one bounded JSON object and rejects unsupported content types or oversized bodies. */
async function readJson(request: IncomingMessage, maxBytes = 262_144): Promise<unknown> {
  const contentType = request.headers["content-type"] ?? "";
  if (!contentType.toLowerCase().startsWith("application/json")) {
    throw new HttpBoundaryError(
      415,
      "content-type-required",
      "Mutation requests require application/json.",
    );
  }
  const chunks: Buffer[] = [];
  let bytes = 0;
  for await (const chunk of request) {
    const buffer = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk as Uint8Array);
    bytes += buffer.byteLength;
    if (bytes > maxBytes)
      throw new HttpBoundaryError(
        413,
        "request-too-large",
        "Request body exceeds the Operator limit.",
      );
    chunks.push(buffer);
  }
  try {
    return JSON.parse(Buffer.concat(chunks).toString("utf8") || "{}") as unknown;
  } catch {
    throw new HttpBoundaryError(400, "malformed-json", "Request body is not valid JSON.");
  }
}

/** Resolves a decoded static path only within the compiled client root. */
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

/** Serves one immutable client asset or the SPA entrypoint without arbitrary filesystem reads. */
async function serveStatic(
  response: ServerResponse,
  staticRoot: string,
  pathname: string,
  headOnly: boolean,
  requestId: string,
): Promise<void> {
  let candidate = safeStaticPath(staticRoot, pathname);
  if (!candidate) {
    apiError(response, 400, "invalid-path", "Invalid request path.", requestId);
    return;
  }
  try {
    const info = await stat(candidate);
    if (info.isDirectory()) candidate = resolve(candidate, "index.html");
  } catch {
    if (extname(pathname)) {
      apiError(response, 404, "asset-not-found", "Static asset is unavailable.", requestId);
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
    if (headOnly) response.end();
    else createReadStream(candidate).pipe(response);
  } catch {
    apiError(
      response,
      503,
      "operator-not-built",
      "Operator client assets are unavailable; run the production build.",
      requestId,
    );
  }
}

/** Starts one normalized SSE response and flushes proxy-sensitive headers immediately. */
function startSse(response: ServerResponse): void {
  commonHeaders(response);
  response.statusCode = 200;
  response.setHeader("Cache-Control", "no-store, no-transform");
  response.setHeader("Content-Type", "text/event-stream; charset=utf-8");
  response.setHeader("Connection", "keep-alive");
  response.setHeader("X-Accel-Buffering", "no");
  response.flushHeaders();
}

/** Emits one typed SSE frame with a stable event name and JSON data payload. */
function sendSse<T extends { type: string }>(response: ServerResponse, event: T): void {
  if (!response.destroyed && !response.writableEnded) {
    response.write(`event: ${event.type}\ndata: ${JSON.stringify(event)}\n\n`);
  }
}

/** Streams existing and future job events until the job becomes terminal or the client disconnects. */
function streamJobEvents(
  services: OperatorServices,
  request: IncomingMessage,
  response: ServerResponse,
  jobId: string,
): void {
  const job = services.jobs.get(jobId);
  if (!job) throw new HttpBoundaryError(404, "job-not-found", "Job does not exist.");
  startSse(response);
  for (const event of job.events) sendSse(response, { type: "job-event", event, job });
  if (["cancelled", "completed", "failed"].includes(job.state)) {
    response.end();
    return;
  }
  const unsubscribe = services.jobs.subscribe(jobId, (event, next) => {
    sendSse(response, { type: "job-event", event, job: next });
    if (["cancelled", "completed", "failed"].includes(next.state)) {
      unsubscribe();
      response.end();
    }
  });
  const keepAlive = setInterval(() => response.write(": keepalive\n\n"), 15_000);
  keepAlive.unref();
  request.once("close", () => {
    clearInterval(keepAlive);
    unsubscribe();
  });
}

/** Handles the fixed versioned API surface and returns true when a route was consumed. */
async function routeApi(
  services: OperatorServices,
  request: IncomingMessage,
  response: ServerResponse,
  url: URL,
  requestId: string,
): Promise<boolean> {
  const method = request.method ?? "GET";
  if (!url.pathname.startsWith("/api/")) return false;
  authorizeApi(services, request, response, method);

  if (method === "OPTIONS") {
    response.statusCode = 204;
    response.setHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
    response.setHeader("Access-Control-Allow-Methods", "GET, HEAD, POST, PATCH, DELETE, OPTIONS");
    response.end();
    return true;
  }
  if (url.pathname === "/api/v1/version" && method === "GET") {
    json(response, 200, {
      apiVersion: API_VERSION,
      schemaVersion: SCHEMA_VERSION,
      adapterVersion: ADAPTER_VERSION,
    });
    return true;
  }
  if (
    (url.pathname === "/api/v1/system/health" || url.pathname === "/api/v1/health") &&
    method === "GET"
  ) {
    const manifest = await services.capabilities.manifest();
    json(response, 200, buildSystemHealth(services.config, manifest));
    return true;
  }
  if (url.pathname === "/api/v1/system/binary-resolution" && method === "GET") {
    json(response, 200, (await services.resolver.resolve()).resolution);
    return true;
  }
  if (url.pathname === "/api/v1/system/reload" && method === "POST") {
    services.settings.reload();
    services.resolver.clear();
    services.adapter.clearCache();
    services.provider.clearStatus();
    services.events.record(
      "configuration-reload",
      "info",
      "Operator configuration and observations reloaded.",
      requestId,
    );
    json(response, 200, { reloaded: true, observedAt: new Date().toISOString() });
    return true;
  }
  if (url.pathname === "/api/v1/capabilities" && method === "GET") {
    json(response, 200, await services.capabilities.manifest());
    return true;
  }
  if (url.pathname === "/api/v1/workspace" && method === "GET") {
    json(response, 200, await services.workspace.snapshot());
    return true;
  }
  if (url.pathname === "/api/v1/targets" && method === "GET") {
    json(response, 200, await services.workspace.targets());
    return true;
  }
  if (url.pathname === "/api/v1/workspace/artifacts" && method === "GET") {
    json(response, 200, await services.workspace.artifacts());
    return true;
  }
  if (url.pathname === "/api/v1/workspace/target" && method === "POST") {
    const input = workspaceTargetSelectionSchema.parse(await readJson(request));
    json(response, 200, await services.workspace.selectTarget(input.targetId));
    return true;
  }
  if (url.pathname === "/api/v1/workspace/artifact" && method === "POST") {
    const input = workspaceArtifactSelectionSchema.parse(await readJson(request));
    json(response, 200, await services.workspace.selectArtifact(input.artifactId));
    return true;
  }
  if (url.pathname === "/api/v1/build" && method === "GET") {
    json(response, 200, await services.workspace.build());
    return true;
  }
  if (url.pathname === "/api/v1/build/stages" && method === "GET") {
    json(response, 200, await services.workspace.stages());
    return true;
  }
  if (url.pathname === "/api/v1/settings" && method === "GET") {
    json(response, 200, await services.settings.publicSnapshot());
    return true;
  }
  if (url.pathname === "/api/v1/settings/operator" && method === "PATCH") {
    const patch = interfaceSettingsPatchSchema.parse(await readJson(request));
    json(response, 200, await services.settings.updateInterface(patch));
    return true;
  }
  if (url.pathname === "/api/v1/settings/yvex" && method === "PATCH") {
    const patch = yvexSettingsPatchSchema.parse(await readJson(request));
    if (patch.binaryPath !== null) {
      try {
        const normalized = validateTrustedBinaryPath(patch.binaryPath);
        await services.resolver.validateCandidate(normalized);
        patch.binaryPath = normalized;
      } catch (error) {
        throw new HttpBoundaryError(
          400,
          "invalid-yvex-binary",
          "The YVEX binary candidate failed trusted-path or identity validation.",
          error instanceof Error ? error.message : undefined,
        );
      }
    }
    const snapshot = await services.settings.updateYvex(patch);
    services.resolver.clear();
    services.adapter.clearCache();
    services.events.record(
      "configuration-reload",
      "info",
      patch.binaryPath
        ? "Validated YVEX binary setting saved."
        : "Persisted YVEX binary setting cleared.",
      requestId,
    );
    json(response, 200, snapshot);
    return true;
  }
  if (url.pathname === "/api/v1/settings/comparison-endpoint" && method === "PATCH") {
    const patch = comparisonEndpointPatchSchema.parse(await readJson(request));
    if (patch.baseUrl !== undefined) {
      const { validateProviderBaseUrl } = await import("./provider.ts");
      validateProviderBaseUrl(patch.baseUrl, services.config.allowRemoteProviders);
    }
    const snapshot = await services.settings.updateComparisonEndpoint(patch);
    services.provider.clearStatus();
    services.events.record(
      "configuration-reload",
      "info",
      "External comparison settings saved; prior observation invalidated.",
      requestId,
    );
    json(response, 200, snapshot);
    return true;
  }
  if (url.pathname === "/api/v1/settings/comparison-endpoint/test" && method === "POST") {
    json(response, 200, await services.provider.testConnection());
    return true;
  }
  if (url.pathname === "/api/v1/settings/cache/reset" && method === "POST") {
    services.adapter.clearCache(false);
    services.resolver.clear();
    services.provider.clearStatus();
    json(response, 200, { cleared: true, observedAt: new Date().toISOString() });
    return true;
  }
  if (url.pathname === "/api/v1/producers" && method === "GET") {
    json(response, 200, { producers: await services.adapter.listProducers() });
    return true;
  }
  const producerMatch = /^\/api\/v1\/producers\/([^/]+)$/.exec(url.pathname);
  if (producerMatch?.[1] && method === "GET") {
    if (!isCliProducerId(producerMatch[1]))
      throw new HttpBoundaryError(404, "producer-not-found", "Producer is not allowlisted.");
    const producer = (await services.adapter.listProducers()).find(
      (item) => item.id === producerMatch[1],
    );
    json(response, 200, {
      producer,
      latestRun:
        services.adapter.producerRuns().find((run) => run.producerId === producerMatch[1]) ?? null,
    });
    return true;
  }
  const producerRunMatch = /^\/api\/v1\/producers\/([^/]+)\/run$/.exec(url.pathname);
  if (producerRunMatch?.[1] && method === "POST") {
    json(response, 200, await services.adapter.runExplicit(producerRunMatch[1]));
    return true;
  }
  if (url.pathname === "/api/v1/producer-runs" && method === "GET") {
    json(response, 200, { runs: services.adapter.producerRuns() });
    return true;
  }
  const runMatch = /^\/api\/v1\/producer-runs\/([^/]+)$/.exec(url.pathname);
  if (runMatch?.[1] && method === "GET") {
    const run = services.adapter.producerRun(runMatch[1]);
    if (!run)
      throw new HttpBoundaryError(404, "producer-run-not-found", "Producer run does not exist.");
    json(response, 200, run);
    return true;
  }
  const projectionPath: Readonly<Record<string, string>> = {
    "/api/v1/models": "models",
    "/api/v1/sources": "sources",
    "/api/v1/compilation": "compilation",
    "/api/v1/quantization": "quantization",
    "/api/v1/artifacts": "artifacts",
    "/api/v1/backends": "backends",
    "/api/v1/runtime": "runtime",
    "/api/v1/evidence": "evidence",
  };
  const projectionDomain = projectionPath[url.pathname];
  if (projectionDomain && method === "GET" && isDomainId(projectionDomain)) {
    const controller = new AbortController();
    request.once("aborted", () => controller.abort());
    json(
      response,
      200,
      await buildDomainProjection(
        services.adapter,
        services.capabilities,
        projectionDomain,
        controller.signal,
      ),
    );
    return true;
  }
  const modelMatch = /^\/api\/v1\/models\/([^/]+)$/.exec(url.pathname);
  if (modelMatch?.[1] && method === "GET") {
    const targetId = modelMatch[1];
    const projection = await buildDomainProjection(
      services.adapter,
      services.capabilities,
      "models",
    );
    const catalog = projection.reports["target-catalog"]?.data as TargetCatalog | null;
    const target = catalog?.targets.find((item) => item.target_id === decodeURIComponent(targetId));
    if (!target)
      throw new HttpBoundaryError(
        404,
        "target-not-found",
        "Target is not present in the typed catalog.",
      );
    json(response, 200, { ...projection, target });
    return true;
  }
  if (url.pathname === "/api/v1/jobs" && method === "GET") {
    json(response, 200, { jobs: services.jobs.list() });
    return true;
  }
  const jobMatch = /^\/api\/v1\/jobs\/([^/]+)$/.exec(url.pathname);
  if (jobMatch?.[1] && method === "GET") {
    const job = services.jobs.get(jobMatch[1]);
    if (!job) throw new HttpBoundaryError(404, "job-not-found", "Job does not exist.");
    json(response, 200, job);
    return true;
  }
  const jobCancelMatch = /^\/api\/v1\/jobs\/([^/]+)\/cancel$/.exec(url.pathname);
  if (jobCancelMatch?.[1] && method === "POST") {
    json(response, 202, await services.jobs.cancel(jobCancelMatch[1]));
    return true;
  }
  const jobEventsMatch = /^\/api\/v1\/jobs\/([^/]+)\/events$/.exec(url.pathname);
  if (jobEventsMatch?.[1] && method === "GET") {
    streamJobEvents(services, request, response, jobEventsMatch[1]);
    return true;
  }
  if (url.pathname === "/api/v1/events" && method === "GET") {
    json(response, 200, {
      events: services.events.recent(Number(url.searchParams.get("limit") ?? 50)),
    });
    return true;
  }
  if (url.pathname === "/api/v1/comparison/reference/status" && method === "GET") {
    json(response, 200, await services.provider.status());
    return true;
  }
  if (url.pathname === "/api/v1/comparison/reference/models" && method === "GET") {
    const controller = new AbortController();
    request.once("aborted", () => controller.abort());
    json(response, 200, { models: await services.provider.models(controller.signal) });
    return true;
  }
  if (url.pathname === "/api/v1/comparison/reference/sessions" && method === "GET") {
    json(response, 200, { sessions: await services.comparison.list() });
    return true;
  }
  if (url.pathname === "/api/v1/comparison/reference/sessions" && method === "POST") {
    json(response, 201, await services.comparison.create(await readJson(request)));
    return true;
  }
  const sessionMatch = /^\/api\/v1\/comparison\/reference\/sessions\/([^/]+)$/.exec(url.pathname);
  if (sessionMatch?.[1] && method === "GET") {
    const session = await services.comparison.get(sessionMatch[1]);
    if (!session)
      throw new HttpBoundaryError(404, "chat-session-not-found", "Chat session does not exist.");
    json(response, 200, session);
    return true;
  }
  if (sessionMatch?.[1] && method === "PATCH") {
    json(response, 200, await services.comparison.rename(sessionMatch[1], await readJson(request)));
    return true;
  }
  if (sessionMatch?.[1] && method === "DELETE") {
    if (!(await services.comparison.delete(sessionMatch[1])))
      throw new HttpBoundaryError(404, "chat-session-not-found", "Chat session does not exist.");
    response.statusCode = 204;
    response.end();
    return true;
  }
  const sessionClearMatch = /^\/api\/v1\/comparison\/reference\/sessions\/([^/]+)\/clear$/.exec(
    url.pathname,
  );
  if (sessionClearMatch?.[1] && method === "POST") {
    json(response, 200, await services.comparison.clear(sessionClearMatch[1]));
    return true;
  }
  const messageMatch = /^\/api\/v1\/comparison\/reference\/sessions\/([^/]+)\/messages$/.exec(
    url.pathname,
  );
  if (messageMatch?.[1] && method === "POST") {
    const session = await services.comparison.get(messageMatch[1]);
    if (!session)
      throw new HttpBoundaryError(404, "chat-session-not-found", "Chat session does not exist.");
    const input = sendChatMessageSchema.parse(await readJson(request));
    startSse(response);
    let activeRequestId: string | null = null;
    request.once("aborted", () => {
      if (activeRequestId) {
        try {
          services.comparison.cancel(activeRequestId);
        } catch {
          /* Request may have already completed. */
        }
      }
    });
    response.once("close", () => {
      if (!response.writableEnded && activeRequestId) {
        try {
          services.comparison.cancel(activeRequestId);
        } catch {
          /* Request may have already completed. */
        }
      }
    });
    await services.comparison.streamMessage(messageMatch[1], input, (event: ChatStreamEvent) => {
      if (event.type === "request-started") activeRequestId = event.requestId;
      sendSse(response, event);
    });
    if (!response.writableEnded) response.end();
    return true;
  }
  const requestCancelMatch = /^\/api\/v1\/comparison\/reference\/requests\/([^/]+)\/cancel$/.exec(
    url.pathname,
  );
  if (requestCancelMatch?.[1] && method === "POST") {
    json(response, 202, services.comparison.cancel(requestCancelMatch[1]));
    return true;
  }
  throw new HttpBoundaryError(404, "api-route-not-found", "Unknown Operator API route.");
}

/** Creates the local BFF server with no arbitrary command, argv, filesystem, environment, or URL route. */
export function createOperatorHttpServer(services: OperatorServices, staticRoot: string): Server {
  return createServer((request, response) => {
    const requestId = randomUUID();
    response.setHeader("X-Request-Id", requestId);
    void (async () => {
      commonHeaders(response);
      const method = request.method ?? "GET";
      const url = new URL(request.url ?? "/", "http://127.0.0.1");
      if (await routeApi(services, request, response, url, requestId)) return;
      if (method !== "GET" && method !== "HEAD") {
        response.setHeader("Allow", "GET, HEAD");
        throw new HttpBoundaryError(
          405,
          "method-not-allowed",
          "Static Operator routes accept GET and HEAD only.",
        );
      }
      await serveStatic(response, staticRoot, url.pathname, method === "HEAD", requestId);
    })().catch((error: unknown) => {
      if (response.headersSent) {
        if (!response.writableEnded) response.end();
        return;
      }
      if (error instanceof HttpBoundaryError) {
        apiError(response, error.status, error.code, error.message, requestId, error.detail);
      } else if (error instanceof ZodError) {
        apiError(
          response,
          400,
          "invalid-request",
          "Request failed validation.",
          requestId,
          error.issues[0]?.message,
        );
      } else if (error instanceof ForbiddenProducerError) {
        apiError(response, 404, "producer-not-found", "Producer is not allowlisted.", requestId);
      } else if (error instanceof WorkspaceSelectionError) {
        apiError(response, 409, error.code, error.message, requestId);
      } else if (error instanceof ProviderTransportError) {
        apiError(
          response,
          error.status && error.status >= 400 ? 502 : 400,
          error.code,
          error.message,
          requestId,
        );
      } else {
        apiError(
          response,
          500,
          "adapter-internal",
          `YVEX Operator adapter ${ADAPTER_VERSION} could not complete the request.`,
          requestId,
        );
      }
    });
  });
}
