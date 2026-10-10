#!/usr/bin/env python3
"""Render the bounded, fixed-layout documentation figures using only stdlib.

JSON owns the typed graph and print coordinates; this file owns Mermaid and SVG projections. No
layout engine, font download, timestamp, random ID, script, or external asset
enters a render. This is a documentation projection, not an architecture DB.
"""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import math
import os
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT / "docs/assets/diagrams"
KINDS = {"external": "X", "interface": "I", "semantic": "S", "runtime": "R",
         "mutable": "M", "physical": "P", "evidence": "E"}
EDGES = {"flow": ("", "solid", "data / result"),
         "control": ("9 5", "open", "control / request"),
         "observe": ("2 5", "open", "observation"),
         "bind": ("9 4 2 4", "none", "identity binding"),
         "lifecycle": ("", "open", "lifecycle / gate")}
STYLE = """
text{fill:#261b38}
.title{font-size:26px;font-weight:700}.panel-title{font-size:19px;font-weight:700}
.node-title{font-size:20px;font-weight:700}.body{font-size:18px}
.note,.legend{font-size:16px}.code{font-size:16px}
.tag{font-size:15px;font-weight:700;text-anchor:middle}
.label{font-size:16px}
.panel{fill:none;stroke:#c6b6db;stroke-width:1}
.node{fill:#fff;stroke:#665275;stroke-width:1.5}
.semantic{fill:#efe5fc;stroke:#7541ba}.physical{fill:#f4effb;stroke:#8054b2}
.runtime{fill:#eeeafb;stroke:#6a4ca3}.mutable{fill:#fff3db;stroke:#8e6920}
.interface{fill:#edf3fb;stroke:#456789}.external{fill:#f2f2f4;stroke:#707078}
.evidence{fill:#eaf5ef;stroke:#3d7255}
.external{stroke-dasharray:6 4}.runtime{stroke-width:2.5}.mutable{stroke-width:2}
.edge{fill:none;stroke:#292929;stroke-width:1.7;stroke-linejoin:round}
@media(prefers-color-scheme:dark){.title,.legend,.panel-title{fill:#e9e2f4}.edge{stroke:#b9b0c5}#solid path{fill:#b9b0c5}#open path{stroke:#b9b0c5}}
""".strip()

# Landing-page figures project the same typed graph, with a self-contained
# palette that remains legible on both GitHub themes and in offline readers.
PRODUCT_STYLE = """
text{fill:#f5f2ff;font-family:DejaVu Sans,Arial,sans-serif}
.eyebrow{fill:#baa1ff;font-size:14px;font-weight:700;letter-spacing:2px}
.title{font-size:40px;font-weight:700;letter-spacing:-1px}
.node-title{font-size:25px;font-weight:700;letter-spacing:-.5px}
.body{fill:#ccc6db;font-size:20px}.code{fill:#ccc6db;font-size:16px}
.note,.label,.legend{fill:#b6accb;font-size:17px}
.panel-title{fill:#d0baff;font-size:19px;font-weight:700}
.kind{fill:#baa1ff;font-size:12px;font-weight:700;letter-spacing:1.5px}
.panel{fill:#ffffff03;stroke:#594774;stroke-width:1;stroke-dasharray:6 6}
.edge{fill:none;stroke:#ab89f5;stroke-width:2;stroke-linejoin:round}
.node{fill:url(#card);stroke:#665086;stroke-width:1}
""".strip()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fields(record, required, optional=()):
    require(set(required) <= record.keys(), f"missing fields: {set(required) - record.keys()}")
    require(record.keys() <= set(required) | set(optional), f"unknown fields: {record.keys() - set(required) - set(optional)}")


def coordinate(value, count):
    require(isinstance(value, list) and len(value) == count and
            all(type(v) in (int, float) and math.isfinite(v) for v in value), "invalid coordinates")


def extent(text, size):
    # Conservative editorial guard, not a substitute for visual font review.
    narrow, wide = " ilI.,:;!'|", "MW@%≠→"
    return sum(.30 if c in narrow else .90 if c in wide else .58 for c in text) * size


def validate(data):
    fields(data, ("schema", "number", "title", "description", "size", "owner", "authority",
                  "panels", "nodes", "edges", "labels"), ("bands", "presentation"))
    require(data["schema"] == 1 and type(data["number"]) is int, "unsupported figure schema")
    require(data.get("presentation", "technical") in ("technical", "product"), "unknown presentation")
    product = data.get("presentation") == "product"
    coordinate(data["size"], 2)
    width, height = data["size"]
    require(width == 1200 and 600 <= height <= 1200, "use the common publication canvas")
    if product:
        require(extent(data["title"], 40) <= width-80, "product title overflow")
    require(data["description"] and data["authority"], "missing accessible description / authorities")
    for path in [data["owner"], *data["authority"]]:
        require(isinstance(path, str) and not Path(path).is_absolute() and
                ".." not in Path(path).parts and (ROOT / path).is_file(), f"missing authority: {path}")
    for collection in ("panels", "nodes", "bands"):
        for item in data.get(collection, []):
            coordinate(item["box"], 4)
            x, y, w, h = item["box"]
            require(x >= 24 and y >= 70 and w > 0 and h > 0 and
                    x + w <= width - 24 and y + h <= height - 115, "box outside content area")
    ids = set()
    for panel in data["panels"]:
        fields(panel, ("box", "title"))
        require(extent(panel["title"], 19) <= panel["box"][2] - 28, "panel title overflow")
    for node in data["nodes"]:
        fields(node, ("id", "box", "kind", "title", "lines"), ("code",))
        require(node["id"] not in ids, "duplicate node identity")
        ids.add(node["id"])
        require(node["kind"] in KINDS, "unknown semantic class")
        require(extent(node["title"], 25 if product else 20) <= node["box"][2] - (48 if product else 58), f"title overflow: {node['id']}")
        require(58 + max(0, len(node["lines"]) - 1) * 25 <= node["box"][3] - 12
                if node["lines"] else node["box"][3] >= 48, f"vertical overflow: {node['id']}")
        if product:
            require(99 + max(0, len(node["lines"])-1)*27 <= node["box"][3]-20,
                    f"product vertical overflow: {node['id']}")
        for index, line in enumerate(node["lines"]):
            require(isinstance(line, str) and "\n" not in line, "use explicit text lines")
            needed = len(line) * 9.7 if index in node.get("code", []) else extent(line, 20 if product else 18)
            require(needed <= node["box"][2] - (48 if product else 32), f"body overflow: {node['id']}: {line}")
    for index, node in enumerate(data["nodes"]):
        x, y, w, h = node["box"]
        for other in data["nodes"][index + 1:]:
            a, b, c, d = other["box"]
            require(not (x < a+c and a < x+w and y < b+d and b < y+h),
                    f"overlapping nodes: {node['id']}, {other['id']}")
    for edge in data["edges"]:
        fields(edge, ("kind", "points", "source", "target"))
        endpoints = ids | {f'panel-{i}' for i in range(len(data['panels']))}
        require(edge['source'] in endpoints and edge['target'] in endpoints, 'unknown semantic edge endpoint')
        require(edge["kind"] in EDGES and len(edge["points"]) >= 2, "invalid edge")
        for point in edge["points"]:
            coordinate(point, 2)
            require(24 <= point[0] <= width-24 and 70 <= point[1] <= height-115,
                    "edge outside content area")
        for a, b in zip(edge["points"], edge["points"][1:]):
            require((a[0] == b[0]) != (a[1] == b[1]), "edges must be orthogonal and nonzero")
            for node in data["nodes"]:
                x, y, w, h = node["box"]
                crossed = (y < a[1] < y+h and min(a[0], b[0]) < x+w and max(a[0], b[0]) > x
                           if a[1] == b[1] else
                           x < a[0] < x+w and min(a[1], b[1]) < y+h and max(a[1], b[1]) > y)
                require(not crossed, f"edge crosses node interior: {node['id']}")
    for label in data["labels"]:
        fields(label, ("at", "text"), ("style",))
        coordinate(label["at"], 2)
        require(label.get("style", "label") in ("label", "note", "panel-title", "code"), "unknown label style")
        x, y = label["at"]
        require(x >= 24 and y >= 70 and y <= height-115 and
                x + extent(label["text"], 19) < width-24, "label outside content area")
    for band in data.get("bands", []):
        fields(band, ("box", "text"))
        require(extent(band["text"], 18) <= band["box"][2]-20, "band overflow")


def text(x, y, value, style="body"):
    background = (f'<rect x="{x-3:g}" y="{y-16:g}" width="{extent(value, 16)+6:g}" '
                  'height="21" fill="#fff"/>') if style == "label" else ""
    font = "DejaVu Sans Mono, monospace" if style == "code" else "Liberation Sans, Arial, sans-serif"
    return background + f'<text x="{x:g}" y="{y:g}" class="{style}" font-family="{font}">{html.escape(value)}</text>'


def render(data):
    validate(data)
    width, height = data["size"]
    subject = hashlib.sha256(json.dumps(data, sort_keys=True, ensure_ascii=False,
                                       separators=(",", ":")).encode()).hexdigest()
    if data.get("presentation") == "product":
        return render_product(data, subject)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
           f'viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc">',
           f'<title id="title">{html.escape(data["title"])}</title>',
           f'<desc id="desc">{html.escape(data["description"])}</desc>',
           f'<!-- Generated by tools/render_diagrams.py; source SHA-256 {subject} -->',
           '<defs><style>' + STYLE + '</style>',
           '<marker id="solid" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto" markerUnits="userSpaceOnUse"><path d="M0 0L8 4L0 8Z" fill="#292929"/></marker>',
           '<marker id="open" markerWidth="9" markerHeight="10" refX="8" refY="5" orient="auto" markerUnits="userSpaceOnUse"><path d="M1 1L8 5L1 9" fill="none" stroke="#292929" stroke-width="1.5"/></marker></defs>',
           text(32, 41, f'{data["number"]:02d}  {data["title"]}', "title"),
           f'<path d="M32 57H{width-32}" stroke="#161616" stroke-width="2"/>']
    for panel in data["panels"]:
        x, y, w, h = panel["box"]
        svg += [f'<rect class="panel" x="{x}" y="{y}" width="{w}" height="{h}"/>',
                text(x+14, y+27, panel["title"], "panel-title")]
    for edge in data["edges"]:
        dash, arrow, _ = EDGES[edge["kind"]]
        points = " ".join(f'{x},{y}' for x, y in edge["points"])
        svg.append(f'<polyline class="edge" points="{points}" stroke-dasharray="{dash or "none"}" marker-end="{("url(#"+arrow+")") if arrow != "none" else "none"}"/>')
    for node in data["nodes"]:
        x, y, w, h = node["box"]
        kind = node["kind"]
        svg.append(f'<g id="{html.escape(node["id"], quote=True)}"><rect class="node {kind}" x="{x}" y="{y}" width="{w}" height="{h}"/>')
        if kind in ("mutable", "physical"):
            svg.append(f'<path d="M{x+5} {y+5}V{y+h-5}" stroke="#525252" stroke-width="{3 if kind == "physical" else 1}"/>')
        svg += [f'<rect x="{x+12}" y="{y+14}" width="23" height="23" fill="#f7f7f7" stroke="#525252"/>',
                text(x+23.5, y+31, KINDS[kind], "tag"), text(x+44, y+32, node["title"], "node-title")]
        for index, line in enumerate(node["lines"]):
            svg.append(text(x+16, y+58+index*25, line, "code" if index in node.get("code", []) else "body"))
        svg.append('</g>')
    for band in data.get("bands", []):
        x, y, w, h = band["box"]
        svg += [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="#e0e0e0" stroke="#525252"/>',
                text(x+10, y+22, band["text"])]
    for label in data["labels"]:
        svg.append(text(*label["at"], label["text"], label.get("style", "label")))
    svg += [f'<path d="M32 {height-95}H{width-32}" stroke="#a3a3a3"/>',
            text(32, height-69, 'X external · I interface · S semantic · R runtime · M mutable · P physical · E evidence', 'legend')]
    for index, kind in enumerate(dict.fromkeys(edge["kind"] for edge in data["edges"])):
        dash, arrow, title = EDGES[kind]
        x, y = 32 + 228*index, height-38
        svg += [f'<path class="edge" d="M{x} {y}h36" stroke-dasharray="{dash or "none"}" marker-end="{("url(#"+arrow+")") if arrow != "none" else "none"}"/>',
                text(x+46, y+5, title, "legend")]
    svg.append('</svg>')
    return "\n".join(svg) + "\n"


def render_product(data, subject):
    """Editorial SVG view; graph semantics and provenance still come from JSON."""
    width, height = data["size"]
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
           f'viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc">',
           f'<title id="title">{html.escape(data["title"])}</title>',
           f'<desc id="desc">{html.escape(data["description"])}</desc>',
           f'<!-- Generated by tools/render_diagrams.py; source SHA-256 {subject} -->',
           '<defs><style>'+PRODUCT_STYLE+'</style>',
           '<linearGradient id="canvas" x2="1" y2="1"><stop stop-color="#201332"/><stop offset="1" stop-color="#100e19"/></linearGradient>',
           '<linearGradient id="card" x2="0" y2="1"><stop stop-color="#30213f"/><stop offset="1" stop-color="#211a2d"/></linearGradient>',
           '<radialGradient id="glow"><stop stop-color="#8950e5" stop-opacity=".2"/><stop offset="1" stop-color="#8950e5" stop-opacity="0"/></radialGradient>',
           '<marker id="solid" markerWidth="9" markerHeight="10" refX="8" refY="5" orient="auto" markerUnits="userSpaceOnUse"><path d="M1 1L8 5L1 9Z" fill="#ab89f5"/></marker>',
           '<marker id="open" markerWidth="9" markerHeight="10" refX="8" refY="5" orient="auto" markerUnits="userSpaceOnUse"><path d="M1 1L8 5L1 9" fill="none" stroke="#ab89f5" stroke-width="2"/></marker></defs>',
           f'<rect width="{width}" height="{height}" rx="24" fill="url(#canvas)"/>',
           '<ellipse cx="1020" cy="80" rx="450" ry="340" fill="url(#glow)"/>',
           f'<rect x=".5" y=".5" width="{width-1}" height="{height-1}" rx="24" fill="none" stroke="#4e3b67"/>',
           text(40, 40, f'YVEX / {data["number"]:02d} / EXECUTION ARCHITECTURE', 'eyebrow'),
           text(40, 94, data["title"], 'title')]
    for panel in data["panels"]:
        x, y, w, h = panel["box"]
        svg += [f'<rect class="panel" x="{x}" y="{y}" width="{w}" height="{h}" rx="18"/>',
                text(x+24, y+30, panel["title"], 'panel-title')]
    for edge in data["edges"]:
        dash, arrow, _ = EDGES[edge["kind"]]
        points = " ".join(f'{x},{y}' for x, y in edge["points"])
        marker = f'url(#{arrow})' if arrow != 'none' else 'none'
        svg.append(f'<polyline class="edge" points="{points}" stroke-dasharray="{dash or "none"}" marker-end="{marker}"/>')
    for node in data["nodes"]:
        x, y, w, h = node["box"]
        svg += [f'<g id="{html.escape(node["id"], quote=True)}"><rect class="node" x="{x}" y="{y}" width="{w}" height="{h}" rx="14"/>',
                f'<path d="M{x+24} {y+27}h16" stroke="#ad83ff" stroke-width="3"/>',
                text(x+52, y+31, node["kind"].upper(), 'kind'),
                text(x+24, y+66, node["title"], 'node-title')]
        for index, line in enumerate(node["lines"]):
            svg.append(text(x+24, y+99+index*27, line, 'code' if index in node.get('code', []) else 'body'))
        svg.append('</g>')
    for band in data.get("bands", []):
        x, y, w, h = band["box"]
        svg += [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="10" fill="#332244" stroke="#665086"/>',
                text(x+16, y+25, band["text"], 'note')]
    for label in data["labels"]:
        # Landing labels sit on the canvas; never insert the technical view's
        # white edge-label background over the self-contained dark palette.
        style = label.get("style", "note")
        svg.append(text(*label["at"], label["text"], 'note' if style == 'label' else style))
    svg.append(f'<path d="M40 {height-78}H{width-40}" stroke="#4b395f"/>')
    for index, kind in enumerate(dict.fromkeys(edge["kind"] for edge in data["edges"])):
        dash, arrow, title = EDGES[kind]
        x, y = 40 + 228*index, height-40
        marker = f'url(#{arrow})' if arrow != 'none' else 'none'
        svg += [f'<path class="edge" d="M{x} {y}h30" stroke-dasharray="{dash or "none"}" marker-end="{marker}"/>',
                text(x+42, y+5, title, 'legend')]
    svg.append('</svg>')
    return '\n'.join(svg)+'\n'


def check_output(path, expected):
    require(path.is_file() and path.read_text(encoding="utf-8") == expected,
            f"stale diagram: {path.name}; run python3 tools/render_diagrams.py")

def mermaid(data):
    """Project the same typed graph into GitHub-native Mermaid, without coordinates."""
    validate(data)
    def identity(value):return 'n_'+value.replace('-', '_')
    def label(value):return value.replace('&','&amp;').replace('"','&quot;').replace('<','&lt;').replace('>','&gt;')
    panels=data['panels'];parents={};members={i:[] for i in range(len(panels))}
    def contains(outer,inner):
        a,b,w,h=outer;x,y,v,z=inner
        return a<=x and b<=y and x+v<=a+w and y+z<=b+h and outer!=inner
    def parent(box):
        candidates=[i for i,p in enumerate(panels) if contains(p['box'],box)]
        return min(candidates,key=lambda i:panels[i]['box'][2]*panels[i]['box'][3]) if candidates else None
    for i,p in enumerate(panels):parents[i]=parent(p['box'])
    loose=[]
    for n in data['nodes']:
        owner=parent(n['box'])
        (members[owner] if owner is not None else loose).append(n)
    out=['%%{init: {"themeVariables": {"background": "transparent"}}}%%', 'flowchart TB']
    def node(n):
        # Mermaid is the orientation view; exact annotations remain in the
        # companion static figure and explanatory owner, from this same model.
        body='<br/>'.join(label(t) for t in [n['kind'].upper(),n['title'],*n['lines'][:1]])
        return f'  {identity(n["id"])}["{body}"]:::{n["kind"]}'
    def panel(i):
        out.append(f'  subgraph {identity("panel-"+str(i))}["{label(panels[i]["title"])}"]')
        out.append('    direction TB')
        out.extend(node(n) for n in members[i])
        for j in range(len(panels)):
            if parents[j]==i:panel(j)
        for j,b in enumerate(data.get('bands',[])):
            if parent(b['box'])==i:out.append(f'  band_{j}["{label(b["text"])}"]:::runtime')
        # Invisible constraints express reading order, never a system edge.
        local=[n['id'] for n in members[i]]
        if len(local)>1 and not any(e['source'] in local and e['target'] in local for e in data['edges']):
            out.append('  '+' ~~~ '.join(identity(n) for n in local))
        bands=[f'band_{j}' for j,b in enumerate(data.get('bands',[])) if parent(b['box'])==i]
        if len(bands)>1:out.append('  '+' ~~~ '.join(bands))
        out.append('  end')
    out.extend(node(n) for n in loose)
    for i in range(len(panels)):
        if parents[i] is None:panel(i)
    styles={'flow':'-->', 'control':'-. request .->', 'observe':'-. observation .->',
            'bind':'---|identity|', 'lifecycle':'-->|gate|'}
    for e in data['edges']:
        out.append(f'  {identity(e["source"])} {styles[e["kind"]]} {identity(e["target"])}')
    roots=[identity('panel-'+str(i)) for i in range(len(panels)) if parents[i] is None]
    if len(roots)>1:out.append('  '+' ~~~ '.join(roots))
    palette={'semantic':('#efe5fc','#7541ba'),'physical':('#f4effb','#8054b2'),
      'runtime':('#eeeafb','#6a4ca3'),'mutable':('#fff3db','#8e6920'),
      'interface':('#edf3fb','#456789'),'external':('#f2f2f4','#707078'),'evidence':('#eaf5ef','#3d7255')}
    for kind,(fill,stroke) in palette.items():
        out.append(f'  classDef {kind} fill:{fill},stroke:{stroke},color:#261b38')
    for i in range(len(panels)):
        out.append(f'  style {identity("panel-"+str(i))} fill:transparent,stroke:#b8a5d0')
    return '\n'.join(out)+'\n'

def diagram_block(name, data, consumer):
    relative=os.path.relpath(DIRECTORY/(name+'.svg'),consumer.parent)
    source=os.path.relpath(DIRECTORY/(name+'.json'),consumer.parent)
    return (f'<!-- docs:diagram {name} -->\n```mermaid\n%% yvex-figure: {name}\n'+mermaid(data)+
      f'```\n\n[Static figure]({relative}) · [Editable source]({source})\n<!-- /docs:diagram -->')

def synchronize_markdown(sources, check):
    models={p.stem:json.loads(p.read_text()) for p in sources}
    pattern=re.compile(r'<!-- docs:diagram ([a-z_]+) -->.*?<!-- /docs:diagram -->',re.S)
    consumers=[ROOT/'README.md',*(ROOT/'docs').rglob('*.md')]
    for path in consumers:
        text=path.read_text()
        def replacement(match):
            require(match[1] in models,'unknown Mermaid source: '+match[1])
            return diagram_block(match[1],models[match[1]],path)
        expected=pattern.sub(replacement,text)
        if check:require(text==expected,'stale Mermaid projection: '+str(path.relative_to(ROOT)))
        elif text!=expected:path.write_text(expected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="reject stale or orphan SVGs without writing")
    args = parser.parse_args()
    try:
        sources = sorted(DIRECTORY.glob("*.json"))
        require(sources, "no figure sources")
        require({p.stem for p in DIRECTORY.glob("*.svg")} <= {p.stem for p in sources}, "orphan SVG")
        for source in sources:
            output = source.with_suffix(".svg")
            svg = render(json.loads(source.read_text(encoding="utf-8")))
            if args.check:
                check_output(output, svg)
            else:
                output.write_text(svg, encoding="utf-8")
        synchronize_markdown(sources,args.check)
        print(f'documentation figures: {len(sources)} synchronized ({"check" if args.check else "render"})')
    except (ValueError, KeyError, TypeError, OSError) as exc:
        parser.exit(1, f"documentation figures: {exc}\n")


if __name__ == "__main__":
    main()
