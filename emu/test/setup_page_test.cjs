// Opens site/setup.html in headless Chrome against a pretend device (a fake Web Serial port that speaks the
// device's N protocol), saves several WiFi networks, edits and removes one, and checks what the "device" holds.
//   (from the repo folder)  python3 -m http.server 8765 &   then   node emu/test/setup_page_test.cjs
const { spawn } = require('child_process');
const fs = require('fs');
const CHROME = process.env.CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const URL_ = process.env.URL_ || 'http://localhost:8765/site/setup.html';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const FAKE = `
(() => {
  const names = ['wifi_ssid','wifi_pass','wifi_user','openai_key','anthropic_key','cleanup','cleanup_model','gh_on','gh_repo','gh_branch','gh_dir','gh_token','wifi_ca','cal_url',
                 'transcribe','elevenlabs_key','note_lang'];
  if (localStorage.getItem('oldFirmware')) names.splice(names.indexOf('note_lang'), 1);  // before the note language
  for (let i = 2; i <= 5; i++) for (const p of ['ssid','pass','user','ca']) names.push('wifi' + i + '_' + p);
  const secret = n => /_pass$|_key$|_token$|_ca$|^cal_url$/.test(n);
  const hex = s => [...new TextEncoder().encode(s)].map(b => b.toString(16).padStart(2, '0')).join('');
  const unhex = h => new TextDecoder().decode(new Uint8Array((h.match(/../g) || []).map(x => parseInt(x, 16))));
  const store = window.__device = {};
  const chunks = {};
  function reply(line) {
    const w = line.split(' ');
    if (line === '?') return ['unidex 1'];
    if (w[0] === 'N' && w[1] === '?') {
      const out = names.map(n => 'NS ' + n + ' ' + (store[n] ? 'set' : 'unset') + ' ' + (secret(n) ? '' : hex(store[n] || '')));
      return [...out, 'NC 1 0 0', 'OK N ?'];
    }
    if (w[0] === 'N' && w[1] === 'SET') {
      const v = unhex(w[3] || ''), n = w[2];
      if (!names.includes(n)) return ['ERR'];
      store[n] = (chunks[n] || '') + v; delete chunks[n];
      if (!store[n]) delete store[n];
      return ['OK N SET ' + n];
    }
    if (w[0] === 'N' && w[1] === 'ADD') { chunks[w[2]] = (chunks[w[2]] || '') + unhex(w[3] || ''); return ['OK N ADD ' + w[2]]; }
    if (w[0] === 'N' && w[1] === 'CLR') { for (const k of Object.keys(store)) delete store[k]; return ['OK N CLR']; }
    if (w[0] === 'N' && w[1] === 'TEST') return ['OK N TEST ' + w[2] + (w[2] === 'calendar' ? ' ok 12 events this week' : ' ok')];
    return [];
  }
  let controller, pending = '';
  const readable = new ReadableStream({ start(c) { controller = c; } });
  const writable = new WritableStream({ write(chunk) {
    pending += new TextDecoder().decode(chunk);
    let nl;
    while ((nl = pending.indexOf('\\n')) >= 0) {
      const line = pending.slice(0, nl); pending = pending.slice(nl + 1);
      for (const r of reply(line)) controller.enqueue(new TextEncoder().encode(r + '\\n'));
    }
  } });
  const port = { open: async () => {}, close: async () => {}, setSignals: async () => {}, readable, writable, getInfo: () => ({ usbVendorId: 0x303a }) };
  Object.defineProperty(navigator, 'serial', { value: { requestPort: async () => port, getPorts: async () => [port] } });
  window.confirm = () => true;
})();`;

(async () => {
  const chrome = spawn(CHROME, ['--headless=new', '--disable-gpu', '--remote-debugging-port=9334', '--window-size=1000,1500',
    '--user-data-dir=/tmp/setup-chrome', 'about:blank'], { stdio: 'ignore' });
  let targets;
  for (let i = 0; i < 40; i++) {
    try { targets = await (await fetch('http://localhost:9334/json')).json(); if (targets.length) break; } catch (e) {}
    await sleep(250);
  }
  const ws = new WebSocket(targets.find((t) => t.type === 'page').webSocketDebuggerUrl);
  await new Promise((r) => (ws.onopen = r));
  let id = 0; const waits = {};
  ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && waits[d.id]) waits[d.id](d); };
  const send = (method, params = {}) => new Promise((r) => { waits[++id] = r; ws.send(JSON.stringify({ id, method, params })); });
  const js = async (e) => { const r = (await send('Runtime.evaluate', { expression: e, awaitPromise: true, returnByValue: true })).result; if (r.exceptionDetails) throw new Error(JSON.stringify(r.exceptionDetails)); return r.result.value; };
  const shot = async (name) => fs.writeFileSync(name, Buffer.from((await send('Page.captureScreenshot', { format: 'png' })).result.data, 'base64'));
  await send('Page.enable'); await send('Runtime.enable');
  await send('Page.addScriptToEvaluateOnNewDocument', { source: FAKE });
  await send('Page.navigate', { url: URL_ });
  await sleep(1500);
  const fill = (id, v) => js(`(() => { const e = document.getElementById('${id}'); e.value = ${JSON.stringify(v)}; e.dispatchEvent(new Event('input')); })()`);
  const click = (sel) => js(`document.querySelector(${JSON.stringify(sel)}).click()`);
  const kind = (k) => js(`(() => { const r = document.querySelector('input[name=wifiKind][value=${k}]'); r.checked = true; r.dispatchEvent(new Event('change')); })()`);
  const status = () => js(`document.getElementById('wifiStatus').textContent`);
  const list = () => js(`[...document.querySelectorAll('#netList li')].map(li => li.querySelector('.name') ? li.querySelector('.name').textContent + '/' + li.querySelector('.kind').textContent : li.textContent)`);
  const dev = () => js(`JSON.stringify(window.__device)`);
  const out = {};
  await click('#connect'); await sleep(800);
  out.empty = await list();
  await fill('wifi_ssid', 'HomeNet'); await fill('wifi_pass', 'homepass'); await click('#saveNet'); await sleep(800);
  out.afterHome = [await status(), await list()];
  await kind('enterprise'); await fill('wifi_ssid', 'eduroam'); await fill('wifi_user', 'me@uni.ac.uk'); await fill('wifi_pass', 'unipass'); await click('#saveNet'); await sleep(800);
  out.afterEdu = [await status(), await list()];
  await kind('home'); await fill('wifi_ssid', 'Phone'); await fill('wifi_pass', 'hotspot'); await click('#saveNet'); await sleep(800);
  out.afterPhone = await list();
  await fill('wifi_ssid', 'HomeNet'); await click('#saveNet'); await sleep(500);
  out.duplicate = await status();
  out.device = JSON.parse(await dev());
  await shot('/tmp/setup-page.png');
  // edit the second network (slot 1 = wifi2_*): change the password only
  await js(`document.querySelectorAll('#netList li')[1].querySelectorAll('button')[0].click()`); await sleep(300);
  out.editTitle = await js(`document.getElementById('netFormTitle').textContent`);
  await fill('wifi_pass', 'newunipass'); await click('#saveNet'); await sleep(800);
  out.afterEditPass = JSON.parse(await dev()).wifi2_pass;
  // remove the first
  await js(`document.querySelectorAll('#netList li')[0].querySelectorAll('button')[1].click()`); await sleep(1000);
  out.afterRemove = await list();
  out.deviceAfterRemove = Object.keys(JSON.parse(await dev())).filter(k => k.startsWith('wifi')).sort();
  // fill to five and try a sixth
  for (const n of ['A', 'B', 'C']) { await fill('wifi_ssid', 'Net' + n); await fill('wifi_pass', 'x'); await click('#saveNet'); await sleep(700); }
  out.five = await list();
  await fill('wifi_ssid', 'NetD'); await click('#saveNet'); await sleep(500);
  out.sixth = await status();
  // a key in the tidy-up model box is refused; a model name saves
  const cleanupStatus = () => js(`document.getElementById('cleanupStatus').textContent`);
  await fill('cleanup_model', 'sk-proj-abc'); await click('[data-save=cleanup]'); await sleep(500);
  out.modelKey = [await cleanupStatus(), JSON.parse(await dev()).cleanup_model || ''];
  await fill('cleanup_model', 'gpt-4o-mini'); await click('[data-save=cleanup]'); await sleep(800);
  out.modelName = [await cleanupStatus(), JSON.parse(await dev()).cleanup_model];
  // the calendar link: only a web link saves; Test says what it found
  const calendarStatus = () => js(`document.getElementById('calendarStatus').textContent`);
  await fill('cal_url', 'my calendar'); await click('[data-save=calendar]'); await sleep(500);
  out.calBad = [await calendarStatus(), JSON.parse(await dev()).cal_url || ''];
  await fill('cal_url', 'webcal://p01-caldav.icloud.com/published/2/abc'); await click('[data-save=calendar]'); await sleep(800);
  out.calSaved = [await calendarStatus(), JSON.parse(await dev()).cal_url, await js(`document.getElementById('calendarSaved').textContent`)];
  await click('[data-test=calendar]'); await sleep(800);
  out.calTest = await calendarStatus();
  await click('[data-test=openai]'); await sleep(800);
  out.openaiTest = await js(`document.getElementById('openaiStatus').textContent`);
  // the note language: saved with the transcription choice, and Auto clears it
  const lang = (v) => js(`(() => { const e = document.getElementById('note_lang'); e.value = '${v}'; e.dispatchEvent(new Event('change')); })()`);
  out.langShown = await js(`!document.getElementById('langBox').hidden`);
  await lang('fr'); await click('[data-save=transcribe]'); await sleep(800);
  out.langFrench = [JSON.parse(await dev()).note_lang, JSON.parse(await dev()).transcribe, await js(`document.getElementById('transcribeSaved').textContent`)];
  await lang(''); await click('[data-save=transcribe]'); await sleep(800);
  out.langAuto = [JSON.parse(await dev()).note_lang || '(unset)', await js(`document.getElementById('transcribeSaved').textContent`)];
  // firmware from before it: no language box, and the transcription choice still saves
  await js(`localStorage.setItem('oldFirmware', '1')`); await send('Page.navigate', { url: URL_ }); await sleep(1500);
  await click('#connect'); await sleep(800);
  out.oldShown = await js(`!document.getElementById('langBox').hidden`);
  await js(`(() => { const r = document.querySelector('input[name=transcribe][value=elevenlabs]'); r.checked = true; r.dispatchEvent(new Event('change')); })()`);
  await click('[data-save=transcribe]'); await sleep(800);
  out.oldSave = [await js(`document.getElementById('transcribeStatus').textContent`), JSON.parse(await dev()).transcribe];
  await js(`localStorage.removeItem('oldFirmware')`);
  console.log(JSON.stringify(out, null, 1));
  chrome.kill(); process.exit(0);
})().catch((e) => { console.error(e); process.exit(1); });
