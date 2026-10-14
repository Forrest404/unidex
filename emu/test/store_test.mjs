// The website's store code (site/store-device.js) against the real firmware in the emulator: install, list,
// update, offload, delete and the refusals. Needs emu/build.sh and tools/make_store.py first.
//   node emu/test/store_test.mjs
import { createRequire } from 'module';
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import path from 'path';
const here = path.dirname(fileURLToPath(import.meta.url)), root = path.join(here, '..', '..');
const require = createRequire(import.meta.url);
// site/ has no package.json saying "module", so load the file's text as a module
const src = readFileSync(path.join(root, 'site/store-device.js'), 'utf8');
const store = await import('data:text/javascript,' + encodeURIComponent(src));
const createUnidex = require(path.join(root, 'emu/out/unidex.js'));

const M = await createUnidex({ onScreen() {}, print: () => {}, printErr: () => {} });
let buf = '';
setInterval(() => { buf += M.ccall('emu_serial_out', 'string', [], []) || ''; }, 5);
const dev = {
  send: async t => M.ccall('emu_serial_in', null, ['string'], [t + '\n']),
  expect: async (prefix, ms, onOther) => {  // as serial.js does it
    const end = Date.now() + ms;
    while (Date.now() < end) {
      let nl;
      while ((nl = buf.indexOf('\n')) >= 0) {
        const line = buf.slice(0, nl).replace(/\r$/, '');
        buf = buf.slice(nl + 1);
        if (line.startsWith(prefix)) return line;
        if (line === 'ERR' || line.startsWith('ERR ')) throw Object.assign(new Error('The device rejected it.'), { reply: line });
        if (onOther) onOther(line);
      }
      await new Promise(r => setTimeout(r, 5));
    }
    return null;
  },
};
let failures = 0;
const check = (ok, what) => { console.log(`${ok ? 'ok  ' : 'FAIL'}  ${what}`); failures += !ok; };
const throws = async (f, what, re) => {
  try { await f(); check(false, what); } catch (e) { check(!re || re.test(e.message), `${what} (${e.message})`); }
};

await new Promise(r => setTimeout(r, 1500));  // start-up
const catalogue = JSON.parse(readFileSync(path.join(root, 'site/store/index.json'), 'utf8'));
const byId = id => catalogue.apps.find(a => a.id === id);
const getFile = async (entry, f) => new Uint8Array(readFileSync(path.join(root, 'site/store/apps', entry.id, entry.version, f.path)));

check((await store.listApps(dev)).length === 0, 'a new card has no apps');
let steps = 0;
await store.installApp(dev, byId('hello'), getFile, () => steps++);
await store.installApp(dev, byId('clock'), getFile);
let apps = await store.listApps(dev);
check(apps.length === 2 && apps.every(a => a.version === byId(a.id).version && !a.problem && a.code > 0), 'both installed, with versions');
check(steps > 2, 'progress was reported');

const hello = () => store.listApps(dev).then(a => a.find(x => x.id === 'hello'));
check((await store.dataBytes(dev, 'hello')) === 0, 'no data yet');

await store.removeApp(dev, 'hello', true);
check((await hello()).version === null, 'offloaded: listed with no version');
await store.installApp(dev, byId('hello'), getFile);
check((await hello()).version === byId('hello').version, 'reinstalled after an offload');

// an update: Hello with its app.json saying 9.9.9
const enc = new TextEncoder();
const h = byId('hello');
const json11 = enc.encode(new TextDecoder().decode(await getFile(h, h.files.find(f => f.path === 'app.json'))).replace(`"${h.version}"`, '"9.9.9"'));
const newer = { ...h, version: '9.9.9', files: await Promise.all(h.files.map(async f => f.path !== 'app.json' ? f
  : { ...f, size: json11.length, sha256: await store.sha256hex(json11) })) };
await store.installApp(dev, newer, async (e, f) => f.path === 'app.json' ? json11 : getFile(h, f));
check((await hello()).version === '9.9.9', 'updated to 9.9.9');
await throws(() => store.installApp(dev, { ...byId('hello'), files: [{ ...byId('hello').files[0], sha256: '0'.repeat(64) }] }, getFile),
             'a file that doesn’t match its SHA-256 is never sent', /didn’t download/);
await throws(() => store.installApp(dev, { ...byId('hello'), files: byId('hello').files.filter(f => f.path !== 'app.json') }, getFile),
             'an app without app.json is refused by the device', /no app\.json/);
check((await hello()).version === '9.9.9', 'a refused install leaves the installed one alone');

await store.removeApp(dev, 'hello', false);
await store.removeApp(dev, 'clock', false);
check((await store.listApps(dev)).length === 0, 'deleted: nothing left');
check(store.compareAppVersions('1.2.10', '1.2.9') === 1 && store.compareAppVersions('1.0.0', '1.0.0') === 0 &&
      store.compareAppVersions('x', '0.0.1') === -1, 'versions compare by number');

console.log(failures ? `${failures} FAILED` : 'all passed');
process.exit(failures ? 1 : 0);
