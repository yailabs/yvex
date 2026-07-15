/*
 * Owner: apps/operator browser entrypoint.
 * Owns: React root lifecycle, browser router selection, and global stylesheet attachment.
 * Does not own: routes, adapter connectivity, facts, service workers, or fixture fallback.
 * Invariants: startup requires the declared root node and installs no background mutation behavior.
 * Boundary: client startup is operator UI availability only.
 */
import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { BrowserRouter } from "react-router-dom";

import { App } from "./App.tsx";
import "./styles.css";

const root = document.getElementById("root");
if (!root) throw new Error("YVEX operator root element is unavailable");

createRoot(root).render(
  <StrictMode>
    <BrowserRouter>
      <App />
    </BrowserRouter>
  </StrictMode>,
);
