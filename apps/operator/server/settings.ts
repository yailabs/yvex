/*
 * Owner: apps/operator persisted local settings.
 * Owns: non-secret configuration, restrictive secret persistence, redacted snapshots, validation, and reload.
 * Does not own: HTTP authorization, binary execution, provider requests, browser persistence, or runtime capability.
 * Invariants: API keys are written only to a mode-0600 secret file and are never returned by public snapshots.
 * Boundary: persisting a candidate does not admit a binary or establish provider reachability.
 */
import { chmod, mkdir, readFile, rename, writeFile } from "node:fs/promises";
import { basename, isAbsolute, join, normalize, sep } from "node:path";

import {
  API_VERSION,
  SCHEMA_VERSION,
  interfaceSettingsPatchSchema,
  referenceProviderPatchSchema,
  type InterfaceSettingsPatch,
  type ReferenceProviderPatch,
  type SettingsResponse,
  type YvexSettingsPatch,
  yvexSettingsPatchSchema,
} from "../shared/contracts.ts";
import type { OperatorConfig } from "./config.ts";

interface PersistedSettings {
  schemaVersion: "1";
  yvex: { binaryPath: string | null };
  referenceProvider: {
    enabled: boolean;
    displayName: string;
    baseUrl: string;
    defaultModel: string;
    requestTimeoutMs: number;
  };
  interface: {
    defaultLane: "native-yvex" | "reference-provider";
    chatDefaultMode: "closed" | "compact" | "docked" | "expanded" | "fullscreen";
  };
}

interface PersistedSecrets {
  schemaVersion: "1";
  referenceProviderApiKey: string | null;
}

export interface InternalSettings {
  persisted: PersistedSettings;
  referenceProviderApiKey: string | null;
}

const defaultSettings: PersistedSettings = {
  schemaVersion: "1",
  yvex: { binaryPath: null },
  referenceProvider: {
    enabled: false,
    displayName: "Local reference",
    baseUrl: "http://127.0.0.1:8080/v1",
    defaultModel: "",
    requestTimeoutMs: 120_000,
  },
  interface: { defaultLane: "reference-provider", chatDefaultMode: "closed" },
};

/** Rejects relative traversal and returns a normalized absolute binary candidate. */
export function validateTrustedBinaryPath(value: string): string {
  const trimmed = value.trim();
  if (!isAbsolute(trimmed)) throw new Error("binary path must be absolute");
  if (trimmed.split(/[\\/]+/).includes("..")) throw new Error("binary path traversal is forbidden");
  const normalized = normalize(trimmed);
  if (!normalized || normalized === sep) throw new Error("binary path must name a file");
  return normalized;
}

/** Returns a browser-safe local file label with no directory disclosure. */
export function safePathLabel(path: string | null): string | null {
  if (!path) return null;
  const leaf = basename(path);
  return leaf ? `[local]/${leaf}` : "[local-path]";
}

/** Parses persisted JSON defensively and falls back only to explicit defaults, never partial unvalidated values. */
function parsePersistedSettings(value: unknown): PersistedSettings {
  if (!value || typeof value !== "object") return structuredClone(defaultSettings);
  const record = value as Partial<PersistedSettings>;
  const provider = record.referenceProvider;
  const ui = record.interface;
  const binaryPath = record.yvex?.binaryPath;
  return {
    schemaVersion: "1",
    yvex: {
      binaryPath: typeof binaryPath === "string" ? validateTrustedBinaryPath(binaryPath) : null,
    },
    referenceProvider: {
      enabled: typeof provider?.enabled === "boolean" ? provider.enabled : false,
      displayName:
        typeof provider?.displayName === "string" && provider.displayName
          ? provider.displayName
          : defaultSettings.referenceProvider.displayName,
      baseUrl:
        typeof provider?.baseUrl === "string"
          ? provider.baseUrl
          : defaultSettings.referenceProvider.baseUrl,
      defaultModel: typeof provider?.defaultModel === "string" ? provider.defaultModel : "",
      requestTimeoutMs:
        typeof provider?.requestTimeoutMs === "number"
          ? provider.requestTimeoutMs
          : defaultSettings.referenceProvider.requestTimeoutMs,
    },
    interface: {
      defaultLane: ui?.defaultLane === "native-yvex" ? "native-yvex" : "reference-provider",
      chatDefaultMode: ["closed", "compact", "docked", "expanded", "fullscreen"].includes(
        ui?.chatDefaultMode ?? "",
      )
        ? (ui?.chatDefaultMode as PersistedSettings["interface"]["chatDefaultMode"])
        : "closed",
    },
  };
}

/** Owns serialized settings mutations and atomic file replacement below one private directory. */
export class OperatorSettingsStore {
  private readonly configPath: string;
  private readonly secretPath: string;
  private settings: PersistedSettings | null = null;
  private secrets: PersistedSecrets | null = null;
  private mutation: Promise<void> = Promise.resolve();

  constructor(
    private readonly config: OperatorConfig,
    private readonly clock: () => number = Date.now,
  ) {
    this.configPath = join(config.configDirectory, "config.json");
    this.secretPath = join(config.configDirectory, "secrets.json");
  }

  /** Loads both settings files once; missing files are explicit first-run defaults. */
  async internal(): Promise<InternalSettings> {
    if (!this.settings) {
      try {
        this.settings = parsePersistedSettings(
          JSON.parse(await readFile(this.configPath, "utf8")) as unknown,
        );
      } catch {
        this.settings = structuredClone(defaultSettings);
      }
    }
    if (!this.secrets) {
      try {
        const parsed = JSON.parse(
          await readFile(this.secretPath, "utf8"),
        ) as Partial<PersistedSecrets>;
        this.secrets = {
          schemaVersion: "1",
          referenceProviderApiKey:
            typeof parsed.referenceProviderApiKey === "string"
              ? parsed.referenceProviderApiKey
              : null,
        };
      } catch {
        this.secrets = { schemaVersion: "1", referenceProviderApiKey: null };
      }
    }
    return {
      persisted: structuredClone(this.settings),
      referenceProviderApiKey: this.secrets.referenceProviderApiKey,
    };
  }

  /** Returns a redacted settings snapshot and exposes only secret presence. */
  async publicSnapshot(): Promise<SettingsResponse> {
    const internal = await this.internal();
    return {
      apiVersion: API_VERSION,
      schemaVersion: SCHEMA_VERSION,
      observedAt: new Date(this.clock()).toISOString(),
      operator: {
        bindAddress: this.config.host,
        bindMode: this.config.bindMode,
        remoteEnabled: this.config.remoteExposure,
        authenticationRequired: this.config.remoteExposure,
        eventRetention: this.config.eventRetention,
      },
      yvex: {
        binaryConfigured: internal.persisted.yvex.binaryPath !== null,
        binaryPathLabel: safePathLabel(internal.persisted.yvex.binaryPath),
        environmentCandidateConfigured: this.config.binaryEnvironmentCandidate !== null,
      },
      referenceProvider: {
        ...internal.persisted.referenceProvider,
        apiKeyConfigured: internal.referenceProviderApiKey !== null,
      },
      cache: { mutableTtlMs: this.config.mutableTtlMs, binaryTtlMs: this.config.binaryLookupTtlMs },
      safety: {
        shellEnabled: false,
        arbitraryArgvEnabled: false,
        remoteProvidersAllowed: this.config.allowRemoteProviders,
        providerSecretsReturned: false,
      },
      interface: internal.persisted.interface,
    };
  }

  /** Validates and persists one trusted absolute binary candidate; null clears only the persisted choice. */
  async updateYvex(patch: YvexSettingsPatch): Promise<SettingsResponse> {
    const value = yvexSettingsPatchSchema.parse(patch);
    const internal = await this.internal();
    internal.persisted.yvex.binaryPath =
      value.binaryPath === null ? null : validateTrustedBinaryPath(value.binaryPath);
    await this.persist(internal);
    return this.publicSnapshot();
  }

  /** Validates provider configuration, persists its secret separately, and never returns the secret value. */
  async updateReferenceProvider(patch: ReferenceProviderPatch): Promise<SettingsResponse> {
    const value = referenceProviderPatchSchema.parse(patch);
    const internal = await this.internal();
    internal.persisted.referenceProvider = { ...internal.persisted.referenceProvider, ...value };
    if ("apiKey" in value) internal.referenceProviderApiKey = value.apiKey?.trim() || null;
    delete (internal.persisted.referenceProvider as Record<string, unknown>).apiKey;
    await this.persist(internal);
    return this.publicSnapshot();
  }

  /** Persists presentation defaults only; browser-local sizing remains outside server truth. */
  async updateInterface(patch: InterfaceSettingsPatch): Promise<SettingsResponse> {
    const value = interfaceSettingsPatchSchema.parse(patch);
    const internal = await this.internal();
    internal.persisted.interface = { ...internal.persisted.interface, ...value };
    await this.persist(internal);
    return this.publicSnapshot();
  }

  /** Drops cached disk state so an explicit reload observes external trusted configuration changes. */
  reload(): void {
    this.settings = null;
    this.secrets = null;
  }

  /** Serializes one atomic non-secret and secret replacement with restrictive directory/file modes. */
  private async persist(internal: InternalSettings): Promise<void> {
    this.mutation = this.mutation.then(async () => {
      await mkdir(this.config.configDirectory, { recursive: true, mode: 0o700 });
      await chmod(this.config.configDirectory, 0o700);
      const stamp = `${process.pid}-${this.clock()}`;
      const configTemporary = `${this.configPath}.${stamp}.tmp`;
      const secretTemporary = `${this.secretPath}.${stamp}.tmp`;
      await writeFile(configTemporary, `${JSON.stringify(internal.persisted, null, 2)}\n`, {
        mode: 0o600,
      });
      await writeFile(
        secretTemporary,
        `${JSON.stringify({ schemaVersion: "1", referenceProviderApiKey: internal.referenceProviderApiKey }, null, 2)}\n`,
        { mode: 0o600 },
      );
      await rename(configTemporary, this.configPath);
      await rename(secretTemporary, this.secretPath);
      await chmod(this.configPath, 0o600);
      await chmod(this.secretPath, 0o600);
      this.settings = structuredClone(internal.persisted);
      this.secrets = {
        schemaVersion: "1",
        referenceProviderApiKey: internal.referenceProviderApiKey,
      };
    });
    await this.mutation;
  }
}
