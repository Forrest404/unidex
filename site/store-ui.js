// What the Apps page (store.js) and an app's own page (app.js) share: the catalogue (store/index.json, built by
// tools/make_store.py), the device (connected for each action, then let go, so other pages and tools can use it),
// install / update / offload / delete with their warnings, and small pieces of the look.
import { connect, knownPort, asleepHint } from './serial.js';
import { listApps, installApp, removeApp, dataBytes, compareAppVersions, tooOld } from './store-device.js';
import { el, icon } from './chrome.js';

export { compareAppVersions, el, icon };
export const EXTENDS = { badge: 'Badge' };  // the built-in apps plugins add to
export const PERMISSIONS = { wifi: 'Uses WiFi', mic: 'Uses the microphone', meet: 'Uses the Pet radio' };
export const PERMISSION_DETAIL = {
  wifi: 'It can connect to the internet over your saved WiFi, to the addresses its README names. It never sees your WiFi password or keys.',
  mic: 'It can record from the microphone.',
  meet: 'It can use the radio the Pet uses to meet other unidex.',
};
export const CATEGORIES = { games: 'Games', tools: 'Tools', time: 'Time & dates', badges: 'Badges', learning: 'Learning',
                            fun: 'Fun', other: 'Other' };
// The apps app icon from the device (theme.cpp ICON_APPS), for apps without their own.
const DEFAULT_ICON = [...Array(40)].map((_, y) => [...Array(40)].map((_, x) => {
  const sq = (x0, y0) => x >= x0 && x < x0 + 14 && y >= y0 && y < y0 + 14 &&
    (x < x0 + 2 || x >= x0 + 12 || y < y0 + 2 || y >= y0 + 12) && !((x === x0 || x === x0 + 13) && (y === y0 || y === y0 + 13));
  const plus = (x >= 22 && x < 36 && (y === 28 || y === 29)) || (y >= 22 && y < 36 && (x === 28 || x === 29));
  return sq(4, 4) || sq(22, 4) || sq(4, 22) || plus ? '#' : '.';
}).join(''));

export const kb = n => n < 1024 ? `${n} byte${n === 1 ? '' : 's'}` : `${Math.ceil(n / 1024)} KB`;
export const plural = (n, word) => `${n} ${word}${n === 1 ? '' : 's'}`;
export const kindLabel = a => (a.type === 'plugin' ? `Plugin for ${EXTENDS[a.extends] || a.extends}` : 'App');

// The app's 40x40 icon as an e-ink tile (`size` in CSS pixels).
export function iconTile(rows, size = 48) {
  const c = el('canvas', { width: 40, height: 40, class: 'app-icon', 'aria-hidden': 'true' });
  c.style.width = c.style.height = `${size}px`;
  const g = c.getContext('2d');
  g.fillStyle = '#e9e6da';
  g.fillRect(0, 0, 40, 40);
  g.fillStyle = '#141414';
  (rows && rows.length === 40 ? rows : DEFAULT_ICON).forEach((row, y) => {
    for (let x = 0; x < 40; x++) if (row[x] === '#') g.fillRect(x, y, 1, 1);
  });
  return c;
}

export async function loadCatalogue() {
  const r = await fetch('store/index.json', { cache: 'no-cache' });
  if (!r.ok) throw new Error('The list of apps couldn\'t be loaded. Reload the page to try again.');
  return (await r.json()).apps || [];
}

// ---- the device ----
// state.apps: what it said last ([{id, version|null, code, data, problem}]), or null before the first Connect.
export const device = { apps: null, busy: false };
const listeners = new Set();
export const onDeviceChange = fn => listeners.add(fn);
const changed = () => listeners.forEach(fn => fn());

let reporter = () => {};
export const reportTo = fn => { reporter = fn; };  // (text, kind, fraction?) -> the page shows it

// Opens the board, does `work`, reads the list again, and lets go.
export async function withDevice(work) {
  if (device.busy) return false;
  device.busy = true;
  changed();
  let dev = null, ok = false;
  try {
    dev = await connect(await knownPort() || undefined);
    await work(dev);
    device.apps = await listApps(dev);
    ok = true;
  } catch (e) {
    if (e.name === 'NotFoundError') reporter(asleepHint, 'bad');
    else if (e.name === 'InvalidStateError' || /already open|Failed to open/i.test(e.message))
      reporter('Another page or program is using the unidex. Close it (or its tab), then try again.', 'bad');
    else reporter(e.message, 'bad');
    if (e.message === tooOld) device.apps = null;
  } finally {
    if (dev) await dev.close().catch(() => {});
    device.busy = false;
    changed();
  }
  return ok;
}

export const onDevice = id => device.apps && device.apps.find(a => a.id === id);

// What the device has of a catalogue entry: 'none', 'installed', 'update', 'offloaded' (or null: not connected).
export function stateOf(entry) {
  if (!device.apps) return null;
  const d = onDevice(entry.id);
  if (!d) return 'none';
  if (!d.version) return 'offloaded';
  return compareAppVersions(entry.version, d.version) > 0 ? 'update' : 'installed';
}

async function getFile(entry, f) {
  const r = await fetch(`store/apps/${entry.id}/${entry.version}/${f.path}`, { cache: 'no-cache' });
  if (!r.ok) throw new Error(`${entry.name}: couldn't download ${f.path}.`);
  return new Uint8Array(await r.arrayBuffer());
}

export function install(entry) {
  const was = stateOf(entry);
  return withDevice(async dev => {
    await installApp(dev, entry, getFile, (text, fraction) => reporter(`${entry.name}: ${text}…`, '', fraction));
    const where = entry.type === 'plugin' ? `in ${EXTENDS[entry.extends] || entry.extends}` : 'under Apps';
    reporter(was === 'update' ? `${entry.name} updated to ${entry.version}. Its data was kept.`
                              : `${entry.name} installed. Find it ${where} on your unidex.`, 'ok');
  });
}

// ---- the "are you sure?" box ----
let box;
export function ask(title, text, yes, danger) {
  if (!box) {
    box = el('dialog', { class: 'confirm' }, el('h2', {}), el('p', {}),
      el('div', { class: 'buttons' }, el('button', { class: 'secondary', type: 'button', text: 'Cancel' }), el('button', { type: 'button' })));
    document.body.append(box);
  }
  const [h, p, buttons] = box.children, [no, ok] = buttons.children;
  h.textContent = title;
  p.textContent = text;
  ok.textContent = yes;
  ok.className = danger ? 'danger' : '';
  return new Promise(resolve => {
    const done = v => { box.close(); resolve(v); };
    ok.onclick = () => done(true);
    no.onclick = () => done(false);
    box.oncancel = () => resolve(false);
    box.showModal();
    no.focus();
  });
}

export async function offload(id, name) {
  if (!(await ask(`Offload ${name}?`, 'The app is removed, but its data stays on the device. Install it again ' +
                  'to pick up where you left off.', 'Offload', false))) return;
  return withDevice(async dev => {
    await removeApp(dev, id, true);
    reporter(`${name} offloaded. Its data is still on the device.`, 'ok');
  });
}

export async function remove(id, name, hasCode = true) {
  let bytes = (onDevice(id) || {}).data || 0;  // asked again first: the app may have saved more since
  await withDevice(async dev => { bytes = await dataBytes(dev, id); });
  const what = bytes ? `Its data on the device (${kb(bytes)}) will be lost for good.` : 'It has no saved data.';
  if (!(await ask(hasCode ? `Delete ${name}?` : `Delete ${name}'s data?`, `${what} This can't be undone.`,
                  'Delete', true))) return;
  return withDevice(async dev => {
    await removeApp(dev, id, false);
    reporter(`${name} deleted.`, 'ok');
  });
}
