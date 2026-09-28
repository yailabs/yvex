"""Validate evidence observations and generate static publication projections.

This does not run models or replace runtime benchmark schema v5. Imported
observations retain their original record and unknown provenance explicitly.
"""
from __future__ import annotations
import argparse
import html
import json
import math
from pathlib import Path
from metadata import ROOT, require

AREA=ROOT/'docs/evaluation/benchmarks'
CONTEXT={'source_commit','source_tree','source_stability','date','run_id','model',
         'model_revision','artifact_identity','binding_identity','representation',
         'backend','device','runtime_configuration','workload','warm_state',
         'concurrency','sequence_lengths','memory','limitations','recorded_from'}

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
         f'<rect width="1000" height="{height}" rx="12" fill="#faf8ff"/>',
         '<g font-family="Arial, sans-serif" fill="#231735">',
         f'<text x="30" y="38" font-size="23" font-weight="700">{esc(r["title"])}</text>',
         f'<text x="30" y="67" font-size="16">{esc(r["kind"].upper())} · {esc(rows[0]["unit"])} · not an automatic capability promotion</text>']
    for i,m in enumerate(rows):
        y=110+i*58;w=470*m['value']/maximum
        out += [f'<text x="30" y="{y+19}" font-size="18">{esc(m["label"])}</text>',
                f'<rect x="330" y="{y}" width="{w:.3f}" height="27" rx="4" fill="#7541ba"/>',
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

def generate(check=False):
    out=AREA/'generated';out.mkdir(exist_ok=True)
    paths=sorted((AREA/'data').glob('*.json'));ids=set(); expected=set()
    for path in paths:
        r=validate(load_observation(path.read_text()));require(r['id'] not in ids,'duplicate observation ID');ids.add(r['id'])
        for suffix,text in [('.md',markdown(r,path)),('.svg',figure(r))]:
            dest=out/(r['id']+suffix);expected.add(dest)
            if check:require(dest.is_file() and dest.read_text()==text,f'stale benchmark projection: {dest.name}')
            else:dest.write_text(text)
    require({p for p in out.iterdir() if p.name!='README.md'}<=expected,'orphan benchmark output')
    print(f'benchmark projections: {len(paths)} observations ({"check" if check else "write"})')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true')
    generate(parser.parse_args().check)
