// Calendar files (.ics) to the device's Timetable, and the clock. Used by the Tools page's calendar card and its
// one-click sync. Lines and lengths match the device (src/apps/timetable/timetable.cpp) and tools/calsync.
import ICAL from 'https://cdn.jsdelivr.net/npm/ical.js@2.2.1/dist/ical.min.js';
import { crc32 } from './serial.js';

export const DAYS_AHEAD = 7;
const MAX_EVENTS = 64;
export const LONDON = 'Europe/London';  // the device's zone until it's set (and the only one before firmware had Z)

// Same cleaning as tools/calsync/calsync.swift: the device font is ASCII, fields are comma-separated and an event is
// one line, so a line break or comma becomes a space ("Charrington Street\nLondon" -> "Charrington Street London").
const clean = (s, max) => (s || '').normalize('NFKD').replace(/[̀-ͯ]/g, '')
  .replace(/[\s,]+/g, ' ').replace(/[^\x20-\x7e]/g, '').trim().slice(0, max);
const formats = new Map();  // one formatter per zone: making them is the slow part
const parts = (d, zone) => {
  let f = formats.get(zone);
  if (!f) formats.set(zone, f = new Intl.DateTimeFormat('en-GB', { timeZone: zone, year: 'numeric', month: '2-digit',
    day: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit', hourCycle: 'h23' }));
  return Object.fromEntries(f.formatToParts(d).map(p => [p.type, p.value]));
};
const dayOf = (d, zone) => { const p = parts(d, zone); return `${p.year}-${p.month}-${p.day}`; };
export const hmOf = (d, zone = LONDON) => { const p = parts(d, zone); return `${p.hour}:${p.minute}`; };

// This computer's time zone ("Europe/Paris").
export const computerZone = () => Intl.DateTimeFormat().resolvedOptions().timeZone || LONDON;

// What the device calls a zone: its city, in plain text ("America/Argentina/Buenos_Aires" -> "Buenos Aires").
export const zoneName = zone => clean(zone.split('/').pop().replace(/_/g, ' '), 23);

// A zone as the TZ rule the device takes ("Europe/Paris" -> "STD-1DST,M3.5.0/2,M10.5.0/3"), from when its clocks
// change in `year`. A zone whose changes don't follow a "last Sunday of March" kind of rule (or that changes more
// than twice a year) gets the offset it has now, all year: the next sync puts it right.
export function posixZone(zone, year = new Date().getUTCFullYear(), now = Date.now()) {
  const offsetAt = t => {  // seconds east of UTC at t (ms)
    const p = parts(new Date(t), zone);
    return (Date.UTC(+p.year, p.month - 1, +p.day, +p.hour, +p.minute, +p.second) - Math.floor(t / 1000) * 1000) / 1000;
  };
  const hms = s => {  // POSIX time: "1", "-5:30", "2:00:30"
    const a = Math.abs(s), sign = s < 0 ? '-' : '';
    const pad = n => String(n).padStart(2, '0');
    const h = Math.floor(a / 3600), m = Math.floor(a / 60) % 60, sec = a % 60;
    return sign + h + (m || sec ? `:${pad(m)}` : '') + (sec ? `:${pad(sec)}` : '');
  };
  const from = Date.UTC(year, 0, 1), to = Date.UTC(year + 1, 0, 1), DAY = 86400000;
  const changes = [];
  for (let t = from; t < to && changes.length < 3; t += DAY) {
    const before = offsetAt(t), after = offsetAt(t + DAY);
    if (before === after) continue;
    let lo = t, hi = t + DAY;  // the second it changes: lo still has `before`, hi has the new offset
    while (hi - lo > 1000) {
      const mid = lo + Math.floor((hi - lo) / 2000) * 1000;
      if (offsetAt(mid) === before) lo = mid; else hi = mid;
    }
    changes.push({ at: hi, before, after: offsetAt(hi) });
  }
  const fixed = () => `STD${hms(-offsetAt(now))}`;
  if (changes.length !== 2) return fixed();
  const [a, b] = changes;
  const toDst = a.after > a.before ? a : b, toStd = toDst === a ? b : a;
  if (!(toDst.after > toDst.before && toStd.after < toStd.before && toDst.after === toStd.before &&
        toDst.before === toStd.after)) return fixed();
  const rule = c => {  // ",Mm.w.d/time": local time just before the change, on the wth (5 = last) weekday
    const w = new Date(c.at + c.before * 1000);  // read with the UTC getters: the wall clock there
    const day = w.getUTCDate(), m = w.getUTCMonth() + 1;
    const days = new Date(Date.UTC(w.getUTCFullYear(), m, 0)).getUTCDate();
    const week = day + 7 > days ? 5 : Math.ceil(day / 7);
    const time = w.getUTCHours() * 3600 + w.getUTCMinutes() * 60 + w.getUTCSeconds();
    return `,M${m}.${week}.${w.getUTCDay()}/${hms(time)}`;
  };
  return `STD${hms(-toStd.after)}DST${hms(-toDst.after)}${rule(toDst)}${rule(toStd)}`;
}

// .ics text -> device lines for today + 7 days in `zone` (the device's), soonest first.
export function eventsFrom(text, zone = LONDON) {
  const cal = new ICAL.Component(ICAL.parse(text));
  for (const tz of cal.getAllSubcomponents('vtimezone')) ICAL.TimezoneService.register(tz);
  const now = new Date(), first = dayOf(now, zone), last = dayOf(new Date(now.getTime() + DAYS_AHEAD * 86400000), zone);
  const masters = new Map(), out = [];
  const vevents = cal.getAllSubcomponents('vevent');
  for (const v of vevents) {  // an event without a UID (some apps leave it out) still counts, as itself
    if (!v.hasProperty('recurrence-id')) masters.set(v.getFirstPropertyValue('uid') || Symbol('no uid'), new ICAL.Event(v));
  }
  for (const v of vevents) {  // moved or cancelled single occurrences of a repeating event
    const master = v.hasProperty('recurrence-id') && masters.get(v.getFirstPropertyValue('uid'));
    if (master) master.relateException(new ICAL.Event(v));
  }
  // Lengths match the device (timetable.cpp): title 63, location 47, notes 160.
  const add = (summary, location, description, start, end, allDay) => {
    const title = clean(summary, 63), place = clean(location, 47);
    const notes = clean((description || '').replace(/\s*\n\s*/g, ' / '), 160);
    if (allDay) {  // one line per day it covers, inside the window (end date is exclusive)
      for (let d = start.clone(); d.compare(end) < 0; d.addDuration(ICAL.Duration.fromData({ days: 1 }))) {
        const day = d.toString().slice(0, 10);
        if (day >= first && day <= last) out.push({ at: new Date(day + 'T00:00:00Z'), line: `${day},,,${title},${place},${notes}` });
      }
      return;
    }
    const s = start.toJSDate(), e = end.toJSDate(), day = dayOf(s, zone);
    if (day < first || day > last) return;
    const endText = dayOf(e, zone) === day ? hmOf(e, zone) : '23:59';  // crossing midnight: cut at 23:59
    out.push({ at: s, line: `${day},${hmOf(s, zone)},${endText},${title},${place},${notes}` });
  };
  const windowEnd = ICAL.Time.fromJSDate(new Date(now.getTime() + (DAYS_AHEAD + 2) * 86400000), true);
  const windowStart = ICAL.Time.fromJSDate(new Date(now.getTime() - 2 * 86400000), true);
  for (const ev of masters.values()) {
    if (ev.component.getFirstPropertyValue('status') === 'CANCELLED') continue;
    if (!ev.isRecurring()) {
      add(ev.summary, ev.location, ev.description, ev.startDate, ev.endDate, ev.startDate.isDate);
      continue;
    }
    const it = ev.iterator();
    for (let next, guard = 0; (next = it.next()) && guard < 5000; guard++) {
      if (next.compare(windowEnd) > 0) break;
      if (next.compare(windowStart) < 0) continue;
      const o = ev.getOccurrenceDetails(next);
      if (o.item.component.getFirstPropertyValue('status') === 'CANCELLED') continue;
      add(o.item.summary, o.item.location, o.item.description, o.startDate, o.endDate, o.startDate.isDate);
    }
  }
  return out.sort((a, b) => a.at - b.at).slice(0, MAX_EVENTS).map(e => e.line);
}

// Sets the device's clock to this computer's time, and its time zone to this computer's. Returns the zone the device
// is in now, to write its events in: this computer's, or London from firmware that has no time zone setting (it
// doesn't answer Z).
export async function setTime(dev, zone = computerZone()) {
  await dev.send(`T ${Math.round(Date.now() / 1000)}`);
  if (!await dev.expect('OK T', 3000)) throw new Error("The device didn't confirm the time.");
  const rule = posixZone(zone);
  await dev.send(`Z ${rule} ${zoneName(zone)}`.trimEnd());
  try {
    return await dev.expect('OK Z', 1500) ? zone : LONDON;
  } catch (e) {
    if (e.reply) throw new Error(`The device didn't take the time zone ${zone} (${rule}).`);
    throw e;
  }
}

// Replaces the device's events with `lines` (from eventsFrom).
export async function sendEvents(dev, lines) {
  const body = lines.map(l => l + '\n').join('');
  await dev.send(`E ${lines.length} ${crc32(new TextEncoder().encode(body))}`);
  for (const l of lines) await dev.send(l);
  if (!await dev.expect('OK E', 5000)) throw new Error("The device didn't confirm the events.");
}
