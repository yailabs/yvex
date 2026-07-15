/*
 * Owner: apps/operator browser view lifecycle.
 * Owns: route-scoped fetching, cancellation, retry, and typed report access for the current page.
 * Does not own: navigation, CLI commands, server cache, domain status mapping, or fixture fallback.
 * Invariants: route changes cancel obsolete requests and errors remain visible until a real retry succeeds.
 * Boundary: loading state never supplies invented YVEX data.
 */
import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useState,
  type ReactNode,
} from "react";

import type {
  EvidenceEnvelope,
  OperatorViewResponse,
  ProducerDataMap,
  ProducerId,
  ViewId,
} from "../shared/contracts.ts";
import { fetchOperatorView } from "./api.ts";

interface ViewState {
  view: ViewId;
  response: OperatorViewResponse | null;
  loading: boolean;
  error: string | null;
  retry: () => void;
}

const ViewContext = createContext<ViewState | null>(null);

/** Loads a fixed route view and exposes only observed responses or explicit connectivity errors. */
export function OperatorViewProvider({ view, children }: { view: ViewId; children: ReactNode }) {
  const [response, setResponse] = useState<OperatorViewResponse | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [attempt, setAttempt] = useState(0);
  const retry = useCallback(() => {
    setResponse(null);
    setLoading(true);
    setError(null);
    setAttempt((value) => value + 1);
  }, []);

  useEffect(() => {
    const controller = new AbortController();
    void fetchOperatorView(view, controller.signal)
      .then((next) => {
        setResponse(next);
        setLoading(false);
      })
      .catch((reason: unknown) => {
        if (reason instanceof DOMException && reason.name === "AbortError") return;
        setResponse(null);
        setLoading(false);
        setError(
          reason instanceof Error ? reason.message : "Local adapter connectivity is unavailable.",
        );
      });
    return () => controller.abort();
  }, [attempt, view]);

  const value = useMemo(
    () => ({ view, response, loading, error, retry }),
    [view, response, loading, error, retry],
  );
  return <ViewContext.Provider value={value}>{children}</ViewContext.Provider>;
}

export function useOperatorView(): ViewState {
  const value = useContext(ViewContext);
  if (!value) throw new Error("useOperatorView requires OperatorViewProvider");
  return value;
}

/** Reads typed report data while preserving unavailable/refused envelopes for caller rendering. */
export function reportEnvelope<K extends ProducerId>(
  response: OperatorViewResponse | null,
  id: K,
): EvidenceEnvelope<ProducerDataMap[K]> | null {
  return response?.reports[id] ?? null;
}
