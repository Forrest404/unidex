// The unidex emulator page: draws the screen the firmware hands over and feeds it the two buttons.
(function () {
  'use strict';
  const canvas = document.getElementById('screen');
  const ctx = canvas.getContext('2d');
  const img = ctx.createImageData(200, 200);
  const status = document.getElementById('status');
  let mod = null;

  function draw(bytes, kind) {
    // The controller's layout: 1 bit a pixel, 1 = white, rows top to bottom, leftmost pixel in the top bit.
    const d = img.data;
    for (let i = 0; i < 40000; i++) {
      const on = (bytes[i >> 3] & (0x80 >> (i & 7))) === 0;
      const v = on ? 24 : 233;
      d[i * 4] = v; d[i * 4 + 1] = v; d[i * 4 + 2] = on ? 27 : 218; d[i * 4 + 3] = 255;
    }
    ctx.putImageData(img, 0, 0);
    if (kind === 'F' && document.getElementById('eink').checked) {
      canvas.classList.add('flash');
      setTimeout(() => canvas.classList.remove('flash'), 160);
    }
  }

  function press(which, down) {
    if (mod) mod.ccall('emu_button', null, ['number', 'number'], [which, down ? 1 : 0]);
    document.getElementById(which ? 'btnB' : 'btnA').classList.toggle('down', down);
  }

  function wire(id, which) {
    const el = document.getElementById(id);
    el.addEventListener('pointerdown', (e) => { e.preventDefault(); el.setPointerCapture(e.pointerId); press(which, true); });
    const up = () => press(which, false);
    el.addEventListener('pointerup', up);
    el.addEventListener('pointercancel', up);
  }
  wire('btnA', 0);
  wire('btnB', 1);

  const keys = { a: 0, A: 0, ArrowLeft: 0, b: 1, B: 1, ' ': 1, Enter: 1 };
  addEventListener('keydown', (e) => {
    // Keys press the device's buttons, except while typing or using another control (the device's own A and B count)
    if (!(e.key in keys) || e.repeat || e.metaKey || e.ctrlKey ||
        e.target.matches('input, textarea, select, a, [contenteditable], button:not(#btnA):not(#btnB)')) return;
    e.preventDefault();
    press(keys[e.key], true);
  });
  addEventListener('keyup', (e) => {
    if (e.key in keys) press(keys[e.key], false);
  });
  addEventListener('blur', () => { press(0, false); press(1, false); });

  document.getElementById('restart').addEventListener('click', () => location.reload());

  // Which version runs. The website workflow builds every release that has an emulator, and the next version, into
  // emu/<id>/ and lists them in emu/versions.json (the newest release is "latest"); ?v=<id> picks another. Built
  // here with emu/build.sh, there's no list and just the one build, in emu/.
  const picker = document.getElementById('emuVersion');
  const loadScript = src => new Promise((ok, bad) => {
    const s = document.createElement('script');
    s.src = src;
    s.onload = ok;
    s.onerror = bad;
    document.head.append(s);
  });

  async function chooseBuild() {
    let list = null;
    try {
      const r = await fetch('emu/versions.json', { cache: 'no-cache' });
      if (r.ok) list = await r.json();
    } catch (e) { /* no list: a local build */ }
    if (!list || !list.versions || !list.versions.length) return { dir: 'emu/', name: '' };
    const wanted = new URLSearchParams(location.search).get('v');
    const v = list.versions.find(x => x.id === wanted) || list.versions.find(x => x.id === list.latest) || list.versions[0];
    for (const x of list.versions) picker.append(new Option(x.label, x.id, false, x.id === v.id));
    picker.closest('.version').hidden = false;
    picker.addEventListener('change', () => {
      const q = new URLSearchParams(location.search);
      if (picker.value === list.latest) q.delete('v'); else q.set('v', picker.value);
      location.href = location.pathname + (q.toString() ? `?${q}` : '') + location.hash;  // a fresh start on that build
    });
    return { dir: `emu/${v.id}/`, name: v.id === 'next' ? 'the next version' : v.id };
  }

  chooseBuild()
    .then(async (build) => {
      await loadScript(`${build.dir}unidex.js`);
      const m = await createUnidex({ onScreen: draw, print: () => {}, printErr: () => {} });
      mod = m;
      window.unidexEmu = m;  // for apptry.js (Try your app)
      window.dispatchEvent(new Event('unidex-ready'));
      status.textContent = `Running${build.name ? ` ${build.name}` : ''}. Click the buttons or use the keys.`;
    })
    .catch(() => { status.textContent = "Couldn't start the emulator in this browser."; });
})();
