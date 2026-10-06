// Talks to a unidex over USB with Web Serial (Chrome / Edge on desktop).
// The device's protocol (src/core/usbsync.cpp): "?" -> "unidex 1", T <unix> sets the clock,
// E <count> <crc32> + lines sends calendar events, L / B / D list and upload badges.

export const supported = 'serial' in navigator;

// zlib crc32, the same as the device's esp_rom_crc32_le.
export const crc32 = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return bytes => {
    let c = 0xFFFFFFFF;
    for (const b of bytes) c = t[(c ^ b) & 255] ^ (c >>> 8);
    return (c ^ 0xFFFFFFFF) >>> 0;
  };
})();

export const asleepHint = 'No device picked. Nothing listed? The board is asleep: press one of its buttons, then try again.';

const FILTER = { usbVendorId: 0x303a };

// A board this site was allowed to use before (no picker needed), or null.
export async function knownPort() {
  const ports = await navigator.serial.getPorts();
  return ports.find(p => p.getInfo().usbVendorId === FILTER.usbVendorId) || null;
}

export const pickPort = () => navigator.serial.requestPort({ filters: [FILTER] });

// Opens the board (`port`, or one the user picks) and says hello. Returns {send, expect, close, port}.
export async function connect(port) {
  port = port || await pickPort();
  await port.open({ baudRate: 115200 });
  // The ESP32-S3 resets if RTS is on while DTR is off, so clear RTS first, then DTR.
  await port.setSignals({ requestToSend: false });
  await port.setSignals({ dataTerminalReady: false });
  const writer = port.writable.getWriter(), reader = port.readable.getReader();
  const enc = new TextEncoder(), dec = new TextDecoder();
  let buf = '';
  const pump = (async () => {
    try {
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break;
        buf += dec.decode(value, { stream: true });
      }
    } catch (e) { /* port closed */ }
  })();
  const send = text => writer.write(enc.encode(text + '\n'));
  // Next line starting with `prefix` (others are skipped), or null after `ms`. "ERR" throws.
  const expect = async (prefix, ms, onOther) => {
    const end = Date.now() + ms;
    while (Date.now() < end) {
      let nl;
      while ((nl = buf.indexOf('\n')) >= 0) {
        const line = buf.slice(0, nl).replace(/\r$/, '');
        buf = buf.slice(nl + 1);
        if (line.startsWith(prefix)) return line;
        if (line === 'ERR') throw new Error('The device rejected it.');
        if (onOther) onOther(line);
      }
      await new Promise(r => setTimeout(r, 10));
    }
    return null;
  };
  const close = async () => {
    await reader.cancel().catch(() => {});
    reader.releaseLock();
    writer.releaseLock();
    await pump;
    await port.close().catch(() => {});
  };
  let hello = null;
  for (let i = 0; i < 6 && !hello; i++) {  // it may still be busy redrawing after a wake
    await send('?');
    hello = await expect('unidex 1', 1000);
  }
  if (!hello) {
    await close();
    throw new Error('No reply. Is unidex installed? Press a button on the device to wake it, then try again.');
  }
  return { send, expect, close, port };
}

// The device's firmware version ("v1.4", "v1.4-3-gabc1234"), or null for firmware too old to say.
export async function deviceVersion(dev) {
  await dev.send('V');
  const line = await dev.expect('OK V ', 1500);
  return line ? line.slice(5).trim() : null;
}

// -1 if a is older than b, 0 the same, 1 newer; null if either can't be read (a test build, "dev").
// "v1.4-3-gabc" is 3 commits after v1.4: newer than v1.4, older than v1.5.
export function compareVersions(a, b) {
  const parse = v => {
    const m = /^v?(\d+)\.(\d+)(?:\.(\d+))?(?:-(\d+)-g[0-9a-f]+)?$/.exec(v || '');
    return m && [+m[1], +m[2], +(m[3] || 0), +(m[4] || 0)];
  };
  const x = parse(a), y = parse(b);
  if (!x || !y) return null;
  for (let i = 0; i < 4; i++) if (x[i] !== y[i]) return x[i] < y[i] ? -1 : 1;
  return 0;
}

// ---- Notes (src/apps/notes/usb.h): settings, connection tests and note download ----
// Settings travel as hex so any character survives the line protocol. Secrets are written, never read
// back: the device only reports whether each is set (and the last 4 characters of API keys).

const enc = new TextEncoder(), dec = new TextDecoder();
const toHex = s => [...enc.encode(s)].map(b => b.toString(16).padStart(2, '0')).join('');
export const fromHex = h => dec.decode(new Uint8Array((h.match(/../g) || []).map(x => parseInt(x, 16))));
const tooOld = 'This device doesn’t know about Notes yet. Update its firmware on the Install page first.';

// {settings: {name: {set, value}}, card, notes, waiting}
export async function notesStatus(dev) {
  const settings = {};
  let counts = null;
  await dev.send('N ?');
  const done = await dev.expect('OK N ?', 4000, line => {
    const [tag, name, state, hex = ''] = line.split(' ');
    if (tag === 'NS') settings[name] = { set: state === 'set', value: fromHex(hex) };
    if (tag === 'NC') counts = { card: name === '1', notes: +state, waiting: +hex };
  });
  if (!done) throw new Error(tooOld);
  return { settings, ...counts };
}

// Saves one setting; an empty value clears it.
export async function notesSet(dev, name, value) {
  await dev.send(`N SET ${name} ${toHex(value)}`);
  if (!(await dev.expect(`OK N SET ${name}`, 3000))) throw new Error(`The device didn’t save ${name}.`);
}

// For values longer than one USB line (the eduroam CA): chunks with N ADD, the last one with N SET, which saves.
export async function notesSetLong(dev, name, value) {
  let i = 0;
  for (; value.length - i > 280; i += 280) {
    await dev.send(`N ADD ${name} ${toHex(value.slice(i, i + 280))}`);
    if (!(await dev.expect(`OK N ADD ${name}`, 3000))) throw new Error(`The device didn’t take ${name}.`);
  }
  await notesSet(dev, name, value.slice(i));
}

// Starts the device sending its waiting notes over its own WiFi (it carries on after the page lets go).
// {started: n} | {none: true} | {busy: true} | {fail: reason}
export async function notesSync(dev) {
  await dev.send('N SYNC');
  const line = await dev.expect('OK N SYNC', 15000);
  if (!line) throw new Error(tooOld);
  const [, , , what, ...rest] = line.split(' ');
  if (what === 'started') return { started: +rest[0] };
  if (what === 'none') return { none: true };
  if (what === 'busy') return { busy: true };
  return { fail: rest.join(' ') };
}

export async function notesClear(dev, name = 'all') {
  await dev.send(`N CLR ${name}`);
  if (!(await dev.expect('OK N CLR', 3000))) throw new Error(tooOld);
}

// Runs on the device, with its own WiFi: 'wifi', 'openai', 'anthropic' or 'github'. {ok, reason}
export async function notesTest(dev, what) {
  await dev.send(`N TEST ${what}`);
  const line = await dev.expect(`OK N TEST ${what}`, 45000);
  if (!line) throw new Error('No answer from the device.');
  const rest = line.slice(`OK N TEST ${what} `.length);
  return rest === 'ok' ? { ok: true } : { ok: false, reason: rest.replace(/^fail /, '') };
}

// Two seconds from the microphone: {peak, rms} (0-32767), or {reason}.
export async function notesMic(dev) {
  await dev.send('N MIC');
  const line = await dev.expect('OK N MIC', 6000);
  if (!line) throw new Error(tooOld);
  const [, , , a, ...b] = line.split(' ');
  return a === 'fail' ? { reason: b.join(' ') } : { peak: +a, rms: +b[0] };
}

// [{id, bytes, text, pushed, title}], newest first.
export async function notesList(dev) {
  const notes = [];
  await dev.send('N LIST');
  const done = await dev.expect('OK N LIST', 15000, line => {
    const [tag, id, bytes, text, pushed, hex = ''] = line.split(' ');
    if (tag === 'NF') notes.push({ id, bytes: +bytes, text: text === '1', pushed: pushed === '1', title: fromHex(hex) });
  });
  if (!done) throw new Error(tooOld);
  return notes;
}

// One note's Markdown, checked with its crc32.
export async function notesRead(dev, id) {
  const bytes = [];
  await dev.send(`N READ ${id}`);
  const done = await dev.expect('OK N READ', 15000, line => {
    if (line.startsWith('ND ')) for (const pair of line.slice(3).match(/../g) || []) bytes.push(parseInt(pair, 16));
  });
  if (!done) throw new Error('The device couldn’t read that note.');
  const [, , , size, crc] = done.split(' ');
  const data = new Uint8Array(bytes);
  if (data.length !== +size || crc32(data) !== +crc) throw new Error('The note arrived damaged. Try again.');
  return dec.decode(data);
}

export async function notesDelete(dev, id) {
  await dev.send(`N DEL ${id}`);
  if (!(await dev.expect('OK N DEL', 3000))) throw new Error(tooOld);
}
