/*
 * Owner: apps/operator global detail inspector presentation.
 * Owns: capability/producer/job detail rows, recovery links, modal focus containment, and close behavior.
 * Does not own: data fetching, producer execution, capability calculation, or mutable domain state.
 * Invariants: the inspector renders only caller-owned typed facts and returns focus to its trigger.
 * Boundary: inspecting command provenance never executes the command.
 */
import { ExternalLink, X } from "lucide-react";
import { useEffect, useRef, type RefObject } from "react";
import { Link } from "react-router-dom";

import type { AvailabilityStatus } from "../../shared/contracts.ts";
import { StatusBadge } from "./Status.tsx";

export interface InspectorItem {
  kind: "capability" | "producer" | "job" | "event";
  title: string;
  subtitle: string;
  status: AvailabilityStatus;
  detail: string;
  rows: readonly { label: string; value: string }[];
  recovery?: { label: string; href: string };
}

/** Renders one accessible global inspector and traps focus while open. */
export function InspectorDrawer({
  item,
  onClose,
  returnFocus,
}: {
  item: InspectorItem | null;
  onClose: () => void;
  returnFocus: RefObject<HTMLElement | null>;
}) {
  const drawer = useRef<HTMLElement>(null);
  const close = useRef<HTMLButtonElement>(null);

  useEffect(() => {
    if (!item) return undefined;
    close.current?.focus();
    const onKeyDown = (event: KeyboardEvent): void => {
      if (event.key === "Escape") {
        event.preventDefault();
        onClose();
        returnFocus.current?.focus();
        return;
      }
      if (event.key !== "Tab" || !drawer.current) return;
      const focusable = [
        ...drawer.current.querySelectorAll<HTMLElement>(
          "button, a[href], [tabindex]:not([tabindex='-1'])",
        ),
      ].filter((node) => !node.hasAttribute("disabled"));
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
  }, [item, onClose, returnFocus]);

  return (
    <>
      <button
        type="button"
        className={`overlay-scrim${item ? " open" : ""}`}
        aria-label="Close inspector"
        aria-hidden={!item}
        tabIndex={item ? 0 : -1}
        onClick={onClose}
      />
      <aside
        ref={drawer}
        className={`inspector${item ? " open" : ""}`}
        role="dialog"
        aria-modal="true"
        aria-labelledby="inspector-title"
        aria-hidden={!item}
        inert={!item}
      >
        {item ? (
          <>
            <header className="inspector-header">
              <div>
                <span>{item.kind}</span>
                <h2 id="inspector-title">{item.title}</h2>
                <p>{item.subtitle}</p>
              </div>
              <button
                ref={close}
                type="button"
                className="icon-button"
                aria-label="Close inspector"
                onClick={onClose}
              >
                <X aria-hidden="true" size={18} />
              </button>
            </header>
            <div className="inspector-body">
              <StatusBadge status={item.status} />
              <p className="inspector-detail">{item.detail}</p>
              <dl className="inspector-rows">
                {item.rows.map((row) => (
                  <div key={row.label}>
                    <dt>{row.label}</dt>
                    <dd>{row.value}</dd>
                  </div>
                ))}
              </dl>
              {item.recovery ? (
                <Link className="button primary" to={item.recovery.href} onClick={onClose}>
                  {item.recovery.label} <ExternalLink aria-hidden="true" size={14} />
                </Link>
              ) : null}
            </div>
          </>
        ) : null}
      </aside>
    </>
  );
}
