// The device's time zone over USB and in Settings, against the real firmware in the emulator: the Tools page's own
// code (site/calendar.js: setTime, which sends the computer's zone) sets it, Z reports it, nonsense is refused,
// firmware without Z gets London times, and Settings > Time zone steps to the next city. Needs emu/build.sh first.
//   node emu/test/zone_test.mjs
import { createRequire } from 'module';
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import path from 'path';
const here = path.dirname(fileURLToPath(import.meta.url)), root = path.join(here, '..', '..');
const require = createRequire(import.meta.url);
// site/ has no package.json saying "module", so load the file's text as a module (ical.js isn't needed here)
const src = readFileSync(path.join(root, 'site/calendar.js'), 'utf8')
  .replace(/import ICAL from '[^']+';/, 'const ICAL = null;')
  .replace(/import \{ crc32 \} from '\.\/serial\.js';/, 'const crc32 = () => 0;');
const cal = await import('data:text/javascript,' + encodeURIComponent(src));
const createUnidex = require(path.join(root, 'emu/out/unidex.js'));

const M = await createUnidex({ onScreen() {}, print: () => {}, printErr: () => {} });
let buf = '';
setInterval(() => { buf += M.ccall('emu_serial_out', 'string', [], []) || ''; }, 5);
const dev = {
  send: async t => M.ccall('emu_serial_in', null, ['string'], [t + '\n']),
  expect: async (prefix, ms, onOther) => {  // as serial.js does it
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
const ask = async (line, prefix, ms = 3000) => { await dev.send(line); return dev.expect(prefix, ms); };
const zone = () => ask('Z', 'OK Z ');
let failures = 0;
const check = (ok, what) => { console.log(`${ok ? 'ok  ' : 'FAIL'}  ${what}`); failures += !ok; };

await new Promise(r => setTimeout(r, 1500));  // start-up

check(await zone() === 'OK Z GMT0BST,M3.5.0/1,M10.5.0 London', 'a new device is on London time');

// The Tools page, on a computer in Paris (issue #29)
const paris = cal.posixZone('Europe/Paris');
check(await cal.setTime(dev, 'Europe/Paris') === 'Europe/Paris', 'setTime from a computer in Paris: the device takes it');
check(await zone() === `OK Z ${paris} Paris`, `the device is on ${paris}, called Paris`);
check(await cal.setTime(dev, 'America/Argentina/Buenos_Aires') === 'America/Argentina/Buenos_Aires' &&
        await zone() === `OK Z ${cal.posixZone('America/Argentina/Buenos_Aires')} Buenos Aires`,
      'a name with a space: Buenos Aires');

// Refused, and nothing changed
const before = await zone();
for (const bad of ['Z nonsense', 'Z CET-1CEST', 'Z CET-1 ' + 'x'.repeat(24), 'Z ' + 'A'.repeat(70) + '0', 'Z CET-1 café']) {
  let refused = false;
  try { await ask(bad, 'OK Z', 1500); } catch (e) { refused = e.reply === 'ERR'; }
  check(refused && await zone() === before, `refused, zone kept: ${bad.slice(0, 40)}`);
}
check(await ask('Z CET-1CEST,M3.5.0,M10.5.0/3', 'OK Z') === 'OK Z' && await zone() === 'OK Z CET-1CEST,M3.5.0,M10.5.0/3 Paris',
      'a rule with no name is named after its preset: Paris');
check(await ask('Z STD-3:30', 'OK Z') === 'OK Z' && await zone() === 'OK Z STD-3:30 UTC+3:30',
      'a rule with no name and no preset: UTC+3:30');

// Firmware from before time zones doesn't answer Z: the Tools page writes London times for it
const old = { send: t => (t.startsWith('Z') ? Promise.resolve() : dev.send(t)), expect: dev.expect };
check(await cal.setTime(old, 'Europe/Paris') === cal.LONDON, 'firmware with no Z: setTime says London');

// Settings > Time zone: B steps to the next city
await ask('Z CET-1CEST,M3.5.0,M10.5.0/3 Paris', 'OK Z');
const state = async () => Object.fromEntries((await ask('X STATE', 'OK X STATE')).split(' ').slice(3)
  .map(kv => kv.split('=')).map(([k, v]) => [k, Buffer.from(v || '', 'hex').toString()]));
const press = b => ask(`X BTN ${b}`, 'OK X BTN', 8000);
for (let i = 0; i < 6 && (await state()).screen === 'Welcome'; i++) await press('b');  // a new device's first screens
for (let i = 0; i < 30 && (await state()).sel !== 'Settings'; i++) await press('a');
check((await state()).sel === 'Settings', 'Settings is on the home screen');
await press('b');  // open it: the cursor is on Date & time
await press('a');  // Time zone
await press('b');
check(await zone() === 'OK Z CET-1CEST,M3.5.0,M10.5.0/3 Berlin', 'Settings > Time zone, B: from Paris to Berlin');
for (let i = 0; i < 44; i++) await press('b');
check(await zone() === 'OK Z CET-1CEST,M3.5.0,M10.5.0/3 Paris', 'and 44 more: all the way round, back to Paris');

if (failures) console.log(`\n${failures} check(s) failed`);
process.exit(failures ? 1 : 0);
