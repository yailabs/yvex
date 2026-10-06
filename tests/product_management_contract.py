#!/usr/bin/env python3
"""Every public CLI operation has an explicit product disposition; mappings resolve."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
registry = json.loads((root / 'config/operator/registry.json').read_text())
classification = json.loads((root / 'docs/contracts/product-management-operation-map.json').read_text())
rows = classification['operations']
expected = {entry['operation_id'] for entry in registry['operations']}
actual = {entry['operation'] for entry in rows}
assert len(actual) == len(rows), 'Duplicate CLI dispositions'
assert actual == expected, {'unclassified': sorted(expected-actual), 'removed': sorted(actual-expected)}
public = {entry['operation'] for entry in registry['catalogs']['remote_product_management_operations']}
allowed = {'ordinary_product', 'advanced_engineering', 'cli_interaction', 'unsupported_remote'}
for entry in rows:
    assert entry['class'] in allowed and entry['reason'].strip(), entry
    if entry['class'] == 'ordinary_product':
        assert entry['public_operation'] in public | {'existing inference HTTP contract'}, entry
    else:
        assert entry['public_operation'] is None, entry
assert len(public) == len(registry['catalogs']['remote_product_management_operations'])
assert all(entry['kind'] in {'read', 'job'} for entry in registry['catalogs']['remote_product_management_operations'])
print(f'Product management classification PASS: {len(rows)} CLI operations, {len(public)} public management operations')
