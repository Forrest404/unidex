// The Apps page: what is on the unidex (over USB), the store's list (store/index.json, built by
// tools/make_store.py), and install, update, offload and delete. The USB steps are in store-device.js.
import { supported, connect, knownPort, asleepHint } from './serial.js';
import { listApps, installApp, removeApp, dataBytes, compareAppVersions, tooOld } from './store-device.js';

const $ = id => document.getElementById(id);
const DEFAULT_ICON = [
  '........................................',
  '........................................',
  '........................................',
  '........................................',
  '.....############......############.....',
  '....##############....##############....',
  '....##..........##....##..........##....',
  '....##..........##....##..........##....',
  '....##..........##....##..........##....',
  '....##..........##....##..........##....',
  '....##..........##....##..........##....',
  '....##..........##....##..........##....',
  '....##..........##....##..........##....',
  '....##..........##....##..........##....',
  '....##..........##....##..........##....',
  '....##..........##....##..........##....',
  '....##############....##############....',
  '.....############......############.....',
  '........................................',
  '........................................',
  '........................................',
  '........................................',
  '.....############.......................',
  '....##############..........##..........',
  '....##..........##..........##..........',
  '....##..........##..........##..........',
  '....##..........##..........##..........',
  '....##..........##..........##..........',
  '....##..........##.....############.....',
  '....##..........##.....############.....',
  '....##..........##..........##..........',
  '....##..........##..........##..........',
  '....##..........##..........##..........',
  '....##..........##..........##..........',
  '....##############..........##..........',
  '.....############.......................',
  '........................................',
  '........................................',
  '........................................',
  '........................................',
];
const PERMISSION_TEXT = { wifi: 'uses WiFi', mic: 'uses the microphone', meet: 'uses the Pet radio' };

let catalogue = [], device = null, deviceApps = null, busy = false;

const el = (tag, props = {}, ...children) => {
  const e = Object.assign(document.createElement(tag), props);
  for (const c of children) if (c != null) e.append(c);
  return e;
};

function iconCanvas(rows) {
  const c = el('canvas', { width: 40, height: 40, className: 'icon' });
  c.setAttribute('aria-hidden', 'true');
  const g = c.getContext('2d');
  g.fillStyle = '#fff';
  g.fillRect(0, 0, 40, 40);
  g.fillStyle = '#000';
  (rows && rows.length === 40 ? rows : DEFAULT_ICON).forEach((row, y) => {
    for (let x = 0; x < 40; x++) if (row[x] === '#') g.fillRect(x, y, 1, 1);
  });
  return c;
}

const kb = n => n < 1024 ? `${n} byte${n === 1 ? '' : 's'}` : `${Math.ceil(n / 1024)} KB`;
const inStore = id => catalogue.find(a => a.id === id);
const onDevice = id => deviceApps && deviceApps.find(a => a.id === id);

function status(text, kind = '') {
  $('devStatus').textContent = text;
  $('devStatus').className = 'status ' + kind;
}

function ask(title, text, yes, danger) {
  $('confirmTitle').textContent = title;
  $('confirmText').textContent = text;
  $('confirmYes').textContent = yes;
  $('confirmYes').className = danger ? 'danger' : '';
  return new Promise(resolve => {
    const d = $('confirm');
    const done = ok => { d.close(); resolve(ok); };
    $('confirmYes').onclick = () => done(true);
    $('confirmNo').onclick = () => done(false);
    d.oncancel = () => resolve(false);
    d.showModal();
  });
}

async function getFile(entry, f) {
  const r = await fetch(`store/apps/${entry.id}/${entry.version}/${f.path}`, { cache: 'no-cache' });
  if (!r.ok) throw new Error(`${entry.name}: couldn’t download ${f.path}.`);
  return new Uint8Array(await r.arrayBuffer());
}

// --- drawing the two lists ---
function renderDevice() {
  const list = $('devApps');
  list.replaceChildren();
  list.hidden = !deviceApps;
  if (!deviceApps) return;
  if (!deviceApps.length) {
    list.append(el('li', { className: 'meta' }, 'No apps on it yet. Install one from the store below.'));
    return;
  }
  for (const a of deviceApps) {
    const s = inStore(a.id);
    const name = s ? s.name : a.id;
    const info = el('div', { className: 'info' }, el('div', { className: 'name' }, name));
    const meta = [];
    if (a.version) meta.push(`version ${a.version}`);
    meta.push(a.version ? `${kb(a.code)} of code` : 'offloaded');
    meta.push(`${kb(a.data)} of data`);
    info.append(el('div', { className: 'meta' }, meta.join(' · ')));
    const nameLine = info.firstChild;
    const newer = s && a.version && compareAppVersions(s.version, a.version) > 0;
    if (a.problem) nameLine.append(el('span', { className: 'chip bad', title: a.problem }, 'can’t run'));
    else if (newer) nameLine.append(el('span', { className: 'chip new' }, `${s.version} available`));
    else if (!s) nameLine.append(el('span', { className: 'chip', title: 'Installed by hand, not from this store' }, 'not in the store'));
    if (a.problem) info.append(el('div', { className: 'meta bad' }, a.problem));

    const actions = el('div', { className: 'actions' });
    if (newer) actions.append(el('button', { textContent: 'Update', onclick: () => install(s) }));
    if (!a.version && s) actions.append(el('button', { textContent: 'Reinstall', onclick: () => install(s) }));
    if (a.version) actions.append(el('button', { className: 'secondary', textContent: 'Offload', onclick: () => offload(a, name) }));
    actions.append(el('button', { className: 'danger', textContent: a.version ? 'Delete' : 'Delete data',
                                  onclick: () => remove(a, name) }));
    list.append(el('li', {}, iconCanvas(s && s.icon), info, actions));
  }
}

function renderStore() {
  const grid = $('storeGrid');
  grid.replaceChildren();
  for (const s of catalogue) {
    const d = onDevice(s.id);
    let label = 'Install', disabled = !device || busy;
    if (d && d.version && compareAppVersions(s.version, d.version) <= 0) label = 'Installed', disabled = true;
    else if (d && d.version) label = 'Update';
    else if (d) label = 'Reinstall';
    const perms = s.permissions.length ? s.permissions.map(p => PERMISSION_TEXT[p] || p).join(', ')
                                       : 'No special permissions';
    grid.append(el('div', { className: 'card storeCard' },
      el('div', { className: 'top' }, iconCanvas(s.icon),
        el('div', {}, el('h3', { textContent: s.name }),
          el('div', { className: 'by', textContent: `${s.author || 'unknown'} · version ${s.version}` }))),
      el('p', { textContent: s.description }),
      el('div', { className: 'perms', textContent: perms[0].toUpperCase() + perms.slice(1) + '.' }),
      el('div', { className: 'cardFoot' }, el('span', { className: 'size', textContent: kb(s.bytes) }),
        el('button', { textContent: label, disabled, title: device ? '' : 'Connect your unidex first',
                       onclick: () => install(s) }))));
  }
}

function render() {
  renderDevice();
  renderStore();
  $('connect').textContent = device ? 'Refresh' : 'Connect';
  $('connect').disabled = busy || !supported;
}

// --- talking to the device ---
async function run(work) {
  if (busy) return;
  busy = true;
  render();
  try {
    if (!device) device = await connect(await knownPort() || undefined);
    await work(device);
    deviceApps = await listApps(device);
  } catch (e) {
    if (e.name === 'NotFoundError') status(asleepHint, 'bad');
    else status(e.message, 'bad');
    if (e.message === tooOld || /closed|lost|disconnect|device has been lost/i.test(e.message)) await disconnect();
  } finally {
    busy = false;
    $('progress').hidden = true;
    render();
  }
}

async function disconnect() {
  if (device) await device.close().catch(() => {});
  device = null;
  deviceApps = null;
}

function install(s) {
  return run(async dev => {
    const was = onDevice(s.id);
    $('progress').hidden = false;
    $('progress').value = 0;
    await installApp(dev, s, getFile, (text, fraction) => {
      status(`${s.name}: ${text}…`);
      $('progress').value = fraction;
    });
    status(was && was.version ? `${s.name} updated to ${s.version}. Its data was kept.`
                              : `${s.name} installed. Find it under Apps on the device.`, 'ok');
  });
}

async function offload(a, name) {
  if (!(await ask(`Offload ${name}?`, 'The app is removed, but its data stays on the device. Install it again ' +
                  'to pick up where you left off.', 'Offload', false))) return;
  return run(async dev => {
    await removeApp(dev, a.id, true);
    status(`${name} offloaded. Its data is still on the device.`, 'ok');
  });
}

async function remove(a, name) {
  let bytes = a.data;
  try { if (device) bytes = await dataBytes(device, a.id); } catch (e) { /* the list's figure will do */ }
  const what = bytes ? `Its data on the device (${kb(bytes)}) will be lost for good.` : 'It has no saved data.';
  if (!(await ask(a.version ? `Delete ${name}?` : `Delete ${name}’s data?`, `${what} This can’t be undone.`,
                  'Delete', true))) return;
  return run(async dev => {
    await removeApp(dev, a.id, false);
    status(`${name} deleted.`, 'ok');
  });
}

$('connect').onclick = () => run(async () => status('Connected.', 'ok'));
if (!supported) $('unsupported').hidden = false;
navigator.serial && navigator.serial.addEventListener('disconnect', async () => {
  await disconnect();
  status('The unidex was unplugged (or fell asleep). Press a button on it, then Connect.');
  render();
});

try {
  const r = await fetch('store/index.json', { cache: 'no-cache' });
  if (!r.ok) throw new Error();
  catalogue = (await r.json()).apps || [];
  $('storeStatus').textContent = catalogue.length ? `${catalogue.length} app${catalogue.length === 1 ? '' : 's'}.`
                                                  : 'No apps in the store yet.';
} catch (e) {
  $('storeStatus').textContent = 'The store’s list couldn’t be loaded. Reload the page to try again.';
}
render();
