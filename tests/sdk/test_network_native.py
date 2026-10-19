#!/usr/bin/env python3
# Migrated from yailabs/yai-sdk 7a2a19b0 under MIT; see sdk/README.md.
"""Real producer TLS/UDS + SDK CLI + native Secret Service, disposable profiles.
No operator service, credential enumeration, Case or commercial account is used.
"""
import hashlib,json,os,select,subprocess,tempfile,time
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
SDK = ROOT / "sdk/rust"
SDK_EXAMPLE=SDK/'target/debug/examples/connections'
PRODUCER=Path(os.environ.get('YVEX_NETWORK_TEST_BINARY',str(ROOT/'yvex'))).resolve()

def main():
    if os.environ.get('YVEX_SDK_TEST_NATIVE_CREDENTIALS')!='1':
        raise SystemExit('BLOCKED: set YVEX_SDK_TEST_NATIVE_CREDENTIALS=1 for isolated native vault test')
    with tempfile.TemporaryDirectory(prefix='sdk-network-native-') as tmp:
        root=Path(tmp);root.chmod(0o700)
        for name in ['config','data','models','runtime','xdg']:(root/name).mkdir(mode=0o700)
        env={**os.environ,'YVEX_CONFIG_DIR':str(root/'config'),'YVEX_DATA_DIR':str(root/'data'),'YVEX_MODELS_ROOT':str(root/'models'),'YVEX_MODELS_REGISTRY':str(root/'config/models.local.json'),'XDG_RUNTIME_DIR':str(root/'runtime'),'XDG_DATA_HOME':str(root/'xdg'),'YVEX_SDK_CONNECTION_PROFILE_ROOT':str(root/'sdk-profiles')}
        # The current desktop bus is used only for uniquely generated test keys.
        # No credential identifiers are enumerated and no real account is read.
        for key in ['HF_TOKEN','HUGGING_FACE_HUB_TOKEN','GH_TOKEN','GITHUB_TOKEN']:env.pop(key,None)
        profiles=[];process=None
        def sdk(*args,expected=0):
            r=subprocess.run([str(SDK_EXAMPLE),*args],env=env,text=True,capture_output=True,timeout=25)
            if r.returncode!=expected:
                assert 'credential_store_unavailable' not in r.stdout,'BLOCKED: native protected credential store unavailable; no plaintext fallback'
                raise AssertionError((args[0],r.returncode,r.stdout,r.stderr))
            return json.loads(r.stdout)
        def owner(action,*args):
            r=subprocess.run([str(PRODUCER),'management',action,*args,'--state-dir',str(root/'service')],env=env,text=True,capture_output=True,timeout=20)
            assert r.returncode==0,(action,r.stderr);return json.loads(r.stdout)
        def request(operation,input):
            value={'schema':'yvex.management.request.v2','request_id':os.urandom(32).hex(),'operation':operation,'input':input}
            path=root/'request.json';path.write_text(json.dumps(value));return path,value
        try:
            process=subprocess.Popen([str(PRODUCER),'management','serve','--bind','127.0.0.1:0','--name','SDK isolated fixture','--state-dir',str(root/'service')],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            assert select.select([process.stdout],[],[],10)[0],'service startup timeout'
            line=process.stdout.readline();assert line,process.stderr.read();service=json.loads(line)
            endpoint='https://'+service['address']
            candidate=sdk('probe',endpoint);assert candidate['identity']['device_identity']==service['device_identity']
            candidate_path=root/'candidate.json';candidate_path.write_text(json.dumps(candidate))
            profile=sdk('trust',str(candidate_path),'SDK isolated test','--fingerprint-verified');profiles.append(profile['profile_ref'])
            assert profile['pairing_posture']=='unpaired'
            assert 'credential' not in json.dumps(profile).replace('credential-sha256','public-client-hash')
            owner('pairing-open')
            pending=sdk('request',profile['profile_ref']);assert pending['posture']=='pending'
            owner('pairing-approve',pending['request_id'])
            assert sdk('status',profile['profile_ref'])['posture']=='approved'
            # Every SDK call is a fresh process, proving native secret restoration.
            path,value=request('management.capabilities',{})
            observation=sdk('invoke',profile['profile_ref'],str(path));assert len(observation['value']['operations'])==len(json.loads((SDK/'contract/management.json').read_text())['operations'])
            assert observation['device_identity']==profile['device_identity'] and observation['authenticated_peer']==profile['peer_identity']
            assert sdk('get',profile['profile_ref'])['pairing_posture']=='approved'
            local=sdk('local');profiles.append(local['profile_ref']);assert local['transport']=='local' and local['pairing_posture']=='approved'
            observed=sdk('invoke',local['profile_ref'],str(path));assert observed['authenticated_peer'].startswith('local-user:') and len(observed['value']['operations'])==len(json.loads((SDK/'contract/management.json').read_text())['operations'])
            path,job_request=request('build.start',{'model':'missing-sdk-fixture-model'})
            job=sdk('invoke',profile['profile_ref'],str(path))['value'];assert job['job_id']==job_request['request_id']
            path,_=request('job.get',{'job_id':job_request['request_id']})
            recovered=sdk('invoke',profile['profile_ref'],str(path))['value'];assert recovered['job_id']==job_request['request_id']
            assert sdk('request',profile['profile_ref'])['posture']=='approved','repeat request must observe only'
            owner('pairing-revoke',pending['request_id']);assert sdk('status',profile['profile_ref'])['posture']=='revoked'
            refused=sdk('invoke',profile['profile_ref'],str(path),expected=2);assert refused['code']=='management_pairing_required'
            renewed=sdk('new-request',profile['profile_ref']);profiles.append(renewed['profile_ref']);assert renewed['profile_ref']!=profile['profile_ref'] and renewed['peer_identity']!=profile['peer_identity'] and renewed['pairing_posture']=='unpaired'
            process.terminate();process.wait(timeout=15);process=None
            unavailable=sdk('status',profile['profile_ref'],expected=2);assert unavailable['code']=='transport_unavailable'
            assert sdk('get',profile['profile_ref'])['pairing_posture']=='revoked'
            print('PASS: real disposable producer TLS + SDK native vault cross-process restore; current operation catalog; same-user UDS; exact receipt; revoke != outage; explicit new pairing identity; no operator service/Case')
        finally:
            for profile_ref in profiles:
                sdk('forget',profile_ref)
            if process is not None:process.terminate();process.wait(timeout=15)
if __name__=='__main__':main()
