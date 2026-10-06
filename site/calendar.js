// Calendar files (.ics) to the device's Timetable, and the clock. Used by the Tools page's calendar card and its
// one-click sync. Lines and lengths match the device (src/apps/timetable/timetable.cpp) and tools/calsync.
import ICAL from 'https://cdn.jsdelivr.net/npm/ical.js@2.2.1/dist/ical.min.js';
import { crc32 } from './serial.js';

export const DAYS_AHEAD = 7;
const MAX_EVENTS = 64, TZ = 'Europe/London';  // the device shows London time

// Same cleaning as tools/calsync/calsync.swift: the device font is ASCII, fields are comma-separated.
const clean = (s, max) => (s || '').normalize('NFKD').replace(/[̀-ͯ]/g, '')
  .replace(/[^\x20-\x7e]|,/g, '').trim().slice(0, max);
const parts = d => Object.fromEntries(new Intl.DateTimeFormat('en-GB', { timeZone: TZ, year: 'numeric',
  month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', hourCycle: 'h23' })
  .formatToParts(d).map(p => [p.type, p.value]));
const dayOf = d => { const p = parts(d); return `${p.year}-${p.month}-${p.day}`; };
export const hmOf = d => { const p = parts(d); return `${p.hour}:${p.minute}`; };

// .ics text -> device lines for today + 7 days (London), soonest first.
export function eventsFrom(text) {
  const cal = new ICAL.Component(ICAL.parse(text));
  for (const tz of cal.getAllSubcomponents('vtimezone')) ICAL.TimezoneService.register(tz);
  const now = new Date(), first = dayOf(now), last = dayOf(new Date(now.getTime() + DAYS_AHEAD * 86400000));
  const masters = new Map(), out = [];
  const vevents = cal.getAllSubcomponents('vevent');
  for (const v of vevents) if (!v.hasProperty('recurrence-id')) masters.set(v.getFirstPropertyValue('uid'), new ICAL.Event(v));
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
    const s = start.toJSDate(), e = end.toJSDate(), day = dayOf(s);
    if (day < first || day > last) return;
    const endText = dayOf(e) === day ? hmOf(e) : '23:59';  // crossing midnight: cut at 23:59
    out.push({ at: s, line: `${day},${hmOf(s)},${endText},${title},${place},${notes}` });
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

// Sets the device's clock to this computer's time.
export async function setTime(dev) {
  await dev.send(`T ${Math.round(Date.now() / 1000)}`);
  if (!await dev.expect('OK T', 3000)) throw new Error('The device did not confirm the time.');
}

// Replaces the device's events with `lines` (from eventsFrom).
export async function sendEvents(dev, lines) {
  const body = lines.map(l => l + '\n').join('');
  await dev.send(`E ${lines.length} ${crc32(new TextEncoder().encode(body))}`);
  for (const l of lines) await dev.send(l);
  if (!await dev.expect('OK E', 5000)) throw new Error('The device did not confirm the events.');
}
