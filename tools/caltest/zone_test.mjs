// Checks the Tools page's time zones (site/calendar.js): the TZ rule it sends the device for this computer's zone
// (posixZone), for every zone this Node knows, against Node's own time zone data; and the names it sends.
//   node tools/caltest/zone_test.mjs
// The device reads the same rules with src/core/zone_logic.h (tools/clocktest/zone_test.cpp checks that side).
import fs from 'fs';
import os from 'os';
import path from 'path';
import { pathToFileURL } from 'url';

const here = path.dirname(new URL(import.meta.url).pathname);
const src = fs.readFileSync(path.join(here, '../../site/calendar.js'), 'utf8')
  .replace(/import ICAL from '[^']+';/, 'const ICAL = null;')  // not needed for zones
  .replace(/import \{ crc32 \} from '\.\/serial\.js';/, 'const crc32 = () => 0;');
const tmp = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'cal-')), 'calendar.mjs');
fs.writeFileSync(tmp, src);
const { posixZone, zoneName, LONDON } = await import(pathToFileURL(tmp));

let failures = 0;
const check = (ok, what) => { console.log(`${ok ? 'ok  ' : 'FAIL'}  ${what}`); failures += !ok; };

// Node's offset (seconds east of UTC) for a zone at t (ms).
const formats = new Map();
const offset = (zone, t) => {
  let f = formats.get(zone);
  if (!f) formats.set(zone, f = new Intl.DateTimeFormat('en-GB', { timeZone: zone, year: 'numeric', month: 'numeric',
    day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric', hourCycle: 'h23' }));
  const p = Object.fromEntries(f.formatToParts(new Date(t)).map(x => [x.type, +x.value]));
  return (Date.UTC(p.year, p.month - 1, p.day, p.hour, p.minute, p.second) - Math.floor(t / 1000) * 1000) / 1000;
};

// A rule in the form posixZone writes ("STD-1DST-2,M3.5.0/2,M10.5.0/3"), read independently of it: its offset at t.
const T = '(-?\\d+(?::\\d\\d){0,2})', M = ',M(\\d+)\\.(\\d)\\.(\\d)\\/(\\d+(?::\\d\\d){0,2})';
const RULE = new RegExp(`^STD${T}(?:DST${T}${M}${M})?$`);
const secs = s => {  // "-5:30" -> -19800
  const [h, m = 0, x = 0] = s.replace('-', '').split(':').map(Number);
  return (s[0] === '-' ? -1 : 1) * (h * 3600 + m * 60 + x);
};
const ruleOffset = (rule, t) => {
  const r = RULE.exec(rule);
  const std = -secs(r[1]);
  if (!r[2]) return std;
  const dst = -secs(r[2]);
  const y = new Date(t).getUTCFullYear();
  const day = (m, w, d) => {  // the wth (5 = last) weekday d of month m, as ms at 00:00 UTC
    if (w < 5) {
      const first = Date.UTC(y, m - 1, 1);
      return first + (((d - new Date(first).getUTCDay() + 7) % 7) + (w - 1) * 7) * 86400000;
    }
    const last = Date.UTC(y, m, 0);
    return last - ((new Date(last).getUTCDay() - d + 7) % 7) * 86400000;
  };
  const start = day(+r[3], +r[4], +r[5]) + (secs(r[6]) - std) * 1000;
  const end = day(+r[7], +r[8], +r[9]) + (secs(r[10]) - dst) * 1000;
  const summer = start < end ? t >= start && t < end : t >= start || t < end;
  return summer ? dst : std;
};

// Known zones
const YEAR = 2026, NOW = Date.UTC(2026, 10, 13, 17, 57);  // when issue #29 was opened
const KNOWN = {
  'Europe/Paris': 'STD-1DST-2,M3.5.0/2,M10.5.0/3',
  'Europe/London': 'STD0DST-1,M3.5.0/1,M10.5.0/2',
  'America/New_York': 'STD5DST4,M3.2.0/2,M11.1.0/2',
  'Australia/Sydney': 'STD-10DST-11,M10.1.0/2,M4.1.0/3',
  'Pacific/Auckland': 'STD-12DST-13,M9.5.0/2,M4.1.0/3',
  'Asia/Kolkata': 'STD-5:30',
  'Asia/Kathmandu': 'STD-5:45',
  'Pacific/Chatham': 'STD-12:45DST-13:45,M9.5.0/2:45,M4.1.0/3:45',
  'America/Sao_Paulo': 'STD3',
  'Asia/Tokyo': 'STD-9',
  'UTC': 'STD0',
};
for (const [zone, want] of Object.entries(KNOWN)) {
  const got = posixZone(zone, YEAR, NOW);
  check(got === want, `${zone}: ${got}${got === want ? '' : ` (want ${want})`}`);
}
check(LONDON === 'Europe/London', 'London is the zone for firmware that has none');

// Every zone: a rule the device reads, right all year where the zone's clocks change by a yearly rule
let zones = 0, followed = 0;
const odd = [];
for (const zone of Intl.supportedValuesOf('timeZone')) {
  zones++;
  const rule = posixZone(zone, YEAR, NOW);
  if (!RULE.test(rule) || rule.length > 63) {
    check(false, `${zone}: ${rule} isn't a rule the device reads`);
    continue;
  }
  check(ruleOffset(rule, NOW) === offset(zone, NOW), `${zone}: ${rule} is right now`);
  // the times its clocks change this year
  const from = Date.UTC(YEAR, 0, 1), to = Date.UTC(YEAR + 1, 0, 1);
  let changes = 0;
  for (let t = from; t < to; t += 86400000) changes += offset(zone, t) !== offset(zone, t + 86400000);
  if (changes !== 0 && changes !== 2) { odd.push(`${zone} (${changes})`); continue; }
  followed++;
  let wrong = 0;
  for (let t = from; t < to; t += 3600000) {
    if (ruleOffset(rule, t) !== offset(zone, t)) wrong++;
    if (offset(zone, t) !== offset(zone, t + 3600000))
      for (let u = t; u <= t + 3600000; u += 60000) wrong += ruleOffset(rule, u) !== offset(zone, u);
  }
  if (wrong) check(false, `${zone}: ${rule} is wrong for ${wrong} hours/minutes of ${YEAR}`);
}
check(followed > 350, `${followed} of ${zones} zones right all ${YEAR}, to the minute at each change`);
console.log(`      right from now on only (rules changed this year, or Morocco's Ramadan): ${odd.join(', ') ||
  'none'}`);

// Names: the city, plain text, up to 23 characters
check(zoneName('Europe/Paris') === 'Paris' && zoneName('America/Argentina/Buenos_Aires') === 'Buenos Aires' &&
        zoneName('America/Port-au-Prince') === 'Port-au-Prince' && zoneName('UTC') === 'UTC',
      'names: Paris, Buenos Aires, Port-au-Prince, UTC');
check(Intl.supportedValuesOf('timeZone').every(z => /^[\x20-\x7e]{1,23}$/.test(zoneName(z))),
  'every name is plain text, 1-23 characters');

if (failures) console.log(`\n${failures} check(s) failed`);
process.exit(failures ? 1 : 0);
