#!/usr/bin/env node
/*
 * Owner: apps/operator automated provider fixture.
 * Owns: deterministic OpenAI-compatible models, SSE deltas, usage, slow streaming, and cancellation observation.
 * Does not own: production fallback, external network access, cloud credentials, native YVEX, or model behavior claims.
 * Invariants: the fixture binds loopback only and emits no invented data outside explicitly selected tests.
 * Boundary: fixture output validates Operator transport and UI only.
 */
import { createServer } from "node:http";

const port = Number(process.env.YVEX_FAKE_PROVIDER_PORT ?? 14318);

/** Reads one small JSON request used only by the isolated deterministic fixture. */
async function jsonBody(request) {
  const chunks = [];
  for await (const chunk of request) chunks.push(chunk);
  return JSON.parse(Buffer.concat(chunks).toString("utf8") || "{}");
}

/** Emits one OpenAI-shaped SSE chunk and returns whether the response remains writable. */
function delta(response, content, finishReason = null, usage = undefined) {
  if (response.destroyed || response.writableEnded) return false;
  response.write(
    `data: ${JSON.stringify({ choices: [{ delta: content === null ? {} : { content }, finish_reason: finishReason }], ...(usage ? { usage } : {}) })}\n\n`,
  );
  return true;
}

const server = createServer((request, response) => {
  void (async () => {
    const url = new URL(request.url ?? "/", "http://127.0.0.1");
    if (request.method === "GET" && url.pathname === "/v1/models") {
      response.setHeader("Content-Type", "application/json");
      response.end(
        JSON.stringify({
          object: "list",
          data: [{ id: "fixture-reference-model", object: "model" }],
        }),
      );
      return;
    }
    if (request.method === "POST" && url.pathname === "/v1/chat/completions") {
      const body = await jsonBody(request);
      const prompt = Array.isArray(body.messages)
        ? String(body.messages.at(-1)?.content ?? "")
        : "";
      response.writeHead(200, {
        "Content-Type": "text/event-stream; charset=utf-8",
        "Cache-Control": "no-cache",
        Connection: "keep-alive",
      });
      const pieces =
        prompt === "Reply with OK."
          ? ["OK"]
          : /slow|cancel/i.test(prompt)
            ? ["Partial ", "reference ", "response ", "continues."]
            : ["Reference ", "response."];
      let index = 0;
      const emit = () => {
        if (!delta(response, pieces[index] ?? "")) return;
        index += 1;
        if (index < pieces.length) {
          setTimeout(emit, /slow|cancel/i.test(prompt) ? 180 : 20).unref();
          return;
        }
        delta(response, null, "stop", {
          prompt_tokens: 4,
          completion_tokens: pieces.length,
          total_tokens: 4 + pieces.length,
        });
        response.write("data: [DONE]\n\n");
        response.end();
      };
      setTimeout(emit, 15).unref();
      return;
    }
    response.statusCode = 404;
    response.end();
  })().catch(() => {
    response.statusCode = 500;
    response.end();
  });
});

server.listen(port, "127.0.0.1", () => {
  process.stdout.write(`fake OpenAI provider 127.0.0.1:${port}\n`);
});
