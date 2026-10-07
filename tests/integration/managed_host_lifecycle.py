#!/usr/bin/env python3
"""Disposable HTTPS + native zero-engine Host service control; no operator mutation."""
import hashlib
import json
import os
from pathlib import Path
import secrets
import socket
import ssl
import subprocess
import tempfile
import time

BINARY = Path(os.environ.get('YVEX_BIN', 'yvex')).resolve()


def main():
    with tempfile.TemporaryDirectory(prefix='yvex-net-') as directory:
        root = Path(directory)
        root.chmod(0o700)
        for name in ['config', 'data', 'models', 'runtime', 'xdg', 'home']:
            (root / name).mkdir(mode=0o700)
        env = {**os.environ, 'HOME': str(root / 'home'),
               'YVEX_CONFIG_DIR': str(root / 'config'), 'YVEX_DATA_DIR': str(root / 'data'),
               'YVEX_MODELS_ROOT': str(root / 'models'),
               'YVEX_MODELS_REGISTRY': str(root / 'config/models.local.json'),
               'XDG_RUNTIME_DIR': str(root / 'runtime'), 'XDG_DATA_HOME': str(root / 'xdg')}
        for key in ['HF_TOKEN', 'HUGGING_FACE_HUB_TOKEN', 'GH_TOKEN', 'GITHUB_TOKEN']:
            env.pop(key, None)
        state = root / 'service'
        process = None
        host_process = None
        certificate = None
        server = None
        service_socket = None

        def command(action, *args, expected=0):
            result = subprocess.run([str(BINARY), 'management', action, *args,
                                     '--state-dir', str(state)], env=env,
                                    text=True, capture_output=True, timeout=20)
            assert result.returncode == expected, (action, result.returncode, result.stderr)
            return json.loads(result.stdout) if expected == 0 else result

        def start():
            nonlocal process, certificate, server, service_socket
            process = subprocess.Popen([str(BINARY), 'management', 'serve', '--bind',
                                        '127.0.0.1:0', '--name', 'Generated fixture YVEX',
                                        '--state-dir', str(state)], env=env,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            line = process.stdout.readline()
            assert line, process.stderr.read()
            info = json.loads(line)
            server = ('127.0.0.1', int(info['address'].rsplit(':', 1)[1]))
            service_socket = info['local_socket']
            certificate = info['device_identity']
            assert info['discovery_advertised'] is False
            assert Path(service_socket).stat().st_mode & 0o777 == 0o600
            return info

        def stop():
            nonlocal process
            if process is not None:
                process.terminate()
                process.wait(timeout=15)
                assert not process.stderr.read(), 'No request or credential diagnostics permitted'
                process = None

        def connect(local=False):
            if local:
                client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                client.settimeout(35)
                client.connect(service_socket)
                return client
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
            client = context.wrap_socket(socket.create_connection(server, 5), server_hostname='localhost')
            client.settimeout(35)
            # Fixture oracle explicitly pins before any bearer/request is sent.
            observed = 'tls-sha256:' + hashlib.sha256(client.getpeercert(binary_form=True)).hexdigest()
            assert observed == certificate
            return client

        def exchange(method, path, data=None, credential=None, local=False, extra=''):
            body = json.dumps(data).encode() if data is not None else b''
            auth = f'Authorization: Bearer {credential}\r\n' if credential else ''
            media = 'Content-Type: application/json\r\n' if method == 'POST' else ''
            if local and path == '/v1/management':
                media += f'X-Yvex-Device-Identity: {certificate}\r\n'
            request = (f'{method} {path} HTTP/1.1\r\nHost: localhost\r\n{auth}{media}'
                       f'Content-Length: {len(body)}\r\n{extra}\r\n').encode() + body
            with connect(local) as client:
                client.sendall(request)
                chunks = []
                while True:
                    value = client.recv(8192)
                    if not value:
                        break
                    chunks.append(value)
            header, result = b''.join(chunks).split(b'\r\n\r\n', 1)
            return int(header.split(b' ', 2)[1]), json.loads(result)

        def operation(name, data=None, credential=None, local=False, identity=None):
            request = {'schema': 'yvex.management.request.v2',
                       'request_id': identity or secrets.token_hex(32),
                       'operation': name, 'input': data or {}}
            return exchange('POST', '/v1/management', request, credential, local)

        token = secrets.token_hex(32)
        digest = hashlib.sha256(bytes.fromhex(token)).hexdigest()
        def value(op, data=None, identity=None):
            status, result = operation(op, data, credential=token, identity=identity)
            assert status == 200 and result['status'] == 'ok', (op,result)
            return result['data']
        def terminal(job):
            deadline=time.monotonic()+35
            while time.monotonic()<deadline:
                current=value('job.get',{'job_id':job})
                if current['state'] not in ['accepted','running']:return current
                time.sleep(.04)
            raise AssertionError(('job did not settle',current))
        def mutate(op, data, success=True, identity=None):
            result=value(op,data,identity)
            result=terminal(result['job_id'])
            assert (result['state']=='succeeded')==success,(op,result)
            return result
        def control():return value('host.control')
        try:
            start()
            command('pairing-open')
            exchange('POST','/v1/pairing',{'schema':'yvex.management.pairing.request.v1','credential_hash':digest,'client_name':'Disposable lifecycle client'})
            command('pairing-approve',digest)
            before=control()
            assert before['ownership']=='unconfigured' and before['service_control_granted'] is False
            denied=operation('host.start',{'expected_revision':1},credential=token)[1]
            assert denied['status']=='refused' and 'service_control_grant_required' in denied['reason']
            with socket.socket() as reservation:
                reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
            command('host-configure',str(port))
            assert control()['configured'] and not control()['can_start']
            invite=root/'invitation.json'
            command('owner-invite','--endpoint',f'https://127.0.0.1:{server[1]}','--output',str(invite))
            bundle=json.loads(invite.read_text())
            owner_token=secrets.token_hex(32)
            owner_hash=hashlib.sha256(bytes.fromhex(owner_token)).hexdigest()
            status,claimed=exchange('POST','/v1/owner/bootstrap',{'schema':'yvex.management.owner.bootstrap.v1','invitation_secret':bundle['invitation_secret'],'credential_hash':owner_hash,'client_name':'Disposable server owner'})
            assert status==200 and claimed['posture']=='approved'
            def grant(kind):
                _,owner=exchange('GET','/v1/owner/connections',credential=owner_token)
                request={'schema':'yvex.management.owner.action.v1','request_id':secrets.token_hex(32),'expected_revision':owner['revision'],'action':kind,'target_ref':digest}
                status,result=exchange('POST','/v1/owner/actions',request,owner_token)
                assert status==200 and result['posture']=='applied',result
                assert exchange('POST','/v1/owner/actions',request,owner_token)[1]==result
            grant('service_control_grant')
            first_control=control();assert first_control['can_start'] and first_control['ownership']=='managed'
            # Exact identity recovers an intentionally discarded HTTP response without redispatch.
            action_id=secrets.token_hex(32)
            request={'schema':'yvex.management.request.v2','request_id':action_id,'operation':'host.start','input':{'expected_revision':first_control['revision']}}
            body=json.dumps(request).encode()
            with connect() as client:
                client.sendall((f'POST /v1/management HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer {token}\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\n\r\n').encode()+body)
                # Close after owner acceptance headers, before reading receipt body.
                client.recv(1)
            first=terminal(action_id)
            assert first['state']=='succeeded',first
            current=control();instance=current['host_instance']
            assert current['state']=='running' and current['can_stop'] and len(instance)==64,current
            assert first['result']['host_instance']==instance and first['result']['models_restored'] is False
            assert value('engine.list')['engines']==[]
            assert value('host.get')['status']['openai_ready'] is True
            assert value('host.start',request['input'],action_id)['job_id']==action_id
            assert control()['host_instance']==instance
            assert value('management.capabilities')['protocol']=='yvex.management.v2'
            # Stale control revision never dispatches stop.
            stale=mutate('host.stop',{'expected_revision':first_control['revision'],'host_instance':instance,'acknowledge_disruption':True},False)
            assert 'stale_host_control_revision' in stale['reason'] and control()['host_instance']==instance
            current=control()
            wrong=mutate('host.stop',{'expected_revision':current['revision'],'host_instance':'f'*64,'acknowledge_disruption':True},False)
            assert 'stale_host_instance' in wrong['reason'] and control()['host_instance']==instance
            current=control()
            restarted=mutate('host.restart',{'expected_revision':current['revision'],'host_instance':instance,'acknowledge_disruption':True})
            current=control();second=current['host_instance']
            assert second!=instance and restarted['result']['previous_host_instance']==instance
            assert value('engine.list')['engines']==[] and value('host.get')['status']['session_count']==0
            # Management restart leaves the managed native Host and exact receipt available.
            stop();start()
            assert control()['host_instance']==second
            assert value('job.get',{'job_id':restarted['job_id']})['state']=='succeeded'
            grant('service_control_revoke')
            assert not control()['can_stop']
            assert operation('host.stop',{'expected_revision':current['revision'],'host_instance':second,'acknowledge_disruption':True},credential=token)[1]['status']=='refused'
            grant('service_control_grant')
            current=control();mutate('host.stop',{'expected_revision':current['revision'],'host_instance':second,'acknowledge_disruption':True})
            assert control()['state']=='stopped' and control()['can_start']
            assert exchange('GET','/v1/identity')[0]==200
            # Occupied inference port produces an explicit failed start, never fake readiness.
            with socket.socket() as blocker:
                blocker.bind(('127.0.0.1',port));blocker.listen()
                failed=mutate('host.start',{'expected_revision':control()['revision']},False)
                assert failed['state']=='failed' and control()['state']!='running',failed
            # A separately started operator Host cannot be adopted, stopped or reconfigured implicitly.
            host_process=subprocess.Popen([str(BINARY),'serve','--openai','off','--logs','off'],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,text=True)
            until=time.monotonic()+10
            while time.monotonic()<until and control()['state']!='running':time.sleep(.04)
            external=control();assert external['ownership']=='external' and not external['can_stop'],external
            command('host-configure',str(port),expected=2)
            refused=mutate('host.stop',{'expected_revision':external['revision'],'host_instance':external['host_instance'],'acknowledge_disruption':True},False)
            assert 'externally_managed_host' in refused['reason'] and host_process.poll() is None
            subprocess.run([str(BINARY),'host','stop'],env=env,capture_output=True,check=True,timeout=10)
            host_process.wait(timeout=10);host_process=None
            print(json.dumps({'result':'PASS','scope':'disposable HTTPS + actual zero-engine native Host','controls':['explicit grant default deny','configured stopped start','discarded response exact recovery','duplicate request no second Host','stale revision','stale Host','restart new instance','no model/session restore','management restart preserves Host','grant revocation','settled stop leaves management alive','occupied port fails honestly','external Host protected'],'operator_state_touched':False}))
        finally:
            if process is not None:
                try:
                    status,body=operation('host.control',local=True)
                    c=body.get('data',{})
                    if c.get('can_stop'):
                        operation('host.stop',{'expected_revision':c['revision'],'host_instance':c['host_instance'],'acknowledge_disruption':True},local=True)
                        time.sleep(.4)
                except Exception:pass
            if host_process is not None:
                host_process.terminate();host_process.wait(timeout=10)
            stop()


if __name__=='__main__':main()
