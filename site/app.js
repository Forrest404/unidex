// One app's page (app.html?id=<id>): what it is, what it may use, and Install / Update / Offload / Delete.
import { supported } from './serial.js';
import { STORE_REPO } from './chrome.js';
import {
  el, icon, kb, kindLabel, iconTile, loadCatalogue, device, onDevice, onDeviceChange, reportTo, withDevice,
  stateOf, install, offload, remove, CATEGORIES, EXTENDS, PERMISSIONS, PERMISSION_DETAIL,
} from './store-ui.js';

const $ = id => document.getElementById(id);
const id = new URLSearchParams(location.search).get('id') || '';
let app = null, statusLine, progress;

function notFound(message) {
  $('crumb').textContent = 'Not found';
  document.title = 'App not found · unidex';
  $('app').replaceChildren(el('div', { class: 'empty' },
    el('h2', { text: 'No app called that' }),
    el('p', { class: 'muted', text: message || `There's no "${id}" in the store. It may have been renamed or removed.` }),
    el('a', { class: 'button', href: 'store.html', text: 'Browse all apps' })));
}

function fact(label, value) {
  return [el('dt', { text: label }), el('dd', {}, value)];
}

function actions() {
  const s = stateOf(app);
  const box = el('div', { class: 'app-actions' });
  const busy = device.busy || !supported;
  if (!device.apps) {
    box.append(el('button', { type: 'button', disabled: busy, onclick: () => install(app) }, icon('install'), 'Install on my unidex'),
               el('button', { class: 'secondary', type: 'button', disabled: busy, text: 'Check my unidex',
                              onclick: () => withDevice(async () => {}).then(ok => ok && say(describe(), '')) }));
  } else if (s === 'none' || s === 'offloaded') {
    box.append(el('button', { type: 'button', disabled: busy, onclick: () => install(app) }, icon('install'), s === 'offloaded' ? 'Reinstall' : 'Install'));
  } else {
    if (s === 'update') box.append(el('button', { type: 'button', disabled: busy, text: `Update to ${app.version}`, onclick: () => install(app) }));
    box.append(el('button', { class: 'secondary', type: 'button', disabled: busy, text: 'Offload', onclick: () => offload(app.id, app.name) }));
  }
  if (device.apps && s !== 'none') box.append(el('button', { class: 'danger', type: 'button', disabled: busy,
    text: s === 'offloaded' ? 'Delete its data' : 'Delete', onclick: () => remove(app.id, app.name, s !== 'offloaded') }));
  return box;
}

function describe() {
  const s = stateOf(app), d = onDevice(app.id);
  if (s == null) return supported ? 'Plug your unidex in and press one of its buttons, then install.' : 'Use Chrome or Edge on a computer to install it.';
  if (s === 'installed') return `Installed on your unidex (v${d.version}), with ${kb(d.data)} of data.`;
  if (s === 'update') return `Your unidex has v${d.version}. Updating keeps its data.`;
  if (s === 'offloaded') return `Offloaded: its data (${kb(d.data)}) is still on your unidex. Reinstall to use it again.`;
  return 'Not on your unidex yet.';
}

let lastKind = '';
function say(text, kind = '', fraction) {
  statusLine.textContent = text;
  statusLine.className = `status ${kind}`;
  lastKind = kind;
  progress.hidden = fraction == null;
  if (fraction != null) progress.value = fraction;
}

function render() {
  const where = app.type === 'plugin' ? `It adds to ${EXTENDS[app.extends] || app.extends} on your unidex: open ${EXTENDS[app.extends] || app.extends} to find it.`
                                      : 'It opens from Apps on your unidex.';
  const perms = app.permissions.length
    ? el('ul', { class: 'perm-list' }, app.permissions.map(p => el('li', {}, el('b', { text: PERMISSIONS[p] || p }), ' ', PERMISSION_DETAIL[p] || '')))
    : el('p', { text: 'No special permissions: it draws on the screen, reads the buttons and keeps its own files. Nothing else.' });
  const src = app.source || `${STORE_REPO}/tree/main/apps/${app.id}`;
  const view = el('div', { class: 'app-layout' },
    el('div', { class: 'app-main' },
      el('div', { class: 'app-hero' }, iconTile(app.icon, 112),
        el('div', {},
          el('p', { class: 'eyebrow', text: kindLabel(app) }),
          el('h1', { text: app.name }),
          el('p', { class: 'by', text: `by ${app.author || 'unknown'} · version ${app.version}` }))),
      el('p', { class: 'lead', text: app.description }),
      el('div', { class: 'card app-install' }, actions(), statusLine, progress),
      el('h2', { text: 'Where to find it' }), el('p', { text: where }),
      el('h2', { text: 'What it can use' }), perms,
      el('p', { class: 'note' }, 'Every app runs in a sandbox, with its own memory, its own folder and a time limit. It can\'t reach your WiFi password, keys, notes or other apps. ',
        el('a', { href: 'developers.html#limits', text: 'How the sandbox works' }), '.')),
    el('aside', { class: 'app-side card' },
      el('dl', { class: 'facts' },
        fact('Category', el('a', { href: `store.html?cat=${encodeURIComponent(app.category || 'other')}`, text: CATEGORIES[app.category] || 'Other' })),
        fact('Type', app.type === 'plugin' ? kindLabel(app) : 'App'),
        fact('Version', app.version),
        app.updated ? fact('Updated', new Date(app.updated).toLocaleDateString('en-GB', { day: 'numeric', month: 'long', year: 'numeric' })) : null,
        fact('Size', kb(app.bytes)),
        fact('Licence', app.license || 'Not given'),
        fact('Files', el('span', { class: 'files', text: app.files.map(f => f.path).join(', ') }))),
      el('a', { class: 'button secondary', href: src, target: '_blank', rel: 'noopener' }, icon('code'), 'View the code ↗'),
      el('a', { class: 'quiet-link', href: `${STORE_REPO}/issues`, target: '_blank', rel: 'noopener', text: 'Report a problem with it ↗' })));
  $('app').replaceChildren(view);
}

reportTo((text, kind, fraction) => say(text, kind, fraction));
onDeviceChange(() => {
  if (!app) return;
  const text = statusLine.textContent, kind = lastKind, hidden = progress.hidden;
  render();
  if (device.busy || kind) say(text, kind, hidden ? undefined : progress.value);
  else say(describe());
});

try {
  const all = await loadCatalogue();
  app = all.find(a => a.id === id);
  if (!app) notFound();
  else {
    $('crumb').textContent = app.name;
    document.title = `${app.name} · unidex apps`;
    statusLine = el('p', { class: 'status', role: 'status' });
    progress = el('progress', { max: 1, value: 0, hidden: true });
    render();
    say(describe());
  }
} catch (e) {
  notFound(e.message);
}
