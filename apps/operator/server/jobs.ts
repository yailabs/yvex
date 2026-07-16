/*
 * Owner: apps/operator reusable job lifecycle.
 * Owns: bounded job state, legal transitions, indeterminate progress, cancellation hooks, events, and subscribers.
 * Does not own: producer execution, provider transport, persisted sessions, HTTP SSE formatting, or fake progress.
 * Invariants: exact progress is null unless a producer supplies a truthful measurement; terminal jobs cannot transition.
 * Boundary: a completed Operator job records control-plane completion, not native model capability.
 */
import { randomUUID } from "node:crypto";

import type {
  ExecutionOwner,
  Job,
  JobEvent,
  JobState,
  StructuredError,
} from "../shared/contracts.ts";
import type { EventHistory } from "./events.ts";
import { sanitizeText } from "./redaction.ts";

type JobListener = (event: JobEvent, job: Job) => void;
type CancelHandler = () => void | Promise<void>;

const terminalStates = new Set<JobState>(["cancelled", "completed", "failed"]);
const allowedTransitions: Readonly<Record<JobState, readonly JobState[]>> = {
  queued: ["starting", "cancelling", "cancelled", "failed"],
  starting: ["running", "cancelling", "cancelled", "failed"],
  running: ["cancelling", "cancelled", "completed", "failed"],
  cancelling: ["cancelled", "failed"],
  cancelled: [],
  completed: [],
  failed: [],
};

/** Owns bounded in-memory jobs and propagates transitions to SSE-compatible subscribers. */
export class JobManager {
  private readonly jobs = new Map<string, Job>();
  private readonly listeners = new Map<string, Set<JobListener>>();
  private readonly cancellations = new Map<string, CancelHandler>();

  constructor(
    private readonly capacity: number,
    private readonly history: EventHistory,
    private readonly clock: () => number = Date.now,
  ) {}

  /** Allocates one queued job with indeterminate progress and no side effects beyond bounded state. */
  create(
    type: Job["type"],
    executionOwner: ExecutionOwner,
    phase: string | null,
    cancellable: boolean,
  ): Job {
    const createdAt = new Date(this.clock()).toISOString();
    const job: Job = {
      id: randomUUID(),
      type,
      executionOwner,
      state: "queued",
      createdAt,
      startedAt: null,
      endedAt: null,
      progress: null,
      phase,
      events: [],
      cancellable,
      resultReference: null,
      error: null,
    };
    this.jobs.set(job.id, job);
    this.evictTerminalJobs();
    this.event(job.id, "queued", "info", "Job queued.", phase);
    return structuredClone(job);
  }

  /** Registers one owner-provided cancellation hook without exposing it through transport state. */
  registerCancellation(jobId: string, handler: CancelHandler): void {
    const job = this.require(jobId);
    if (!job.cancellable) throw new Error("job does not admit cancellation");
    this.cancellations.set(jobId, handler);
  }

  /** Applies one legal lifecycle transition and stamps start/end time exactly once. */
  transition(
    jobId: string,
    next: JobState,
    phase: string | null,
    optional: { resultReference?: string | null; error?: StructuredError | null } = {},
  ): Job {
    const job = this.require(jobId);
    if (!allowedTransitions[job.state].includes(next)) {
      throw new Error(`invalid job transition ${job.state} -> ${next}`);
    }
    const now = new Date(this.clock()).toISOString();
    job.state = next;
    job.phase = phase;
    if ((next === "starting" || next === "running") && !job.startedAt) job.startedAt = now;
    if (terminalStates.has(next)) {
      job.endedAt = now;
      job.cancellable = false;
      this.cancellations.delete(jobId);
    }
    if ("resultReference" in optional) job.resultReference = optional.resultReference ?? null;
    if ("error" in optional) job.error = optional.error ?? null;
    this.event(jobId, `state-${next}`, next === "failed" ? "error" : "info", `Job ${next}.`, phase);
    return structuredClone(job);
  }

  /** Requests cancellation through the registered owner and never invents immediate completion. */
  async cancel(jobId: string): Promise<Job> {
    const job = this.require(jobId);
    if (!job.cancellable || terminalStates.has(job.state))
      throw new Error("job is not cancellable");
    if (job.state !== "cancelling") this.transition(jobId, "cancelling", job.phase);
    const handler = this.cancellations.get(jobId);
    if (handler) await handler();
    return this.get(jobId) as Job;
  }

  /** Adds a secret-safe job event and delivers defensive snapshots to current subscribers. */
  event(
    jobId: string,
    type: string,
    severity: JobEvent["severity"],
    message: string,
    phase: string | null,
  ): JobEvent {
    const job = this.require(jobId);
    const event: JobEvent = {
      id: randomUUID(),
      jobId,
      observedAt: new Date(this.clock()).toISOString(),
      type,
      severity,
      message: sanitizeText(message, 500),
      phase,
    };
    job.events.push(event);
    if (job.events.length > 200) job.events.splice(0, job.events.length - 200);
    this.history.record("job-transition", severity, event.message, jobId);
    for (const listener of this.listeners.get(jobId) ?? [])
      listener({ ...event }, structuredClone(job));
    return { ...event };
  }

  /** Returns one defensive job snapshot or null for an unknown identifier. */
  get(jobId: string): Job | null {
    const job = this.jobs.get(jobId);
    return job ? structuredClone(job) : null;
  }

  /** Returns newest-first defensive job snapshots with no mutable cancellation handles. */
  list(): Job[] {
    return [...this.jobs.values()]
      .sort((left, right) => right.createdAt.localeCompare(left.createdAt))
      .map((job) => structuredClone(job));
  }

  /** Subscribes to future events for one existing job and returns an idempotent cleanup callback. */
  subscribe(jobId: string, listener: JobListener): () => void {
    this.require(jobId);
    const listeners = this.listeners.get(jobId) ?? new Set<JobListener>();
    listeners.add(listener);
    this.listeners.set(jobId, listeners);
    return () => {
      listeners.delete(listener);
      if (listeners.size === 0) this.listeners.delete(jobId);
    };
  }

  /** Resolves one known job or throws a stable internal lookup failure. */
  private require(jobId: string): Job {
    const job = this.jobs.get(jobId);
    if (!job) throw new Error("job not found");
    return job;
  }

  /** Evicts only oldest terminal jobs so active work is never discarded by retention pressure. */
  private evictTerminalJobs(): void {
    if (this.jobs.size <= this.capacity) return;
    for (const [id, job] of this.jobs) {
      if (!terminalStates.has(job.state)) continue;
      this.jobs.delete(id);
      this.listeners.delete(id);
      this.cancellations.delete(id);
      if (this.jobs.size <= this.capacity) break;
    }
  }
}
