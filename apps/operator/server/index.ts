/*
 * Owner: apps/operator server entrypoint.
 * Owns: startup configuration, adapter assembly, static-root selection, and loopback listen lifecycle.
 * Does not own: frontend content, YVEX rebuilding, command policy, model files, or external binding.
 * Invariants: startup refuses invalid configuration before accepting traffic.
 * Boundary: a listening operator is not evidence that the YVEX binary or runtime is available.
 */
import { fileURLToPath, pathToFileURL } from "node:url";

import { OperatorAdapter } from "./adapter.ts";
import { loadOperatorConfig } from "./config.ts";
import { createOperatorHttpServer } from "./http.ts";

/** Starts the production local operator and returns only after the loopback listener is active. */
export async function startOperator(): Promise<void> {
  const config = loadOperatorConfig();
  const adapter = new OperatorAdapter(config);
  const staticRoot = fileURLToPath(new URL("./client/", import.meta.url));
  const server = createOperatorHttpServer(adapter, staticRoot);
  await new Promise<void>((resolve, reject) => {
    server.once("error", reject);
    server.listen(config.port, config.host, () => {
      server.removeListener("error", reject);
      resolve();
    });
  });
  process.stdout.write(`YVEX operator ${config.host}:${config.port} read-only\n`);
}

const entry = process.argv[1] ? pathToFileURL(process.argv[1]).href : "";
if (import.meta.url === entry) {
  void startOperator().catch((error: unknown) => {
    const message = error instanceof Error ? error.message : "unknown startup refusal";
    process.stderr.write(`YVEX operator refused startup: ${message}\n`);
    process.exitCode = 1;
  });
}
