// The emulated unidex's serial line, for the pages that drive it: Try your app (try.html) and the App editor
// (editor.html). It speaks the same lines a real device speaks over USB, so the store's own install code
// (store-device.js) works on it unchanged. The emulator is a test build, so the test commands (X STATE, X BTN) are
// there too: they open an app the way a person would, and say when one has failed.
import { installApp, removeApp, sha256hex } from './store-device.js';

export const ready = new Promise(resolve => {
  if (window.unidexEmu) resolve(window.unidexEmu);
  else addEventListener('unidex-ready', () => resolve(window.unidexEmu), { once: true });
});

let M = null, buf = '', talking = 0;  // talking: a conversation (serially) is reading the replies itself
const printers = new Set();
export const onPrint = fn => printers.add(fn);  // fn(text): what an app print()s

// The next whole line from the device, or null. One at a time: what isn't read yet stays for the next reader.
function nextLine() {
  buf += M.ccall('emu_serial_out', 'string', [], []) || '';
  const nl = buf.indexOf('\n');
  if (nl < 0) return null;
  const line = buf.slice(0, nl).replace(/\r$/, '');
  buf = buf.slice(nl + 1);
  return line;
}
const printed = line => { if (line.startsWith('LUA ')) printers.forEach(fn => fn(line.slice(4))); };

ready.then(m => {
  M = m;
  setInterval(() => { for (let l; !talking && (l = nextLine()) !== null;) printed(l); }, 100);
});

// One conversation at a time: an install and a check for errors must not read each other's replies.
let queue = Promise.resolve();
export function serially(work) {
  const run = queue.then(() => ready).then(async m => {
    M = m;
    talking++;
    try { return await work(); } finally { talking--; }
  });
  queue = run.catch(() => {});
  return run;
}

export const dev = {
  send: async t => M.ccall('emu_serial_in', null, ['string'], [t + '\n']),
  expect: async (prefix, ms, onOther) => {
    talking++;
    try {
      const end = Date.now() + ms;
      while (Date.now() < end) {
        for (let line; (line = nextLine()) !== null;) {
          if (line.startsWith(prefix)) return line;
          if (line === 'ERR' || line.startsWith('ERR ')) throw Object.assign(new Error('The device rejected it.'), { reply: line });
          if (line.startsWith('LUA ')) printed(line);
          else if (onOther) onOther(line);
        }
        await new Promise(r => setTimeout(r, 10));
      }
      return null;
    } finally {
      talking--;
    }
  },
};

// Files ({path inside the app's folder, bytes}) as a catalogue entry, the way tools/make_store.py makes one.
export async function entryFrom(files) {
  const manifest = files.find(f => f.path === 'app.json');
  if (!manifest) throw new Error('There is no app.json.');
  let m;
  try { m = JSON.parse(new TextDecoder().decode(manifest.bytes)); }
  catch (e) { throw new Error(`app.json isn't valid JSON: ${e.message}`); }
  if (!m || typeof m !== 'object' || !m.id) throw new Error('app.json needs an "id".');
  const keep = files.filter(f => !f.path.split('/').some(s => s.startsWith('.')) && !/^data\//.test(f.path) &&
                                 f.path !== 'README.md' && f.path !== 'LICENSE');
  keep.sort((a, b) => (a.path === 'app.json') - (b.path === 'app.json'));
  const out = [];
  for (const f of keep) out.push({ path: f.path, size: f.bytes.length, sha256: await sha256hex(f.bytes), bytes: f.bytes });
  return { ...m, name: m.name || m.id, files: out };
}

const hex = h => new TextDecoder().decode(new Uint8Array((h.match(/../g) || []).map(x => parseInt(x, 16))));
async function stateNow() {
  await dev.send('X STATE');
  const r = await dev.expect('OK X STATE', 5000);
  if (!r) throw new Error("The emulator didn't answer.");
  const kv = Object.fromEntries(r.split(' ').slice(3).map(p => p.split('=')));
  return { screen: hex(kv.screen || ''), sel: hex(kv.sel || ''), detail: hex(kv.detail || '') };
}
async function pressNow(k) {
  await dev.send(`X BTN ${k}`);
  await dev.expect('OK X BTN', 10000);
}
export const state = () => serially(stateNow);

// Past the first-start screens, and back out to Home.
async function homeNow() {
  for (let i = 0; i < 6 && (await stateNow()).screen === 'Welcome'; i++) await pressNow('b');
  for (let i = 0; i < 6 && (await stateNow()).screen !== 'Home'; i++) await pressNow('A');
}

// Installs (or updates) the app; `fresh` deletes it and its data first. Returns the catalogue entry.
export function install(files, status = () => {}, { fresh = false } = {}) {
  return serially(async () => {
    const entry = await entryFrom(files);
    await homeNow();  // an app that's open can't be replaced
    if (fresh) await removeApp(dev, entry.id, false).catch(() => {});  // not there yet is fine
    for (let attempt = 0; ; attempt++) {
      try {
        await installApp(dev, entry, async (e, f) => f.bytes, t => status(`${entry.name}: ${t}…`));
        return entry;
      } catch (e) {
        if (attempt < 3 && /open on the device|open on the screen/.test(e.message)) {
          await pressNow('A');  // hold A: leave it, then try again
          continue;
        }
        throw e;
      }
    }
  });
}

// Opens it on the screen as a person would: Home, then Apps and the app, or Badge and the plugin's badge.
// Returns null when it's running, or the error the device shows.
export function open(entry) {
  return serially(async () => {
    await homeNow();
    const plugin = entry.type === 'plugin';
    const target = plugin ? 'Badge' : 'Apps';
    for (let i = 0; i < 16 && (await stateNow()).sel !== target; i++) await pressNow('a');
    await pressNow('b');
    if (plugin) {
      let s = await stateNow();
      for (let i = 0; i < 40 && !s.detail.includes(`plugin:${entry.id}`); i++) { await pressNow('a'); s = await stateNow(); }
      if (s.detail.startsWith('failed:')) return s.detail.split(':').slice(3).join(':');
      return s.detail.includes(`plugin:${entry.id}`) ? null : "It isn't in Badge's list.";
    }
    let s = await stateNow();
    for (let i = 0; i < 40 && s.detail !== `list:${entry.id}`; i++) { await pressNow('a'); s = await stateNow(); }
    if (s.detail !== `list:${entry.id}`) return "It isn't in the Apps list.";
    await pressNow('b');
    s = await stateNow();
    return s.detail.startsWith('failed:') ? s.detail.slice(7) : null;
  });
}

// The error on the screen now, if an app has failed (null if not).
export async function failure() {
  const s = await state();
  if (s.screen === 'Apps' && s.detail.startsWith('failed:')) return s.detail.slice(7);
  if (s.screen === 'Badge' && s.detail.startsWith('failed:plugin:')) return s.detail.split(':').slice(3).join(':');
  return null;
}
