#!/usr/bin/env python3
"""Project canonical Markdown into a portable HTML reader and printable book."""
from __future__ import annotations
import argparse
import html
import json
import os
import re
import shutil
import subprocess
from pathlib import Path
from urllib.parse import unquote
import markdown
from metadata import ROOT, registry

OUT=ROOT/'build/docs'

def slug(value, separator='-'):
    value=re.sub(r'<[^>]+>','',value).replace('`','').replace('*','').lower()
    return re.sub(r'\s+','-',re.sub(r'[^\w\- ]','',value).strip())

def output_path(path):
    relative=path.relative_to(ROOT)
    return relative.with_name('index.html') if path.name=='README.md' else relative.with_suffix('.html')

def render(body):
    # Native Mermaid and static publication share one editable graph. No CDN or
    # browser-side redefinition is required by the offline/PDF projection.
    def figure(match):
        name=match[1];path=ROOT/'docs/assets/diagrams'/(name+'.json')
        data=json.loads(path.read_text())
        return '<img src="'+str(path.with_suffix('.svg'))+'" alt="'+html.escape(data['description'],quote=True)+'">'
    body=re.sub(r'```mermaid\n%% yvex-figure: ([a-z_]+)\n.*?\n```',figure,body,flags=re.S)
    # GitHub parses Markdown inside details; opt into the equivalent local extension.
    body=re.sub(r'<details>', '<details markdown="1">',body)
    return markdown.markdown(body,extensions=['tables','fenced_code','sane_lists','toc','md_in_html'],
       extension_configs={'toc':{'slugify':slug,'permalink':False}})

def rewrite(content, source, destination, known):
    def replace(m):
        value=html.unescape(m[2]);url,sep,frag=value.partition('#')
        if not url or re.match(r'^[a-z]+:',url):return m[0]
        path=(source.parent/unquote(url)).resolve()
        if path in known:
            target=OUT/output_path(path)
            value=os.path.relpath(target,destination.parent)
        elif path.is_relative_to(ROOT/'docs') and path.suffix in {'.svg','.png','.json','.csv'}:
            value=os.path.relpath(OUT/path.relative_to(ROOT),destination.parent)
        elif path.is_relative_to(ROOT):
            mode='tree' if path.is_dir() else 'blob'
            value='https://github.com/yailabs/yvex/'+mode+'/models2/'+path.relative_to(ROOT).as_posix()
        else:return m[0]
        return m[1]+html.escape(value+('#'+frag if sep else ''),quote=True)+m[3]
    return re.sub(r'((?:href|src|srcset)=["\'])([^"\']+)(["\'])',replace,content)

def book_links(content, owners):
    """Keep links among included canonical owners inside the exported dossier."""
    def replace(match):
        value=html.unescape(match[1]);url,sep,fragment=value.partition('#')
        if url not in owners:return match[0]
        target=owners[url]+('--'+fragment if sep else '')
        return 'href="#'+html.escape(target,quote=True)+'"'
    return re.sub(r'href="([^"]+)"',replace,content)

def build():
    docs=registry();ordered=sorted(docs.values(),key=lambda d:(len(d['path'].parts),str(d['path'])))
    known={d['path'] for d in ordered if d['meta']['publication']['html']}
    book_owners={output_path(d['path']).as_posix():d['meta']['id'] for d in ordered if d['meta']['publication']['pdf']}
    OUT.mkdir(parents=True,exist_ok=True)
    for name in ('reader.css','reader.js'):
        shutil.copyfile(ROOT/'tools/docs'/name,OUT/name)
    for path in (ROOT/'docs').rglob('*'):
        if path.is_file() and path.suffix in {'.svg','.png','.json','.csv'}:
            dest=OUT/path.relative_to(ROOT);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,dest)
    landings=[d for d in ordered if d['path'].name=='README.md' and len(d['path'].relative_to(ROOT).parts)<=3]
    # Root docs README is the visible ordering source for area navigation.
    home=next(d for d in ordered if d['path']==ROOT/'docs/README.md')
    order=[(ROOT/'docs'/p).resolve() for p in re.findall(r'\]\(([^)#]*README\.md)\)',home['body'])]
    landings.sort(key=lambda d:order.index(d['path']) if d['path'] in order else -1)
    rendered=[];search=[]
    for doc in ordered:
        path,meta,body=doc['path'],doc['meta'],doc['body']
        if not meta['publication']['html']:continue
        dest=OUT/output_path(path);dest.parent.mkdir(parents=True,exist_ok=True)
        root=os.path.relpath(OUT,dest.parent)
        nav=''.join('<a href="'+html.escape(os.path.relpath(OUT/output_path(d['path']),dest.parent))+'">'+html.escape(d['meta']['title'])+'</a>' for d in landings)
        content=rewrite(render(body),path,dest,known)
        related=''
        if meta.get('related'):
            related='<nav aria-label="Related owners">'+ ' · '.join('<a href="'+html.escape(os.path.relpath(OUT/output_path(docs[i]['path']),dest.parent))+'">'+html.escape(docs[i]['meta']['title'])+'</a>' for i in meta['related'])+'</nav>'
        document=f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>{html.escape(meta['title'])} · YVEX</title><link rel="stylesheet" href="{root}/reader.css">
<script defer src="{root}/reader.js"></script></head>
<body data-root="{root}"><a class="skip" href="#content">Skip to content</a>
<header class="reader-header"><a href="{root}/index.html" class="brand">YVEX <span>Technical library</span></a>
<div><button id="nav-toggle" aria-controls="navigation" aria-expanded="false">Browse</button><button id="theme" aria-label="Change color theme">Theme</button></div></header>
<aside id="navigation"><label for="search">Search documentation</label><input id="search" type="search" placeholder="Concept, contract or Task ID"><div id="search-results" aria-live="polite"></div><nav aria-label="Documentation areas">{nav}</nav></aside>
<main id="content" tabindex="-1"><div class="doc-meta" aria-label="Document classification">{html.escape(meta['document'].upper())} · {html.escape(meta['status'].upper())}</div>
<article>{content}</article>{related}<footer>Canonical owner: {html.escape(path.relative_to(ROOT).as_posix())}. This reader is a projection.</footer></main>
<dialog id="figure-dialog" aria-label="Figure explorer"><div class="figure-controls"><button id="figure-close">Close</button><button id="zoom-in" aria-label="Zoom in">+</button><button id="zoom-out" aria-label="Zoom out">−</button><button id="zoom-reset">Fit</button></div><div class="figure-canvas"></div></dialog>
</body></html>'''
        dest.write_text(document)
        search.append({'title':meta['title'],'url':output_path(path).as_posix(),'text':re.sub(r'\s+',' ',re.sub(r'<[^>]+>','',body))})
        if meta['publication']['pdf']:
            # Book links target the same generated reader. IDs are prefixed per owner.
            book_content=rewrite(render(body),path,OUT/'book.html',known)
            book_content=book_content.replace('<details>', '<details open>')
            prefix=meta['id']+'--'
            book_content=re.sub(r'\bid="([^"]+)"',lambda m:'id="'+prefix+m[1]+'"',book_content)
            book_content=re.sub(r'href="#([^"]+)"',lambda m:'href="#'+prefix+m[1]+'"',book_content)
            book_content=book_links(book_content,book_owners)
            rendered.append((path,meta,'<section class="book-document" id="'+meta['id']+'">'+book_content+'</section>'))
    (OUT/'search.json').write_text(json.dumps(search,ensure_ascii=False))
    rank={p:i for i,p in enumerate([ROOT/'README.md',ROOT/'docs/README.md',*order])}
    rendered.sort(key=lambda item:(rank.get(item[0],rank.get(item[0].parent/'README.md',99)),str(item[0])))
    toc='<nav class="book-contents"><h1>YVEX Technical Dossier</h1><p>Canonical documentation projection; capability and evidence limits remain unchanged.</p><ol>'+''.join('<li><a href="#'+m['id']+'">'+html.escape(m['title'])+'</a></li>' for _,m,_ in rendered)+'</ol></nav>'
    (OUT/'book.html').write_text('<!doctype html><html lang="en"><head><meta charset="utf-8"><title>YVEX Technical Dossier</title><link rel="stylesheet" href="reader.css"></head><body class="book"><main>'+toc+''.join(s for _,_,s in rendered)+'</main></body></html>')
    print(f'HTML reader: {len(known)} pages; PDF source: {len(rendered)} owners; {OUT}')

def pdf(browser):
    executable=shutil.which(browser) if browser else next((shutil.which(p) for p in ['chromium','chromium-browser','google-chrome'] if shutil.which(p)),None)
    if not executable:raise SystemExit('PDF rendering requires an installed Chromium browser; HTML book remains available.')
    target=OUT/'yvex-technical-dossier.pdf'
    subprocess.run([executable,'--headless','--no-sandbox','--disable-gpu','--no-pdf-header-footer','--print-to-pdf='+str(target),(OUT/'book.html').as_uri()],check=True,timeout=180)
    if not target.is_file() or not target.read_bytes().startswith(b'%PDF-'):raise SystemExit('PDF not produced')
    print(f'PDF: {target} ({target.stat().st_size} bytes)')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--pdf',action='store_true');parser.add_argument('--browser')
    args=parser.parse_args();build()
    if args.pdf:pdf(args.browser)
