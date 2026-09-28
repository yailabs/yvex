/* Optional reader behavior. Canonical content works without this script. */
(() => {
  const root = document.body.dataset.root || '.';
  const theme = document.querySelector('#theme');
  try { const saved = localStorage.getItem('yvex-docs-theme'); if (saved) document.documentElement.dataset.theme = saved; } catch (_) {}
  theme?.addEventListener('click', () => {
    const dark = document.documentElement.dataset.theme === 'dark' || (!document.documentElement.dataset.theme && matchMedia('(prefers-color-scheme:dark)').matches);
    const next = dark ? 'light' : 'dark'; document.documentElement.dataset.theme = next;
    try { localStorage.setItem('yvex-docs-theme', next); } catch (_) {}
  });
  document.querySelector('#nav-toggle')?.addEventListener('click', event => {
    const open = document.querySelector('#navigation').classList.toggle('open'); event.currentTarget.setAttribute('aria-expanded', String(open));
  });
  document.querySelectorAll('article table').forEach((table, index) => {
    const wrap = document.createElement('div'); wrap.className = 'table-scroll'; wrap.tabIndex = 0; wrap.setAttribute('role','region'); wrap.setAttribute('aria-label','Scrollable table');
    table.before(wrap); wrap.append(table);
    const rows = [...table.querySelectorAll('tbody tr')];
    if (rows.length < 12) return;
    const label = document.createElement('label'); label.className = 'table-filter-label'; label.htmlFor = `table-${index}`; label.textContent = 'Filter this table';
    const input = document.createElement('input'); input.id = label.htmlFor; input.type = 'search'; input.className = 'table-filter';
    wrap.before(label,input); input.addEventListener('input', () => rows.forEach(row => { row.hidden = !row.textContent.toLowerCase().includes(input.value.toLowerCase()); }));
  });
  const dialog = document.querySelector('#figure-dialog'), canvas = document.querySelector('.figure-canvas');
  let trigger, scale = 1, figure;
  const resize = () => { if (figure) figure.style.width = `${Math.max(280,canvas.clientWidth-4)*scale}px`; };
  document.querySelectorAll('article img[src$=".svg"]').forEach(img => {
    const button = document.createElement('button'); button.className = 'figure-open'; button.textContent = 'Explore figure'; button.setAttribute('aria-label',`Explore: ${img.alt}`); img.after(button);
    button.addEventListener('click', () => { trigger = button; canvas.replaceChildren(); figure = img.cloneNode(); canvas.append(figure); scale = 1; dialog.showModal(); resize(); document.querySelector('#figure-close').focus(); });
  });
  document.querySelector('#figure-close')?.addEventListener('click', () => dialog.close());
  dialog?.addEventListener('close', () => trigger?.focus());
  document.querySelector('#zoom-in')?.addEventListener('click', () => { scale = Math.min(4,scale+.3); resize(); });
  document.querySelector('#zoom-out')?.addEventListener('click', () => { scale = Math.max(.7,scale-.3); resize(); });
  document.querySelector('#zoom-reset')?.addEventListener('click', () => { scale = 1; resize(); });
  let searchIndex;
  const search = document.querySelector('#search'), results = document.querySelector('#search-results');
  search?.addEventListener('input', async () => {
    const query = search.value.trim().toLowerCase(); results.replaceChildren(); if (query.length < 2) return;
    try {
      searchIndex ||= await fetch(`${root}/search.json`).then(response => { if (!response.ok) throw Error('Search unavailable'); return response.json(); });
      if (search.value.trim().toLowerCase() !== query) return;
      const matches = searchIndex.filter(doc => `${doc.title} ${doc.text}`.toLowerCase().includes(query)).slice(0,12);
      for (const doc of matches) { const a = document.createElement('a'); a.href = `${root}/${doc.url}`; a.textContent = doc.title; results.append(a); }
      if (!matches.length) results.textContent = 'No matching document.';
    } catch (_) { results.textContent = 'Search requires the local HTTP reader. Browse the area links below.'; }
  });
  // Printing optional depth must not omit canonical evidence or earned Task exits.
  let printDetails = [];
  addEventListener('beforeprint', () => { printDetails = [...document.querySelectorAll('details:not([open])')]; printDetails.forEach(d => d.open = true); });
  addEventListener('afterprint', () => printDetails.forEach(d => d.open = false));
})();
