// "Try your app": installs an app folder on the emulated unidex with the store's own code (site/store-device.js),
// over the emulator's serial line, and shows what the app prints.
import { installApp, sha256hex } from '../../site/store-device.js';

const $ = id => document.getElementById(id);
let M = null, buf = '', busy = false;

const ready = new Promise(resolve => {
  if (window.unidexEmu) resolve(window.unidexEmu);
  else addEventListener('unidex-ready', () => resolve(window.unidexEmu), { once: true });
});

// The serial line, as serial.js sees a real device: lines out, replies back. LUA lines are the app's print().
function pump() {
  buf += M.ccall('emu_serial_out', 'string', [], []) || '';
  let nl;
  while (!waiting && (nl = buf.indexOf('\n')) >= 0) {
    const line = buf.slice(0, nl).replace(/\r$/, '');
    buf = buf.slice(nl + 1);
    if (line.startsWith('LUA ')) log(line.slice(4));
  }
}
let waiting = false;  // while expect() reads the line itself
function log(text) {
  const t = $('appLog');
  t.value += (t.value ? '\n' : '') + text;
  t.scrollTop = t.scrollHeight;
}

const dev = {
  send: async t => M.ccall('emu_serial_in', null, ['string'], [t + '\n']),
  expect: async (prefix, ms, onOther) => {
    waiting = true;
    try {
      const end = Date.now() + ms;
      while (Date.now() < end) {
        buf += M.ccall('emu_serial_out', 'string', [], []) || '';
        let nl;
        while ((nl = buf.indexOf('\n')) >= 0) {
          const line = buf.slice(0, nl).replace(/\r$/, '');
          buf = buf.slice(nl + 1);
          if (line.startsWith(prefix)) return line;
          if (line === 'ERR' || line.startsWith('ERR ')) throw Object.assign(new Error('The device rejected it.'), { reply: line });
          if (line.startsWith('LUA ')) log(line.slice(4));
          else if (onOther) onOther(line);
        }
        await new Promise(r => setTimeout(r, 10));
      }
      return null;
    } finally {
      waiting = false;
    }
  },
};

// The chosen files ({path inside the folder, bytes}) as a catalogue entry, the way tools/make_store.py makes one.
async function entryFrom(files) {
  const manifest = files.find(f => f.path === 'app.json');
  if (!manifest) throw new Error('No app.json in that folder: choose the app’s own folder.');
  let m;
  try { m = JSON.parse(new TextDecoder().decode(manifest.bytes)); }
  catch (e) { throw new Error(`app.json isn’t valid JSON: ${e.message}`); }
  const keep = files.filter(f => !f.path.split('/').some(s => s.startsWith('.')) && !/^data\//.test(f.path) &&
                                 f.path !== 'README.md' && f.path !== 'LICENSE');
  keep.sort((a, b) => (a.path === 'app.json') - (b.path === 'app.json'));
  const out = [];
  for (const f of keep) out.push({ path: f.path, size: f.bytes.length, sha256: await sha256hex(f.bytes), bytes: f.bytes });
  return { ...m, name: m.name || m.id || 'the app', files: out };
}

async function load(files) {
  if (busy) return;
  busy = true;
  $('pickApp').disabled = true;
  const status = (t, kind = '') => { $('appStatus').textContent = t; $('appStatus').className = 'status ' + kind; };
  try {
    M = M || await ready;
    const entry = await entryFrom(files);
    for (let attempt = 0; ; attempt++) {
      try {
        await installApp(dev, entry, async (e, f) => f.bytes, t => status(`${entry.name}: ${t}…`));
        break;
      } catch (e) {
        // open on the screen: leave it (hold A), then try again
        if (attempt < 3 && /open on the device|open on the screen/.test(e.message)) {
          await dev.send('X BTN A');
          await dev.expect('OK X BTN', 5000);
          continue;
        }
        throw e;
      }
    }
    status(`${entry.name} ${entry.version || ''} installed. On the screen: Home → Apps → ${entry.name}.`, 'ok');
  } catch (e) {
    status(e.message, 'bad');
  } finally {
    busy = false;
    $('pickApp').disabled = false;
  }
}

async function readPicked(fileList) {
  const files = [];
  for (const f of fileList) {
    const rel = (f.webkitRelativePath || f.name).split('/').slice(1).join('/');  // without the folder's own name
    if (rel) files.push({ path: rel, bytes: new Uint8Array(await f.arrayBuffer()) });
  }
  return files;
}

// A dropped folder: walk it with the file system entries API.
async function readDropped(item) {
  const files = [];
  const walk = (entry, prefix) => new Promise(resolve => {
    if (entry.isFile) {
      entry.file(async f => { files.push({ path: prefix + f.name, bytes: new Uint8Array(await f.arrayBuffer()) }); resolve(); });
    } else {
      const reader = entry.createReader(), all = [];
      const more = () => reader.readEntries(async batch => {
        if (batch.length) { all.push(...batch); more(); return; }
        for (const e of all) await walk(e, prefix + entry.name + '/');
        resolve();
      });
      more();
    }
  });
  const root = item.webkitGetAsEntry();
  if (!root || !root.isDirectory) throw new Error('Drop the app’s folder itself.');
  const reader = root.createReader();
  const top = await new Promise(r => { const all = []; const more = () => reader.readEntries(b => b.length ? (all.push(...b), more()) : r(all)); more(); });
  for (const e of top) await walk(e, '');
  return files;
}

$('pickApp').onclick = () => $('appFolder').click();
$('appFolder').onchange = async () => { await load(await readPicked($('appFolder').files)); $('appFolder').value = ''; };
const zone = $('tryApp');
zone.addEventListener('dragover', e => e.preventDefault());
zone.addEventListener('drop', async e => {
  e.preventDefault();
  const item = [...e.dataTransfer.items].find(i => i.kind === 'file');
  if (!item) return;
  try { await load(await readDropped(item)); }
  catch (err) { $('appStatus').textContent = err.message; $('appStatus').className = 'status bad'; }
});
ready.then(async m => {
  M = m;
  setInterval(pump, 100);
  // For checking this page without a person (try.html?test=apps/examples/hello, served from the repo's root):
  // loads that folder's files as if it had been chosen.
  const test = new URLSearchParams(location.search).get('test');
  if (test && /^[a-z0-9/_-]+$/.test(test)) {
    const names = ['app.json', 'main.lua'];
    const files = [];
    for (const n of names) files.push({ path: n, bytes: new Uint8Array(await (await fetch(`../../${test}/${n}`, { cache: 'no-store' })).arrayBuffer()) });
    await load(files);
    if (new URLSearchParams(location.search).get('open') === '1') await openForTest(JSON.parse(new TextDecoder().decode(files[0].bytes)).id);
  }
});

// Test only: Home, then Apps, then the app, then B twice (as app_smoke.mjs does, with the test commands).
async function openForTest(id) {
  const hex = h => new TextDecoder().decode(new Uint8Array((h.match(/../g) || []).map(x => parseInt(x, 16))));
  const state = async () => {
    await dev.send('X STATE');
    const r = await dev.expect('OK X STATE', 5000);
    const kv = Object.fromEntries(r.split(' ').slice(3).map(p => p.split('=')));
    return { screen: hex(kv.screen), sel: hex(kv.sel), detail: hex(kv.detail) };
  };
  const press = async k => { await dev.send(`X BTN ${k}`); await dev.expect('OK X BTN', 10000); };
  for (let i = 0; i < 6 && (await state()).screen === 'Welcome'; i++) await press('b');
  for (let i = 0; i < 4 && (await state()).screen !== 'Home'; i++) await press('A');
  for (let i = 0; i < 14 && (await state()).sel !== 'Apps'; i++) await press('a');
  await press('b');
  for (let i = 0; i < 30 && (await state()).detail !== `list:${id}`; i++) await press('a');
  await press('b');
  await press('b');
  await press('b');
}
