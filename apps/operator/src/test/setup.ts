/*
 * Owner: apps/operator component-test runtime.
 * Owns: DOM assertion registration and deterministic browser API shims used only by tests.
 * Does not own: production fixtures, adapter responses, component behavior, or CLI execution.
 * Invariants: this module is loaded only by Vitest.
 * Boundary: test conveniences never enter the production bundle.
 */
import "@testing-library/jest-dom/vitest";
import { cleanup } from "@testing-library/react";
import { afterEach } from "vitest";

afterEach(() => cleanup());
