#!/usr/bin/env python3
# Migrated from yailabs/yai-sdk 7a2a19b0 under MIT; see sdk/README.md.
"""Synthetic public-protocol peer; no YVEX engine/model/Case implementation."""
import json
from pathlib import Path
import sys
import time

identity_path, mode_path, audit_path, device, peer = sys.argv[1:]
request = json.loads(sys.stdin.readline())
with Path(audit_path).open("a") as audit:
    audit.write(request["request_id"] + "\n")
mode = Path(mode_path).read_text().strip()
if mode == "timeout":
    time.sleep(2)
if mode == "lost":
    raise SystemExit(1)
if mode == "refused":
    print(json.dumps({"schema":"yvex.finite.response.v1", "request_id":None,
        "status":"refused", "dispatch_state":"not_dispatched", "reason":"peer_revoked_or_authority_unavailable"}))
    raise SystemExit(0)
value = request["input"]
response = {"schema":"yvex.finite.response.v1", "request_id":request["request_id"],
    "operation":"finite.decision.execute", "model_alias":value["model_alias"],
    "device_identity":device, "authenticated_peer":peer, "status":"ok", "dispatch_state":"completed"}
if mode == "producer_error":
    response.update(status="error", dispatch_state="outcome_unavailable", reason="producer_error",
        error={"code":-8, "name":"YVEX_ERR_STATE", "owner":"DO_NOT_LOG_THIS_SYNTHETIC_CONTEXT"})
else:
    count = len(value["candidates"])
    result = dict(json.loads(Path(identity_path).read_text()), schema_version=1, score_kind="model-logit",
        engine_generation=value["expected_generation"], token_count=16, candidate_count=count,
        model_forward_count=1, sampling_invocation_count=0, generated_token_count=0, resident_backbone_count=1,
        elapsed_nanoseconds=1000000, source_mapped_bytes=128, parameter_execution_bytes=64,
        workspace_host_bytes=32, workspace_device_bytes=0, input_identity="3"*64,
        candidate_population_identity="4"*64, result_identity="5"*64, calibrated=False,
        candidates=[dict(id=c["id"], raw_score=float(i), relative_candidate_probability=1/count)
            for i,c in enumerate(value["candidates"])])
    if mode == "stale": result["engine_generation"] += 1
    if mode == "foreign_model": result["logical_model_identity"] = "6"*64
    if mode == "foreign_candidate": result["candidates"][0]["id"] = "foreign"
    if mode == "reordered": result["candidates"].reverse()
    if mode == "unsupported": result["score_kind"] = "confidence"
    if mode == "unknown_field": result["invented"] = True
    response["result"] = result
if mode == "wrong_correlation": response["request_id"] = "0"*64
if mode == "wrong_device": response["device_identity"] = "ssh-ed25519:sha256:" + "0"*64
if mode == "wrong_peer": response["authenticated_peer"] = "ssh-ed25519:sha256:" + "0"*64
if mode == "oversized":
    print("x"*40000)
else:
    print(json.dumps(response))
