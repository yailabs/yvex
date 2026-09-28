"""Small shared document registry: Markdown + hidden metadata, no state database."""
from __future__ import annotations
import json
import re
from pathlib import Path
import yaml

ROOT = Path(__file__).resolve().parents[2]
FIELDS = {'title', 'id', 'document', 'status', 'owner', 'audience',
          'publication', 'related', 'plane', 'generated', 'source'}
ROLES = {'product', 'architecture', 'architecture-plane', 'project-control',
         'guide', 'evaluation', 'research', 'reference'}
POSTURES = {'current', 'mixed', 'target', 'research', 'live', 'deprecated'}
OWNERS = {'yvex', 'source', 'model', 'compiler', 'artifact', 'runtime',
          'backend', 'interfaces', 'evaluation', 'project', 'research', 'docs'}
AUDIENCES = {'founder', 'engineer', 'developer', 'evaluator', 'researcher', 'agent'}
IDENTIFIER = re.compile(r'^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)*$')

class UniqueLoader(yaml.SafeLoader):
    pass

def mapping(loader, node, deep=False):
    result = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node, deep=deep)
        if not isinstance(key, str) or key in result:
            raise ValueError(f'duplicate or non-string metadata key: {key!r}')
        result[key] = loader.construct_object(value_node, deep=deep)
    return result

UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, mapping)

def require(condition, message):
    if not condition:
        raise ValueError(message)

def parse(text, *, allow_visible=False, branded_root=False):
    hidden = re.match(r'\A<!-- docs:metadata\n(.*?)\n-->\n?', text, re.S)
    visible = re.match(r'\A---\n(.*?)\n---\n?', text, re.S)
    require(not visible or allow_visible, 'visible YAML front matter is forbidden')
    match = hidden or (visible if allow_visible else None)
    require(match is not None, 'missing hidden docs:metadata')
    require(len(re.findall(r'(?m)^<!-- docs:metadata$', text)) <= 1, 'duplicate metadata block')
    try:
        meta = yaml.load(match[1], Loader=UniqueLoader)
    except yaml.YAMLError as exc:
        raise ValueError(f'invalid metadata YAML: {exc}') from exc
    require(isinstance(meta, dict), 'metadata must be a mapping')
    require(set(meta) <= FIELDS, f'unsupported metadata fields: {set(meta)-FIELDS}')
    required = FIELDS - {'related', 'plane', 'generated', 'source'}
    require(required <= meta.keys(), f'missing metadata fields: {required-meta.keys()}')
    for key in ('title', 'id', 'document', 'status', 'owner'):
        require(isinstance(meta[key], str) and bool(meta[key].strip()), f'invalid {key}')
    require(IDENTIFIER.fullmatch(meta['id']), 'invalid document ID')
    require(meta['document'] in ROLES, 'unsupported document role')
    require(meta['status'] in POSTURES, 'unsupported document posture')
    require(meta['owner'] in OWNERS, 'unsupported owner')
    require(isinstance(meta['audience'], list) and bool(meta['audience']) and
            all(isinstance(v, str) and v in AUDIENCES for v in meta['audience']) and
            len(set(meta['audience'])) == len(meta['audience']), 'invalid audiences')
    require(isinstance(meta['publication'], dict) and
            set(meta['publication']) == {'html','pdf','index'} and
            all(type(v) is bool for v in meta['publication'].values()), 'invalid publication flags')
    require(not meta['publication']['pdf'] or meta['publication']['html'], 'PDF requires HTML projection')
    if 'related' in meta:
        require(isinstance(meta['related'], list) and all(isinstance(x, str) and IDENTIFIER.fullmatch(x)
                for x in meta['related']) and len(set(meta['related'])) == len(meta['related']), 'invalid related IDs')
    if 'plane' in meta:
        require(meta['document'] == 'architecture-plane' and isinstance(meta['plane'],str)
                and IDENTIFIER.fullmatch(meta['plane']), 'invalid plane identifier')
    if 'generated' in meta:
        require(type(meta['generated']) is bool, 'generated must be boolean')
    if meta.get('generated'):
        require(isinstance(meta.get('source'),str) and bool(meta['source']), 'projection requires source')
    elif 'source' in meta:
        raise ValueError('source is reserved for generated projection provenance')
    body = text[match.end():].lstrip('\n')
    branded = branded_root and meta['id']=='yvex' and body.startswith('<picture>') and 'alt="YVEX"' in body.split('</picture>',1)[0]
    require(branded or body.startswith('# '+meta['title']+'\n'), 'visible Markdown title must match metadata')
    require(not re.search(r'<(?:header|script)\b|\bstyle=["\']',body,re.I), 'canonical Markdown contains page styling/script')
    return meta, body

def canonical_paths(root=ROOT):
    return sorted([root/'README.md', root/'AGENTS.md', root/'ROADMAP.md',
                   *root.glob('docs/**/*.md')])

def registry(root=ROOT):
    docs = {}; planes = set()
    for path in canonical_paths(root):
        if not path.is_file():
            continue
        try:
            meta, body = parse(path.read_text(),branded_root=path==root/'README.md')
        except ValueError as exc:
            raise ValueError(f'{path.relative_to(root)}: {exc}') from exc
        require(meta['id'] not in docs, f'duplicate document ID: {meta["id"]}')
        if 'plane' in meta:
            require(meta['plane'] not in planes, f'duplicate plane: {meta["plane"]}')
            planes.add(meta['plane'])
        docs[meta['id']] = {'path':path, 'meta':meta, 'body':body}
    for doc in docs.values():
        for related in doc['meta'].get('related',[]):
            require(related in docs and related != doc['meta']['id'], f'invalid related document: {related}')
        if doc['meta'].get('generated'):
            source=(doc['path'].parent/doc['meta']['source']).resolve()
            require(source.is_relative_to(root) and source.is_file(), 'missing/outside projection source')
    return docs

def schema():
    text={'type':'string','minLength':1}
    identifier={'type':'string','pattern':IDENTIFIER.pattern}
    return {'$schema':'https://json-schema.org/draft/2020-12/schema',
      'title':'YVEX document metadata','type':'object','additionalProperties':False,
      'required':sorted(FIELDS-{'related','plane','generated','source'}),
      'properties':{'title':text,'id':identifier,'document':{'enum':sorted(ROLES)},
        'status':{'enum':sorted(POSTURES)},'owner':{'enum':sorted(OWNERS)},
        'audience':{'type':'array','minItems':1,'uniqueItems':True,'items':{'enum':sorted(AUDIENCES)}},
        'publication':{'type':'object','additionalProperties':False,'required':['html','pdf','index'],
          'properties':{key:{'type':'boolean'} for key in ['html','pdf','index']}},
        'plane':identifier,'related':{'type':'array','uniqueItems':True,'items':identifier},
        'generated':{'type':'boolean'},'source':text}}

if __name__ == '__main__':
    print(f'document metadata: {len(registry())} canonical documents')
