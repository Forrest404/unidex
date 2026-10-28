// Tries third-party apps in the emulator, the way a person would: installs each app folder over the USB protocol
// (the website's own code, site/store-device.js), opens it from Apps, presses every button, waits for its ticks,
// and leaves. An app passes if it never lands on "App error". Saves a screenshot of each screen it passes through,
// for the reviewer.
//   node emu/test/app_smoke.mjs <app folder>... [--shots <folder>]
// Needs emu/build.sh first. Exit status 1 if any app fails.
import { createRequire } from 'module';
import { readFileSync, readdirSync, statSync, mkdirSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { deflateSync } from 'zlib';
import path from 'path';
const here = path.dirname(fileURLToPath(import.meta.url)), root = path.join(here, '..', '..');
const require = createRequire(import.meta.url);
const store = await import('data:text/javascript,' + encodeURIComponent(readFileSync(path.join(root, 'site/store-device.js'), 'utf8')));
const createUnidex = require(path.join(root, 'emu/out/unidex.js'));

const args = process.argv.slice(2);
const shotsAt = args.indexOf('--shots');
const shotsDir = shotsAt >= 0 ? args.splice(shotsAt, 2)[1] : null;
if (!args.length) { console.error('usage: node emu/test/app_smoke.mjs <app folder>... [--shots <folder>]'); process.exit(2); }

const M = await createUnidex({ onScreen() {}, print: () => {}, printErr: () => {} });
let buf = '';
setInterval(() => { buf += M.ccall('emu_serial_out', 'string', [], []) || ''; }, 5);
const dev = {
  send: async t => M.ccall('emu_serial_in', null, ['string'], [t + '\n']),
  expect: async (prefix, ms, onOther) => {
    const end = Date.now() + ms;
    while (Date.now() < end) {
      let nl;
      while ((nl = buf.indexOf('\n')) >= 0) {
        const line = buf.slice(0, nl).replace(/\r$/, '');
        buf = buf.slice(nl + 1);
        if (line.startsWith(prefix)) return line;
        if (line === 'ERR' || line.startsWith('ERR ')) throw Object.assign(new Error('The device rejected it.'), { reply: line });
        if (onOther) onOther(line);
      }
      await new Promise(r => setTimeout(r, 5));
    }
    return null;
  },
};
const sleep = ms => new Promise(r => setTimeout(r, ms));
const hex = h => Buffer.from(h || '', 'hex').toString();

async function cmd(line, prefix, ms = 20000) {
  for (;;) {
    await dev.send(line);
    const reply = await dev.expect(prefix, ms).catch(e => e.reply);
    if (reply !== 'ERR busy') return reply;
    await sleep(300);  // an animation is playing
  }
}
async function state() {
  const r = await cmd('X STATE', 'OK X STATE');
  const kv = Object.fromEntries(r.split(' ').slice(3).map(p => p.split('=')));
  return { screen: hex(kv.screen), sel: hex(kv.sel), detail: hex(kv.detail) };
}
const press = key => cmd(`X BTN ${key}`, 'OK X BTN');

function png(data, file) {  // the 200x200 1-bit screen, at 2x
  const S = 2, W = 200 * S, rows = [];
  for (let y = 0; y < W; y++) {
    const row = Buffer.alloc(W + 1);
    for (let x = 0; x < W; x++) {
      const px = Math.floor(x / S), py = Math.floor(y / S), i = py * 200 + px;
      row[x + 1] = data[i >> 3] & (0x80 >> (i & 7)) ? 0 : 255;  // (in a shot, a set bit is ink)
    }
    rows.push(row);
  }
  const chunk = (t, d) => {
    const c = Buffer.alloc(12 + d.length);
    c.writeUInt32BE(d.length, 0); c.write(t, 4); d.copy(c, 8);
    let crc = ~0;
    for (const b of c.subarray(4, 8 + d.length)) { crc ^= b; for (let k = 0; k < 8; k++) crc = crc & 1 ? 0xEDB88320 ^ (crc >>> 1) : crc >>> 1; }
    c.writeUInt32BE(~crc >>> 0, 8 + d.length);
    return c;
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(W, 4); ihdr[8] = 8; ihdr[9] = 0;
  writeFileSync(file, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr),
                                     chunk('IDAT', deflateSync(Buffer.concat(rows))), chunk('IEND', Buffer.alloc(0))]));
}
async function shot(name) {
  if (!shotsDir) return;
  await dev.send('X SHOT');
  const lines = [];
  await dev.expect('OK X SHOT', 20000, l => lines.push(l));
  const data = Buffer.from(lines.filter(l => l.startsWith('XD ')).map(l => l.slice(3)).join(''), 'hex');
  if (data.length === 5000) png(data, path.join(shotsDir, name + '.png'));
}

// A catalogue entry for a folder, as tools/make_store.py would make it.
async function entryOf(folder) {
  const m = JSON.parse(readFileSync(path.join(folder, 'app.json'), 'utf8'));
  const files = [];
  const walk = rel => {
    for (const n of readdirSync(path.join(folder, rel)).sort()) {
      if (n.startsWith('.')) continue;
      const r = rel ? `${rel}/${n}` : n;
      if (statSync(path.join(folder, r)).isDirectory()) walk(r);
      else if (r !== 'README.md' && r !== 'LICENSE') files.push(r);
    }
  };
  walk('');
  files.sort((a, b) => (a === 'app.json') - (b === 'app.json'));
  const out = [];
  for (const f of files) {
    const bytes = new Uint8Array(readFileSync(path.join(folder, f)));
    out.push({ path: f, size: bytes.length, sha256: await store.sha256hex(bytes), bytes });
  }
  return { ...m, files: out };
}

await sleep(1500);  // start-up
for (let i = 0; i < 6 && (await state()).screen === 'Welcome'; i++) await press('b');  // a new device's first screens
if (shotsDir) mkdirSync(shotsDir, { recursive: true });
let failed = 0;
for (const folder of args) {
  const entry = await entryOf(folder);
  const id = entry.id, problems = [];
  const failure = async () => {
    const s = await state();
    return s.detail.startsWith('failed:') ? s.detail.slice(7) || 'App error' : null;
  };
  try {
    await store.installApp(dev, entry, async (e, f) => f.bytes);
    if (entry.type === 'plugin' && entry.extends === 'badge') {
      // Badge: A until it is showing the plugin's badge (Badge redraws it with badge() each time)
      for (let i = 0; i < 14 && (await state()).sel !== 'Badge'; i++) await press('a');
      await press('b');
      let s = await state();
      for (let i = 0; i < 40 && !s.detail.includes(`plugin:${id}`); i++) { await press('a'); s = await state(); }
      if (!s.detail.includes(`plugin:${id}`)) problems.push('it isn\'t in the Badge list');
      else if (s.detail.startsWith('failed:')) problems.push(`its badge: ${s.detail.split(':').slice(3).join(':')}`);
      else { await sleep(3000); await shot(`${id}-badge`); }  // (after Badge's hint band has gone)
      for (let i = 0; i < 4 && (await state()).screen !== 'Home'; i++) await press('A');
      await store.removeApp(dev, id, false);
      console.log(problems.length ? `FAIL  ${id}: ${problems.join('; ')}` : `ok    ${id} (badge plugin)`);
      failed += problems.length > 0;
      continue;
    }
    // Games: a game, and an app in the games category, are listed there by name, not in Apps
    const inGames = entry.type === 'game' || (entry.type !== 'plugin' && entry.category === 'games');
    if (inGames) {
      for (let i = 0; i < 14 && (await state()).sel !== 'Games'; i++) await press('a');
      if ((await state()).screen === 'Home') await press('b');
      for (let i = 0; i < 40 && (await state()).detail !== `list:${entry.name}`; i++) await press('a');
      if ((await state()).detail !== `list:${entry.name}`) throw new Error('it isn\'t in the Games list');
    }
    if (entry.type === 'game') {
      // A round on a fixed seed, frame by frame: B tapped now and then, then held, then left alone
      await cmd('X SEED 7', 'OK X SEED');
      await cmd('X MANUAL 1', 'OK X MANUAL');
      await press('b');
      await shot(`${id}-1-start`);
      let s = await state();
      const pattern = ('000000001'.repeat(8) + '1'.repeat(20) + '0'.repeat(60)).match(/.{1,40}/g);
      for (const chunk of pattern) {
        if (!s.detail.startsWith('play:')) break;
        await cmd(`X PLAY ${chunk}`, 'OK X PLAY');
        s = await state();
      }
      await shot(`${id}-2-played`);
      if (s.detail.startsWith('failed:')) problems.push('it stopped with an error (see the screenshot)');
      else if (!s.detail.startsWith('play:') && !s.detail.startsWith('over:')) problems.push(`it left the round (${s.detail})`);
      await cmd('X MANUAL 0', 'OK X MANUAL');
      await cmd('X SEED 0', 'OK X SEED');
      if ((await state()).detail.startsWith('failed:')) await press('b');
      for (let i = 0; i < 4 && (await state()).screen !== 'Home'; i++) await press('A');
      await store.removeApp(dev, id, false);
      console.log(problems.length ? `FAIL  ${id}: ${problems.join('; ')}` : `ok    ${id} (game)`);
      failed += problems.length > 0;
      continue;
    }
    if (!inGames) {
      // Home: A until Apps is highlighted, then open it and find the app
      for (let i = 0; i < 14 && (await state()).sel !== 'Apps'; i++) await press('a');
      if ((await state()).screen === 'Home') await press('b');
      for (let i = 0; i < 30 && (await state()).detail !== `list:${id}`; i++) await press('a');
      if ((await state()).detail !== `list:${id}`) throw new Error('it isn\'t in the Apps list');
    }
    await press('b');
    await shot(`${id}-1-open`);
    let why = await failure();
    if (why) problems.push(`on opening: ${why}`);
    const steps = [['a', 'A'], ['b', 'B'], ['B', 'hold B'], ['b', 'B again'], ['a', 'A again']];
    for (const [key, name] of steps) {
      if (problems.length) break;
      await press(key);
      await sleep(300);
      why = await failure();
      if (why) problems.push(`after ${name}: ${why}`);
      else await shot(`${id}-${steps.findIndex(s => s[1] === name) + 2}-${name.replace(/ /g, '-')}`);
    }
    if (!problems.length) {
      await sleep(2500);  // its tick() runs about once a second
      why = await failure();
      if (why) problems.push(`while waiting: ${why}`);
    }
    // leave: hold A (an app may use it to go back a step first)
    for (let i = 0; i < 4 && (await state()).detail.startsWith('run:'); i++) await press('A');
    if (!problems.length && (await state()).detail.startsWith('run:')) problems.push('hold A doesn\'t leave it');
    if ((await state()).detail.startsWith('failed:')) await press('b');
    await store.removeApp(dev, id, false);
  } catch (e) {
    problems.push(e.message);
  }
  console.log(problems.length ? `FAIL  ${id}: ${problems.join('; ')}` : `ok    ${id}`);
  failed += problems.length > 0;
  // back to Home for the next app
  for (let i = 0; i < 4 && (await state()).screen !== 'Home'; i++) await press('A');
}
process.exit(failed ? 1 : 0);
