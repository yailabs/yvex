/*
 * Owner: apps/operator browser route assembly.
 * Owns: lifecycle-centered canonical URLs, compatibility redirects, and the shared shell boundary.
 * Does not own: page evidence, adapter calls, navigation metadata, layout CSS, or fallback data.
 * Invariants: every retained workbench is directly addressable and former report routes redirect into their owning lifecycle stage.
 * Boundary: route availability is not capability availability.
 */
import { Navigate, Route, Routes } from "react-router-dom";

import { OperatorShell } from "./components/Shell.tsx";
import { ArtifactsPage } from "./pages/ArtifactsPage.tsx";
import { BuildPage } from "./pages/BuildPage.tsx";
import { EvidencePage } from "./pages/EvidencePage.tsx";
import { RuntimePage } from "./pages/RuntimePage.tsx";
import { ReferenceComparisonPage } from "./pages/ReferenceComparisonPage.tsx";
import { SettingsPage } from "./pages/SettingsPage.tsx";
import { SystemHealthPage } from "./pages/SystemHealthPage.tsx";
import { WorkspacePage } from "./pages/WorkspacePage.tsx";

/** Registers canonical workbenches and maps former dashboard pages into their new lifecycle owner. */
export function App() {
  return (
    <Routes>
      <Route element={<OperatorShell />}>
        <Route index element={<Navigate to="/workspace" replace />} />
        <Route path="workspace" element={<WorkspacePage />} />
        <Route path="build" element={<BuildPage />} />
        <Route path="artifacts" element={<ArtifactsPage />} />
        <Route path="runtime" element={<RuntimePage />} />
        <Route path="evidence" element={<EvidencePage />} />
        <Route path="environment" element={<SystemHealthPage />} />
        <Route path="settings" element={<SettingsPage />} />
        <Route path="settings/reference-comparison" element={<ReferenceComparisonPage />} />

        <Route path="overview" element={<Navigate to="/workspace" replace />} />
        <Route path="models" element={<Navigate to="/workspace?panel=targets" replace />} />
        <Route path="sources" element={<Navigate to="/build?stage=source" replace />} />
        <Route
          path="compilation"
          element={<Navigate to="/build?stage=transformation-ir" replace />}
        />
        <Route path="quantization" element={<Navigate to="/build?stage=quantization" replace />} />
        <Route path="system-health" element={<Navigate to="/environment" replace />} />
        <Route path="*" element={<Navigate to="/workspace" replace />} />
      </Route>
    </Routes>
  );
}
