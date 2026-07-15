/*
 * Owner: apps/operator dependency reproducibility guard.
 * Owns: package/lock root dependency equality and exact-version enforcement.
 * Does not own: dependency installation, vulnerability policy, source behavior, or network access.
 * Invariants: every declared package has one exact lock-root declaration.
 * Boundary: a valid lock is build reproducibility evidence, not dependency safety proof.
 */
import { readFile } from "node:fs/promises";

const packageJson = JSON.parse(await readFile(new URL("../package.json", import.meta.url), "utf8"));
const lock = JSON.parse(await readFile(new URL("../package-lock.json", import.meta.url), "utf8"));
const root = lock.packages?.[""];

if (lock.lockfileVersion !== 3 || !root) {
  throw new Error("package-lock.json must use lockfileVersion 3 and include a root package record");
}

for (const section of ["dependencies", "devDependencies"]) {
  const declared = packageJson[section] ?? {};
  const locked = root[section] ?? {};
  if (JSON.stringify(declared) !== JSON.stringify(locked)) {
    throw new Error(`${section} differs between package.json and package-lock.json`);
  }
  for (const [name, version] of Object.entries(declared)) {
    if (typeof version !== "string" || !/^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$/.test(version)) {
      throw new Error(`${section}.${name} must use an exact semantic version`);
    }
  }
}

for (const [name, version] of Object.entries(packageJson.overrides ?? {})) {
  if (typeof version !== "string" || lock.packages?.[`node_modules/${name}`]?.version !== version) {
    throw new Error(`override ${name} is not resolved exactly in package-lock.json`);
  }
}

process.stdout.write("operator dependency lock: verified\n");
