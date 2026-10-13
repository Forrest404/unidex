// Opens site/try.html in headless Chrome, presses buttons through the page's own keyboard handling and checks the
// screen changes. Needs the emulator built (emu/build.sh) and the site served on :8765 (python3 -m http.server 8765).
const { spawn } = require('child_process');
const fs = require('fs');
const CHROME = process.env.CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const URL_ = process.env.URL_ || 'http://localhost:8765/emu/web/try.html';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
(async () => {
  const chrome = spawn(CHROME, ['--headless=new', '--disable-gpu', '--remote-debugging-port=9333', '--window-size=900,900',
    '--user-data-dir=/tmp/emu-chrome', 'about:blank'], { stdio: 'ignore' });
  let targets;
  for (let i = 0; i < 40; i++) {
    try { targets = await (await fetch('http://localhost:9333/json')).json(); if (targets.length) break; } catch (e) {}
    await sleep(250);
  }
  const ws = new WebSocket(targets.find((t) => t.type === 'page').webSocketDebuggerUrl);
  await new Promise((r) => (ws.onopen = r));
  let id = 0; const waits = {};
  ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && waits[d.id]) waits[d.id](d); };
  const send = (method, params = {}) => new Promise((r) => { waits[++id] = r; ws.send(JSON.stringify({ id, method, params })); });
  const evalJs = async (e) => (await send('Runtime.evaluate', { expression: e, awaitPromise: true, returnByValue: true })).result.result.value;
  const shot = async (name) => fs.writeFileSync(name, Buffer.from((await send('Page.captureScreenshot', { format: 'png' })).result.data, 'base64'));
  const key = async (k, hold = 60) => {
    await send('Input.dispatchKeyEvent', { type: 'keyDown', key: k, text: k.length === 1 ? k : undefined });
    await sleep(hold);
    await send('Input.dispatchKeyEvent', { type: 'keyUp', key: k });
    await sleep(500);
  };
  const hash = () => evalJs(`(() => { const c = document.getElementById('screen'); const d = c.getContext('2d').getImageData(0,0,200,200).data; let h = 0; for (let i = 0; i < d.length; i += 4) h = (h * 31 + d[i]) | 0; return h; })()`);
  await send('Page.enable'); await send('Runtime.enable');
  await send('Page.navigate', { url: URL_ });
  await sleep(4000);
  const out = [];
  out.push(['loaded', await evalJs(`document.getElementById('status').textContent`)]);
  const h0 = await hash();
  await key('b'); await key('b'); await key('b');           // the welcome pages -> home
  const h1 = await hash();
  await key('a'); await key('a');                          // next app twice
  const h2 = await hash();
  await key('b');                                          // open it
  const h3 = await hash();
  await key('a', 600);                                     // hold A: back
  const h4 = await hash();
  out.push(['screens changed', h0 !== h1 && h1 !== h2 && h2 !== h3 && h3 !== h4, [h0, h1, h2, h3, h4]]);
  await shot('/tmp/emu-page.png');
  console.log(JSON.stringify(out));
  chrome.kill(); process.exit(0);
})().catch((e) => { console.error(e); process.exit(1); });
