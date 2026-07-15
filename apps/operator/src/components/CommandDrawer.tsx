/*
 * Owner: apps/operator contextual producer control.
 * Owns: read-only provenance drawer, exact redacted command display, copy action, and last-exit display.
 * Does not own: command entry, execution, refresh, paths, flags, or adapter selection.
 * Invariants: the drawer renders only producers already attached to the current typed view.
 * Boundary: copying a producer command does not execute it or change YVEX state.
 */
import { Check, Clipboard, Command, X } from "lucide-react";
import { useEffect, useRef, useState, type RefObject } from "react";

import type { EvidenceEnvelope } from "../../shared/contracts.ts";
import { useOperatorView } from "../view-context.tsx";
import { AvailabilityBadge } from "./Status.tsx";

/** Manages accessible drawer focus/copy state; it never accepts, mutates, or executes command text. */
export function CommandDrawer({
  open,
  onClose,
  returnFocus,
}: {
  open: boolean;
  onClose: () => void;
  returnFocus: RefObject<HTMLButtonElement | null>;
}) {
  const { response, view } = useOperatorView();
  const drawerRef = useRef<HTMLElement>(null);
  const closeRef = useRef<HTMLButtonElement>(null);
  const [copied, setCopied] = useState<string | null>(null);
  const reports = Object.values(response?.reports ?? {}) as EvidenceEnvelope<unknown>[];

  useEffect(() => {
    if (!open) return undefined;
    closeRef.current?.focus();
    const onKeyDown = (event: KeyboardEvent): void => {
      if (event.key === "Escape") {
        event.preventDefault();
        onClose();
        returnFocus.current?.focus();
      }
      if (event.key !== "Tab" || !drawerRef.current) return;
      const focusable = [
        ...drawerRef.current.querySelectorAll<HTMLElement>(
          "button, [href], [tabindex]:not([tabindex='-1'])",
        ),
      ].filter((element) => !element.hasAttribute("disabled"));
      const first = focusable[0];
      const last = focusable.at(-1);
      if (!first || !last) return;
      if (event.shiftKey && document.activeElement === first) {
        event.preventDefault();
        last.focus();
      } else if (!event.shiftKey && document.activeElement === last) {
        event.preventDefault();
        first.focus();
      }
    };
    document.addEventListener("keydown", onKeyDown);
    return () => document.removeEventListener("keydown", onKeyDown);
  }, [onClose, open, returnFocus]);

  const copyCommand = async (report: EvidenceEnvelope<unknown>): Promise<void> => {
    if (!report.producer.displayCommand || report.producer.command.length === 0) return;
    try {
      await navigator.clipboard.writeText(report.producer.displayCommand);
      setCopied(report.producer.id);
      window.setTimeout(() => setCopied(null), 1_500);
    } catch {
      setCopied(null);
    }
  };

  return (
    <>
      <button
        type="button"
        className={`drawer-scrim${open ? " open" : ""}`}
        aria-label="Close producer drawer"
        aria-hidden={!open}
        tabIndex={open ? 0 : -1}
        onClick={onClose}
      />
      <aside
        ref={drawerRef}
        className={`command-drawer${open ? " open" : ""}`}
        aria-hidden={!open}
        inert={!open}
        aria-modal="true"
        aria-labelledby="command-drawer-title"
        role="dialog"
      >
        <header className="command-drawer-header">
          <div className="drawer-title-mark">
            <Command aria-hidden="true" size={18} />
          </div>
          <div>
            <span className="micro-label">Current view / {view}</span>
            <h2 id="command-drawer-title">Evidence producers</h2>
          </div>
          <button
            ref={closeRef}
            type="button"
            className="icon-button"
            onClick={onClose}
            aria-label="Close producer drawer"
          >
            <X aria-hidden="true" size={18} />
          </button>
        </header>
        <div className="command-drawer-body">
          <p className="drawer-intro">
            Audited read-only producers behind this view. Commands are fixed by the adapter and
            cannot be edited here.
          </p>
          {reports.length === 0 ? (
            <div className="drawer-empty">This view uses only local typed probes.</div>
          ) : (
            reports.map((report) => (
              <section className="producer-record" key={report.producer.id}>
                <div className="producer-record-head">
                  <div>
                    <span className="micro-label">{report.producer.evidenceClass}</span>
                    <h3>{report.producer.label}</h3>
                  </div>
                  <AvailabilityBadge value={report.availability} />
                </div>
                <p>{report.producer.description}</p>
                {report.producer.command.length ? (
                  <div className="command-copy-row">
                    <code>{report.producer.displayCommand}</code>
                    <button
                      type="button"
                      onClick={() => void copyCommand(report)}
                      aria-label={`Copy ${report.producer.label} command`}
                    >
                      {copied === report.producer.id ? (
                        <Check aria-hidden="true" size={15} />
                      ) : (
                        <Clipboard aria-hidden="true" size={15} />
                      )}
                    </button>
                  </div>
                ) : (
                  <div className="local-probe-label">{report.producer.displayCommand}</div>
                )}
                <dl className="producer-meta">
                  <div>
                    <dt>Last exit</dt>
                    <dd>{report.lastExit.code === null ? "—" : report.lastExit.code}</dd>
                  </div>
                  <div>
                    <dt>State</dt>
                    <dd>{report.lastExit.state}</dd>
                  </div>
                  <div>
                    <dt>Cache</dt>
                    <dd>{report.producer.cachePolicy}</dd>
                  </div>
                </dl>
              </section>
            ))
          )}
          {response?.missingProducers.length ? (
            <section className="missing-producers-drawer">
              <span className="micro-label">Excluded or missing contracts</span>
              {response.missingProducers.map((producer) => (
                <div key={producer.id}>
                  <strong>{producer.label}</strong>
                  <AvailabilityBadge value={producer.availability} />
                </div>
              ))}
            </section>
          ) : null}
        </div>
      </aside>
    </>
  );
}
