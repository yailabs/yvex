#!/usr/bin/env python3
"""Refusal and projection regression coverage for documentation infrastructure."""
import copy
import json
import re
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/docs'))
import metadata
import benchmarks
import importlib.util
spec=importlib.util.spec_from_file_location('docs_site',ROOT/'tools/docs/site.py')
site=importlib.util.module_from_spec(spec);spec.loader.exec_module(site)

class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.text=(ROOT/'docs/architecture/computational-state.md').read_text()
        self.record=json.loads((ROOT/'docs/evaluation/benchmarks/data/mamba-readout-characterization.json').read_text())

    def test_hidden_body_complete(self):
        meta,body=metadata.parse(self.text)
        self.assertTrue(body.startswith('# Computational State\n'))
        self.assertNotIn('docs:metadata',body)
        html=site.render(body)
        self.assertIn('<h1',html);self.assertIn('<table>',html)
        self.assertNotIn('publication:',html)

    def test_centered_readme_brand_is_root_only(self):
        text=(ROOT/'README.md').read_text()
        meta,body=metadata.parse(text,branded_root=True)
        self.assertEqual(meta['id'],'yvex')
        self.assertTrue(body.startswith('<p align="center">\n  <picture>'))
        self.assertIn('<p align="center">',site.render(body))
        self.assertEqual(body.count('https://img.shields.io/'),5)
        with self.assertRaisesRegex(ValueError,'visible Markdown title'):
            metadata.parse(text)
        with self.assertRaisesRegex(ValueError,'visible Markdown title'):
            metadata.parse(text.replace('alt="YVEX"','alt="Other"',1),branded_root=True)

    def test_visible_yaml_refuses(self):
        visible=self.text.replace('<!-- docs:metadata','---',1).replace('-->','---',1)
        with self.assertRaisesRegex(ValueError,'visible YAML'):metadata.parse(visible)
        self.assertEqual(metadata.parse(visible,allow_visible=True),metadata.parse(self.text))

    def test_metadata_refusals(self):
        invalid=[self.text.replace('owner: runtime','owner: runtime\nowner: compiler',1),
          self.text.replace('owner: runtime','owner: imaginary',1),
          self.text.replace('status: mixed','status: established',1),
          self.text.replace('owner: runtime','owner: runtime\nmodel_support: complete',1),
          self.text.replace('pdf: true','pdf: yesplease',1),
          self.text.replace('# Computational State','# Competing Title',1),
          self.text+'\n<header>Page chrome</header>\n']
        for text in invalid:
            with self.subTest(text=text[:180]),self.assertRaises(ValueError):metadata.parse(text)

    def test_registry_identity_and_related(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);(root/'docs').mkdir()
            text=self.text
            (root/'README.md').write_text(text)
            (root/'docs/README.md').write_text(text)
            with self.assertRaisesRegex(ValueError,'duplicate document ID'):metadata.registry(root)
            (root/'docs/README.md').unlink()
            with self.assertRaisesRegex(ValueError,'invalid related'):metadata.registry(root)

    def test_plane_and_publication_refusals(self):
        text=self.text.replace('document: architecture-plane','document: reference',1)
        with self.assertRaisesRegex(ValueError,'plane'):metadata.parse(text)
        text=self.text.replace('html: true','html: false',1)
        with self.assertRaisesRegex(ValueError,'PDF'):metadata.parse(text)

    def test_details_remain_markdown(self):
        result=site.render('<details>\n<summary>Exact exits</summary>\n\n| ID | State |\n| --- | --- |\n| A | COMPLETE |\n\n</details>')
        self.assertIn('<table>',result);self.assertIn('<summary>',result)

    def test_github_anchor_and_link_projection(self):
        self.assertEqual(site.slug('E, L and three continuity classes'),'e-l-and-three-continuity-classes')
        source=ROOT/'docs/project-control/TASKS.md';target=ROOT/'docs/architecture/README.md'
        result=site.rewrite('<a href="../architecture/README.md#logical-planes">Plane</a>',source,site.OUT/'docs/project-control/TASKS.html',{target})
        self.assertIn('../architecture/index.html#logical-planes',result)

    def test_benchmark_missing_provenance_not_promoted(self):
        record=copy.deepcopy(self.record);record['kind']='benchmark'
        with self.assertRaisesRegex(ValueError,'provenance'):benchmarks.validate(record)

    def test_benchmark_negative_paths(self):
        records=[]
        for key,value in [('value',float('nan')),('samples',0),('unit','ms')]:
            r=copy.deepcopy(self.record);r['measurements'][0][key]=value;records.append(r)
        r=copy.deepcopy(self.record);r['measurements'].append(r['measurements'][0]);records.append(r)
        r=copy.deepcopy(self.record);r['context']['source_commit']='b5f';records.append(r)
        r=copy.deepcopy(self.record);r['kind']='fixture';records.append(r)
        for key,value in [('device',[]),('date','2026-02-31'),('run_id',True),('model','')]:
            r=copy.deepcopy(self.record);r['context'][key]=value;records.append(r)
        for record in records:
            with self.subTest(record=record['id']),self.assertRaises(ValueError):benchmarks.validate(record)

    def test_duplicate_evidence_keys_refuse(self):
        with self.assertRaisesRegex(ValueError,'duplicate observation key'):
            benchmarks.load_observation('{"context":{"device":"a","device":"b"}}')

    def test_qualification_population_missing_is_not_zero(self):
        record = dict(measurements=[dict(case='coding/turn-0')],
                      provenance=dict(observations=[dict(generated_tokens=256, accepted_tokens=0)]))
        text = '\n'.join(benchmarks.qualification_populations(record))
        self.assertIn('| Committed output | 256 | token | 1 |', text)
        self.assertIn('| Accepted draft | 0 | token | 1 |', text)
        self.assertIn('| Reasoning | NOT MEASURED |', text)
        record['provenance']['observations'][0]['generated_tokens'] = float('nan')
        with self.assertRaisesRegex(ValueError, 'invalid runtime population'):
            benchmarks.qualification_populations(record)
        record['measurements'].append(dict(case='other/turn-0'))
        with self.assertRaisesRegex(ValueError, 'ambiguous'):
            benchmarks.qualification_populations(record)

    def test_qualification_population_separates_actual_histories(self):
        record = dict(measurements=[
            dict(case='coding/turn-1/input-a', prompt_identity='a'),
            dict(case='coding/turn-1/input-b', prompt_identity='b')],
            provenance=dict(observations=[
                dict(case='coding', turn_index=1, input_identity='a', generated_tokens=10),
                dict(case='coding', turn_index=1, input_identity='b', generated_tokens=100),
                dict(case='coding', turn_index=1, input_identity='b', generated_tokens=200)]))
        text = '\n'.join(benchmarks.qualification_populations(record))
        first, second = text.split('### coding/turn-1/input-b')
        self.assertIn('### coding/turn-1/input-a', first)
        self.assertIn('| Committed output | 10 | token | 1 |', first)
        self.assertIn('| Committed output | 150 | token | 2 |', second)
        self.assertNotIn('| Committed output | 100 | token | 3 |', text)
        for identity in [None, '', 'unmatched']:
            bad = copy.deepcopy(record)
            bad['provenance']['observations'][0]['input_identity'] = identity
            with self.subTest(identity=identity), self.assertRaises(ValueError):
                benchmarks.qualification_populations(bad)
        record['measurements'].append(dict(case='coding/turn-1'))
        with self.assertRaisesRegex(ValueError, 'mixed grouped and ungrouped'):
            benchmarks.qualification_populations(record)

    def test_qualification_population_refuses_wrong_input_identity(self):
        record = dict(measurements=[dict(case='coding/turn-0', prompt_identity='expected')],
                      provenance=dict(observations=[dict(case='coding', turn_index=0,
                                                       input_identity='different', generated_tokens=256)]))
        with self.assertRaisesRegex(ValueError, 'differs from measurement input identity'):
            benchmarks.qualification_populations(record)

    def test_qualification_diagnostics_keep_zero_and_unknown_separate(self):
        record = json.loads((ROOT/'docs/evaluation/benchmarks/qualification/deepseek-native-residency-diagnostic-15.json').read_text())
        record['provenance']['diagnostics'] = [dict(id='measured', case='fixture', value=0,
            unit='byte', definition='Measured diagnostic', evidence='fixture'),
            dict(id='unknown', case='fixture', value=None, unit='byte',
                 definition='Unavailable diagnostic', evidence='fixture')]
        with tempfile.TemporaryDirectory() as directory:
            area = Path(directory)
            (area/'qualification').mkdir()
            (area/'qualification/fixture.json').write_text(json.dumps(record))
            with patch.object(benchmarks, 'AREA', area):
                views = benchmarks.qualification_views()
            page = views[area/'generated'/('qualification-'+record['id']+'.md')]
        self.assertIn('| measured / fixture | 0 | byte |', page)
        self.assertIn('| unknown / fixture | NOT MEASURED | byte |', page)
        self.assertIn('not timed benchmark samples', page)
        self.assertIn('No quality or performance metric is published', page)

    def test_independent_continuation_view_is_not_a_quality_pass(self):
        target = dict(schema=benchmarks.qualification.TARGET_SCHEMA,
                      **{key:None for key in benchmarks.qualification.TARGET_FIELDS})
        comparison = dict(state='CHARACTERIZED', exact_input_tokens=True, first_token_agreement=False,
            exact_bounded_continuation=False, greedy_prefix_tokens=0, reference_sampled_tokens=256,
            candidate_sampled_tokens=256, candidate_committed_tokens=256, comparison_policy='fixture comparison',
            unavailable_metrics=['PPL', 'KL'])
        row = dict(case='coding', input_identity='fixture', prompt_tokens=53, maximum_output=256,
                   candidate_sha256='a'*64, reference_capture_sha256='b'*64, evidence='fixture', comparison=comparison)
        provenance = dict(source_stability='fixture', evidence_class='fixture',
                          independent_implementation=dict(name='fixture-oracle', revision='c'*40, executable_sha256='d'*64))
        record = benchmarks.qualification.continuation_receipt(target, [row], provenance, 'fixture-comparison', 'Fixture only')
        with tempfile.TemporaryDirectory() as directory:
            area = Path(directory)
            (area/'qualification').mkdir()
            (area/'qualification/fixture.json').write_text(json.dumps(record))
            with patch.object(benchmarks, 'AREA', area):
                views = benchmarks.qualification_views()
            page = views[area/'generated/qualification-fixture-comparison.md']
        self.assertIn('| coding | 53 | DIFFERS | 0 | 256 / 256 | 256 | DIFFERS |', page)
        self.assertIn('teacher-forced NLL/PPL, KL and RMS probability delta: **NOT MEASURED**', page)
        self.assertIn('| representation-quality | BLOCKED |', page)
        self.assertNotIn('256 token/s', page)

    def test_pdf_links_remain_inside_dossier(self):
        owners={'docs/architecture/index.html':'yvex.architecture'}
        result=site.book_links('<a href="docs/architecture/index.html#logical-planes">Plane</a>',owners)
        self.assertEqual(result,'<a href="#yvex.architecture--logical-planes">Plane</a>')
        self.assertEqual(site.book_links('<a href="https://example.com">External</a>',owners),'<a href="https://example.com">External</a>')

    def test_observation_projections_share_values(self):
        record=benchmarks.validate(self.record)
        figure=benchmarks.figure(record);page=benchmarks.markdown(record,Path(record['id']+'.json'))
        for m in record['measurements']:
            self.assertIn(str(m['value']),page)
            self.assertIn(f'{m["value"]:g}',figure)
        self.assertIn('CHARACTERIZATION',page);self.assertIn('role="img"',figure)
        self.assertEqual(figure,benchmarks.figure(copy.deepcopy(record)))

    def test_fixture_cannot_hide_its_kind(self):
        path=ROOT/'docs/evaluation/benchmarks/data/fixture-publication.json'
        record=benchmarks.validate(json.loads(path.read_text()))
        self.assertIn('FIXTURE',benchmarks.markdown(record,path))
        record['kind']='characterization'
        with self.assertRaises(ValueError):benchmarks.validate(record)

    def test_all_canonical_pages_render(self):
        for doc in metadata.registry().values():
            with self.subTest(path=str(doc['path'])):
                rendered=site.render(doc['body'])
                if doc['path']==ROOT/'README.md' and not doc['body'].startswith('# '):
                    self.assertIn('alt="YVEX"',rendered)
                else:self.assertIn('<h1',rendered)
                self.assertNotIn('<!-- docs:metadata',rendered)

    def test_mermaid_and_static_projection_share_graph(self):
        import importlib.util
        spec=importlib.util.spec_from_file_location('diagrams',ROOT/'tools/render_diagrams.py')
        diagrams=importlib.util.module_from_spec(spec);spec.loader.exec_module(diagrams)
        path=ROOT/'docs/assets/diagrams/physical_compilation.json'
        data=json.loads(path.read_text());block=diagrams.diagram_block(path.stem,data,ROOT/'README.md')
        self.assertIn('```mermaid',block)
        self.assertIn('"background": "transparent"',block)
        self.assertIn('style n_panel_0 fill:transparent,stroke:#b8a5d0',block)
        self.assertNotIn('fill:#faf8fe',block)
        svg=diagrams.render(data)
        self.assertIn('.panel{fill:none;',svg)
        self.assertNotIn(f'<rect width="{data["size"][0]}" height="{data["size"][1]}" fill="#fff"/>',svg)
        self.assertIn('n_execution ---|identity| n_join',block)
        self.assertIn('n_peir ---|identity| n_join',block)
        self.assertIn('physical_compilation.svg',site.render(block))
        data['edges'][0]['target']='invented'
        with self.assertRaisesRegex(ValueError,'endpoint'):diagrams.mermaid(data)

    def test_branded_root_does_not_weaken_document_headers(self):
        text=(ROOT/'README.md').read_text()
        meta,body=metadata.parse(text,branded_root=True)
        no_title=text.replace('# '+meta['title']+'\n','').replace('**From model source to verified execution.**\n','')
        with self.assertRaisesRegex(ValueError,'title'):metadata.parse(no_title)

if __name__=='__main__':unittest.main()
