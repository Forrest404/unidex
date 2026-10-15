// "Try your app": installs an app folder on the emulated unidex with the store's own code (emu-device.js and
// store-device.js), and shows what the app prints. The App editor (editor.html) does the same for code typed there.
import { ready, install, open, onPrint } from './emu-device.js';

const $ = id => document.getElementById(id);
let busy = false;

function log(text) {
  const t = $('appLog');
  t.value += (t.value ? '\n' : '') + text;
  t.scrollTop = t.scrollHeight;
}
onPrint(log);

async function load(files) {
  if (busy) return null;
  busy = true;
  $('pickApp').disabled = true;
  const status = (t, kind = '') => { $('appStatus').textContent = t; $('appStatus').className = 'status ' + kind; };
  try {
    const entry = await install(files, t => status(t));
    status(`${entry.name} ${entry.version || ''} installed. On the screen: Home → Apps → ${entry.name}.`, 'ok');
    return entry;
  } catch (e) {
    status(e.message, 'bad');
    return null;
  } finally {
    busy = false;
    $('pickApp').disabled = false;
  }
}

async function readPicked(fileList) {
  const files = [];
  for (const f of fileList) {
    const rel = (f.webkitRelativePath || f.name).split('/').slice(1).join('/');  // without the folder's own name
    if (rel) files.push({ path: rel, bytes: new Uint8Array(await f.arrayBuffer()) });
  }
  return files;
}

// A dropped folder: walk it with the file system entries API.
async function readDropped(item) {
  const files = [];
  const walk = (entry, prefix) => new Promise(resolve => {
    if (entry.isFile) {
      entry.file(async f => { files.push({ path: prefix + f.name, bytes: new Uint8Array(await f.arrayBuffer()) }); resolve(); });
    } else {
      const reader = entry.createReader(), all = [];
      const more = () => reader.readEntries(async batch => {
        if (batch.length) { all.push(...batch); more(); return; }
        for (const e of all) await walk(e, prefix + entry.name + '/');
        resolve();
      });
      more();
    }
  });
  const root = item.webkitGetAsEntry();
  if (!root || !root.isDirectory) throw new Error('Drop the app’s folder itself.');
  const reader = root.createReader();
  const top = await new Promise(r => { const all = []; const more = () => reader.readEntries(b => b.length ? (all.push(...b), more()) : r(all)); more(); });
  for (const e of top) await walk(e, '');
  return files;
}

$('pickApp').onclick = () => $('appFolder').click();
$('appFolder').onchange = async () => { await load(await readPicked($('appFolder').files)); $('appFolder').value = ''; };
const zone = $('tryApp');
zone.addEventListener('dragover', e => e.preventDefault());
zone.addEventListener('drop', async e => {
  e.preventDefault();
  const item = [...e.dataTransfer.items].find(i => i.kind === 'file');
  if (!item) return;
  try { await load(await readDropped(item)); }
  catch (err) { $('appStatus').textContent = err.message; $('appStatus').className = 'status bad'; }
});
ready.then(async () => {
  // For checking this page without a person (site/try.html?test=apps/examples/hello, served from the repo's root):
  // loads that folder's files as if it had been chosen, and with &open=1 opens it.
  const test = new URLSearchParams(location.search).get('test');
  if (test && /^[a-z0-9/_-]+$/.test(test)) {
    const files = [];
    for (const n of ['app.json', 'main.lua']) files.push({ path: n, bytes: new Uint8Array(await (await fetch(`../${test}/${n}`, { cache: 'no-store' })).arrayBuffer()) });
    const entry = await load(files);
    if (entry && new URLSearchParams(location.search).get('open') === '1') await open(entry);
  }
});
