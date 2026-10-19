#!/usr/bin/env python3
# Migrated from yailabs/yai-sdk 7a2a19b0 under MIT; see sdk/README.md.
"""Synthetic public management contract peer; never runs YVEX or a model."""
import json
from pathlib import Path
import sys
import time
root, device, peer = sys.argv[1:]
root = Path(root)
request = json.loads(sys.stdin.readline())
with (root/'audit').open('a') as log: log.write(json.dumps({'operation':request['operation'],'request_id':request['request_id']})+'\n')
mode = (root/'mode').read_text().strip()
if mode=='timeout': time.sleep(2)
response = dict(schema='yvex.management.response.v2', request_id=request['request_id'],device_identity=device,authenticated_peer=peer,status='ok')
if request['operation']=='job.get':
    path=root/request['input']['job_id']
    if path.exists(): response['data']=json.loads(path.read_text())
    else: response.update(status='refused',reason='job_not_found')
else:
    result = dict(host_instance='c'*64, model='synthetic', generation=1, session='test', channels=[{'channel':'final_text','text':'Synthetic public output'}], turn_identity=None, complete=True) if request['operation']=='generation.start' else None
    job=dict(job_id=request['request_id'],operation=request['operation'],state='succeeded' if result else 'running',submitted_at_unix_ms=1,updated_at_unix_ms=2,input=request['input'],result=result,reason=None)
    (root/request['request_id']).write_text(json.dumps(job))
    response['data']=job
if mode=='lost': raise SystemExit(1)
if mode in ('refused','unsupported','unavailable'):
    response.pop('data',None);response.update(status=mode,reason='synthetic_owner_reason')
if mode=='wrong_correlation': response['request_id']='0'*64
if mode=='wrong_device': response['device_identity']='ssh-ed25519:sha256:'+'0'*64
if mode=='wrong_peer': response['authenticated_peer']='ssh-ed25519:sha256:'+'0'*64
if mode=='wrong_job': response['data']['job_id']='0'*64
if mode=='oversized': print('x'*(1024*1024+1))
else: print(json.dumps(response))
