#!/usr/bin/env python3
"""Read-only Linux memory witness; not runtime admission or a cleanup policy.

Samples may overlap and are not atomic. No model bytes, command lines,
environment values or credentials are read. Missing observations stay unknown.
The runtime's authenticated capacity calculation remains authoritative.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import time


def read(path, limit=64 * 1024 * 1024):
    with path.open() as stream:
        parts, size = [], 0
        while True:
            # sysctl procfs readers may allocate the requested read size.
            part = stream.read(min(4096, limit + 1 - size))
            if not part:
                return "".join(parts)
            size += len(part)
            if size > limit:
                raise ValueError("observation exceeds bound")
            parts.append(part)


def counters(value):
    """Linux reports kB in powers of 1024; unitless counters stay unitless."""
    result = {}
    for line in value.splitlines():
        if ":" not in line:
            continue
        name, payload = line.split(":", 1)
        fields = payload.split()
        if fields and fields[0].isdigit():
            if len(fields) == 1:
                result[name] = int(fields[0])
            elif fields[1:] == ["kB"]:
                result[name] = int(fields[0]) * 1024
    return result


def identity(value):
    end = value.rfind(")")
    fields = value[end + 2:].split()
    if end < 0 or len(fields) < 22:
        raise ValueError("invalid process stat")
    return dict(pid=int(value.split("(", 1)[0]), name=value[value.find("(") + 1:end],
                state=fields[0], parent_pid=int(fields[1]), start_ticks=int(fields[19]))


def mappings(value):
    """Group by device/inode/path; RSS/PSS are observations, never unique VRAM.

    Preserve anonymous/COW bytes within file mappings instead of attributing
    their complete RSS to file cache. Anonymous arenas are not allocation owners.
    """
    header = re.compile(r"^[0-9a-f]+-[0-9a-f]+\s+\S+\s+\S+\s+(\S+)\s+(\d+)(?:\s+(.*))?$")
    groups, current, values = {}, None, {}
    fields = ("Size", "Rss", "Pss", "Anonymous", "Private_Dirty", "Shared_Dirty", "Swap", "Locked")
    def finish():
        if current is not None:
            for key in fields:
                previous, value = current[key], values.get(key)
                current[key] = previous + value if previous is not None and value is not None else None
    for line in value.splitlines():
        match = header.match(line)
        if match:
            finish()
            values = {}
            device, inode, path = match.groups()
            key = (device, int(inode), path or "[anonymous]")
            current = groups.setdefault(key, dict(device=device, inode=int(inode),
                path=key[2], mapping_count=0, **{field: 0 for field in fields}))
            current["mapping_count"] += 1
        elif current is not None:
            for key, number in counters(line).items():
                if key in fields:
                    values[key] = number
    finish()
    return sorted(groups.values(), key=lambda row: row["Rss"] or 0, reverse=True)


def optional(path, parser=lambda value: value.strip()):
    try:
        return dict(value=parser(read(path)), error=None)
    except (OSError, ValueError) as error:
        return dict(value=None, error=type(error).__name__)


def process(root, pid, detail=False):
    directory = root / str(pid)
    before = optional(directory / "stat", identity)
    result = dict(pid=pid, identity=before, status=optional(directory / "status", counters),
                  rollup=optional(directory / "smaps_rollup", counters))
    if detail:
        result.update(cgroup=optional(directory / "cgroup"),
                      mappings=optional(directory / "smaps", mappings))
        try:
            result["executable"] = dict(value=str((directory / "exe").readlink()), error=None)
        except OSError as error:
            result["executable"] = dict(value=None, error=type(error).__name__)
    after = optional(directory / "stat", identity)
    first, last = before["value"], after["value"]
    result["stable_identity"] = bool(first and last and
        first["pid"] == last["pid"] and first["start_ticks"] == last["start_ticks"])
    if not result["stable_identity"]:
        # Do not let a reused PID's counters look like a valid observation.
        for key in ("status", "rollup", "mappings", "cgroup", "executable"):
            if key in result:
                result[key] = dict(value=None, error="process disappeared or identity changed")
    return result


def snapshot(detail_pids=(), root=Path("/proc")):
    start = time.monotonic_ns()
    boot = optional(root / "sys/kernel/random/boot_id")
    pids = sorted({int(path.name) for path in root.iterdir() if path.name.isdecimal()} | set(detail_pids))
    result = dict(schema="yvex.qualification.linux-memory-observation.v1",
        observed_unix_ns=time.time_ns(), boot=boot, meminfo=optional(root / "meminfo", counters),
        pressure=optional(root / "pressure/memory"),
        processes=[process(root, pid, pid in detail_pids) for pid in pids],
        observer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        scope="non-atomic procfs sample; unreadable processes explicit; no allocation-owner, pinning, unique physical total, admission or cleanup inference")
    result["same_boot_during_sample"] = bool(boot["value"] and boot == optional(root / "sys/kernel/random/boot_id"))
    result["observation_duration_ns"] = time.monotonic_ns() - start
    return result


def difference(before, after):
    """A changed boot witnesses a reboot, not its causal memory benefit."""
    key = "yvex.qualification.linux-memory-observation.v1"
    if before.get("schema") != key or after.get("schema") != key:
        raise ValueError("incompatible memory observations")
    old, new = before["boot"]["value"], after["boot"]["value"]
    a, b = before["meminfo"]["value"], after["meminfo"]["value"]
    if not old or not new or not a or not b or not all(
            row.get("same_boot_during_sample") for row in (before, after)):
        raise ValueError("boot/memory observation unavailable or unstable")
    if "MemAvailable" not in a or "MemAvailable" not in b:
        raise ValueError("MemAvailable unavailable")
    return dict(boot_changed=old != new,
                available_delta_bytes=b["MemAvailable"] - a["MemAvailable"],
                scope="sample delta only; compare service/build/model/workload identities separately; not reboot recovery or admission proof")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, action="append", default=[], help="include detailed mappings for this PID")
    parser.add_argument("--output", type=Path, required=True, help="new external raw-evidence JSON path")
    parser.add_argument("--compare-before", type=Path)
    args = parser.parse_args()
    if any(pid <= 0 for pid in args.pid):
        parser.error("PID must be positive")
    result = snapshot(args.pid)
    if args.compare_before:
        result["comparison"] = difference(json.loads(read(args.compare_before)), result)
    # Never overwrite a prior witness. This tool performs no process mutation.
    with args.output.open("x") as stream:
        json.dump(result, stream, indent=2, allow_nan=False)
        stream.write("\n")


if __name__ == "__main__":
    main()
