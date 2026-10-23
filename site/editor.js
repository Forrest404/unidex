// The App editor (editor.html): an app's files typed in the browser, installed on the emulated unidex and opened on
// its screen with Run, the way the store installs an app (emu-device.js). The work is kept in this browser
// (localStorage) and downloads as a zip of the app's folder, ready to send to the store. Starting points: the
// templates from unidex-apps (starters/, copied in by the website workflow) and any app in the store.
import { install, open, failure, onPrint, ready } from './emu-device.js';
import { crc32 } from './serial.js';
import { loadCatalogue } from './store-ui.js';

const $ = id => document.getElementById(id);
const text = $('text'), paint = $('paint'), gutter = $('gutter'), logEl = $('log');
const enc = new TextEncoder(), dec = new TextDecoder();
const KEY = 'unidex-editor';
const STARTERS = { app: 'An app', badge: 'A badge plugin' };
const TEXT_FILE = /\.(lua|json|md|txt|csv)$/i;

let files = [];       // [{path, text} or {path, bytes}], in tab order
let active = '';      // the path in the editor
let errorAt = null;   // {path, line} of the last error
let catalogue = [];

// ---- the files, kept in this browser ----
const order = p => (p === 'main.lua' ? 0 : p === 'app.json' ? 1 : p === 'README.md' ? 3 : 2);
function setFiles(list) {
  files = list.sort((a, b) => order(a.path) - order(b.path) || a.path.localeCompare(b.path));
  active = files.some(f => f.path === active) ? active : (files[0] || {}).path;
  errorAt = null;
  renderTabs();
  showFile();
  save();
}
const b64 = bytes => btoa(String.fromCharCode(...bytes));
const unb64 = s => Uint8Array.from(atob(s), c => c.charCodeAt(0));
let saving;
function save() {
  clearTimeout(saving);
  saving = setTimeout(() => {
    try {
      localStorage.setItem(KEY, JSON.stringify({ active, files: files.map(f => f.bytes ? { path: f.path, b64: b64(f.bytes) } : f) }));
    } catch (e) { /* storage full or blocked: the work stays on the page */ }
  }, 300);
}
function restore() {
  try {
    const s = JSON.parse(localStorage.getItem(KEY) || 'null');
    if (!s || !Array.isArray(s.files) || !s.files.length) return false;
    active = s.active;
    setFiles(s.files.map(f => (f.b64 != null ? { path: f.path, bytes: unb64(f.b64) } : { path: f.path, text: f.text || '' })));
    return true;
  } catch (e) { return false; }
}

async function fetchFiles(base, paths) {
  const out = [];
  for (const p of paths) {
    const r = await fetch(`${base}/${p}`, { cache: 'no-cache' });
    if (!r.ok) throw new Error(`Couldn't load ${p}.`);
    if (TEXT_FILE.test(p)) out.push({ path: p, text: await r.text() });
    else out.push({ path: p, bytes: new Uint8Array(await r.arrayBuffer()) });
  }
  return out;
}
const loadStarter = kind => fetchFiles(`starters/${kind}`, ['app.json', 'main.lua', 'README.md']);
function loadStoreApp(id) {
  const a = catalogue.find(x => x.id === id);
  if (!a) throw new Error(`There's no app called ${id} in the store.`);
  return fetchFiles(`store/apps/${a.id}/${a.version}`, a.files.map(f => f.path));
}

// ---- the editor: a textarea over a painted copy of the code, with line numbers ----
const esc = s => s.replace(/[&<>]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' })[c]);
const LUA_KW = new Set(('and break do else elseif end false for function goto if in local nil not or repeat return ' +
                        'then true until while').split(' '));
const LUA = /(--\[(=*)\[[\s\S]*?(?:\]\2\]|$))|(--[^\n]*)|(\[(=*)\[[\s\S]*?(?:\]\5\]|$))|("(?:[^"\\\n]|\\[\s\S])*"?|'(?:[^'\\\n]|\\[\s\S])*'?)|(\b(?:0[xX][0-9a-fA-F]+|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?)\b)|([A-Za-z_]\w*)/g;
const JSON_ = /("(?:[^"\\\n]|\\.)*"?)|(-?\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b)|\b(true|false|null)\b/g;
function highlight(src, path) {
  const re = path.endsWith('.lua') ? LUA : path.endsWith('.json') ? JSON_ : null;
  if (!re) return esc(src);
  let out = '', last = 0, m;
  re.lastIndex = 0;
  while ((m = re.exec(src))) {
    out += esc(src.slice(last, m.index));
    const t = m[0];
    let cls = null;
    if (re === LUA) {
      if (m[1] || m[3]) cls = 'com';
      else if (m[4] || m[6]) cls = 'str';
      else if (m[7]) cls = 'num';
      else if (LUA_KW.has(t)) cls = 'kw';
      else if (t === 'unidex' || src[re.lastIndex] === '(') cls = 'call';
    } else {
      cls = m[1] ? (src.slice(re.lastIndex).match(/^\s*:/) ? 'kw' : 'str') : m[2] ? 'num' : 'kw';
    }
    out += cls ? `<span class="tk-${cls}">${esc(t)}</span>` : esc(t);
    last = re.lastIndex;
    if (!t.length) re.lastIndex++;
  }
  return out + esc(src.slice(last));
}
const current = () => files.find(f => f.path === active);
function paintCode() {
  const f = current();
  const src = f ? (f.bytes ? `(${f.path} is not a text file: it's kept as it is)` : f.text) : '';
  paint.innerHTML = highlight(src, f && !f.bytes ? f.path : '') + '\n ';
  const lines = src.split('\n').length;
  const errLine = errorAt && f && errorAt.path === f.path ? errorAt.line : 0;
  let g = '';
  for (let i = 1; i <= lines; i++) g += `<div${i === errLine ? ' class="err"' : ''}>${i}</div>`;
  gutter.innerHTML = g;
  syncScroll();
}
function syncScroll() {
  paint.scrollTop = text.scrollTop;
  paint.scrollLeft = text.scrollLeft;
  gutter.scrollTop = text.scrollTop;
}
function showFile() {
  const f = current();
  text.value = f ? (f.bytes ? '' : f.text) : '';
  text.readOnly = !f || !!f.bytes;
  text.scrollTop = 0;
  paintCode();
}
function renderTabs() {
  $('tabs').replaceChildren(...files.map(f => {
    const b = document.createElement('button');
    b.type = 'button';
    b.role = 'tab';
    b.textContent = f.path;
    b.setAttribute('aria-selected', String(f.path === active));
    b.onclick = () => { active = f.path; renderTabs(); showFile(); save(); };
    return b;
  }));
}

text.addEventListener('input', () => {
  const f = current();
  if (!f || f.bytes) return;
  f.text = text.value;
  paintCode();
  save();
});
text.addEventListener('scroll', syncScroll);

// Typing: Tab indents, Enter keeps the indent (one more after then, do, else, repeat, function), Ctrl/Cmd+Enter runs.
function insert(s) {
  if (!document.execCommand('insertText', false, s)) {
    text.setRangeText(s, text.selectionStart, text.selectionEnd, 'end');
    text.dispatchEvent(new Event('input'));
  }
}
text.addEventListener('keydown', e => {
  const mod = e.metaKey || e.ctrlKey;
  if (mod && e.key === 'Enter') { e.preventDefault(); run(); return; }
  if (mod && e.key.toLowerCase() === 's') { e.preventDefault(); save(); say('Saved in this browser.', 'muted'); return; }
  if (text.readOnly) return;
  const v = text.value, s = text.selectionStart, end = text.selectionEnd;
  const lineStart = v.lastIndexOf('\n', s - 1) + 1;
  if (e.key === 'Tab') {
    e.preventDefault();
    if (s === end && !e.shiftKey) { insert('  '); return; }
    const blockEnd = v.indexOf('\n', end - (end > s && v[end - 1] === '\n' ? 1 : 0));
    const stop = blockEnd < 0 ? v.length : blockEnd;
    const block = v.slice(lineStart, stop);
    const changed = e.shiftKey ? block.replace(/^ {1,2}/gm, '') : block.replace(/^/gm, '  ');
    text.setSelectionRange(lineStart, stop);
    insert(changed);
    text.setSelectionRange(lineStart, lineStart + changed.length);
    return;
  }
  if (e.key === 'Enter' && !e.shiftKey && !e.altKey) {
    e.preventDefault();
    const before = v.slice(lineStart, s);
    let indent = before.match(/^\s*/)[0];
    if (/(\bthen|\bdo|\belse|\brepeat|\bfunction\b.*\)|[{(])\s*$/.test(before)) indent += '  ';
    insert('\n' + indent);
  }
});

// ---- the output ----
function say(msg, kind = '') {
  const line = document.createElement('div');
  if (kind) line.className = kind;
  line.textContent = msg;
  logEl.append(line);
  logEl.scrollTop = logEl.scrollHeight;
  return line;
}
onPrint(t => say(t.replace(/^[a-z0-9-]+: /, '')));

function goTo(path, line) {
  if (!files.some(f => f.path === path)) return;
  active = path;
  renderTabs();
  showFile();
  const lines = text.value.split('\n');
  const startAt = lines.slice(0, line - 1).reduce((n, l) => n + l.length + 1, 0);
  text.focus();
  text.setSelectionRange(startAt, startAt + (lines[line - 1] || '').length);
  text.scrollTop = Math.max(0, (line - 1) * 20 - text.clientHeight / 3);
  syncScroll();
}
function showError(msg) {
  const m = /([\w./-]+\.(?:lua|json)):(\d+):/.exec(msg);
  const line = say(msg, 'err');
  if (m && files.some(f => f.path === m[1])) {
    errorAt = { path: m[1], line: +m[2] };
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'secondary';
    b.textContent = `Go to line ${m[2]}`;
    b.onclick = () => goTo(m[1], +m[2]);
    line.append(b);
    paintCode();
  }
}

// ---- Run ----
let running = false, ran = false, lastFailure = null;
async function run() {
  if (running) return;
  running = true;
  $('run').disabled = true;
  logEl.replaceChildren();
  errorAt = null;
  paintCode();
  try {
    const manifest = files.find(f => f.path === 'app.json');
    if (!manifest || manifest.bytes) throw new Error('The app needs an app.json.');
    try { JSON.parse(manifest.text); } catch (e) {
      const at = /position (\d+)/.exec(e.message), line = /line (\d+)/.exec(e.message);
      const n = line ? +line[1] : at ? manifest.text.slice(0, +at[1]).split('\n').length : 0;
      throw new Error(`app.json${n ? `:${n}` : ''}: this isn't valid JSON (${e.message.replace(/^JSON\.parse: /, '')}).`);
    }
    const list = files.map(f => ({ path: f.path, bytes: f.bytes || enc.encode(f.text) }));
    const status = say('Installing…', 'muted');
    const entry = await install(list, t => { status.textContent = t; }, { fresh: $('fresh').checked });
    status.textContent = `Installed ${entry.name}. Opening it…`;
    const err = await open(entry);
    lastFailure = err;
    ran = true;
    if (err) showError(err);
    else say(entry.type === 'plugin' ? `${entry.name} is showing in Badge.` : `${entry.name} is running. Use the buttons beside it.`, 'ok');
    if (matchMedia('(max-width: 1000px)').matches) document.querySelector('.side').scrollIntoView({ behavior: 'smooth', block: 'start' });
  } catch (e) {
    showError(e.message || String(e));
  } finally {
    running = false;
    $('run').disabled = false;
  }
}
$('run').onclick = run;

// An app can also fail later, when a button is pressed: look now and then, and say so once.
setInterval(async () => {
  if (!ran || running || document.hidden) return;
  try {
    const f = await failure();
    if (f && f !== lastFailure) showError(f);
    lastFailure = f;
  } catch (e) { /* the emulator is busy restarting */ }
}, 1500);

// ---- Download: the app's folder as a zip ----
function zip(entries) {  // [{name, bytes}], stored (no compression)
  const now = new Date();
  const time = (now.getHours() << 11) | (now.getMinutes() << 5) | (now.getSeconds() >> 1);
  const date = ((now.getFullYear() - 1980) << 9) | ((now.getMonth() + 1) << 5) | now.getDate();
  const parts = [], central = [];
  let offset = 0;
  for (const { name, bytes } of entries) {
    const n = enc.encode(name), crc = crc32(bytes);
    const head = new Uint8Array(30 + n.length), h = new DataView(head.buffer);
    h.setUint32(0, 0x04034b50, true); h.setUint16(4, 10, true); h.setUint16(6, 0x0800, true);
    h.setUint16(10, time, true); h.setUint16(12, date, true); h.setUint32(14, crc, true);
    h.setUint32(18, bytes.length, true); h.setUint32(22, bytes.length, true); h.setUint16(26, n.length, true);
    head.set(n, 30);
    const dir = new Uint8Array(46 + n.length), d = new DataView(dir.buffer);
    d.setUint32(0, 0x02014b50, true); d.setUint16(4, 20, true); d.setUint16(6, 10, true); d.setUint16(8, 0x0800, true);
    d.setUint16(12, time, true); d.setUint16(14, date, true); d.setUint32(16, crc, true);
    d.setUint32(20, bytes.length, true); d.setUint32(24, bytes.length, true); d.setUint16(28, n.length, true);
    d.setUint32(42, offset, true);
    dir.set(n, 46);
    parts.push(head, bytes);
    central.push(dir);
    offset += head.length + bytes.length;
  }
  const size = central.reduce((s, c) => s + c.length, 0);
  const tail = new Uint8Array(22), t = new DataView(tail.buffer);
  t.setUint32(0, 0x06054b50, true); t.setUint16(8, entries.length, true); t.setUint16(10, entries.length, true);
  t.setUint32(12, size, true); t.setUint32(16, offset, true);
  return new Blob([...parts, ...central, tail], { type: 'application/zip' });
}
$('download').onclick = () => {
  let id = 'my-app';
  try { id = JSON.parse(files.find(f => f.path === 'app.json').text).id || id; } catch (e) { /* keeps my-app */ }
  id = String(id).replace(/[^a-z0-9-]/g, '') || 'my-app';
  const blob = zip(files.map(f => ({ name: `${id}/${f.path}`, bytes: f.bytes || enc.encode(f.text) })));
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = `${id}.zip`;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  say(`Downloaded ${id}.zip. To send it to the store, see Learn › Send it to the store.`, 'muted');
};

// ---- Start from: a template or an app in the store (it replaces what's in the editor, so it asks first) ----
let box;
function ask(title, msg, yes) {
  if (!box) {
    box = document.createElement('dialog');
    box.className = 'confirm';
    box.innerHTML = '<h2></h2><p></p><div class="buttons"><button class="secondary" type="button">Cancel</button><button type="button"></button></div>';
    document.body.append(box);
  }
  box.querySelector('h2').textContent = title;
  box.querySelector('p').textContent = msg;
  const [no, ok] = box.querySelectorAll('button');
  ok.textContent = yes;
  return new Promise(resolve => {
    const done = v => { box.close(); resolve(v); };
    ok.onclick = () => done(true);
    no.onclick = () => done(false);
    box.oncancel = () => resolve(false);
    box.showModal();
    no.focus();
  });
}
async function replaceWith(label, load) {
  if (!(await ask(`Open ${label}?`, 'It replaces the code in the editor. Download your work first if you want to keep it.', 'Open'))) return;
  try {
    setFiles(await load());
    logEl.replaceChildren();
    say(`Opened ${label}. Press Run to try it.`, 'muted');
  } catch (e) {
    showError(e.message);
  }
}
function fillStarter() {
  const sel = $('starter');
  sel.replaceChildren(new Option('Choose…', ''));
  const fresh = document.createElement('optgroup');
  fresh.label = 'New';
  for (const [k, v] of Object.entries(STARTERS)) fresh.append(new Option(v, `new:${k}`));
  sel.append(fresh);
  if (catalogue.length) {
    const store = document.createElement('optgroup');
    store.label = 'From the store';
    for (const a of [...catalogue].sort((x, y) => x.name.localeCompare(y.name))) store.append(new Option(a.name, `store:${a.id}`));
    sel.append(store);
  }
}
$('starter').onchange = async () => {
  const v = $('starter').value;
  $('starter').value = '';
  if (v.startsWith('new:')) await replaceWith(STARTERS[v.slice(4)].toLowerCase(), () => loadStarter(v.slice(4)));
  else if (v.startsWith('store:')) {
    const a = catalogue.find(x => x.id === v.slice(6));
    await replaceWith(a ? a.name : 'that app', () => loadStoreApp(v.slice(6)));
  }
};

// ---- start ----
$('runKey').textContent = /Mac|iPhone|iPad/.test(navigator.platform) ? '⌘ Enter' : 'Ctrl Enter';
$('run').disabled = true;
ready.then(() => { $('run').disabled = running; });
try { catalogue = await loadCatalogue(); } catch (e) { catalogue = []; }
fillStarter();
const wanted = new URLSearchParams(location.search).get('app');
const had = restore();
if (wanted && catalogue.some(a => a.id === wanted)) {
  if (!had) {
    try { setFiles(await loadStoreApp(wanted)); } catch (e) { showError(e.message); }
  } else {
    await replaceWith(catalogue.find(a => a.id === wanted).name, () => loadStoreApp(wanted));
  }
  history.replaceState(null, '', location.pathname);
} else if (!had) {
  try { setFiles(await loadStarter('app')); }
  catch (e) { setFiles([{ path: 'app.json', text: '' }, { path: 'main.lua', text: '' }]); showError(e.message); }
}
