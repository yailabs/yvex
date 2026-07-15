/*
 * Owner: apps/operator browser route assembly.
 * Owns: direct URL registration beneath the shared navigation shell.
 * Does not own: page evidence, adapter calls, navigation metadata, layout CSS, or fallback data.
 * Invariants: every required operator route is directly addressable and unknown paths canonicalize to Overview.
 * Boundary: route availability is not capability availability.
 */
import { Navigate, Route, Routes } from "react-router-dom";

import { OperatorShell } from "./components/Shell.tsx";
import { ArtifactsPage } from "./pages/ArtifactsPage.tsx";
import { CompilationPage } from "./pages/CompilationPage.tsx";
import { EvidencePage } from "./pages/EvidencePage.tsx";
import { ModelsPage } from "./pages/ModelsPage.tsx";
import { OverviewPage } from "./pages/OverviewPage.tsx";
import { QuantizationPage } from "./pages/QuantizationPage.tsx";
import { RuntimePage } from "./pages/RuntimePage.tsx";
import { SettingsPage } from "./pages/SettingsPage.tsx";
import { SourcesPage } from "./pages/SourcesPage.tsx";
import { SystemHealthPage } from "./pages/SystemHealthPage.tsx";

/** Registers static routes only; it allocates no domain state and performs no IO or capability checks. */
export function App() {
  return (
    <Routes>
      <Route element={<OperatorShell />}>
        <Route index element={<Navigate to="/overview" replace />} />
        <Route path="overview" element={<OverviewPage />} />
        <Route path="models" element={<ModelsPage />} />
        <Route path="sources" element={<SourcesPage />} />
        <Route path="compilation" element={<CompilationPage />} />
        <Route path="quantization" element={<QuantizationPage />} />
        <Route path="artifacts" element={<ArtifactsPage />} />
        <Route path="runtime" element={<RuntimePage />} />
        <Route path="evidence" element={<EvidencePage />} />
        <Route path="system-health" element={<SystemHealthPage />} />
        <Route path="settings" element={<SettingsPage />} />
        <Route path="*" element={<Navigate to="/overview" replace />} />
      </Route>
    </Routes>
  );
}
