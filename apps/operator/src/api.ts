/*
 * Owner: apps/operator browser API client.
 * Owns: same-origin view fetch, response-shape validation, abort propagation, and connectivity errors.
 * Does not own: retries, CLI execution, fixtures, domain inference, or mutation requests.
 * Invariants: only fixed view identifiers form URLs and malformed responses never reach pages.
 * Boundary: API connectivity is distinct from YVEX binary and capability availability.
 */
import {
  operatorViewResponseSchema,
  type OperatorViewResponse,
  type ViewId,
} from "../shared/contracts.ts";

export class OperatorApiError extends Error {
  readonly status: number | null;

  constructor(message: string, status: number | null = null) {
    super(message);
    this.name = "OperatorApiError";
    this.status = status;
  }
}

/** Fetches and validates one fixed read-only operator view. */
export async function fetchOperatorView(
  view: ViewId,
  signal?: AbortSignal,
): Promise<OperatorViewResponse> {
  let response: Response;
  try {
    response = await fetch(`/api/v1/views/${view}`, {
      method: "GET",
      headers: { Accept: "application/json" },
      cache: "no-store",
      ...(signal ? { signal } : {}),
    });
  } catch (error) {
    if (error instanceof DOMException && error.name === "AbortError") throw error;
    throw new OperatorApiError("Local adapter connectivity is unavailable.");
  }
  if (!response.ok) {
    throw new OperatorApiError(`Local adapter returned HTTP ${response.status}.`, response.status);
  }
  let payload: unknown;
  try {
    payload = (await response.json()) as unknown;
  } catch {
    throw new OperatorApiError("Local adapter returned malformed JSON.", response.status);
  }
  const parsed = operatorViewResponseSchema.safeParse(payload);
  if (!parsed.success || parsed.data.view !== view) {
    throw new OperatorApiError(
      "Local adapter response failed the typed view contract.",
      response.status,
    );
  }
  return parsed.data as OperatorViewResponse;
}
