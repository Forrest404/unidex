// The parts every page shares: the header with its menus (a drawer on narrow screens), the footer, and the
// site-wide search (Cmd/Ctrl+K or /). Each page has <header data-chrome></header> and <footer data-chrome></footer>
// and loads this module. Everything is built with DOM calls (no innerHTML from data), so app names from the store
// can't inject anything.

export const STORE_REPO = 'https://github.com/Forrest404/unidex-apps';
const REPO = 'https://github.com/Forrest404/unidex';
const SHOP = 'https://www.unidex.shop';

// One icon set: 24x24, stroked (styled by .icon in style.css)
const ICONS = {
  install: '<path d="M12 3v12"/><path d="m7 10 5 5 5-5"/><path d="M5 21h14"/>',
  tools: '<path d="M4 6h9M17 6h3M4 12h3M11 12h9M4 18h11M19 18h1"/><circle cx="15" cy="6" r="2"/><circle cx="9" cy="12" r="2"/><circle cx="17" cy="18" r="2"/>',
  key: '<circle cx="7.5" cy="15.5" r="4.5"/><path d="m10.7 12.3 9.3-9.3"/><path d="m16 7 3 3"/><path d="m19 4 2 2"/>',
  apps: '<rect x="4" y="4" width="7" height="7" rx="1.5"/><rect x="13" y="4" width="7" height="7" rx="1.5"/><rect x="4" y="13" width="7" height="7" rx="1.5"/><path d="M16.5 13v7M13 16.5h7"/>',
  code: '<path d="m8 8-5 4 5 4"/><path d="m16 8 5 4-5 4"/><path d="m14 4-4 16"/>',
  help: '<circle cx="12" cy="12" r="9"/><path d="M9.5 9.5a2.5 2.5 0 1 1 3.5 2.3c-.6.3-1 .9-1 1.6V14"/><path d="M12 17.5h.01"/>',
  search: '<circle cx="11" cy="11" r="7"/><path d="m20 20-3.5-3.5"/>',
  ext: '<path d="M7 17 17 7"/><path d="M8 7h9v9"/>',
  menu: '<path d="M4 7h16M4 12h16M4 17h16"/>',
  close: '<path d="M6 6l12 12M18 6 6 18"/>',
  chevron: '<path d="m6 9 6 6 6-6"/>',
  sync: '<path d="M20 11a8 8 0 0 0-14.3-4.9L4 8"/><path d="M4 4v4h4"/><path d="M4 13a8 8 0 0 0 14.3 4.9L20 16"/><path d="M20 20v-4h-4"/>',
  image: '<rect x="4" y="4" width="16" height="16" rx="2"/><circle cx="9" cy="9" r="1.5"/><path d="m20 15-4.5-4.5L6 20"/>',
  pet: '<circle cx="12" cy="12" r="9"/><path d="M8.5 14.5a4.5 4.5 0 0 0 7 0"/><path d="M9 9.5h.01M15 9.5h.01"/>',
  calendar: '<rect x="4" y="5" width="16" height="15" rx="2"/><path d="M4 10h16M9 3v4M15 3v4"/>',
  laptop: '<rect x="5" y="5" width="14" height="10" rx="1.5"/><path d="M3 19h18"/>',
  wifi: '<path d="M2 9a15 15 0 0 1 20 0"/><path d="M5 12.5a10 10 0 0 1 14 0"/><path d="M8.5 16a5 5 0 0 1 7 0"/><path d="M12 19.5h.01"/>',
  mic: '<rect x="9" y="3" width="6" height="11" rx="3"/><path d="M5 11a7 7 0 0 0 14 0M12 18v3"/>',
  check: '<circle cx="12" cy="12" r="9"/><path d="m8.5 12.5 2.5 2.5 4.5-5"/>',
  file: '<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5M9 13h6M9 17h4"/>',
  book: '<path d="M4 5a2 2 0 0 1 2-2h13v16H6a2 2 0 0 0-2 2z"/><path d="M4 19V5"/><path d="M8 7h7"/>',
  play: '<rect x="3" y="4" width="18" height="16" rx="2"/><path d="m10 9 5 3-5 3z"/>',
  send: '<path d="M21 3 10 14"/><path d="M21 3 14 21l-4-7-7-4z"/>',
  chat: '<path d="M20 15a2 2 0 0 1-2 2H8l-4 4V6a2 2 0 0 1 2-2h12a2 2 0 0 1 2 2z"/>',
  list: '<path d="M9 6h11M9 12h11M9 18h11M4.5 6h.01M4.5 12h.01M4.5 18h.01"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  device: '<rect x="6" y="2.5" width="12" height="19" rx="3"/><rect x="8.5" y="6" width="7" height="7" rx="1"/>',
  flag: '<path d="M5 21V4M5 4h11l-2 4 2 4H5"/>',
  layers: '<path d="m12 3 9 5-9 5-9-5z"/><path d="m3 13 9 5 9-5"/>',
  bag: '<path d="M5 8h14l-1 12H6z"/><path d="M9 8V6a3 3 0 0 1 6 0v2"/>',
  heart: '<path d="M12 20s-7-4.5-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 10c0 5.5-7 10-7 10z"/>',
  trash: '<path d="M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3"/>',
  wrench: '<path d="M14.5 6.5a4 4 0 0 0 5 5l-9 9a2.1 2.1 0 0 1-3-3z"/><path d="M14.5 6.5 17 4l3 3-2.5 2.5"/>',
};

const NS = 'http://www.w3.org/2000/svg';
export function icon(name, cls = 'icon') {
  const svg = document.createElementNS(NS, 'svg');
  svg.setAttribute('viewBox', '0 0 24 24');
  svg.setAttribute('class', cls);
  svg.setAttribute('aria-hidden', 'true');
  svg.innerHTML = ICONS[name] || ICONS.apps;  // (fixed strings above, never data)
  return svg;
}

// The logo (docs/brand/logo.svg): the Pet robot and the name in pixels, in the text colour so it follows the theme.
const LOGO_D = 'M87.07 0h12.44v6.58h-12.44zM80.85 6.58h24.88v6.58h-24.88zM24.88 13.16h18.66v6.58h-18.66zM80.85 13.16h24.88v6.58h-24.88zM18.66 19.75h6.22v6.58h-6.22zM43.54 19.75h6.22v6.58h-6.22zM87.07 19.75h12.44v6.58h-12.44zM12.44 26.33h12.44v6.58h-12.44zM43.54 26.33h12.44v6.58h-12.44zM87.07 26.33h12.44v6.58h-12.44zM6.22 32.91h6.22v6.58h-6.22zM24.88 32.91h18.66v6.58h-18.66zM55.98 32.91h111.95v6.58h-111.95zM6.22 39.49h6.22v6.58h-6.22zM24.88 39.49h18.66v6.58h-18.66zM55.98 39.49h118.17v6.58h-118.17zM6.22 46.07h6.22v6.58h-6.22zM24.88 46.07h18.66v6.58h-18.66zM55.98 46.07h6.22v6.58h-6.22zM161.71 46.07h12.44v6.58h-12.44zM12.44 52.65h12.44v6.58h-12.44zM43.54 52.65h12.44v6.58h-12.44zM161.71 52.65h12.44v6.58h-12.44zM12.44 59.24h12.44v6.58h-12.44zM43.54 59.24h6.22v6.58h-6.22zM161.71 59.24h12.44v6.58h-12.44zM12.44 65.82h31.1v6.58h-31.1zM161.71 65.82h12.44v6.58h-12.44zM12.44 72.4h12.44v6.58h-12.44zM161.71 72.4h12.44v6.58h-12.44zM12.44 78.98h12.44v6.58h-12.44zM161.71 78.98h12.44v6.58h-12.44zM12.44 85.56h12.44v6.58h-12.44zM161.71 85.56h12.44v6.58h-12.44zM0 92.14h24.88v6.58h-24.88zM49.76 92.14h12.44v6.58h-12.44zM124.39 92.14h12.44v6.58h-12.44zM161.71 92.14h24.88v6.58h-24.88zM0 98.73h24.88v6.58h-24.88zM43.54 98.73h24.88v6.58h-24.88zM118.17 98.73h24.88v6.58h-24.88zM161.71 98.73h24.88v6.58h-24.88zM0 105.31h24.88v6.58h-24.88zM43.54 105.31h24.88v6.58h-24.88zM118.17 105.31h24.88v6.58h-24.88zM161.71 105.31h24.88v6.58h-24.88zM12.44 111.89h12.44v6.58h-12.44zM49.76 111.89h12.44v6.58h-12.44zM124.39 111.89h12.44v6.58h-12.44zM161.71 111.89h12.44v6.58h-12.44zM12.44 118.47h12.44v6.58h-12.44zM161.71 118.47h12.44v6.58h-12.44zM12.44 125.05h12.44v6.58h-12.44zM68.41 125.05h6.22v6.58h-6.22zM111.95 125.05h6.22v6.58h-6.22zM161.71 125.05h12.44v6.58h-12.44zM12.44 131.63h12.44v6.58h-12.44zM68.41 131.63h12.44v6.58h-12.44zM105.73 131.63h12.44v6.58h-12.44zM161.71 131.63h12.44v6.58h-12.44zM12.44 138.22h12.44v6.58h-12.44zM74.63 138.22h37.32v6.58h-37.32zM161.71 138.22h12.44v6.58h-12.44zM12.44 144.8h12.44v6.58h-12.44zM161.71 144.8h12.44v6.58h-12.44zM12.44 151.38h12.44v6.58h-12.44zM161.71 151.38h12.44v6.58h-12.44zM12.44 157.96h43.54v6.58h-43.54zM62.19 157.96h12.44v6.58h-12.44zM111.95 157.96h12.44v6.58h-12.44zM130.61 157.96h43.54v6.58h-43.54zM18.66 164.54h37.32v6.58h-37.32zM62.19 164.54h24.88v6.58h-24.88zM99.51 164.54h24.88v6.58h-24.88zM130.61 164.54h37.32v6.58h-37.32zM37.32 171.12h18.66v6.58h-18.66zM62.19 171.12h18.66v6.58h-18.66zM87.07 171.12h12.44v6.58h-12.44zM105.73 171.12h18.66v6.58h-18.66zM130.61 171.12h18.66v6.58h-18.66zM31.1 177.71h24.88v6.58h-24.88zM62.19 177.71h24.88v6.58h-24.88zM99.51 177.71h24.88v6.58h-24.88zM130.61 177.71h24.88v6.58h-24.88zM37.32 184.29h18.66v6.58h-18.66zM62.19 184.29h12.44v6.58h-12.44zM111.95 184.29h12.44v6.58h-12.44zM130.61 184.29h18.66v6.58h-18.66zM43.54 190.87h12.44v6.58h-12.44zM80.85 190.87h24.88v6.58h-24.88zM130.61 190.87h12.44v6.58h-12.44zM43.54 197.45h99.51v6.58h-99.51zM49.76 204.03h87.07v6.58h-87.07zM683.82 23.64h18.6v17.55h-18.6zM683.82 41.19h18.6v17.55h-18.6zM218.84 58.74h18.6v17.55h-18.6zM330.43 58.74h18.6v17.55h-18.6zM404.83 58.74h55.8v17.55h-55.8zM535.02 58.74h18.6v17.55h-18.6zM609.42 58.74h55.8v17.55h-55.8zM683.82 58.74h18.6v17.55h-18.6zM758.21 58.74h55.8v17.55h-55.8zM869.81 58.74h18.6v17.55h-18.6zM981.4 58.74h18.6v17.55h-18.6zM218.84 76.29h18.6v17.55h-18.6zM330.43 76.29h18.6v17.55h-18.6zM386.23 76.29h18.6v17.55h-18.6zM460.63 76.29h18.6v17.55h-18.6zM590.82 76.29h18.6v17.55h-18.6zM665.22 76.29h37.2v17.55h-37.2zM739.61 76.29h18.6v17.55h-18.6zM814.01 76.29h18.6v17.55h-18.6zM888.41 76.29h18.6v17.55h-18.6zM962.8 76.29h18.6v17.55h-18.6zM218.84 93.83h18.6v17.55h-18.6zM330.43 93.83h18.6v17.55h-18.6zM367.63 93.83h18.6v17.55h-18.6zM479.23 93.83h18.6v17.55h-18.6zM516.42 93.83h37.2v17.55h-37.2zM572.22 93.83h18.6v17.55h-18.6zM683.82 93.83h18.6v17.55h-18.6zM721.01 93.83h18.6v17.55h-18.6zM832.61 93.83h18.6v17.55h-18.6zM907 93.83h18.6v17.55h-18.6zM944.2 93.83h18.6v17.55h-18.6zM218.84 111.38h18.6v17.55h-18.6zM330.43 111.38h18.6v17.55h-18.6zM367.63 111.38h18.6v17.55h-18.6zM479.23 111.38h18.6v17.55h-18.6zM535.02 111.38h18.6v17.55h-18.6zM572.22 111.38h18.6v17.55h-18.6zM683.82 111.38h18.6v17.55h-18.6zM721.01 111.38h111.59v17.55h-111.59zM925.6 111.38h18.6v17.55h-18.6zM218.84 128.93h18.6v17.55h-18.6zM330.43 128.93h18.6v17.55h-18.6zM367.63 128.93h18.6v17.55h-18.6zM479.23 128.93h18.6v17.55h-18.6zM535.02 128.93h18.6v17.55h-18.6zM572.22 128.93h18.6v17.55h-18.6zM683.82 128.93h18.6v17.55h-18.6zM721.01 128.93h18.6v17.55h-18.6zM907 128.93h18.6v17.55h-18.6zM944.2 128.93h18.6v17.55h-18.6zM237.44 146.48h18.6v17.55h-18.6zM311.83 146.48h18.6v17.55h-18.6zM367.63 146.48h18.6v17.55h-18.6zM479.23 146.48h18.6v17.55h-18.6zM535.02 146.48h18.6v17.55h-18.6zM590.82 146.48h18.6v17.55h-18.6zM683.82 146.48h18.6v17.55h-18.6zM739.61 146.48h18.6v17.55h-18.6zM888.41 146.48h18.6v17.55h-18.6zM962.8 146.48h18.6v17.55h-18.6zM256.04 164.03h55.8v17.55h-55.8zM367.63 164.03h18.6v17.55h-18.6zM479.23 164.03h18.6v17.55h-18.6zM535.02 164.03h18.6v17.55h-18.6zM609.42 164.03h74.4v17.55h-74.4zM758.21 164.03h74.4v17.55h-74.4zM869.81 164.03h18.6v17.55h-18.6zM981.4 164.03h18.6v17.55h-18.6z';
function logo() {
  const svg = document.createElementNS(NS, 'svg');
  svg.setAttribute('viewBox', '0 0 1000 210.61');
  svg.setAttribute('class', 'logo');
  svg.setAttribute('role', 'img');
  svg.setAttribute('aria-label', 'unidex');
  svg.setAttribute('shape-rendering', 'crispEdges');
  const path = document.createElementNS(NS, 'path');
  path.setAttribute('d', LOGO_D);
  path.setAttribute('fill', 'currentColor');
  svg.append(path);
  return svg;
}

export const el = (tag, props = {}, ...children) => {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(props)) {
    if (v == null || v === false) continue;
    if (k === 'class') e.className = v;
    else if (k === 'text') e.textContent = v;
    else if (k.startsWith('on')) e.addEventListener(k.slice(2), v);
    else if (k in e && typeof v !== 'string') e[k] = v;
    else e.setAttribute(k, v === true ? '' : v);
  }
  for (const c of children.flat(Infinity)) if (c != null && c !== false) e.append(c);
  return e;
};

// ---- the menus ----
const MENUS = [
  { label: 'Install', page: 'index.html', items: [
    { label: 'Install or update', desc: 'Put unidex on a board, or update it and keep your files', href: './', icon: 'install' },
    { label: "What's new", desc: 'Every change, release by release', href: `${REPO}/blob/main/CHANGELOG.md`, icon: 'list', ext: true },
  ] },
  { label: 'Tools', page: 'tools.html', items: [
    { label: 'Sync everything', desc: 'Firmware, clock, calendar and notes in one click', href: 'tools.html#sync', icon: 'sync' },
    { label: 'Badge maker', desc: 'Turn any picture into a name badge', href: 'tools.html#badges', icon: 'image' },
    { label: 'Pet designer', desc: 'Dress up and name your Pet', href: 'tools.html#pet', icon: 'pet' },
    { label: 'Clock & calendar', desc: 'Set the time, send a calendar file', href: 'tools.html#clock', icon: 'calendar' },
    { label: 'Mac calendar sync', desc: 'Your Apple Calendar, every time you plug in', href: 'tools.html#mac', icon: 'laptop' },
  ] },
  { label: 'Setup', page: 'setup.html', items: [
    { label: 'WiFi & keys', desc: 'Networks, OpenAI or Claude, and GitHub for Notes', href: 'setup.html#connections', icon: 'wifi' },
    { label: 'Microphone test', desc: 'Check it hears you', href: 'setup.html#microphone', icon: 'mic' },
    { label: 'Your notes', desc: 'Read and download your voice notes', href: 'setup.html#notes', icon: 'file' },
    { label: 'Remove keys', desc: 'Clear every key and network from the device', href: 'setup.html#remove', icon: 'trash' },
  ] },
  { label: 'Apps', page: ['store.html', 'app.html'], items: [
    { label: 'Browse apps & plugins', desc: 'Search and filter everything in the store', href: 'store.html', icon: 'apps' },
    { label: 'On your unidex', desc: 'Install, update, offload and delete', href: 'store.html#device', icon: 'device' },
    { label: 'Create an app', desc: 'Start from the template on GitHub', href: STORE_REPO, icon: 'plus', ext: true },
  ] },
  { label: 'Developers', page: ['developers.html', 'try.html', 'editor.html'], items: [
    { label: 'App editor', desc: 'Write an app here and run it straight away', href: 'editor.html', icon: 'code' },
    { label: 'Make an app', desc: 'The example, the API and the sandbox', href: 'developers.html', icon: 'book' },
    { label: 'The API', desc: 'Every unidex.* call, in one short list', href: `${STORE_REPO}/blob/main/API.md`, icon: 'book', ext: true },
    { label: 'Try it without a device', desc: 'Run unidex, and your app, in the browser emulator', href: 'try.html', icon: 'play' },
    { label: 'Send it to the store', desc: 'A pull request, checked and reviewed', href: 'developers.html#share', icon: 'send' },
  ] },
  { label: 'Help', page: 'help.html', items: [
    { label: 'Getting started', desc: 'Install, the SD card and your timetable', href: 'help.html#start', icon: 'flag' },
    { label: 'Questions', desc: 'Battery, files, privacy, updates', href: 'help.html#faq', icon: 'help' },
    { label: 'Troubleshooting', desc: "When it can't be found, or something's off", href: 'help.html#trouble', icon: 'wrench' },
    { label: 'Ask a question', desc: 'Open an issue on GitHub', href: `${REPO}/issues`, icon: 'chat', ext: true },
  ] },
];

const here = location.pathname.split('/').pop() || 'index.html';
const isHere = page => (Array.isArray(page) ? page : [page]).includes(here);
const extProps = item => (item.ext ? { target: '_blank', rel: 'noopener' } : {});

function menuLink(item) {
  const a = el('a', { href: item.href, ...extProps(item) });
  const sameTarget = !item.ext && item.href.split('#')[0] === here && !item.href.includes('#');
  if (sameTarget) a.setAttribute('aria-current', 'page');
  a.textContent = item.label + (item.ext ? ' ↗' : '');
  return a;
}

let openPanel = null;
function closePanels(except) {
  for (const b of document.querySelectorAll('.menus .top[aria-expanded=true]')) {
    if (b === except) continue;
    b.setAttribute('aria-expanded', 'false');
    document.getElementById(b.getAttribute('aria-controls')).classList.remove('open');
  }
  if (!except) openPanel = null;
}

function header() {
  const menus = el('ul', { class: 'menus' });
  MENUS.forEach((m, i) => {
    const id = `menu-${i}`;
    const btn = el('button', { class: 'top', type: 'button', 'aria-expanded': 'false', 'aria-controls': id },
                   m.label, icon('chevron'));
    if (isHere(m.page)) btn.setAttribute('aria-current', 'page');
    const panel = el('ul', { class: 'panel', id }, m.items.map(it => el('li', {}, menuLink(it))));
    btn.addEventListener('click', () => {
      const open = btn.getAttribute('aria-expanded') !== 'true';
      closePanels(btn);
      btn.setAttribute('aria-expanded', String(open));
      panel.classList.toggle('open', open);
      openPanel = open ? btn : null;
      if (open && btn.dataset.keyboard) panel.querySelector('a').focus();
      delete btn.dataset.keyboard;
    });
    btn.addEventListener('keydown', e => {
      if (e.key === 'ArrowDown' || e.key === 'Enter' || e.key === ' ') btn.dataset.keyboard = '1';
      if (e.key === 'ArrowDown') { e.preventDefault(); btn.click(); }
    });
    panel.addEventListener('keydown', e => {
      const links = [...panel.querySelectorAll('a')], i2 = links.indexOf(document.activeElement);
      if (e.key === 'ArrowDown') { e.preventDefault(); links[(i2 + 1) % links.length].focus(); }
      if (e.key === 'ArrowUp') { e.preventDefault(); links[(i2 - 1 + links.length) % links.length].focus(); }
    });
    menus.append(el('li', {}, btn, panel));
  });

  const searchBtn = el('button', { class: 'search-btn', type: 'button', 'aria-label': 'Search (press /)', onclick: () => openSearch() },
                       icon('search'), el('span', { class: 'label', text: 'Search' }), el('kbd', { text: '/' }));
  const create = el('a', { class: 'button small', href: STORE_REPO, target: '_blank', rel: 'noopener' }, 'Create an app ↗');
  const menuBtn = el('button', { class: 'menu-btn', type: 'button', 'aria-label': 'Menu', 'aria-expanded': 'false',
                                 onclick: () => openDrawer() }, icon('menu'));
  return el('div', { class: 'bar' },
    el('a', { class: 'brand', href: './', 'aria-label': 'unidex tools, home' }, logo(), el('small', { text: 'tools' })),
    el('nav', { 'aria-label': 'Main' }, menus),
    el('div', { class: 'head-right' }, searchBtn,
      el('a', { class: 'shop-link', href: SHOP, target: '_blank', rel: 'noopener', text: 'unidex.shop ↗' }), create, menuBtn));
}

// ---- the drawer (narrow screens) ----
let drawer;
function openDrawer() {
  if (!drawer) {
    const close = () => { drawer.classList.remove('open'); document.querySelector('.menu-btn').setAttribute('aria-expanded', 'false');
                          document.querySelector('.menu-btn').focus(); };
    drawer = el('div', { class: 'drawer', role: 'dialog', 'aria-modal': 'true', 'aria-label': 'Menu' },
      el('div', { class: 'scrim', onclick: close }),
      el('div', { class: 'sheet' },
        el('div', { class: 'sheet-top' },
          el('a', { class: 'brand', href: './' }, logo()),
          el('button', { class: 'quiet', type: 'button', 'aria-label': 'Close the menu', onclick: close }, icon('close'))),
        MENUS.map(m => [el('h2', { text: m.label }), el('ul', {}, m.items.map(it => el('li', {}, menuLink(it))))]),
        el('div', { class: 'actions' },
          el('button', { class: 'secondary', type: 'button', onclick: () => { close(); openSearch(); } }, 'Search'),
          el('a', { class: 'button', href: STORE_REPO, target: '_blank', rel: 'noopener' }, 'Create an app ↗'),
          el('a', { href: SHOP, target: '_blank', rel: 'noopener', text: 'unidex.shop ↗' }))));
    drawer.addEventListener('keydown', e => { if (e.key === 'Escape') close(); });
    drawer.addEventListener('click', e => { if (e.target.closest('a')) drawer.classList.remove('open'); });
    document.body.append(drawer);
  }
  drawer.classList.add('open');
  document.querySelector('.menu-btn').setAttribute('aria-expanded', 'true');
  drawer.querySelector('.sheet-top button').focus();
}

// ---- the footer ----
function footer() {
  const link = (text, href) => el('a', { href, text, ...(href.startsWith('http') ? { target: '_blank', rel: 'noopener' } : {}) });
  return [
    el('div', { class: 'foot-row' },
      el('a', { class: 'brand', href: './' }, logo(), el('small', { text: 'tools' })),
      el('nav', { 'aria-label': 'Footer' },
        link('Install', './'), link('Tools', 'tools.html'), link('Setup', 'setup.html'), link('Apps', 'store.html'),
        link('Developers', 'developers.html'), link('Help', 'help.html'))),
    el('div', { class: 'foot-row small' },
      el('span', { text: 'Free for non-commercial use.' }),
      el('nav', { 'aria-label': 'More' },
        link('unidex.shop ↗', SHOP), link('Source ↗', REPO), link('Ko-fi ↗', 'https://ko-fi.com/forrest404'),
        link('Licence ↗', `${REPO}/blob/main/LICENSE`), link('Privacy ↗', `${REPO}/blob/main/PRIVACY.md`))),
  ];
}

// ---- site-wide search ----
// Pages, sections and topics (fixed), then the store's apps when this page may load them (Setup may not: its
// CSP blocks any request, so it searches the rest).
const PAGES = [
  ...MENUS.flatMap(m => m.items.map(it => ({ group: m.label === 'Apps' ? 'Apps' : m.label === 'Help' ? 'Help' : 'Pages',
    title: it.label, sub: it.desc, href: it.href, icon: it.icon, ext: it.ext, words: `${m.label} ${it.desc}` }))),
  { group: 'Help', title: 'The website can\'t find my unidex', sub: 'It\'s asleep, or needs BOOT mode', href: 'help.html#not-found', icon: 'wrench', words: 'usb serial port connect missing list nothing' },
  { group: 'Help', title: 'Battery and charging', sub: 'Low battery, Charge me, the percentage', href: 'help.html#battery', icon: 'help', words: 'power flat charge me percent' },
  { group: 'Help', title: '"App error" on the device', sub: 'What it means and what to do', href: 'help.html#app-error', icon: 'wrench', words: 'crash broken lua' },
  { group: 'Help', title: 'The SD card', sub: 'FAT32, the starter files, No SD card', href: 'help.html#sd', icon: 'file', words: 'card fat32 files storage' },
  { group: 'Help', title: 'Passing it on (factory reset)', sub: 'Wipe your WiFi, keys and Pet', href: 'help.html#reset', icon: 'trash', words: 'wipe sell give erase' },
  { group: 'Help', title: 'Privacy', sub: 'What the device and this site send, and where', href: `${REPO}/blob/main/PRIVACY.md`, icon: 'key', ext: true, words: 'data keys tracking' },
  { group: 'Pages', title: 'Install unidex', sub: 'Install erases; Update keeps your files', href: './', icon: 'install', words: 'firmware flash erase update version esp32' },
  { group: 'Pages', title: 'App editor', sub: 'Write an app in your browser and run it on the emulator', href: 'editor.html', icon: 'code', words: 'editor write code lua playground ide run test' },
  { group: 'Pages', title: 'Example: Hello', sub: 'A whole app in about 20 lines of Lua', href: 'developers.html#example', icon: 'code', words: 'lua sample code template' },
  { group: 'Pages', title: 'Building apps', sub: 'How an app runs, laying out a screen, saving through sleep', href: `${STORE_REPO}/blob/main/APPS.md`, icon: 'book', ext: true, words: 'guide tutorial lua layout sleep data tick' },
  { group: 'Pages', title: 'Writing plugins', sub: 'Badge plugins, with a template', href: `${STORE_REPO}/blob/main/PLUGINS.md`, icon: 'layers', ext: true, words: 'guide plugin badge extend' },
  { group: 'Pages', title: 'Plugins', sub: 'Add to the built-in apps (Badge)', href: 'developers.html#plugins', icon: 'layers', words: 'extend badge plugin' },
  { group: 'Pages', title: 'App limits and the sandbox', sub: 'Memory, time, files, permissions', href: 'developers.html#limits', icon: 'key', words: 'memory sandbox permission wifi safe' },
];
let catalogue = null;
async function loadApps() {
  if (catalogue) return catalogue;
  try {
    const r = await fetch('store/index.json', { cache: 'no-cache' });
    catalogue = r.ok ? (await r.json()).apps || [] : [];
  } catch (e) {
    catalogue = [];
  }
  return catalogue;
}

const norm = s => (s || '').toLowerCase().normalize('NFKD').replace(/[̀-ͯ]/g, '');
function score(entry, q) {
  const title = norm(entry.title), all = norm(`${entry.title} ${entry.sub} ${entry.words || ''}`);
  const words = norm(q).split(/\s+/).filter(Boolean);
  if (!words.length) return 0;
  let s = 0;
  for (const w of words) {
    if (title.startsWith(w)) s += 6;
    else if (title.includes(w)) s += 4;
    else if (all.includes(w)) s += 1;
    else return 0;
  }
  return s;
}

let dialog;
function openSearch() {
  closePanels();
  if (!dialog) buildSearch();
  dialog.showModal();
  const input = dialog.querySelector('input');
  input.select();
  run(input.value);
  loadApps().then(() => run(input.value));
}

function results(q) {
  const apps = (catalogue || []).map(a => ({
    group: 'Apps', title: a.name, sub: `${a.type === 'plugin' ? 'Plugin' : 'App'} · ${a.description}`,
    href: `app.html?id=${encodeURIComponent(a.id)}`, icon: a.type === 'plugin' ? 'layers' : 'apps',
    words: `${a.author} ${a.category || ''} ${a.id}` }));
  const all = [...apps, ...PAGES];
  if (!q.trim()) return [...apps.slice(0, 4), ...PAGES.filter(p => ['./', 'store.html', 'developers.html', 'help.html#start', 'tools.html#sync', 'setup.html#connections'].includes(p.href))];
  const order = { Apps: 0, Pages: 1, Help: 2 };  // grouped, best first within each group
  return all.map(e => [score(e, q), e]).filter(([s]) => s > 0).sort((a, b) => b[0] - a[0]).slice(0, 12)
    .map(([, e]) => e).sort((a, b) => (order[a.group] ?? 9) - (order[b.group] ?? 9));
}

let active = 0;
function run(q) {
  const list = dialog.querySelector('.search-results');
  const found = results(q);
  list.replaceChildren();
  active = 0;
  if (!found.length) {
    list.append(el('li', { class: 'search-empty' }, `Nothing found for "${q}". Try a shorter word, or `,
      el('a', { href: 'store.html', text: 'browse the apps' }), '.'));
    return;
  }
  let group = null, i = 0;
  for (const e of found) {
    if (e.group !== group) list.append(el('li', { class: 'group', role: 'presentation', text: (group = e.group) }));
    const opt = el('li', {}, el('a', { role: 'option', id: `sr-${i}`, href: e.href, 'aria-selected': i === 0 ? 'true' : 'false',
      ...(e.ext ? { target: '_blank', rel: 'noopener' } : {}) },
      el('span', {}, el('b', { text: e.title + (e.ext ? ' ↗' : '') }), el('small', { text: e.sub })),
      el('span', { class: 'chip', text: e.group === 'Apps' ? (e.href.startsWith('app.html') ? (e.icon === 'layers' ? 'plugin' : 'app') : 'apps') : e.group === 'Help' ? 'help' : 'page' })));
    list.append(opt);
    i++;
  }
  dialog.querySelector('input').setAttribute('aria-activedescendant', 'sr-0');
}

function move(by) {
  const opts = [...dialog.querySelectorAll('[role=option]')];
  if (!opts.length) return;
  opts[active].setAttribute('aria-selected', 'false');
  active = (active + by + opts.length) % opts.length;
  opts[active].setAttribute('aria-selected', 'true');
  opts[active].scrollIntoView({ block: 'nearest' });
  dialog.querySelector('input').setAttribute('aria-activedescendant', opts[active].id);
}

function buildSearch() {
  const input = el('input', { type: 'search', placeholder: 'Search apps, tools and help', 'aria-label': 'Search',
                              role: 'combobox', 'aria-expanded': 'true', 'aria-controls': 'search-results', autocomplete: 'off', spellcheck: 'false' });
  dialog = el('dialog', { class: 'search', 'aria-label': 'Search' },
    el('div', { class: 'search-field' }, icon('search'), input,
      el('button', { class: 'quiet', type: 'button', 'aria-label': 'Close search', onclick: () => dialog.close() }, el('kbd', { text: 'esc' }))),
    el('ul', { class: 'search-results', id: 'search-results', role: 'listbox', 'aria-label': 'Results' }),
    el('div', { class: 'search-foot' }, el('span', {}, el('kbd', { text: '↑' }), ' ', el('kbd', { text: '↓' }), ' to move'),
      el('span', {}, el('kbd', { text: 'enter' }), ' to open'), el('span', {}, el('kbd', { text: 'esc' }), ' to close')));
  input.addEventListener('input', () => run(input.value));
  input.addEventListener('keydown', e => {
    if (e.key === 'ArrowDown') { e.preventDefault(); move(1); }
    if (e.key === 'ArrowUp') { e.preventDefault(); move(-1); }
    if (e.key === 'Enter') {
      const opt = dialog.querySelectorAll('[role=option]')[active];
      if (opt) { e.preventDefault(); opt.click(); dialog.close(); }
    }
  });
  dialog.addEventListener('click', e => { if (e.target === dialog) dialog.close(); });  // the backdrop
  document.body.append(dialog);
}

// ---- put it all on the page ----
function mount() {
  let h = document.querySelector('header[data-chrome]');
  if (!h) document.body.prepend(h = el('header'));
  h.className = 'site-head';
  h.replaceChildren(header());
  const main = document.querySelector('main');
  if (main && !main.id) main.id = 'main';
  document.body.prepend(el('a', { class: 'skip', href: '#main', text: 'Skip to content' }));
  const foot = document.querySelector('footer[data-chrome]');
  if (foot) {
    foot.className = 'site-foot';
    foot.replaceChildren(...footer());
  }
}
mount();

document.addEventListener('click', e => { if (openPanel && !e.target.closest('.menus')) closePanels(); });
document.addEventListener('keydown', e => {
  if (e.key === 'Escape' && openPanel) { const b = openPanel; closePanels(); b.focus(); }
  const typing = /^(input|textarea|select)$/i.test(e.target.tagName) || e.target.isContentEditable;
  if ((e.key === 'k' && (e.metaKey || e.ctrlKey)) || (e.key === '/' && !typing)) { e.preventDefault(); openSearch(); }
});
// Section links on the same page: close the menu after following one
document.addEventListener('focusin', e => { if (openPanel && !e.target.closest('.menus')) closePanels(); });

// "On this page" lists (.toc): mark the section being read
const toc = document.querySelector('.toc');
if (toc && 'IntersectionObserver' in window) {
  const links = new Map([...toc.querySelectorAll('a[href^="#"]')].map(a => [a.getAttribute('href').slice(1), a]));
  const seen = new Set();
  const spy = new IntersectionObserver(entries => {
    for (const e of entries) e.isIntersecting ? seen.add(e.target.id) : seen.delete(e.target.id);
    const first = [...links.keys()].find(id => seen.has(id));
    if (!first) return;
    for (const [id, a] of links) id === first ? a.setAttribute('aria-current', 'true') : a.removeAttribute('aria-current');
  }, { rootMargin: '-80px 0px -55% 0px' });
  for (const id of links.keys()) { const s = document.getElementById(id); if (s) spy.observe(s); }
}

// A link to a question on Help (help.html#battery) opens it
function openTarget() {
  const t = location.hash && document.getElementById(decodeURIComponent(location.hash.slice(1)));
  if (t && t.tagName === 'DETAILS') { t.open = true; t.scrollIntoView(); }
}
addEventListener('hashchange', openTarget);
openTarget();
