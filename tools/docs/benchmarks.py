"""Validate evidence observations and generate static publication projections.

This does not run models or replace runtime benchmark schema v5. Imported
observations retain their original record and unknown provenance explicitly.
"""
from __future__ import annotations
import argparse
import html
import json
import math
import os
import re
from pathlib import Path
import sys
from metadata import ROOT, require

sys.path.insert(0, str(ROOT/'tools'))
import qualification

AREA=ROOT/'docs/evaluation/benchmarks'
CONTEXT={'source_commit','source_tree','source_stability','date','run_id','model',
         'model_revision','artifact_identity','binding_identity','representation',
         'backend','device','runtime_configuration','workload','warm_state',
         'concurrency','sequence_lengths','memory','limitations','recorded_from'}

# Editorial selection of an accepted publication checkpoint, not a leaderboard
# or a second measurement database. Values and claim states come only from receipts.
SHOWCASE = (
    ('deepseek-0731-program-p-native-mxfp4-publication-coding-hash-table-20261010',
     'C hash table', 'decode.post-first.committed'),
    ('deepseek-0731-program-p-native-mxfp4-publication-speculative-coding-hash-table-20261010',
     'C hash table', 'decode.post-first.committed'),
    ('deepseek-0731-program-p-native-mxfp4-publication-prefill-promessi-2048-20261010',
     'Long text · 2K', 'prefill.uncached'),
    ('deepseek-0731-program-p-native-mxfp4-publication-prefill-promessi-8192-20261010',
     'Long text · 8K', 'prefill.uncached'),
)


def benchmark_excerpt(records, consumer):
    """Bounded public view of exact targets; never select the fastest sample."""
    by_id = {r['id']: r for r in records}
    selected = []
    for identifier, label, metric in SHOWCASE:
        require(identifier in by_id, 'missing benchmark showcase receipt: '+identifier)
        r = qualification.validate(by_id[identifier])
        measurements = [m for m in r['measurements'] if m['metric'] == metric]
        require(len(measurements) == 1, 'ambiguous showcase measurement')
        require(r['origin'] == 'yvex-published', 'showcase requires published receipt')
        selected.append((r, label, measurements[0]))
    t = selected[0][0]['target']
    shared = ('checkpoint', 'upstream_repository', 'representation', 'backend',
              'artifact_set', 'binding', 'specialization', 'physical_policy',
              'transformation_ir', 'tokenizer_conversation', 'backend_implementation',
              'hardware_model', 'device_count', 'topology', 'source_commit', 'source_tree',
              'build', 'executable', 'prefill_geometry', 'reasoning', 'sampling',
              'product_path', 'sequence_geometry', 'concurrency', 'driver', 'runtime_toolkit')
    require(all(all(r['target'][k] == t[k] for k in shared) for r, _, _ in selected),
            'incompatible showcase context; publish separate groups')
    def link(r):
        return os.path.relpath(AREA/'generated'/('qualification-'+r['id']+'.md'), consumer.parent)
    def value(m):
        s = m['statistics']
        return f'{s["median"]:.2f}', f'{s["minimum"]:.2f}–{s["maximum"]:.2f}'
    lines = [f'**{t["upstream_repository"].split("/")[-1]} · {t["hardware_model"]} × {t["device_count"]} · {t["backend"].upper()}.**', '',
             'One retained publication checkpoint, not a hardware-independent speed claim.',
             'Native protocol measurements use an isolated resident host, a separate warmup',
             'and fresh sessions without prefix reuse; they do not describe whichever engine is installed today.',
             'File-cache state was uncontrolled. Full build, artifact, binding and specialization identities are linked per row.', '',
             '| Configuration | Exact measured scope |', '| --- | --- |',
             f'| Checkpoint / build source | `{t["checkpoint"][:12]}` / `{t["source_commit"][:12]}`; full identities in each receipt |',
             f'| Physical representation | {t["representation"].split(";")[0]} |',
             f'| Input / execution | {t["prefill_geometry"]}; {t["sequence_geometry"]}; concurrency {t["concurrency"]}; reasoning `{t["reasoning"]}` |',
             f'| Sampling / transport | Temperature {json.loads(t["sampling"])["temperature"]:g}, deterministic; `{t["product_path"]}` |',
             f'| Driver / toolkit | {t["driver"]}; {t["runtime_toolkit"]} |', '',
             '| Workload / metric | Strategy · context | Input / output | Median (tok/s) | Min–max | N |',
             '| --- | --- | ---: | ---: | ---: | ---: |']
    for r, label, m in selected:
        median, span = value(m)
        metric = 'committed decode' if m['metric'].startswith('decode.') else 'new prefill'
        facts = {d['id']: d['value'] for d in r['provenance'].get('diagnostics', [])}
        require(facts.get('native.reused_tokens') == 0 and m['session_state'] == 'fresh',
                'showcase requires measured uncached fresh sessions')
        counts = [facts.get('native.prefill_tokens'), facts.get('native.generated_tokens')]
        population = ' / '.join(str(v) if v is not None else 'UNKNOWN' for v in counts)
        lines.append(f'| [{label}]({link(r)}) · {metric} | {r["target"]["strategy"]} · {r["target"]["context"]} | {population} | {median} | {span} | {m["statistics"]["count"]} |')
    lines += ['', 'Decode excludes the first committed token and its latency. Input/output counts are server-authored;',
              'prefill counts newly executed input positions, not reused context.',
              'Different strategies and context bands remain separate rows, not an averaged score.', '',
              '| Coding request | Server TTFT | First visible content | Complete client request |',
              '| --- | ---: | ---: | ---: |']
    for r, _, _ in selected[:2]:
        metrics = {m['metric']: m for m in r['measurements']}
        cells = []
        for key in ('ttft.server', 'ttft.client-visible', 'request.client-complete'):
            m = metrics.get(key)
            cells.append(f'{m["statistics"]["median"]:.3f} s' if m else 'NOT MEASURED')
        lines.append(f'| [{r["target"]["strategy"]}]({link(r)}) | '+ ' | '.join(cells)+' |')
    states = sorted({r['claims']['deployment-performance']['state'] for r, _, _ in selected})
    quality = sorted({r['claims']['representation-quality']['state'] for r, _, _ in selected})
    lines += ['', f'Performance: **{" / ".join(states)}**. Independent representation quality: **{" / ".join(quality)}**.',
              'Latency cells are medians; their ranges, MAD, raw sample identities and memory/preparation',
              'observations are in the linked receipts. No quality metric or confidence interval is invented.',
              'These records do not establish high/maximum reasoning, other models, HTTP latency or release readiness.', '']
    return '\n'.join(lines)


def synchronize_excerpt(records, path):
    text = path.read_text()
    pattern = r'<!-- docs:benchmark showcase -->.*?<!-- /docs:benchmark -->'
    require(len(re.findall(pattern, text, re.S)) == 1, 'missing/duplicate benchmark publication slot')
    block = '<!-- docs:benchmark showcase -->\n\n'+benchmark_excerpt(records, path)+'\n<!-- /docs:benchmark -->'
    return re.sub(pattern, lambda _: block, text, flags=re.S)

def load_observation(text):
    def unique_pairs(pairs):
        result={}
        for key,value in pairs:
            require(key not in result, 'duplicate observation key: '+key)
            result[key]=value
        return result
    return json.loads(text,object_pairs_hook=unique_pairs)

def validate(record):
    require(isinstance(record,dict) and set(record)=={'schema','id','title','kind','context','measurements'}, 'invalid observation fields')
    require(record['schema']=='yvex.evaluation.observation.v1','invalid observation schema')
    import re
    require(isinstance(record['id'],str) and re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*',record['id']), 'invalid observation ID')
    require(isinstance(record['title'],str) and record['title'], 'missing observation title')
    require(record['kind'] in {'benchmark','characterization','numerical','fixture'}, 'invalid observation kind')
    c=record['context'];require(isinstance(c,dict) and set(c)==CONTEXT,'incomplete/unknown observation context')
    for key in ['date','run_id','model','model_revision','representation','backend','device','workload']:
        require(c[key] is None or isinstance(c[key],str) and bool(c[key].strip()), 'invalid context '+key)
    if c['date'] is not None:
        from datetime import date
        require(bool(re.fullmatch(r'\d{4}-\d{2}-\d{2}',c['date'])), 'invalid observation date')
        date.fromisoformat(c['date'])
    for key in ['source_commit','source_tree']:
        require(c[key] is None or isinstance(c[key],str) and re.fullmatch(r'[a-f0-9]{40}',c[key]),f'invalid {key}')
    for key in ['artifact_identity','binding_identity']:
        require(c[key] is None or isinstance(c[key],str) and re.fullmatch(r'[a-f0-9]{64}',c[key]),f'invalid {key}')
    require(c['source_stability'] in {'frozen','unknown','fixture'},'invalid source stability')
    require(c['warm_state'] in {'cold','warm','mixed','unknown','fixture'},'invalid warm state')
    require(c['concurrency'] is None or type(c['concurrency']) is int and c['concurrency']>0,'invalid concurrency')
    for key in ['runtime_configuration','sequence_lengths','memory']:
        require(isinstance(c[key],dict),'context requires structured '+key)
    require(isinstance(c['limitations'],list) and bool(c['limitations']) and all(isinstance(x,str) and x for x in c['limitations']), 'limitations required')
    require(isinstance(c['recorded_from'],str) and bool(c['recorded_from']), 'missing evidence pointer')
    if record['kind']=='benchmark':
        require(c['source_stability']=='frozen' and all(c[x] for x in ['source_commit','source_tree','date','run_id','model','model_revision','artifact_identity','binding_identity','representation','backend','device','workload','concurrency']), 'benchmark lacks exact provenance')
        require(c['warm_state'] in {'cold','warm'}, 'benchmark must declare warm/cold')
    if record['kind']=='fixture':
        require(c['source_stability']=='fixture','fixture must not masquerade as a real run')
    else:
        require(c['source_stability']!='fixture','real observation has fixture provenance')
    measurements=record['measurements'];require(isinstance(measurements,list) and bool(measurements),'empty observation')
    ids=set()
    for m in measurements:
        require(isinstance(m,dict) and set(m)=={'id','label','metric','unit','value','samples','statistic','scope'},'invalid measurement fields')
        require(all(isinstance(m[k],str) and bool(m[k]) for k in ['id','label','metric','unit','statistic','scope']),'invalid metric identity')
        require(m['id'] not in ids,'duplicate measurement ID');ids.add(m['id'])
        require(type(m['value']) in {int,float} and math.isfinite(m['value']) and m['value']>=0,'non-finite/negative measurement')
        require(m['samples'] is None or type(m['samples']) is int and m['samples']>0,'invalid sample count')
        if record['kind']=='benchmark':require(m['samples'] is not None,'benchmark sample count unavailable')
    require(len({(m['metric'],m['unit']) for m in measurements})==1,'one comparable metric/unit per observation figure')
    return record

def figure(r):
    rows=r['measurements']; height=160+len(rows)*58
    esc=html.escape;maximum=max(m['value'] for m in rows) or 1
    out=[f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 {height}" role="img" aria-labelledby="title desc">',
         f'<title id="title">{esc(r["title"])}</title>',
         f'<desc id="desc">{esc(r["kind"])}; {esc(r["context"]["limitations"][0])}. Values are printed beside each bar.</desc>',
         '<style>text{fill:#292434}.bar{fill:#7541ba}@media(prefers-color-scheme:dark){text{fill:#ede8f5}.bar{fill:#ba97f2}}</style>',
         '<g font-family="Arial, sans-serif">',
         f'<text x="30" y="38" font-size="23" font-weight="700">{esc(r["title"])}</text>',
         f'<text x="30" y="67" font-size="16">{esc(r["kind"].upper())} · {esc(rows[0]["unit"])} · not an automatic capability promotion</text>']
    for i,m in enumerate(rows):
        y=110+i*58;w=470*m['value']/maximum
        out += [f'<text x="30" y="{y+19}" font-size="18">{esc(m["label"])}</text>',
                f'<rect class="bar" x="330" y="{y}" width="{w:.3f}" height="27" rx="4"/>',
                f'<text x="{350+w:.3f}" y="{y+19}" font-size="17">{m["value"]:g}</text>']
    out+=['</g></svg>'];return '\n'.join(out)+'\n'

def markdown(r,source):
    import yaml
    name=r['id']; meta={'title':r['title'],'id':'yvex.evaluation.observation.'+name,
       'document':'evaluation','status':'current','owner':'evaluation','audience':['engineer','evaluator','agent'],
       'publication':{'html':True,'pdf':True,'index':True},'generated':True,'source':'../data/'+source.name}
    c=r['context']
    lines=['<!-- docs:metadata',yaml.safe_dump(meta,sort_keys=False,default_flow_style=None).rstrip(),'-->','',
      '# '+r['title'],'',f'**{r["kind"].upper()} — one structured observation, with its original limits.**','',
      '[Benchmarks](../README.md) · [Source observation](../data/'+source.name+')','',
      '!['+r['title']+']('+name+'.svg)','',
      '| Measurement | Value | Unit | Samples | Scope |','| --- | ---: | --- | ---: | --- |']
    for m in r['measurements']:
        lines.append(f'| {m["label"]} | {m["value"]} | {m["unit"]} | {m["samples"] or "unknown"} | {m["scope"]} |')
    lines+=['','## Context','',f'Model: **{c["model"] or "unknown"}**. Backend: **{c["backend"] or "unknown"}**. Device: **{c["device"] or "unknown"}**.',
      f'Source commit: `{c["source_commit"] or "not retained"}`. Source stability: **{c["source_stability"]}**. Warm/cold: **{c["warm_state"]}**.',
      '', 'Evidence pointer: '+c['recorded_from'], '', '## Limits','']
    lines+=['- '+x for x in c['limitations']]
    lines+=['','The [methodology](../methodology.md) defines comparability. A document import does not rerun this experiment.','']
    return '\n'.join(lines)

def qualification_populations(record):
    """Project measured populations/economics without inventing missing counters."""
    rows = record['provenance'].get('observations', [])
    if not rows:
        return []
    cases = {m['case'] for m in record['measurements']}
    groups = {}
    for row in rows:
        if 'case' in row and 'turn_index' in row:
            case = row['case'] + '/turn-' + str(row['turn_index'])
            split = {c for c in cases if c.startswith(case + '/input-')}
            if split:
                require(case not in cases, 'mixed grouped and ungrouped observation population')
                identity = row.get('input_identity')
                require(isinstance(identity, str) and bool(identity),
                        'grouped observation population lacks exact input identity')
                case += '/input-' + identity
        elif 'case' in row:
            # Native imports already retain the fully qualified turn/history
            # key. A separate load measurement must not make it ambiguous.
            case = row['case']
        else:
            require(len(cases) == 1, 'ambiguous observation case population')
            case = next(iter(cases))
        require(case in cases, 'observation population lacks matching measurement')
        identities = {m.get('prompt_identity') for m in record['measurements']
                      if m['case'] == case and m.get('prompt_identity') is not None}
        require(len(identities) <= 1, 'inconsistent measurement input population')
        if identities and row.get('input_identity') is not None:
            require(row['input_identity'] in identities,
                    'observation population differs from measurement input identity')
        groups.setdefault(case, []).append(row)
    fields = [
        ('prompt_tokens', 'Rendered prompt', 'token'), ('reused_tokens', 'Reused prefix', 'token'),
        ('prefill_tokens', 'New prefill', 'token'), ('generated_tokens', 'Committed output', 'token'),
        ('reasoning_tokens', 'Reasoning', 'token'), ('final_tokens', 'Final content', 'token'),
        ('draft_cycles', 'Draft cycles', 'cycle'), ('draft_forwards', 'Draft forwards', 'forward'),
        ('proposed_tokens', 'Proposed', 'token'), ('selected_verification_tokens', 'Selected verification', 'token'),
        ('target_verifications', 'Target verifications', 'verification'),
        ('accepted_tokens', 'Accepted draft', 'token'), ('rejected_tokens', 'Rejected draft', 'token'),
        ('discarded_tokens', 'Discarded draft', 'token'), ('correction_or_bonus_tokens', 'Correction/bonus', 'token'),
        ('mean_accepted_prefix', 'Per-sample mean accepted prefix', 'token'),
        ('maximum_accepted_prefix', 'Per-sample maximum accepted prefix', 'token'),
        ('draft_seconds', 'Draft phase', 's'), ('verification_seconds', 'Verification phase', 's'),
        ('commit_seconds', 'Speculative commit phase', 's')]
    lines = ['', '## Runtime populations and speculative work', '',
             'Counters come from terminal runtime facts. Missing counters are not zero.',
             'Channel totals may exclude control delimiters; phase spans are not assumed additive.', '']
    for case, observations in sorted(groups.items()):
        lines += ['### ' + case, '', '| Fact | Median | Unit | N | Min–max | MAD |',
                  '| --- | ---: | --- | ---: | --- | ---: |']
        for key, label, unit in fields:
            values = [r.get(key) for r in observations]
            if any(v is None for v in values):
                lines.append(f'| {label} | NOT MEASURED | {unit} | — | — | — |')
                continue
            require(all(type(v) in (int, float) and math.isfinite(v) and v >= 0 for v in values),
                    'invalid runtime population: ' + key)
            s = qualification.statistics_for(values)
            lines.append(f'| {label} | {s["median"]:.6g} | {unit} | {s["count"]} | {s["minimum"]:.6g}–{s["maximum"]:.6g} | {s["median_absolute_deviation"]:.6g} |')
    return lines


def qualification_views(include_landings=False):
    """Same publication owner; receipts refer to producer evidence, not another database."""
    paths = sorted((AREA/'qualification').glob('*.json'))
    records = [qualification.validate(load_observation(p.read_text())) for p in paths]
    require(len({r['id'] for r in records}) == len(records), 'duplicate qualification receipt ID')
    def cell(value):
        return str(value if value is not None else 'NOT RETAINED').replace('|','\\|').replace('\n',' ')
    def header(title, identifier, source):
        return ['<!-- docs:metadata', 'title: '+json.dumps(title), 'id: yvex.evaluation.qualification.'+identifier,
                'document: evaluation', 'status: mixed', 'owner: evaluation',
                'audience: [engineer, evaluator, agent]', 'publication: {html: true, pdf: true, index: true}',
                'generated: true', 'source: '+source, '-->', '', '# '+title, '',
                '[Benchmarks](../README.md) · [Methodology](../methodology.md)', '']
    result = {}
    overview = header('Qualification targets', 'index', '../methodology.md')
    overview += ['A sparse evidence catalog, not a leaderboard or a list of everything that can execute.', '',
                 'Start with the [benchmark snapshot](../README.md#measured-snapshot), then select an exact receipt below.',
                 'LOCAL receipts are not YVEX-published qualification. Each plane stands alone; missing quality is not zero error.', '',
                 '## Checkpoint directory', '',
                 '| Source / checkpoint | Exact targets | Representation quality | Performance |',
                 '| --- | ---: | --- | --- |']
    groups = {}
    for r in records:
        key = (r['target']['upstream_repository'], r['target']['checkpoint'])
        groups.setdefault(key, []).append(r)
    def counts(group, plane):
        states = [r['claims'][plane]['state'] for r in group]
        return '; '.join(f'{s}: {states.count(s)}' for s in sorted(set(states)))
    for index, ((repository, checkpoint), group) in enumerate(groups.items(), 1):
        overview.append(f'| [{cell(repository)} · `{cell(checkpoint)[:12]}`](#checkpoint-{index}) | {len(group)} | {counts(group, "representation-quality")} | {counts(group, "deployment-performance")} |')
    for index, ((repository, checkpoint), group) in enumerate(groups.items(), 1):
        overview += ['', f'## Checkpoint {index}', '', f'**{cell(repository)}** · `{cell(checkpoint)}`', '',
                     'Counts describe independent records, not a percentage of model support.', '',
                     '<details>', f'<summary>Inspect {len(group)} exact targets, variants and experiments</summary>', '',
                     '| Target / full context | Execution | Quality | Performance | Origin |',
                     '| --- | --- | --- | --- | --- |']
        for r in group:
            t = r['target']
            overview.append(f'| [{cell(r["title"])}](qualification-{r["id"]}.md) | {cell(t["backend"])} × {cell(t["device_count"])}; {cell(t["product_path"])}; {cell(t["strategy"])} | {r["claims"]["representation-quality"]["state"]} | {r["claims"]["deployment-performance"]["state"]} | {r["origin"]} |')
        overview += ['', '</details>']
    for path, r in zip(paths, records):
        t = r['target']; name = 'qualification-'+r['id']
        lines = header(r['title'], r['id'], '../qualification/'+path.name)
        lines += ['[All targets](qualification-index.md) · [Machine receipt](../qualification/'+path.name+')', '',
                  f'Target identity: `{r["target_identity"]}`. Origin: **{r["origin"]}**.', '',
                  '## Measurements', '', '| Metric / exact case | Median | Unit | N | Min–max | MAD |',
                  '| --- | ---: | --- | ---: | --- | ---: |']
        for m in r['measurements']:
            s=m['statistics']
            lines.append(f'| {m["metric"]} / {cell(m["case"])} | {s["median"]:.6g} | {m["unit"]} | {s["count"]} | {s["minimum"]:.6g}–{s["maximum"]:.6g} | {s["median_absolute_deviation"]:.6g} |')
        if not r['measurements']: lines += ['', 'No quality or performance metric is published for this target.']
        lines += qualification_populations(r)
        if r['provenance'].get('continuation_comparisons'):
            lines += ['', '## Independent continuation comparison', '',
                      'This is exact-input characterization, not cross-realization numerical equivalence or a performance measurement.', '',
                      '| Case | Exact input tokens | First token | Matching greedy prefix | Reference / candidate sampled | Candidate committed | Exact bounded continuation |',
                      '| --- | --- | --- | ---: | ---: | ---: | --- |']
            for row in r['provenance']['continuation_comparisons']:
                c = row['comparison']
                lines.append(f'| {cell(row["case"])} | {row["prompt_tokens"]} | {"MATCH" if c["first_token_agreement"] else "DIFFERS"} | {c["greedy_prefix_tokens"]} | {c["reference_sampled_tokens"]} / {c["candidate_sampled_tokens"]} | {c["candidate_committed_tokens"]} | {"MATCH" if c["exact_bounded_continuation"] else "DIFFERS"} |')
            producer = r['provenance']['independent_implementation']
            lines += ['', f'Independent implementation: **{cell(producer["name"])}** @ `{producer["revision"]}`; executable `{producer["executable_sha256"]}`.',
                      'Full distributions, teacher-forced NLL/PPL, KL and RMS probability delta: **NOT MEASURED**.', '']
        if r['provenance'].get('diagnostics'):
            lines += ['', '## Diagnostic facts (not timed benchmark samples)', '',
                      'These observations explain this exact run; they do not qualify a throughput or quality claim.', '',
                      '| Fact / case | Value | Unit | Definition / evidence |', '| --- | ---: | --- | --- |']
            for fact in r['provenance']['diagnostics']:
                value = 'NOT MEASURED' if fact['value'] is None else f'{fact["value"]:.9g}'
                lines.append(f'| {cell(fact["id"])} / {cell(fact["case"])} | {value} | {cell(fact["unit"])} | {cell(fact["definition"])}; {cell(fact["evidence"])} |')
        if r['provenance'].get('case_outcomes'):
            lines += ['', '## Request outcomes (not performance samples)', '',
                      '| Case | Result | Reason |', '| --- | --- | --- |']
            lines += [f'| {cell(o["case"])} | {o["result"]} | {cell(o["reason"])} |'
                      for o in r['provenance']['case_outcomes']]
        lines += ['', '## Claim boundaries', '', '| Plane | State | Exact scope / missing gate |','| --- | --- | --- |']
        for plane, claim in r['claims'].items():
            lines.append(f'| {plane} | {claim["state"]} | {cell(claim["scope"])}; {cell("; ".join(claim["blockers"]))} |')
        lines += ['', '## Exact configuration', '', '| Identity / setting | Value |','| --- | --- |']
        lines += [f'| {key} | {cell(t[key])} |' for key in qualification.TARGET_FIELDS]
        lines += ['', '## Definitions and reproducibility', '']
        for m in r['measurements']:
            lines += [f'- **{m["metric"]}**: {m["definition"]}. Scope: {m["scope"]}. Session: {m["session_state"]}; warm/cold: {m["warm_state"]}; output bound: {m["output_bound"]}. Evidence: {cell(m["evidence"])}.']
        lines += ['', 'No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.', '', '## Non-claims', '']
        lines += ['- '+cell(x) for x in r['limitations']]
        result[AREA/'generated'/(name+'.md')] = '\n'.join(lines)+'\n'
    overview += ['', 'See the [plain-language methodology](../methodology.md#qualification-targets-and-local-receipts). No result here qualifies a different checkpoint, quantization, backend, device topology or transport.', '']
    result[AREA/'generated/qualification-index.md'] = '\n'.join(overview)
    result[AREA/'generated/qualification.json'] = json.dumps(records, sort_keys=True, indent=2, allow_nan=False)+'\n'
    result[AREA/'schema/qualification-target.schema.json'] = json.dumps(qualification.schema(), indent=2)+'\n'
    result[AREA/'schema/qualification-rules.json'] = json.dumps({
        'schema':'yvex.qualification.rules.v1', 'target_schema':qualification.TARGET_SCHEMA,
        'receipt_schema':qualification.RECEIPT_SCHEMA, 'fields':qualification.TARGET_FIELDS,
        'planes':qualification.PLANES, 'states':qualification.STATES,
        'receipt_fields':qualification.RECEIPT_FIELDS, 'claim_fields':qualification.CLAIM_FIELDS,
        'outcome_fields':qualification.OUTCOME_FIELDS, 'outcome_results':qualification.OUTCOME_RESULTS,
        'metric_fields':qualification.METRIC_FIELDS,
        'diagnostic_fields':qualification.DIAGNOSTIC_FIELDS,
        'diagnostic_units':qualification.DIAGNOSTIC_UNITS,
        'quality_key':qualification.QUALITY_KEY, 'metrics':qualification.METRICS,
        'metric_admission':qualification.METRIC_ADMISSION}, indent=2)+'\n'
    import qualification_run
    suites = qualification_run.corpora(ROOT/'tests/vectors')
    cells = [dict(suite=s['id'], suite_identity=qualification.identity(s), **c)
             for s in suites for c in qualification_run.configurations(s)]
    # Both views reference exact targets, never a hardware-free suite PASS.
    for c in cells:
        c['targets'] = [dict(id=r['id'], target_identity=r['target_identity'],
                             path=r['target']['product_path'],
                             plane='checkpoint-reference' if r['provenance'].get('continuation_comparisons') else 'product-path',
                             state=r['claims']['checkpoint-reference' if r['provenance'].get('continuation_comparisons') else 'product-path']['state'],
                             outcomes=[o['result'] for o in r['provenance'].get('case_outcomes', []) if o['case'] == c['case']])
                        for r in records if r['target']['suite'] == c['suite_identity']
                        and r['target']['reasoning'] == c['reasoning'] and r['target']['strategy'] == c['strategy']
                        and (any(m['case'] == c['case'] or m['case'].startswith(c['case']+'/turn-') for m in r['measurements'])
                             or any(o['case'] == c['case'] for o in r['provenance'].get('case_outcomes', [])))]
    result[AREA/'generated/qualification-workloads.json'] = json.dumps(
        {'schema':'yvex.qualification.workloads.v1', 'suites':suites, 'cells':cells}, indent=2)+'\n'
    lines = header('Workload and reasoning matrix', 'workloads', '../methodology.md')
    lines += ['Unexecuted cells are not evidence. Case input is identical across its declared modes;',
              'source-authored rendering may add policy-specific instructions. Synthetic cases are separate controls.', '']
    for s in suites:
        axes = sorted({(mode, strategy) for c in s['cases'] for mode in c['reasoning_modes'] for strategy in c['execution_strategies']})
        lines += ['## '+s['id'], '', '| Case | Class | '+' | '.join(strategy+' / '+mode for mode,strategy in axes)+' |',
                  '| --- | --- | '+' | '.join('---' for _ in axes)+' |']
        for case in s['cases']:
            states=[]
            for mode,strategy in axes:
                state='UNQUALIFIED' if mode in case['reasoning_modes'] and strategy in case['execution_strategies'] else 'NOT APPLICABLE'
                matching=next((c['targets'] for c in cells if c['suite']==s['id'] and c['case']==case['id']
                               and c['reasoning']==mode and c['strategy']==strategy), [])
                if matching:
                    state='<br>'.join(f'[{r["id"]}: {r["plane"]} {r["state"]}{(" / " + ", ".join(r["outcomes"])) if r["outcomes"] else ""}](qualification-{r["id"]}.md)' for r in matching)
                states.append(state)
            lines += [f'| {case["id"]} | {case["class"]} | '+ ' | '.join(states)+' |']
        lines += ['', 'Native adapter exclusions:', '']
        lines += ['- '+c['id']+': '+c['native_disposition'] for c in s['cases'] if 'native_disposition' in c]
    lines += ['', 'These are the current suite admission cells, not historical synthetic results. No full-model reference or throughput target is inferred from an input manifest.', '']
    result[AREA/'generated/qualification-workloads.md'] = '\n'.join(lines)
    dimensions = {
        'families':('Family and checkpoint evidence', ['family_contract','upstream_repository','checkpoint']),
        'representations':('Representation quality comparison', ['checkpoint','representation','physical_policy','artifact_set']),
        'hardware':('Hardware and backend evidence', ['backend','hardware_model','device_count','topology','context','strategy','reasoning']),
    }
    for identifier,(title,fields) in dimensions.items():
        view=header(title,identifier,'../methodology.md')
        view += ['Every row links its complete context. Missing metrics are not zero; these rows do not establish cross-target comparability.', '',
                 '| Target | '+' | '.join(fields)+' | Quality | Performance |',
                 '| --- | '+' | '.join('---' for _ in fields)+' | --- | --- |']
        for r in records:
            view += [f'| [{cell(r["title"])}](qualification-{r["id"]}.md) | '+' | '.join(cell(r['target'][f]) for f in fields)+f' | {r["claims"]["representation-quality"]["state"]} | {r["claims"]["deployment-performance"]["state"]} |']
        result[AREA/'generated'/('qualification-'+identifier+'.md')]='\n'.join(view)+'\n'
    result[AREA/'generated/qualification-index.md'] += '\n[Family/checkpoint](qualification-families.md) · [Representation quality](qualification-representations.md) · [Hardware/backend](qualification-hardware.md) · [Workload/reasoning](qualification-workloads.md)\n'
    references = header('Independent checkpoint reference captures', 'references', '../methodology.md')
    references += ['Capture presence is not YVEX numerical agreement or representation-quality qualification.',
                   'Source-parser results below concern the independent producer. Missing quality metrics are not zero.', '']
    machine_references = []
    for path in sorted((AREA/'references').glob('*.json')):
        r = qualification.validate_reference_observation(load_observation(path.read_text()))
        machine_references.append(r)
        producer = r['implementation']
        references += ['## '+path.stem, '', '[Machine observation](../references/'+path.name+')', '',
                       '| Identity | Value |', '| --- | --- |',
                       f'| Model / checkpoint | {cell(r["source"]["repository"])} |',
                       f'| Upstream revision | {cell(r["source"]["revision"])} |',
                       f'| Independent producer | {cell(producer["name"])} @ {cell(producer["revision"])} |',
                       f'| Executable | {producer["executable_sha256"]} |',
                       f'| Representation receipt identity | {producer["representation_identity"]} |',
                       f'| Environment receipt identity | {producer["environment_identity"]} |',
                       f'| Reference capture | {r["reference_sha256"]} |',
                       f'| Workload suite bytes | {r["suite_sha256"]} |',
                       f'| Weight manifest | {r["checkpoint_manifest_sha256"]} |',
                       f'| Capture / YVEX conformance | {r["status"]} / {r["yvex_conformance"]} |', '',
                       '| Case | Reasoning | Input | Output / bound | Finish | Source grammar | Reasoning boundary positions |',
                       '| --- | --- | ---: | ---: | --- | --- | --- |']
        for row in r['cases']:
            g = row['grammar']
            references += [f'| {cell(row["case"])} | {cell(row["reasoning"])} | {row["prompt_tokens"]} | {row["output_tokens"]} / {row["maximum_output"]} | {row["finish"]} | {g["state"]} | {cell(g["reasoning_boundary_positions"])} |']
        if r.get('continuation_dispositions'):
            references += ['', '### Independent generated-history continuation', '',
                           '| Case | Reasoning | Next turn | Prior grammar | Evidence state | Reason |',
                           '| --- | --- | ---: | --- | --- | --- |']
            for row in r['continuation_dispositions']:
                references += [f'| {cell(row["case"])} | {cell(row["reasoning"])} | {row["turn_index"]} | {row["prior_grammar"]} | {row["state"]} | {cell(row["reason"])} |']
        references += ['', '### Limits', ''] + ['- '+cell(x) for x in r['limitations']] + ['']
    result[AREA/'generated/qualification-references.md'] = '\n'.join(references).rstrip()+'\n'
    result[AREA/'generated/qualification-references.json'] = json.dumps(machine_references, indent=2, allow_nan=False)+'\n'
    result[AREA/'generated/qualification-index.md'] += '\n[Independent reference captures](qualification-references.md)\n'
    if include_landings:
        for consumer in (ROOT/'README.md', AREA/'README.md'):
            result[consumer] = synchronize_excerpt(records, consumer)
    return result


def generate(check=False):
    out=AREA/'generated';out.mkdir(exist_ok=True)
    paths=sorted((AREA/'data').glob('*.json'));ids=set(); expected=set()
    for path in paths:
        r=validate(load_observation(path.read_text()));require(r['id'] not in ids,'duplicate observation ID');ids.add(r['id'])
        for suffix,text in [('.md',markdown(r,path)),('.svg',figure(r))]:
            dest=out/(r['id']+suffix);expected.add(dest)
            if check:require(dest.is_file() and dest.read_text()==text,f'stale benchmark projection: {dest.name}')
            else:dest.write_text(text)
    for dest, text in qualification_views(include_landings=True).items():
        if dest.parent == out: expected.add(dest)
        if check: require(dest.is_file() and dest.read_text()==text, f'stale qualification projection: {dest.name}')
        else: dest.write_text(text)
    require({p for p in out.iterdir() if p.name!='README.md'}<=expected,'orphan benchmark output')
    print(f'benchmark projections: {len(paths)} observations ({"check" if check else "write"})')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true')
    generate(parser.parse_args().check)
