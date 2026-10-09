// Consumer PTY captures, not producer example output. Scope: retained scrollback
// in xterm/headless 5.5.0; not the active cursor paragraph or every emulator.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { Terminal } = require('@xterm/headless');
function logical(terminal) {
  const buffer = terminal.buffer.active;
  const lines = [];
  for (let row = 0; row < buffer.length; row++) {
    const line = buffer.getLine(row);
    // A wide glyph wrapping at the last cell leaves an unoccupied padding
    // cell. It is not an authored space; preserve actual U+0020/TAB cells.
    let end = line.length;
    while (end && line.getCell(end - 1).getChars() === '' && line.getCell(end - 1).getWidth() !== 0) end--;
    const value = line.translateToString(false, 0, end);
    if (line.isWrapped) lines[lines.length - 1] += value;
    else lines.push(value);
  }
  while (lines.length && !lines.at(-1).trim()) lines.pop();
  return lines;
}
async function observe(text) {
  const term = new Terminal({cols: 40, rows: 4, scrollback: 2000, allowProposedApi: true});
  await new Promise(resolve => term.write((text + '\ntail1\ntail2\ntail3\ntail4\n').replaceAll('\n', '\r\n'), resolve));
  const original = logical(term);
  const lengths = [];
  for (const cols of [180, 24, 240, 40]) {
    term.resize(cols, 4);
    assert.deepEqual(logical(term), original, `logical content changed at ${cols}`);
    lengths.push(term.buffer.active.length);
  }
  assert(lengths[1] > lengths[0], 'narrowing failed to wrap retained flow');
  assert(lengths[2] < lengths[1], 'widening failed to rejoin retained flow');
  term.dispose();
  return original;
}
(async () => {
  const directory = process.argv[2];
  const files = fs.readdirSync(directory).filter(name => /^flow-.*\.txt$/.test(name));
  assert.equal(files.length, 9, 'missing three widths/modes and resize captures');
  const identifier = 'identifier_' + 'abcdefghijklmnopqrstuvwxyz_'.repeat(5) + 'abcdefghijklmnopqrstuvwxyz';
  for (const file of files) {
    const text = fs.readFileSync(path.join(directory, file), 'utf8');
    const actual = await observe(text);
    assert(actual.some(line => line.includes(identifier)), 'code identifier split by physical LF');
  }
  // Reproduce the old consumer's irreversible width-based splitting. This
  // negative oracle must keep the identifier broken after widening.
  const broken = identifier.match(/.{1,94}/g).join('\n  ');
  const negative = await observe(broken);
  assert(!negative.some(line => line.includes(identifier)), 'negative control did not retain hard breaks');
  console.log('PASS YVEX native captures: nine flow cases; 40→180→24→240→40 retained-scrollback reflow; old hard-wrap negative');
})().catch(error => { console.error(error); process.exitCode = 1; });
