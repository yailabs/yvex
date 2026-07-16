/*
 * Owner: apps/operator server entrypoint.
 * Owns: startup validation, service assembly, static-root selection, listener lifecycle, and bounded startup logging.
 * Does not own: frontend content, command policy, provider secrets, native YVEX building, or capability promotion.
 * Invariants: insecure remote configuration refuses before accepting traffic and startup logs contain no credentials.
 * Boundary: a listening Operator proves adapter availability only.
 */
import { fileURLToPath, pathToFileURL } from "node:url";

import { loadOperatorConfig } from "./config.ts";
import { createOperatorHttpServer } from "./http.ts";
import { createOperatorServices } from "./services.ts";

/** Starts the production Operator only after configuration and security policy validate. */
export async function startOperator(): Promise<void> {
  const config = loadOperatorConfig();
  const services = createOperatorServices(config);
  const staticRoot = fileURLToPath(new URL("./client/", import.meta.url));
  const server = createOperatorHttpServer(services, staticRoot);
  await new Promise<void>((resolve, reject) => {
    server.once("error", reject);
    server.listen(config.port, config.host, () => {
      server.removeListener("error", reject);
      resolve();
    });
  });
  process.stdout.write(`YVEX Operator ${config.host}:${config.port} ${config.bindMode} API v1\n`);
}

const entry = process.argv[1] ? pathToFileURL(process.argv[1]).href : "";
if (import.meta.url === entry) {
  void startOperator().catch((error: unknown) => {
    const message = error instanceof Error ? error.message : "unknown startup refusal";
    process.stderr.write(`YVEX Operator refused startup: ${message}\n`);
    process.exitCode = 1;
  });
}
