// The Apps page: the library (search, filters, sort, all kept in the address so links and Back keep them) and your
// unidex (what's on it, with Update, Offload and Delete). Shared pieces are in store-ui.js.
import { supported } from './serial.js';
import { STORE_REPO } from './chrome.js';
import {
  el, icon, kb, plural, kindLabel, iconTile, loadCatalogue, device, onDevice, onDeviceChange, reportTo, withDevice,
  stateOf, install, offload, remove, compareAppVersions, CATEGORIES, EXTENDS, PERMISSIONS,
} from './store-ui.js';

const $ = id => document.getElementById(id);
let catalogue = [];

// ---- filters ----
// Each facet: a URL key and how an app's values are read. Within a facet any ticked option matches (or);
// across facets all must (and).
const FACETS = [
  { key: 'type', title: 'Type', values: a => [a.type || 'app'], label: v => (v === 'plugin' ? 'Plugins' : 'Apps') },
  { key: 'cat', title: 'Category', values: a => [a.category || 'other'], label: v => CATEGORIES[v] || v },
  { key: 'with', title: 'Works with', values: a => (a.type === 'plugin' ? [a.extends] : []), label: v => EXTENDS[v] || v,
    hint: 'Plugins that add to a built-in app' },
  { key: 'perm', title: 'Permissions', values: a => (a.permissions.length ? a.permissions : ['none']),
    label: v => (v === 'none' ? 'No special permissions' : PERMISSIONS[v] || v) },
  { key: 'dev', title: 'On your unidex', values: a => { const s = stateOf(a); return s ? [s === 'offloaded' ? 'none' : s] : []; },
    label: v => ({ installed: 'Installed', update: 'Update available', none: 'Not installed' })[v], fixed: ['installed', 'update', 'none'],
    needsDevice: true },
];

const params = () => new URLSearchParams(location.search);
const chosen = () => {
  const p = params(), out = {};
  for (const f of FACETS) out[f.key] = new Set((p.get(f.key) || '').split(',').filter(Boolean));
  return out;
};
const query = () => (params().get('q') || '').trim();
const sortBy = () => params().get('sort') || 'name';

function setParam(key, value) {
  const p = params();
  if (value) p.set(key, value); else p.delete(key);
  const s = p.toString();
  history.replaceState(null, '', s ? `?${s}${location.hash}` : location.pathname + location.hash);
  render();
}

const norm = s => (s || '').toLowerCase();
function textMatch(a, q) {
  const hay = norm(`${a.name} ${a.author} ${a.description} ${a.id} ${CATEGORIES[a.category] || ''} ${kindLabel(a)}`);
  return norm(q).split(/\s+/).filter(Boolean).every(w => hay.includes(w));
}

// Does `a` pass the search and every facet except `skip` (for an option's count)?
function passes(a, sel, q, skip) {
  if (q && !textMatch(a, q)) return false;
  for (const f of FACETS) {
    if (f.key === skip || !sel[f.key].size) continue;
    if (!f.values(a).some(v => sel[f.key].has(v))) return false;
  }
  return true;
}

function sorted(list) {
  const by = sortBy();
  const cmp = {
    name: (a, b) => a.name.localeCompare(b.name),
    updated: (a, b) => (b.updated || '').localeCompare(a.updated || '') || a.name.localeCompare(b.name),
    small: (a, b) => a.bytes - b.bytes,
    large: (a, b) => b.bytes - a.bytes,
  }[by] || ((a, b) => a.name.localeCompare(b.name));
  return [...list].sort(cmp);
}

function renderFacets(sel, q) {
  const box = $('facets');
  box.replaceChildren();
  for (const f of FACETS) {
    const options = f.fixed || [...new Set(catalogue.flatMap(f.values))].sort((x, y) => f.label(x).localeCompare(f.label(y)));
    if (!options.length && !f.needsDevice) continue;
    const disabled = f.needsDevice && !device.apps;
    const set = el('fieldset', { class: 'facet', disabled },
      el('legend', { text: f.title }),
      disabled ? el('p', { class: 'hint', text: 'Connect your unidex to use these.' }) : f.hint ? el('p', { class: 'hint', text: f.hint }) : null);
    for (const v of options) {
      const n = disabled ? null : catalogue.filter(a => passes(a, sel, q, f.key) && f.values(a).includes(v)).length;
      const box2 = el('input', { type: 'checkbox', checked: sel[f.key].has(v),
        onchange: e => {
          const s = new Set(sel[f.key]);
          e.target.checked ? s.add(v) : s.delete(v);
          setParam(f.key, [...s].join(','));
        } });
      set.append(el('label', { class: 'check' }, box2, el('span', { text: f.label(v) }), n == null ? null : el('small', { text: String(n) })));
    }
    box.append(set);
  }
}

function renderChips(sel, q) {
  const box = $('activeChips');
  box.replaceChildren();
  const chips = [];
  if (q) chips.push([`“${q}”`, () => { $('q').value = ''; setParam('q', ''); }]);
  for (const f of FACETS) for (const v of sel[f.key]) {
    chips.push([f.label(v), () => { const s = new Set(sel[f.key]); s.delete(v); setParam(f.key, [...s].join(',')); }]);
  }
  for (const [text, drop] of chips)
    box.append(el('button', { class: 'chip removable', type: 'button', 'aria-label': `Remove the filter ${text}`, onclick: drop },
      text, icon('close')));
  $('clearFilters').hidden = !chips.length;
  $('filterCount').hidden = !chips.length;
  $('filterCount').textContent = chips.length;
}

// ---- the cards ----
function stateChip(a) {
  const s = stateOf(a);
  if (s === 'installed') return el('span', { class: 'chip ok' }, icon('check'), 'Installed');
  if (s === 'update') return el('span', { class: 'chip new', text: `Update to ${a.version}` });
  if (s === 'offloaded') return el('span', { class: 'chip', text: 'Offloaded' });
  return null;
}

function actionButton(a) {
  const s = stateOf(a);
  const label = s === 'installed' ? 'Installed' : s === 'update' ? 'Update' : s === 'offloaded' ? 'Reinstall' : 'Install';
  return el('button', { class: s === 'installed' ? 'quiet' : 'secondary', type: 'button', text: label,
    disabled: !supported || device.busy || s === 'installed',
    title: supported ? '' : 'Use Chrome or Edge on a computer to install apps',
    onclick: () => install(a) });
}

function card(a) {
  const link = `app.html?id=${encodeURIComponent(a.id)}`;
  const tags = el('div', { class: 'tags' }, el('span', { class: 'chip', text: CATEGORIES[a.category] || 'Other' }));
  if (a.type === 'plugin') tags.append(el('span', { class: 'chip' }, icon('layers'), kindLabel(a)));
  for (const p of a.permissions) tags.append(el('span', { class: 'chip', text: PERMISSIONS[p] || p }));
  return el('article', { class: 'app-card' },
    el('div', { class: 'top' }, iconTile(a.icon, 52),
      el('div', {}, el('h3', {}, el('a', { href: link, class: 'stretch', text: a.name })),
        el('p', { class: 'by', text: `${a.author || 'unknown'} · v${a.version}` }))),
    el('p', { class: 'desc', text: a.description }),
    tags,
    el('div', { class: 'foot' },
      el('span', { class: 'meta', text: kb(a.bytes) }),
      stateChip(a), actionButton(a)));
}

function createCard() {
  return el('a', { class: 'app-card create', href: STORE_REPO, target: '_blank', rel: 'noopener' },
    el('span', { class: 'ic' }, icon('plus')),
    el('h3', { text: 'Create an app ↗' }),
    el('p', { class: 'desc', text: 'Make your own in a few lines of Lua, try it in the emulator, and send it in. Start from the template.' }));
}

function render() {
  const sel = chosen(), q = query();
  if ($('q').value.trim() !== q && document.activeElement !== $('q')) $('q').value = q;
  $('sort').value = sortBy();
  renderFacets(sel, q);
  renderChips(sel, q);
  const shown = sorted(catalogue.filter(a => passes(a, sel, q)));
  $('resultsTitle').textContent = catalogue.length
    ? (shown.length === catalogue.length ? plural(catalogue.length, 'app') : `${shown.length} of ${plural(catalogue.length, 'app')}`)
    : 'No apps in the store yet';
  const grid = $('grid');
  grid.replaceChildren(...shown.map(card));
  if (!shown.length && catalogue.length) {
    grid.append(el('div', { class: 'empty' }, el('h3', { text: 'Nothing matches' }),
      el('p', { class: 'muted', text: 'Try fewer filters or a shorter word.' }),
      el('button', { class: 'secondary', type: 'button', text: 'Clear all filters', onclick: clearAll })));
  }
  grid.append(createCard());
  renderDevice();
}

function clearAll() {
  $('q').value = '';
  history.replaceState(null, '', location.pathname + (sortBy() !== 'name' ? `?sort=${sortBy()}` : '') + location.hash);
  render();
}

// ---- your unidex ----
function renderDevice() {
  $('connect').textContent = device.busy ? 'Working…' : device.apps ? 'Refresh' : 'Connect';
  $('connect').disabled = device.busy || !supported;
  const list = $('devApps');
  list.hidden = !device.apps;
  list.replaceChildren();
  if (!device.apps) return;
  if (!device.apps.length) {
    list.append(el('li', { class: 'muted', text: 'No apps on it yet. Pick one below.' }));
    return;
  }
  for (const d of device.apps) {
    const s = catalogue.find(a => a.id === d.id);
    const name = s ? s.name : d.id;
    const newer = s && d.version && compareAppVersions(s.version, d.version) > 0;
    const meta = [d.version ? `v${d.version}` : 'offloaded', `${kb(d.data)} of data`];
    const title = s ? el('a', { href: `app.html?id=${encodeURIComponent(d.id)}`, text: name }) : el('span', { text: name });
    const actions = el('div', { class: 'actions' });
    if (newer) actions.append(el('button', { type: 'button', text: `Update to ${s.version}`, onclick: () => install(s) }));
    if (!d.version && s) actions.append(el('button', { class: 'secondary', type: 'button', text: 'Reinstall', onclick: () => install(s) }));
    if (d.version) actions.append(el('button', { class: 'secondary', type: 'button', text: 'Offload', onclick: () => offload(d.id, name) }));
    actions.append(el('button', { class: 'danger', type: 'button', text: d.version ? 'Delete' : 'Delete data', onclick: () => remove(d.id, name, !!d.version) }));
    for (const b of actions.children) b.disabled = device.busy;
    list.append(el('li', {}, iconTile(s && s.icon, 36),
      el('div', { class: 'info' }, el('b', {}, title), el('small', { text: meta.join(' · ') + (d.problem ? ` · can't run: ${d.problem}` : !s ? ' · not in the store' : '') })),
      actions));
  }
}

reportTo((text, kind = '', fraction) => {
  $('devStatus').textContent = text;
  $('devStatus').className = `status ${kind}`;
  $('progress').hidden = fraction == null;
  if (fraction != null) $('progress').value = fraction;
});
onDeviceChange(render);

// ---- start ----
$('devIcon').append(icon('device'));
$('createTop').href = STORE_REPO;
$('createTop').prepend(icon('plus'));
if (!supported) $('unsupported').hidden = false;
$('connect').onclick = () => withDevice(async () => {}).then(ok => {
  if (ok) $('devStatus').textContent = device.apps.length ? `Connected. ${plural(device.apps.length, 'app')} on it.` : 'Connected. No apps on it yet.';
});
let typing;
$('q').addEventListener('input', () => { clearTimeout(typing); typing = setTimeout(() => setParam('q', $('q').value.trim()), 150); });
$('sort').addEventListener('change', () => setParam('sort', $('sort').value === 'name' ? '' : $('sort').value));
$('clearFilters').onclick = clearAll;
if (matchMedia('(max-width: 1000px)').matches) $('filterBox').open = false;
navigator.serial && navigator.serial.addEventListener('disconnect', () => {
  if (device.busy) return;
  device.apps = null;
  $('devStatus').textContent = 'Your unidex was unplugged (or fell asleep). Press a button on it, then Connect.';
  render();
});

try {
  catalogue = await loadCatalogue();
} catch (e) {
  $('resultsTitle').textContent = e.message;
}
render();
