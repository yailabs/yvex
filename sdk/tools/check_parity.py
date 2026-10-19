#!/usr/bin/env python3
# Migrated from yailabs/yai-sdk 7a2a19b0 under MIT; see sdk/README.md.
"""Compose YVEX's canonical remote operation inventory with its SDK projection."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[2]
SDK = ROOT / "sdk/rust"



def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def compare(producer: dict, client: dict) -> None:
    require(producer.get("schema") == "yvex.operator.registry.v1", "YVEX registry schema changed")
    require(client.get("schema") == "platform.sdk.yvex.management.v1", "YVEX SDK projection schema changed")
    require(client.get("producer_request_schema") == "yvex.management.request.v1", "request schema drift")
    require(client.get("producer_response_schema") == "yvex.management.response.v1", "response schema drift")
    require(client.get("posture") == "contract_supported_runtime_not_implied", "evidence posture drift")
    require(client.get("openai_capacity_profile") == "yvex.openai.compat.v3", "YVEX OpenAI profile drift")
    owned = producer["catalogs"]["remote_management_operations"]
    projected = client["operations"]
    require(isinstance(owned, list) and len(owned) == len(set(owned)), "producer operation inventory invalid")
    require(isinstance(projected, list) and len(projected) == len(set(projected)), "SDK operation inventory invalid")
    require(sorted(owned) == sorted(projected), f"YVEX SDK drift: producer={sorted(owned)} SDK={sorted(projected)}")
    product_owned = producer["catalogs"].get("remote_product_management_operations")
    product_client = client.get("product_management")
    if product_owned is not None or product_client is not None:
        require(isinstance(product_owned, list) and isinstance(product_client, dict), "product management publication missing")
        require(product_client.get("request_schema") == "yvex.management.request.v2" and
                product_client.get("response_schema") == "yvex.management.response.v2", "product management schema drift")
        require(product_client.get("grant") == "product-management", "product management grant drift")
        require(product_client.get("automatic_retry") is False and product_client.get("mutation_recovery") == "job.get",
                "product management recovery drift")
        require(product_client.get("runtime_qualified") is False, "SDK cannot qualify deployed product management")
        product_projected = product_client.get("operations", [])
        require(len({row["operation"] for row in product_projected}) == len(product_projected), "duplicate product operation")
        require(sorted(product_owned, key=lambda row: row["operation"]) == sorted(product_projected, key=lambda row: row["operation"]), "product management operation/kind drift")
        require(all(row.get("kind") in {"read", "job", "control"} for row in product_projected), "invalid product operation kind")
    # Finite computation is a distinct protocol/grant, not another management
    # v1 read. Its forced entry and wire schemas remain producer-owned.
    remote = client["finite_decision"]["remote"]
    finite = [row for row in producer["operations"] if row.get("operation_id") == "finite.remote.protocol"]
    require(len(finite) == 1, "remote finite producer entry missing/duplicated")
    require(finite[0].get("input_schema") == remote.get("request_schema"), "finite request schema drift")
    require(finite[0].get("result_schema") == remote.get("response_schema"), "finite response schema drift")
    require(remote.get("operation") == "finite.decision.execute", "finite execution operation drift")
    require(remote.get("transport") == "restricted_ssh" and remote.get("automatic_retry") is False,
            "finite trust/replay posture drift")
    require(remote.get("runtime_qualified") is False, "SDK type presence cannot qualify runtime")


def compare_finite_schemas(producer_root: pathlib.Path) -> None:
    contract = json.loads((SDK / "contract/finite-remote.json").read_text())
    for side in ("request", "response"):
        data = (producer_root / contract[f"{side}_schema_path"]).read_bytes()
        require(hashlib.sha256(data).hexdigest() == contract[f"{side}_schema_sha256"],
                f"finite {side} layout drift requires reviewed client disposition")
        require(json.loads(data)["$id"] == contract[f"{side}_schema"], f"finite {side} identity drift")


def compare_product_schemas(producer_root: pathlib.Path) -> None:
    contract = json.loads((SDK / "contract/management.json").read_text())
    for side in ("request", "response"):
        record = contract["producer_schemas"]
        data = (producer_root / record[f"{side}_schema_path"]).read_bytes()
        require(hashlib.sha256(data).hexdigest() == record[f"{side}_schema_sha256"],
                f"product management {side} layout drift requires reviewed client disposition")
        require(json.loads(data)["$id"] == contract[f"{side}_schema"], f"product management {side} identity drift")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--yvex-root", type=pathlib.Path, default=ROOT)
    args = parser.parse_args()
    producer = json.loads((args.yvex_root / "config/operator/registry.json").read_text(encoding="utf-8"))
    result = subprocess.run(
        ["cargo", "run", "--locked", "--quiet", "-p", "yvex-sdk@0.2.0", "--example", "capabilities"],
        cwd=SDK, capture_output=True, text=True, check=False,
    )
    require(result.returncode == 0, f"YVEX SDK manifest failed: {result.stderr}")
    client = json.loads(result.stdout)
    compare(producer, client)
    compare_finite_schemas(args.yvex_root)
    compare_product_schemas(args.yvex_root)
    network = json.loads((SDK / "contract/network.json").read_text())
    for name, record in network["producer_schemas"].items():
        require(hashlib.sha256((args.yvex_root / record["path"]).read_bytes()).hexdigest() == record["sha256"], f"network {name} schema drift")
    negative = json.loads(json.dumps(producer))
    negative["catalogs"]["remote_management_operations"].append("model.load")
    try:
        compare(negative, client)
    except AssertionError:
        pass
    else:
        raise AssertionError("controlled producer delta did not fail parity")
    finite_negative = json.loads(json.dumps(producer))
    for row in finite_negative["operations"]:
        if row.get("operation_id") == "finite.remote.protocol":
            row["result_schema"] = "yvex.finite.response.unsupported"
    try:
        compare(finite_negative, client)
    except AssertionError:
        pass
    else:
        raise AssertionError("controlled finite schema delta did not fail parity")
    print(f"PASS YVEX→SDK parity: {len(client['operations'])} management v1 reads, product management and separate finite computation, exact schemas; management/finite deltas refused")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, OSError, ValueError) as error:
        print(f"FAIL YVEX→SDK parity: {error}", file=sys.stderr)
        raise SystemExit(1) from error
