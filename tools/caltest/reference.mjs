// The Tools page's calendar reader (site/calendar.js) on a file at a fixed time, to make the expected lines that
// ics_test.cpp checks the device's reader against. Needs ical.js 2.2.1 (npm install ical.js@2.2.1 anywhere). The
// lines are in the device's time zone, London unless a third argument names another; TZ (the computer's own zone,
// which floating times are read in) is the same one, as when the Tools page sets the device to the computer's zone:
//   TZ=Europe/London ICALJS=<path>/node_modules/ical.js/dist/ical.js \
//     node tools/caltest/reference.mjs tools/caltest/fixtures/recur.ics 2026-10-23T09:00:00Z
//   TZ=Europe/Paris ICALJS=... node tools/caltest/reference.mjs tools/caltest/fixtures/basic.ics \
//     2026-10-23T09:00:00Z Europe/Paris
import fs from 'fs';
import os from 'os';
import path from 'path';
import { pathToFileURL } from 'url';

const [, , icsPath, nowIso, zone = 'Europe/London'] = process.argv;
const here = path.dirname(new URL(import.meta.url).pathname);
const src = fs.readFileSync(path.join(here, '../../site/calendar.js'), 'utf8')
  .replace(/import ICAL from '[^']+';/, `import ICAL from '${pathToFileURL(process.env.ICALJS)}';`)
  .replace(/import \{ crc32 \} from '\.\/serial\.js';/, 'const crc32 = () => 0;');
const fixed = new Date(nowIso).getTime();
const RealDate = Date;
globalThis.Date = class extends RealDate {  // new Date() and Date.now() are the fixed time
  constructor(...a) { super(...(a.length ? a : [fixed])); }
  static now() { return fixed; }
};
const tmp = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'cal-')), 'calendar.mjs');
fs.writeFileSync(tmp, src);
const { eventsFrom } = await import(pathToFileURL(tmp));
process.stdout.write(eventsFrom(fs.readFileSync(icsPath, 'utf8'), zone).map(l => l + '\n').join(''));
