#!/usr/bin/env python3
# Migrated from yailabs/yai-sdk 7a2a19b0 under MIT; see sdk/README.md.
"""SDK standalone contract controls; producer checkout remains optional."""
import copy
import json
from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[2]
SDK = ROOT / "sdk/rust"

sys.path.insert(0,str(ROOT/'sdk/tools'))
from check_parity import compare
sys.path.insert(0,str(ROOT/'tests/sdk'))
from test_finite_remote_contract import FiniteRemoteContract

class ManagementContract(unittest.TestCase):
    def setUp(self):
        legacy=FiniteRemoteContract();legacy.setUp();self.producer=legacy.producer;self.client=legacy.client
        self.contract=json.loads((SDK / 'contract/management.json').read_text())
        operations=[{k:r[k] for k in ('operation','kind')} for r in self.contract['operations']]
        self.producer['catalogs']['remote_product_management_operations']=operations
        self.client['product_management']={'request_schema':'yvex.management.request.v2','response_schema':'yvex.management.response.v2','grant':'product-management','operations':copy.deepcopy(operations),'automatic_retry':False,'mutation_recovery':'job.get','runtime_qualified':False}
    def test_exact_product_catalog_and_separate_v1_finite(self):
        compare(self.producer,self.client)
        for field in ('remote_management_operations','remote_product_management_operations'):
            broken=copy.deepcopy(self.producer);broken['catalogs'][field].pop()
            with self.assertRaises(AssertionError):compare(broken,self.client)
    def test_grant_kind_recovery_and_runtime_promotion_refuse(self):
        for field,value in [('grant','management'),('automatic_retry',True),('mutation_recovery','resubmit'),('runtime_qualified',True)]:
            broken=copy.deepcopy(self.client);broken['product_management'][field]=value
            with self.assertRaises(AssertionError):compare(self.producer,broken)
        broken=copy.deepcopy(self.client);broken['product_management']['operations'][0]['kind']='job'
        with self.assertRaises(AssertionError):compare(self.producer,broken)
    def test_mutation_profiles_have_identity_fences(self):
        types=self.contract['types']
        for row in self.contract['operations']:
            if row['kind']=='job' and row['operation'].split('.')[0] in {'engine','session','generation'}:
                self.assertEqual(types[row['input']]['host_instance'],'string')
                if row['operation'] not in {'engine.load','engine.unload','session.create'}:
                    self.assertEqual(types[row['input']]['session_identity'],'string')
        for name in ('AcquisitionResumeInput','AcquisitionCancelInput'):
            self.assertEqual(types[name]['expected_operation_id'],'string')
            self.assertEqual(types[name]['expected_generation'],'u64')
    def test_no_fake_training_operations_or_secret_fields(self):
        self.assertFalse(any(row['operation'].startswith('training.') for row in self.contract['operations']))
        for row in self.contract['operations']:
            self.assertFalse({'token','password','secret'} & self.contract['types'][row['input']].keys())
        self.assertNotIn('input',self.contract['types']['JobSummary'])
        self.assertNotIn('result',self.contract['types']['JobSummary'])

if __name__=='__main__':unittest.main()
