#!/usr/bin/env python3
"""Validate YVEX documentation ownership, project control and publication inputs."""
from __future__ import annotations
import argparse, copy, hashlib, importlib.util, json, re, subprocess, sys, tempfile
import xml.etree.ElementTree as ET
from pathlib import Path
from urllib.parse import unquote
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/docs'))
import metadata
import benchmarks
STATES=('🟢 ESTABLISHED','🟡 PARTIAL','🔴 OPEN','⚪ LATER')
TASK_STATES=('✅ COMPLETE','🔵 IN PROGRESS','⬜ READY','⛔ BLOCKED')
PROGRAMS=set('RCPSNGDOMXQF')
def fail(message: str) -> None:
    print(f"documentation architecture: {message}", file=sys.stderr)
    raise SystemExit(1)


def markdown_paths() -> set[str]:
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "--", "*.md"],
        cwd=ROOT,
        check=True,
        text=True,
        capture_output=True,
    )
    return {
        line
        for line in result.stdout.splitlines()
        if line and (ROOT / line).is_file() and not line.startswith("build/")
    }


def github_anchors(path: Path) -> set[str]:
    anchors: set[str] = set()
    counts: dict[str, int] = {}
    in_fence = False
    for raw in path.read_text(encoding="utf-8").splitlines():
        if raw.lstrip().startswith(("~~~", "```")):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        match = re.match(r"^#{1,6}\s+(.+?)\s*#*\s*$", raw)
        if not match:
            continue
        heading = re.sub(r"!?\[([^]]*)\]\([^)]+\)", r"\1", match.group(1))
        heading = re.sub(r"<[^>]+>", "", heading)
        slug = heading.replace("`", "").replace("*", "").strip().lower()
        slug = re.sub(r"[^\w\- ]", "", slug, flags=re.UNICODE)
        slug = re.sub(r"\s+", "-", slug)
        ordinal = counts.get(slug, 0)
        counts[slug] = ordinal + 1
        anchors.add(slug if ordinal == 0 else f"{slug}-{ordinal}")
    return anchors


def link_targets(text: str) -> list[str]:
    """Current docs use inline/reference Markdown and HTML image/link targets."""
    text = re.sub(r"(?ms)^\s*(`{3,}|~{3,})[^\n]*\n.*?^\s*\1\s*$", "", text)
    pattern = re.compile(r"!?\[[^]]*\]\(([^)\s]+)(?:\s+['\"][^'\"]*['\"])?\)")
    references = re.findall(r"(?m)^\s*\[[^]]+\]:\s*(\S+)", text)
    definitions = {key.casefold() for key in re.findall(r"(?m)^\s*\[([^]]+)\]:", text)}
    for label, key in re.findall(r"!?\[([^]\n]+)\]\[([^]\n]*)\]", text):
        if (key or label).casefold() not in definitions:
            fail(f"undefined Markdown reference: [{label}][{key}]")
    html = re.findall(r"\b(?:src|href)=[\"']([^\"']+)[\"']", text)
    return pattern.findall(text) + references + html


def check_links(paths: set[str]) -> set[Path]:
    anchor_cache: dict[Path, set[str]] = {}
    destinations: set[Path] = set()
    for relative in sorted(paths):
        source = ROOT / relative
        for raw_target in link_targets(source.read_text(encoding="utf-8")):
            target = raw_target.strip("<>")
            if target.startswith(("http://", "https://", "mailto:", "data:")):
                continue
            target, _, fragment = target.partition("#")
            destination = source if not target else (source.parent / unquote(target)).resolve()
            try:
                destination.relative_to(ROOT)
            except ValueError:
                fail(f"{relative} links outside repository: {raw_target}")
            if destination.is_dir() and (destination / "README.md").is_file():
                destination = destination / "README.md"
            if not destination.exists():
                fail(f"{relative} has unresolved link: {raw_target}")
            destinations.add(destination)
            if fragment and destination.suffix == ".md":
                anchors = anchor_cache.setdefault(destination, github_anchors(destination))
                if fragment not in anchors:
                    fail(f"{relative} has unresolved anchor: {raw_target}")
    return destinations


def check_assets(destinations: set[Path]) -> None:
    """Visual projections need real readers, paired sources, and accessible SVGs."""
    assets = set((ROOT / "docs/assets/diagrams").glob("*"))
    assets.update((ROOT / "docs").glob("*.svg"))
    digests: dict[str, Path] = {}
    for path in sorted(assets):
        if path not in destinations:
            fail(f"unconsumed documentation asset: {path.relative_to(ROOT)}")
        if path.suffix == ".json" and path.with_suffix(".svg") not in assets:
            fail(f"diagram lacks SVG projection: {path.name}")
        if path.suffix != ".svg":
            continue
        if path.parent.name == "diagrams" and path.with_suffix(".json") not in assets:
            fail(f"diagram lacks editable source: {path.name}")
        text = path.read_text(encoding="utf-8")
        for marker in ('<svg ', '<title ', '<desc ', 'role="img"'):
            if marker not in text:
                fail(f"inaccessible SVG {path.name}: absent {marker}")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest in digests:
            fail(f"duplicate visual assets: {digests[digest].name}, {path.name}")
        digests[digest] = path


def check_figure_generation() -> None:
    """Exercise the real renderer, plus adversarial layout and stale-output seams."""
    spec = importlib.util.spec_from_file_location("render_diagrams", ROOT / "tools/render_diagrams.py")
    renderer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(renderer)
    numbers = set()
    for path in sorted((ROOT / "docs/assets/diagrams").glob("*.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        if data["number"] in numbers:
            fail("duplicate canonical figure number")
        numbers.add(data["number"])
        svg = renderer.render(data)
        if svg != renderer.render(copy.deepcopy(data)):
            fail(f"non-deterministic figure: {path.name}")
        ET.fromstring(svg)
        owner = (ROOT / data["owner"]).read_text(encoding="utf-8")
        if path.name not in owner or path.with_suffix(".svg").name not in owner:
            fail(f"figure has no consuming owner: {path.name}")
        for mutation in ("overflow", "overlap", "diagonal", "crossing", "authority", "class", "presentation", "unknown"):
            bad = copy.deepcopy(data)
            if mutation == "overflow":
                bad["nodes"][0]["lines"][0] = "overflow " * 100
            elif mutation == "overlap":
                bad["nodes"][1]["box"] = list(bad["nodes"][0]["box"])
            elif mutation == "diagonal":
                bad["edges"][0]["points"] = [[40, 100], [60, 120]]
            elif mutation == "crossing":
                x, y, w, h = bad["nodes"][0]["box"]
                bad["edges"][0]["points"] = [[x, y+h/2], [x+w, y+h/2]]
            elif mutation == "authority":
                bad["authority"] = ["/outside-repository"]
            elif mutation == "class":
                bad["nodes"][0]["kind"] = "invented"
            elif mutation == "presentation":
                bad["presentation"] = "invented"
            else:
                bad["unsupported"] = True
            try:
                renderer.render(bad)
            except ValueError:
                continue
            fail(f"figure renderer accepted {mutation}: {path.name}")
        with tempfile.TemporaryDirectory(prefix="yvex-figure-check-") as temporary:
            output = Path(temporary) / "fixture.svg"
            output.write_text(svg, encoding="utf-8")
            renderer.check_output(output, svg)
            output.write_text(svg + "stale", encoding="utf-8")
            try:
                renderer.check_output(output, svg)
            except ValueError:
                if output.read_text(encoding="utf-8") != svg + "stale":
                    fail("stale-output check mutated the projection")
            else:
                fail("stale SVG was accepted")
    if numbers != set(range(1, 16)):
        fail(f"unexpected canonical figure set: {sorted(numbers)}")
    result = subprocess.run([sys.executable, str(ROOT / "tools/render_diagrams.py"), "--check"],
                            cwd=ROOT, check=False)
    if result.returncode:
        fail("figure source and SVG are not synchronized")
    architecture = (ROOT / "docs/architecture/README.md").read_text(encoding="utf-8")
    if '](../assets/diagrams/system_overview.svg)' not in architecture or '```mermaid' in architecture:
        fail("architecture must expose the canonical fixed-layout SVG")
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    for name in ("product_pipeline", "product_runtime"):
        if f'](docs/assets/diagrams/{name}.svg)' not in readme or f'{name}.json' not in readme:
            fail(f"missing product SVG / editable source: {name}")


def maturity(text, check_counts=True):
    counts=dict.fromkeys(STATES,0); names=set(); headers=0
    for line in text.splitlines():
        if not line.startswith('|'): continue
        cells=[x.strip() for x in line.strip('|').split('|')]
        if cells==['Capability','State','Current YVEX truth','Boundary required for promotion','Program','Evidence / owner']:
            headers+=1;continue
        if len(cells)!=6 or all(re.fullmatch(':?-+:?',x) for x in cells):continue
        name,state,truth,exit_,program,evidence=cells
        metadata.require(all(cells),'empty capability field')
        metadata.require(state in STATES,'unsupported capability state')
        metadata.require(name not in names,'duplicate capability');names.add(name)
        metadata.require(set(program.split(' / '))<=PROGRAMS,'unknown program')
        metadata.require(bool(re.search(r'\[[^]]+\](?:\([^)]*\)|\[[^]]+\])',evidence)),'missing capability evidence/owner')
        counts[state]+=1
    metadata.require(headers and names,'empty capability matrix')
    values=[*counts.values(),sum(counts.values())]
    projection='<!-- maturity-counts:start -->\n| Established | Partial | Open | Later | Total |\n| ---: | ---: | ---: | ---: | ---: |\n| '+' | '.join(map(str,values))+' |\n<!-- maturity-counts:end -->'
    found=re.findall(r'<!-- maturity-counts:start -->.*?<!-- maturity-counts:end -->',text,re.S)
    metadata.require(len(found)==1 and (not check_counts or found[0]==projection),'stale maturity counts')
    spectrum=[]
    for line in text.splitlines():
        if re.match(r'^\| A\d+ \|',line):
            cells=[x.strip() for x in line.strip('|').split('|')]
            metadata.require(len(cells)==5 and cells[4].split(' / ')[0] in {'PLANNED','PARTIAL','BLOCKED','COMPLETE','DONE','DEFERRED'},'invalid spectrum row')
            spectrum.append(cells[0])
    metadata.require(spectrum==[f'A{i:02}' for i in range(1,13)],'spectrum identity/order changed')
    return counts,projection


def tasks(text, check_counts=True):
    counts=dict.fromkeys(TASK_STATES,0);ids=set()
    for line in text.splitlines():
        if not line.startswith('| `'):continue
        cells=[x.strip() for x in line.strip('|').split('|')]
        metadata.require(len(cells)==5 and all(cells),'invalid Task schema')
        tid=cells[0].strip('`')
        metadata.require(bool(re.fullmatch(r'[A-Z0-9_]+(?:\.[A-Z0-9_]+)+',tid)) and tid not in ids,'invalid/duplicate Task ID')
        metadata.require(cells[3] in TASK_STATES,'unsupported Task state')
        ids.add(tid);counts[cells[3]]+=1
    metadata.require(ids,'empty Task board')
    values=[len(ids),*counts.values()]
    projection='<!-- task-counts:start -->\n| Total selected | Complete | In progress | Ready | Blocked |\n| ---: | ---: | ---: | ---: | ---: |\n| '+' | '.join(map(str,values))+' |\n<!-- task-counts:end -->'
    found=re.findall(r'<!-- task-counts:start -->.*?<!-- task-counts:end -->',text,re.S)
    metadata.require(len(found)==1 and (not check_counts or found[0]==projection),'stale Task counts')
    ratio=re.findall(r'\*\*(\d+)/(\d+) selected Tasks complete\.\*\*',text)
    metadata.require(not check_counts or ratio==[(str(counts[TASK_STATES[0]]),str(len(ids)))],'stale Task completion ratio')
    return counts,projection


def check_control():
    status=(ROOT/'docs/project-control/STATUS.md').read_text()
    task=(ROOT/'docs/project-control/TASKS.md').read_text()
    counts,_=maturity(status);tc,_=tasks(task)
    cap=next(l for l in status.splitlines() if '| 🟢 ESTABLISHED |' in l and l.count('|')==7)
    row=next(l for l in task.splitlines() if l.startswith('| `'))
    bad_cases=[(maturity,status.replace(cap,cap+'\n'+cap),'duplicate capability'),
      (maturity,status.replace(cap,cap.replace('🟢 ESTABLISHED','GREEN')),'bad capability state'),
      (maturity,status.replace('| Established |','| Stale |'),'stale capability counts'),
      (maturity,status.replace('| A12 |','| A13 |'),'lost spectrum identity'),
      (tasks,task.replace(row,row+'\n'+row),'duplicate Task'),
      (tasks,task.replace(row,row.replace(row.split('|')[4].strip(),'🟢 ESTABLISHED')),'capability used as Task state'),
      (tasks,task.replace('| Total selected |','| Stale |'),'stale Task counts')]
    for fn,bad,name in bad_cases:
        try:fn(bad)
        except ValueError:continue
        fail('control guard accepted '+name)
    # A legal capability transition updates counts instead of freezing current maturity.
    moved=status.replace(cap,cap.replace('🟢 ESTABLISHED','🟡 PARTIAL'),1)
    _,projection=maturity(moved,False)
    moved=re.sub(r'<!-- maturity-counts:start -->.*?<!-- maturity-counts:end -->',projection,moved,flags=re.S)
    metadata.require(maturity(moved)[0]!=counts,'count regeneration ignored changed maturity')
    roadmap=(ROOT/'ROADMAP.md').read_text()
    metadata.require(len(roadmap.splitlines())<180,'Roadmap absorbed a deep owner')
    metadata.require('Active Next:' not in roadmap and '| Capability |' not in roadmap,'Roadmap duplicates current control')
    print(f'project control: maturity={sum(counts.values())}; Tasks={sum(tc.values())}; negative={len(bad_cases)}')


def check_registry():
    docs=metadata.registry();paths={doc['path'] for doc in docs.values()}
    landings=['product','architecture','project-control','contracts','model-families','guides','evaluation','research','decisions','reference','releases']
    for area in landings:
        metadata.require(ROOT/f'docs/{area}/README.md' in paths,'missing README landing: '+area)
    for relative in ['docs/TEMP.md','docs/architecture/system.md','docs/architecture/compilation.md','docs/architecture/runtime.md','docs/development','docs/index.md','docs/work','docs/' + 'archive','PROJECT.md']:
        metadata.require(not (ROOT/relative).exists(),'retired owner remains: '+relative)
    graph={}
    for path in paths:
        targets=set()
        for link in link_targets(path.read_text()):
            if re.match(r'^[a-z]+:',link):continue
            file=link.split('#')[0]
            dest=(path.parent/unquote(file)).resolve() if file else path
            if dest.is_dir():dest=dest/'README.md'
            if dest in paths:targets.add(dest)
        graph[path]=targets
    seen=set();pending=[ROOT/'docs/README.md']
    while pending:
        path=pending.pop()
        if path in seen:continue
        seen.add(path);pending.extend(graph[path]-seen)
    metadata.require(not(paths-seen),'orphan canonical documents: '+str(sorted(str(p.relative_to(ROOT)) for p in paths-seen)))
    # Keep the public protocol/header relationship from the original guard.
    server=(ROOT/'include/yvex/server.h').read_text()
    version=re.search(r'#define YVEX_LOCAL_PROTOCOL_VERSION (\d+)u',server)[1]
    metadata.require(f'YVEX_LOCAL_PROTOCOL_VERSION = {version}' in (ROOT/'docs/contracts/local-protocol.md').read_text(),'protocol/header drift')
    metadata.require(json.loads((ROOT/'docs/reference/document.schema.json').read_text())==metadata.schema(),'metadata schema drift')
    readme=(ROOT/'README.md').read_text()
    metadata.require(not re.search(r'production-ready|blazing fast|state of the art|enterprise-grade|revolutionary',readme,re.I),'unsupported README marketing claim')
    metadata.require(not re.search(r'/home/|/Users/|\$HOME/',readme),'local paths in product front door')
    agents=(ROOT/'AGENTS.md').read_text()
    for required in ['Start from a Task','Documentation and Task closure','documentation impact','progression_decision','downstream_safe']:
        metadata.require(required.lower() in agents.lower(),'agent routing missing: '+required)
    print(f'document registry: {len(docs)} canonical owners; {sum("plane" in d["meta"] for d in docs.values())} planes; no orphans')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--roadmap-only',action='store_true',help='compatibility alias: validate all project control')
    parser.add_argument('--update-roadmap-counts',action='store_true',help='compatibility alias: update Status and Task projections')
    parser.add_argument('--update-counts',action='store_true')
    args=parser.parse_args()
    try:
        if args.update_counts or args.update_roadmap_counts:
            for filename,fn,marker in [('STATUS.md',maturity,'maturity'),('TASKS.md',tasks,'task')]:
                path=ROOT/'docs/project-control'/filename;text=path.read_text();counts,projection=fn(text,False)
                text=re.sub(f'<!-- {marker}-counts:start -->.*?<!-- {marker}-counts:end -->',projection,text,flags=re.S)
                if marker=='task':text=re.sub(r'\*\*\d+/\d+ selected Tasks complete\.\*\*',f'**{counts[TASK_STATES[0]]}/{sum(counts.values())} selected Tasks complete.**',text)
                path.write_text(text)
            print('Status/Task projections synchronized; legacy flag no longer writes ROADMAP.')
        check_control()
        if args.roadmap_only:return 0
        check_registry();check_assets(check_links(markdown_paths()));check_figure_generation();benchmarks.generate(check=True)
        print('documentation architecture: PASS')
        return 0
    except (ValueError,KeyError,TypeError) as exc:
        fail(str(exc))

if __name__=='__main__':
    raise SystemExit(main())
