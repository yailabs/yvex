/*
 * Owner: apps/operator bounded local event history.
 * Owns: redacted structured event retention, correlation identity, ordering, and recent-event reads.
 * Does not own: application logs, full prompts, provider secrets, job state, or browser rendering.
 * Invariants: retention is bounded and event messages pass through the shared redaction boundary.
 * Boundary: event history is operational context, not native YVEX execution evidence.
 */
import { randomUUID } from "node:crypto";

import type { OperatorEvent } from "../shared/contracts.ts";
import { sanitizeText } from "./redaction.ts";

/** Retains a bounded, newest-last sequence of secret-safe local Operator events. */
export class EventHistory {
  private readonly events: OperatorEvent[] = [];

  constructor(
    private readonly capacity: number,
    private readonly clock: () => number = Date.now,
  ) {}

  /** Appends one sanitized event and evicts oldest entries beyond the configured bound. */
  record(
    type: OperatorEvent["type"],
    severity: OperatorEvent["severity"],
    message: string,
    correlationId: string | null = null,
  ): OperatorEvent {
    const event: OperatorEvent = {
      id: randomUUID(),
      observedAt: new Date(this.clock()).toISOString(),
      severity,
      type,
      message: sanitizeText(message, 500),
      correlationId,
    };
    this.events.push(event);
    if (this.events.length > this.capacity)
      this.events.splice(0, this.events.length - this.capacity);
    return event;
  }

  /** Returns a defensive newest-first snapshot with a caller-bounded result count. */
  recent(limit = 50): OperatorEvent[] {
    return this.events
      .slice(-Math.max(0, Math.min(limit, this.capacity)))
      .reverse()
      .map((event) => ({ ...event }));
  }

  /** Clears only Operator event history and leaves producer, job, and chat state untouched. */
  clear(): void {
    this.events.length = 0;
  }
}
