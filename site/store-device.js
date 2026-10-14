// The app store's side of the USB protocol (src/apps/scripts/appsusb.h, docs/APPS.md): list, install, remove.
// `dev` is what serial.js connect() returns ({send, expect}). Kept apart from the page so it can be tested
// against the emulator (emu/test/store_test.mjs).

const dec = new TextDecoder();
const fromHex = h => dec.decode(new Uint8Array((h.match(/../g) || []).map(x => parseInt(x, 16))));

export const tooOld = 'This unidex doesn\'t know about apps yet. Update its firmware (Install page), then try again.';

// zlib crc32, as the device checks it.
const crcTable = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();
export function crc32(bytes) {
  let c = 0xFFFFFFFF;
  for (const b of bytes) c = crcTable[(c ^ b) & 255] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}

export async function sha256hex(bytes) {
  const d = new Uint8Array(await crypto.subtle.digest('SHA-256', bytes));
  return [...d].map(b => b.toString(16).padStart(2, '0')).join('');
}

// "1.2.10" vs "1.2.9": -1, 0 or 1 (by number, not by text). Anything else sorts lowest.
export function compareAppVersions(a, b) {
  const p = v => (/^(\d+)\.(\d+)\.(\d+)$/.exec(v || '') || [0, -1, -1, -1]).slice(1).map(Number);
  const x = p(a), y = p(b);
  for (let i = 0; i < 3; i++) if (x[i] !== y[i]) return x[i] < y[i] ? -1 : 1;
  return 0;
}

// The reason inside "ERR <hex>", or a plain sentence.
function refusal(e) {
  const hex = /^ERR ([0-9a-f]+)$/.exec(e.reply || '');
  if (hex) return fromHex(hex[1]);
  if (e.reply === 'ERR busy') return 'it\'s open on the device, so leave it (hold A) and try again';
  return e.message;
}

// [{id, version (null: offloaded), code, data, problem}]
export async function listApps(dev) {
  const apps = [];
  await dev.send('A LIST');
  const done = await dev.expect('OK A end', 8000, line => {
    const [tag, id, version, code, data, state] = line.split(' ');
    if (tag === 'A' && id) apps.push({ id, version: version === '-' ? null : version, code: +code, data: +data,
                                     problem: state && state !== 'ok' ? fromHex(state) : '' });
  });
  if (!done) throw new Error(tooOld);
  return apps;
}

export async function dataBytes(dev, id) {
  await dev.send(`A DATA ${id}`);
  const line = await dev.expect('OK A DATA ', 5000);
  if (!line) throw new Error(tooOld);
  return +line.split(' ')[3];
}

// keepData: an offload (the app goes, its data stays).
export async function removeApp(dev, id, keepData) {
  try {
    await dev.send(`A RM ${id}${keepData ? ' keep' : ''}`);
    if (!(await dev.expect(`OK A RM ${id}`, 8000))) throw new Error(tooOld);
  } catch (e) {
    throw new Error(`Couldn't remove ${id}: ${refusal(e)}.`);
  }
}

// Installs (or updates) a catalogue entry. getFile(entry, file) returns the file's bytes (Uint8Array); each is
// checked against the catalogue's SHA-256 before it is sent. progress(text, fraction) reports as it goes.
export async function installApp(dev, entry, getFile, progress = () => {}) {
  const files = [];
  for (const f of entry.files) {
    const bytes = await getFile(entry, f);
    if (bytes.length !== f.size || (await sha256hex(bytes)) !== f.sha256)
      throw new Error(`${entry.name}: ${f.path} didn't download correctly. Reload the page and try again.`);
    files.push({ path: f.path, bytes });
  }
  const total = files.reduce((n, f) => n + f.bytes.length, 0);
  let sent = 0;
  try {
    await dev.send(`A BEGIN ${entry.id}`);
    if (!(await dev.expect('OK A BEGIN', 5000))) throw new Error(tooOld);
    for (const f of files) {
      progress(`Sending ${f.path}`, sent / total);
      await dev.send(`A PUT ${entry.id} ${f.path} ${f.bytes.length} ${crc32(f.bytes)}`);
      if (!(await dev.expect('OK A PUT', 5000))) throw new Error('no answer');
      for (let i = 0; i < f.bytes.length; i += 64) {
        const chunk = f.bytes.subarray(i, i + 64);
        await dev.send('D ' + [...chunk].map(b => b.toString(16).padStart(2, '0')).join(''));
        if (!(await dev.expect('K', 3000))) throw new Error('it stopped answering');
        sent += chunk.length;
        progress(`Sending ${f.path}`, sent / total);
      }
      if (!(await dev.expect('OK A FILE', 5000))) throw new Error(`${f.path} wasn't saved`);
    }
    progress('Checking it on the device', 1);
    await dev.send(`A DONE ${entry.id}`);
    if (!(await dev.expect(`OK A DONE ${entry.id}`, 20000))) throw new Error('no answer');
  } catch (e) {
    if (e.message === tooOld) throw e;
    throw new Error(`Couldn't install ${entry.name}: ${refusal(e)}.`);
  }
}
