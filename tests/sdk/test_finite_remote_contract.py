#!/usr/bin/env python3
# Migrated from yailabs/yai-sdk 7a2a19b0 under MIT; see sdk/README.md.
"""Standalone producer-delta controls; no private repository or live runtime."""
import copy
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
SDK = ROOT / "sdk/rust"

sys.path.insert(0, str(ROOT/"sdk/tools"))
from check_parity import compare

class FiniteRemoteContract(unittest.TestCase):
    def setUp(self):
        contract = json.loads((SDK / "contract/finite-remote.json").read_text())
        self.producer = {"schema":"yvex.operator.registry.v1",
            "catalogs":{"remote_management_operations":["device.describe","host.status"]},
            "operations":[{"operation_id":contract["producer_entry_operation"],
                "input_schema":contract["request_schema"], "result_schema":contract["response_schema"]}]}
        self.client = {"schema":"platform.sdk.yvex.management.v1", "producer_request_schema":"yvex.management.request.v1",
            "producer_response_schema":"yvex.management.response.v1", "operations":["device.describe","host.status"],
            "openai_capacity_profile":"yvex.openai.compat.v3", "posture":"contract_supported_runtime_not_implied",
            "finite_decision":{"remote":{"request_schema":contract["request_schema"], "response_schema":contract["response_schema"],
                "operation":contract["producer_operation"], "transport":"restricted_ssh", "automatic_retry":False, "runtime_qualified":False}}}
    def test_separate_protocol_not_management_promotion(self):
        compare(self.producer, self.client)
        bad = copy.deepcopy(self.producer)
        bad["catalogs"]["remote_management_operations"].append("finite.decision.execute")
        with self.assertRaises(AssertionError): compare(bad, self.client)
    def test_missing_or_changed_producer_refuses(self):
        for field in ["input_schema", "result_schema", "operation_id"]:
            bad = copy.deepcopy(self.producer); bad["operations"][0][field] = "unsupported"
            with self.assertRaises(AssertionError): compare(bad, self.client)
    def test_client_cannot_claim_runtime_from_contract(self):
        for field,value in [("runtime_qualified",True), ("automatic_retry",True), ("operation","chat.completion")]:
            bad = copy.deepcopy(self.client); bad["finite_decision"]["remote"][field] = value
            with self.assertRaises(AssertionError): compare(self.producer, bad)

if __name__ == "__main__": unittest.main()
