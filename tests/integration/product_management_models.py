#!/usr/bin/env python3
"""Controlled local forced-protocol/domain evidence; NOT an SSH or live HF claim."""
import hashlib
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile
import time

binary = Path(os.environ.get('YVEX_BIN', 'yvex')).resolve()
repo = Path(__file__).resolve().parents[2]
sequence = 0


def main():
    global sequence
    with tempfile.TemporaryDirectory(prefix='yvex-management-models-') as directory:
        root = Path(directory)
        root.chmod(0o700)
        for name in ['config', 'data', 'models', 'runtime', 'xdg']:
            (root / name).mkdir(mode=0o700)
        env = {**os.environ, 'YVEX_CONFIG_DIR': str(root / 'config'),
               'YVEX_DATA_DIR': str(root / 'data'), 'YVEX_MODELS_ROOT': str(root / 'models'),
               'YVEX_MODELS_REGISTRY': str(root / 'config/models.local.json'),
               'XDG_RUNTIME_DIR': str(root / 'runtime'), 'XDG_DATA_HOME': str(root / 'xdg'),
               'YVEX_HF_CLI': str(repo / 'tests/fixtures/bin/fake-hf'),
               'YVEX_GH_CLI': str(repo / 'tests/fixtures/bin/fake-gh'),
               'YVEX_FAKE_GH_STATE': str(root / 'github.auth'),
               'YVEX_FAKE_HF_LOG': str(root / 'provider.log'),
               'YVEX_FAKE_HF_STATE': str(root / 'provider.auth'),
               'YVEX_FAKE_HF_AUTH': '1', 'YVEX_FAKE_HF_STEP_DELAY': '1',
               'YVEX_FAKE_HF_STEPS': '6', 'NO_COLOR': '1'}
        for secret in ['HF_TOKEN', 'HUGGING_FACE_HUB_TOKEN', 'GH_TOKEN', 'GITHUB_TOKEN']:
            env.pop(secret, None)
        env.pop('SSH_TTY', None)
        env.pop('SSH_ORIGINAL_COMMAND', None)

        def run(*args, **kwargs):
            result = subprocess.run([str(binary), *map(str, args)], env=env,
                                    text=True, capture_output=True, timeout=30, **kwargs)
            assert result.returncode == 0, (args, result.returncode, result.stdout, result.stderr)
            return result.stdout

        for name in ['host', 'peer']:
            subprocess.run(['ssh-keygen', '-q', '-t', 'ed25519', '-N', '', '-f', str(root / name)], check=True)
        peer = run('management', 'identity', root / 'peer.pub').strip().rsplit(':', 1)[1]
        trust = root / 'authorized_keys'
        run('management', 'trust-init', trust)
        run('management', 'enroll', root / 'peer.pub', trust, root / 'host.pub', peer,
            '--scope', 'product-management')
        env['SSH_CONNECTION'] = '127.0.0.1 40000 127.0.0.1 40001'

        def request(operation, data, request_id=None, expect='ok'):
            global sequence
            sequence += 1
            identity = request_id or hashlib.sha256(f'fixture-{sequence}'.encode()).hexdigest()
            message = {'schema': 'yvex.management.request.v2', 'request_id': identity,
                       'operation': operation, 'input': data}
            out = json.loads(run('management', 'product-protocol', root / 'host.pub', peer, trust,
                                 input=json.dumps(message) + '\n'))
            assert out['status'] == expect, out
            return out

        def job(operation, data, request_id=None):
            first = request(operation, data, request_id)['data']
            return wait_job(first)

        def wait_job(first):
            limit = time.monotonic() + 30
            while time.monotonic() < limit:
                current = request('job.get', {'job_id': first['job_id']})['data']
                if current['state'] not in ['accepted', 'running']:
                    return current
                time.sleep(.05)
            raise AssertionError(('job timeout', first))

        def acquisition(source, predicate):
            limit = time.monotonic() + 30
            last = None
            while time.monotonic() < limit:
                last = request('acquisition.get', {'source': source})['data']['acquisition']
                if predicate(last):
                    return last
                time.sleep(.1)
            raise AssertionError(('acquisition timeout', last))

        capabilities = request('management.capabilities', {})['data']
        supported = {row['operation'] for row in capabilities['operations']}
        assert {'model.list', 'model.inspect', 'acquisition.start', 'build.start', 'package.verify'} <= supported
        before = request('job.list', {})['data']['jobs']
        for operation, data in [
            ('acquisition.start', {'repository': 'org/model', 'revision': 'a' * 40, 'representation': 'bf16', 'token': 'REJECTED_FIXTURE'}),
            ('build.start', {'model': 'x', 'out': '/tmp/unowned'}),
            ('acquisition.resume', {'source': 'x'}),
            ('model.evict', {'model': 'x', 'representation': 'a' * 64, 'kind': 'package'}),
        ]:
            request(operation, data, expect='refused')
        assert request('job.list', {})['data']['jobs'] == before
        assert not any('REJECTED_FIXTURE' in path.read_text() for path in (root / 'xdg').rglob('*.json'))
        listing = request('model.list', {})['data']
        assert isinstance(listing['models'], list) and listing['runtime_observation'] == 'unavailable'
        remote = request('model.search', {'query': 'MiniMax', 'limit': 5})['data']
        assert remote['models']
        revision = 'b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08'
        inspected = request('model.inspect', {'repository': 'MiniMaxAI/MiniMax-H3', 'revision': revision})['data']['models'][0]
        assert inspected['resolved_revision'] == revision
        representation = next(row['identity'] for row in inspected['representations'] if row['format'] == 'safetensors')
        source_name = 'management-fixture'
        source_input = {'repository': 'MiniMaxAI/MiniMax-H3', 'revision': revision,
                        'representation': representation, 'name': source_name, 'credential_ref': 'registry:huggingface:default'}
        # Credential reference is an existing host owner, never a new token store.
        env['YVEX_FAKE_HF_AUTH'] = '0'
        missing_credential = job('acquisition.start', {**source_input, 'name': 'credential-unavailable'})
        assert missing_credential['state'] == 'failed', missing_credential
        assert 'registry_authentication_required_on_host' in missing_credential['reason'], missing_credential
        assert not list((root / 'models').rglob('*credential-unavailable*'))
        env['YVEX_FAKE_HF_AUTH'] = '1'
        submit_id = hashlib.sha256(b'acquisition-exact-request').hexdigest()
        try:
            submitted = job('acquisition.start', source_input, submit_id)
            assert submitted['state'] == 'succeeded', submitted
            initial = submitted['result']['acquisition']
            assert initial['operation_id'] and initial['generation'] == 1
            # Lost acknowledgement: observation and identical receipt replay cannot start duplicate work.
            repeated = request('acquisition.start', source_input, submit_id)['data']
            assert repeated['job_id'] == submitted['job_id']
            current = acquisition(source_name, lambda value: value['active'])
            assert current['operation_id'] == initial['operation_id']
            stale = job('acquisition.cancel', {'source': source_name,
                        'expected_operation_id': current['operation_id'], 'expected_generation': 999})
            assert stale['state'] == 'failed' and 'stale_acquisition_generation' in stale['reason'], stale
            cancelled = job('acquisition.cancel', {'source': source_name,
                            'expected_operation_id': current['operation_id'], 'expected_generation': current['generation']})
            assert cancelled['state'] == 'succeeded', cancelled
            stopped = acquisition(source_name, lambda value: value['lifecycle'] == 'stopped')
            assert stopped['resume_available'] and not stopped['active']
            resumed = job('acquisition.resume', {'source': source_name,
                          'expected_operation_id': stopped['operation_id'], 'expected_generation': stopped['generation'],
                          'credential_ref': 'registry:huggingface:default'})
            assert resumed['state'] == 'succeeded', resumed
            assert resumed['result']['acquisition']['generation'] == stopped['generation'] + 1
            complete = acquisition(source_name, lambda value: value['lifecycle'] in ['complete', 'failed'])
            assert complete['lifecycle'] == 'complete', complete
            assert complete['inflight_selected_bytes'] is None
            verified = job('source.verify', {'source': source_name})
            assert verified['state'] == 'failed', verified
            assert 'exact source metadata and headers must verify before payload trust' in verified['reason'], verified
            # Planning fixture represents acquired provenance, deliberately no weights.
            build_revision = '62af8fffb2f7030cac4de2f0169f5b8d1101b646'
            build_source = root / 'models/source/hf/deepseek-ai/DeepSeek-V4-Flash-DSpark' / build_revision
            build_source.mkdir(parents=True)
            records = root / 'models/registry/sources'
            records.mkdir(parents=True, exist_ok=True)
            record = records / 'build-plan.source.json'
            record.write_text(json.dumps({'schema': 'yvex.model-source.registry.v1', 'name': 'build-plan',
                'family': 'deepseek', 'provider': 'huggingface',
                'repository': 'deepseek-ai/DeepSeek-V4-Flash-DSpark', 'revision': build_revision,
                'origin_uri': f'hf://deepseek-ai/DeepSeek-V4-Flash-DSpark@{build_revision}',
                'source_path': str(build_source), 'storage': 'managed', 'format': 'safetensors',
                'precision': 'BF16', 'digest': hashlib.sha256(b'').hexdigest(), 'size_bytes': 0,
                'file_count': 0, 'directory': True, 'status': 'complete', 'verification': 'revision-verified'}))
            before_record = record.read_bytes()
            planned = job('build.start', {'model': 'build-plan', 'dry_run': True})
            assert planned['state'] == 'succeeded', planned
            assert planned['result']['state'] == 'PLANNED' and not planned['result']['changed'], planned
            assert planned['result']['revision'] == build_revision
            assert planned['result']['target'] == 'deepseek4-v4-flash-dspark'
            assert len(planned['result']['plan_id']) == 64
            same_plan = job('build.start', {'model':'build-plan','dry_run':True,'expected_plan':planned['result']['plan_id']})
            assert same_plan['state'] == 'succeeded' and same_plan['result']['plan_id'] == planned['result']['plan_id']
            stale_plan = job('build.start', {'model':'build-plan','dry_run':False,'expected_plan':'0'*64})
            assert stale_plan['state'] == 'failed' and 'build_plan_changed_review_again' in stale_plan['reason'], stale_plan
            assert not list((root / 'models').rglob('physical.plan'))
            assert record.read_bytes() == before_record
            # No valid compiler input is invented from the small download fixture.
            refused_build = job('build.start', {'model': 'model-that-does-not-exist', 'dry_run': True})
            assert refused_build['state'] == 'failed', refused_build
            unknown_package = job('package.verify', {'model': 'model-that-does-not-exist', 'package': 'a' * 64})
            assert unknown_package['state'] == 'failed', unknown_package
            fixture = root / 'models/diagnostic.gguf'
            shutil.copyfile(repo / 'tests/fixtures/gguf/valid-tokenizer-simple.gguf', fixture)
            run('profile', 'create', '--path', fixture, '--alias', 'qwen-diagnostic-selected-embed-base',
                '--family', 'qwen', '--model', 'diagnostic', '--scope', 'selected', '--class', 'embed')
            model = next(row for row in request('model.list', {})['data']['models']
                         if any(package['identity'] == hashlib.sha256(fixture.read_bytes()).hexdigest() for package in row['representations']))
            package = model['representations'][0]['identity']
            # Concurrent native profile transactions retain every independent alias.
            creates = [request('profile.create', {'model': model['identity'], 'package': package,
                       'alias': f'qwen-diagnostic-inspection-embed-{index}'})['data'] for index in range(4)]
            for created in creates:
                observed = wait_job(created)
                assert observed['state'] == 'succeeded', observed
            for index in range(4):
                verified_profile = job('profile.verify', {'profile': f'qwen-diagnostic-inspection-embed-{index}'})
                assert verified_profile['state'] == 'succeeded' and verified_profile['result']['passed'], verified_profile
            scanned = job('profile.scan', {})
            assert scanned['state'] == 'succeeded' and isinstance(scanned['result']['candidates'], list), scanned
            stale_remove = job('profile.remove', {'profile': 'qwen-diagnostic-inspection-embed-0', 'expected_package': '0' * 64, 'confirm': True})
            assert stale_remove['state'] == 'failed' and 'stale_profile_package' in stale_remove['reason'], stale_remove
            assert job('profile.verify', {'profile': 'qwen-diagnostic-inspection-embed-0'})['result']['passed']
            removed = job('profile.remove', {'profile': 'qwen-diagnostic-inspection-embed-0', 'expected_package': package, 'confirm': True})
            assert removed['state'] == 'succeeded' and removed['result']['removed'], removed
            assert fixture.exists(), 'Removing a profile must retain the package.'
            clean_input = {'source': source_name, 'expected_operation_id': complete['operation_id'],
                           'expected_generation': complete['generation'], 'logs': True, 'dry_run': True}
            stale_cleanup = job('source.cleanup', {**clean_input, 'expected_generation': 999})
            assert stale_cleanup['state'] == 'failed' and 'stale_acquisition_generation' in stale_cleanup['reason'], stale_cleanup
            cleanup = job('source.cleanup', clean_input)
            assert cleanup['state'] == 'succeeded' and cleanup['result']['deleted_paths'] == 0, cleanup
            cleanup = job('source.cleanup', {**clean_input, 'dry_run': False, 'confirm': True})
            assert cleanup['state'] == 'succeeded' and cleanup['result']['deleted_paths'] >= 1, cleanup
            assert request('acquisition.get', {'source': source_name})['data']['acquisition']['lifecycle'] == 'complete'
            profiles = request('registry.accounts', {})['data']
            assert profiles['credential_configuration'] == 'host_owned'
            assert all('token' not in row for row in profiles['providers'])
            hf = next(row for row in profiles['providers'] if row['provider'] == 'huggingface')
            assert hf['credential_ref'] == 'registry:huggingface:default'
            assert hf['credential_provisioning'] == 'host_provider'
        finally:
            # Owner-authored cancellation only; never kill unrelated processes.
            subprocess.run([str(binary), 'source', 'stop', source_name, '--output', 'json'],
                           env=env, text=True, capture_output=True, timeout=20)
        print(json.dumps({'evidence': 'controlled-local-protocol-and-provider-fixture',
                          'source_job': 'submit-observe-cancel-resume-complete',
                          'recovery': 'same request receipt; stale native generation refused',
                          'secret_rejection': 'before journal', 'build': 'native planning; missing input refusal; no compiler execution claim',
                          'profiles': 'concurrent create; verify; scan; stale remove refusal; exact remove retains package',
                          'cleanup': 'revision-fenced dry run and logs-only cleanup', 'real_ssh': False, 'real_huggingface': False, 'operator_state_modified': False}))


if __name__ == '__main__':
    main()
