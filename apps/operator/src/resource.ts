/*
 * Owner: apps/operator browser server-resource lifecycle.
 * Owns: initial loading, background refresh, stale preservation, abort cleanup, structured errors, and explicit retry.
 * Does not own: endpoint selection, server cache, global selection, capability inference, or fallback fixtures.
 * Invariants: background refresh never discards the last valid response and route changes abort obsolete requests.
 * Boundary: stale browser data remains labeled and cannot override newer server truth.
 */
import { useCallback, useEffect, useState } from "react";

import { OperatorApiError } from "./api.ts";

export interface ResourceError {
  code: string;
  message: string;
  detail: string | null;
  requestId: string | null;
}

export interface ResourceState<T> {
  data: T | null;
  initialLoading: boolean;
  refreshing: boolean;
  stale: boolean;
  error: ResourceError | null;
  refresh: () => void;
}

/** Loads one abortable server resource and preserves validated data through refresh failures. */
export function useApiResource<T>(
  key: string,
  loader: (signal: AbortSignal) => Promise<T>,
): ResourceState<T> {
  const [result, setResult] = useState<{
    data: T | null;
    error: ResourceError | null;
    settledKey: string | null;
  }>({ data: null, error: null, settledKey: null });
  const [attempt, setAttempt] = useState(0);
  const refresh = useCallback(() => setAttempt((value) => value + 1), []);
  const requestKey = `${key}:${attempt}`;

  useEffect(() => {
    const controller = new AbortController();
    void loader(controller.signal)
      .then((next) => {
        setResult({ data: next, error: null, settledKey: requestKey });
      })
      .catch((reason: unknown) => {
        if (reason instanceof DOMException && reason.name === "AbortError") return;
        const apiError = reason instanceof OperatorApiError ? reason : null;
        setResult((current) => ({
          data: current.data,
          error: {
            code: apiError?.code ?? "resource-failed",
            message: reason instanceof Error ? reason.message : "Operator resource request failed.",
            detail: apiError?.detail ?? null,
            requestId: apiError?.requestId ?? null,
          },
          settledKey: requestKey,
        }));
      });
    return () => controller.abort();
  }, [loader, requestKey]);

  const pending = result.settledKey !== requestKey;
  return {
    data: result.data,
    initialLoading: pending && result.data === null,
    refreshing: pending && result.data !== null,
    stale: Boolean(result.data && result.error),
    error: result.error,
    refresh,
  };
}
