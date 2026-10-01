// The Notes setup page (notes.html). Talks to the device over Web Serial only.
import { supported, connect, asleepHint, notesStatus, notesSet, notesClear, notesTest, notesMic, notesList,
         notesRead, notesDelete } from './serial.js';

const $ = id => document.getElementById(id);
const DEFAULT_MODEL = { openai: 'gpt-4o-mini', anthropic: 'claude-haiku-4-5', off: '' };
let dev = null, busy = false;

if (!supported) {
  $('unsupported').hidden = false;
  $('connect').disabled = true;
}

const say = (id, text, kind = '') => {
  $(id).textContent = text;
  $(id).className = 'status ' + kind;
};

// One device command at a time: the line protocol can't interleave.
async function run(statusId, fn) {
  if (busy || !dev) return;
  busy = true;
  document.querySelectorAll('#panel button').forEach(b => (b.disabled = true));
  try {
    await fn();
  } catch (e) {
    say(statusId, e.message, 'bad');
  } finally {
    busy = false;
    document.querySelectorAll('#panel button').forEach(b => (b.disabled = false));
    $('downloadAll').disabled = !shownNotes.some(n => n.text);
  }
}

const cleanupChoice = () => document.querySelector('input[name=cleanup]:checked').value;
const enterprise = () => document.querySelector('input[name=wifiKind]:checked').value === 'enterprise';

function showWifiKind() {
  $('userBox').hidden = !enterprise();
  $('enterpriseNote').hidden = !enterprise();
  if ($('wifi_ssid').value === '' && enterprise()) $('wifi_ssid').value = 'eduroam';
}
document.querySelectorAll('input[name=wifiKind]').forEach(r => (r.onchange = showWifiKind));

function showCleanup() {
  const c = cleanupChoice();
  $('anthropicBox').hidden = c !== 'anthropic';
  $('testClaude').hidden = c !== 'anthropic';
  $('cleanup_model').disabled = c === 'off';
  $('modelHint').textContent = c === 'off' ? '' : `(blank = ${DEFAULT_MODEL[c]})`;
}
document.querySelectorAll('input[name=cleanup]').forEach(r => (r.onchange = showCleanup));

// Fills the form from the device. Secrets only say whether they're saved.
async function load() {
  const st = await notesStatus(dev);
  const s = name => st.settings[name] || { set: false, value: '' };
  for (const name of ['wifi_ssid', 'wifi_user', 'gh_repo', 'cleanup_model']) $(name).value = s(name).set ? s(name).value : '';
  document.querySelector(`input[name=wifiKind][value=${s('wifi_user').set ? 'enterprise' : 'home'}]`).checked = true;
  showWifiKind();
  for (const name of ['gh_branch', 'gh_dir']) $(name).value = s(name).set ? s(name).value : '';
  $('gh_on').checked = s('gh_on').value === '1';
  const cleanup = s('cleanup').value || 'openai';
  document.querySelector(`input[name=cleanup][value=${cleanup}]`).checked = true;
  showCleanup();
  for (const name of ['wifi_pass', 'openai_key', 'anthropic_key', 'gh_token']) {
    $(name).value = '';
    $(name).placeholder = s(name).set ? (s(name).value ? `saved (…${s(name).value})` : 'saved') : '';
  }
  $('wifiSaved').textContent = s('wifi_ssid').set ? '· saved' : '';
  $('openaiSaved').textContent = s('openai_key').set ? '· saved' : '';
  $('anthropicSaved').textContent = s('anthropic_key').set ? '· saved' : '';
  $('githubSaved').textContent = s('gh_on').value === '1' ? '· on' : '· off';
  $('cleanupSaved').textContent = cleanup === 'off' ? '· off' : cleanup === 'anthropic' ? '· Claude' : '· OpenAI';
  $('device').textContent = st.card
    ? `Connected. ${st.notes} note${st.notes === 1 ? '' : 's'} on the card${st.waiting ? `, ${st.waiting} waiting to sync` : ''}.`
    : 'Connected. No SD card: notes are only sent to GitHub, nothing is kept on the device.';
  $('device').textContent += ' Disconnect when you\u2019re done, so the Mac sync and updates can reach it.';
  $('cardInfo').textContent = st.card ? 'Notes are kept on the SD card as Markdown.' : 'No SD card in the device.';
}

$('connect').onclick = async () => {
  try {
    say('device', 'Connecting…');
    dev = await connect();
    await load();
    $('panel').disabled = false;
    $('connect').hidden = true;
    $('disconnect').hidden = false;
  } catch (e) {
    if (dev) await dev.close().catch(() => {});
    dev = null;
    say('device', e.name === 'NotFoundError' ? asleepHint : e.message, 'bad');
  }
};

$('disconnect').onclick = async () => {
  if (dev) await dev.close().catch(() => {});
  dev = null;
  $('panel').disabled = true;
  $('connect').hidden = false;
  $('disconnect').hidden = true;
  say('device', 'Disconnected.');
};

// Which fields each Save sends. Secrets are only sent when typed (blank keeps the saved one).
const GROUPS = {
  wifi: { status: 'wifiStatus', plain: ['wifi_ssid'], secret: ['wifi_pass'],
          extra: () => ({ wifi_user: enterprise() ? $('wifi_user').value.trim() : '' }) },  // '' = home network
  openai: { status: 'openaiStatus', plain: [], secret: ['openai_key'] },
  cleanup: { status: 'cleanupStatus', plain: ['cleanup_model'], secret: ['anthropic_key'], extra: () => ({ cleanup: cleanupChoice() }) },
  github: { status: 'githubStatus', plain: ['gh_repo', 'gh_branch', 'gh_dir'], secret: ['gh_token'],
            extra: () => ({ gh_on: $('gh_on').checked ? '1' : '0' }) },
};

function check(group) {
  if (group === 'wifi' && !$('wifi_ssid').value.trim()) return 'Type the network name.';
  if (group === 'wifi' && enterprise() && !$('wifi_user').value.trim()) return 'Type your username.';
  if (group === 'openai' && $('openai_key').value && !$('openai_key').value.trim().startsWith('sk-'))
    return 'OpenAI keys start with sk-.';
  if (group === 'github' && $('gh_repo').value && !/^[\w.-]+\/[\w.-]+$/.test($('gh_repo').value.trim()))
    return 'Repo should look like owner/name.';
  if (group === 'github' && $('gh_on').checked && !$('gh_repo').value.trim()) return 'Type the repo first.';
  return '';
}

document.querySelectorAll('[data-save]').forEach(btn => {
  btn.onclick = () => {
    const name = btn.dataset.save, g = GROUPS[name];
    const problem = check(name);
    if (problem) return say(g.status, problem, 'bad');
    run(g.status, async () => {
      say(g.status, 'Saving…');
      const values = { ...(g.extra ? g.extra() : {}) };
      for (const f of g.plain) values[f] = $(f).value.trim();
      for (const f of g.secret) if ($(f).value.trim()) values[f] = $(f).value.trim();
      if (!Object.keys(values).length) return say(g.status, 'Nothing to save: type the key first.');
      for (const [key, value] of Object.entries(values)) await notesSet(dev, key, value);
      await load();
      say(g.status, 'Saved on the device.', 'ok');
    });
  };
});

document.querySelectorAll('[data-test]').forEach(btn => {
  btn.onclick = () => {
    const what = btn.dataset.test;
    const status = { wifi: 'wifiStatus', openai: 'openaiStatus', anthropic: 'cleanupStatus', github: 'githubStatus' }[what];
    run(status, async () => {
      say(status, 'Testing on the device… (a few seconds)');
      const r = await notesTest(dev, what);
      say(status, r.ok ? 'Works.' : r.reason, r.ok ? 'ok' : 'bad');
    });
  };
});

$('mic').onclick = () => run('micStatus', async () => {
  say('micStatus', 'Listening for 2 seconds… say something.');
  $('meter').firstElementChild.style.width = '0';
  const r = await notesMic(dev);
  if (r.reason) return say('micStatus', r.reason, 'bad');
  const pct = Math.min(100, Math.round((r.peak * 3 * 100) / 32767));  // x3, as on the device's level bar
  $('meter').firstElementChild.style.width = pct + '%';
  say('micStatus', r.peak < 300 ? 'Very quiet: is anything covering the microphone?' : `Peak ${r.peak}, average ${r.rms}.`,
      r.peak < 300 ? 'bad' : 'ok');
});

// --- notes ---
let shownNotes = [];

const recorded = id => {
  const m = /^(\d{4})(\d\d)(\d\d)-(\d\d)(\d\d)/.exec(id);
  return m ? `${m[3]}/${m[2]}/${m[1]} ${m[4]}:${m[5]}` : '';
};

function save(name, text) {
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([text], { type: 'text/markdown' }));
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
}

const fileName = n => `${(n.title || n.id).replace(/[\\/:*?"<>|]/g, '').trim() || n.id} (${n.id}).md`;

function renderNotes() {
  const body = $('noteTable').tBodies[0];
  body.textContent = '';
  for (const n of shownNotes) {
    const tr = body.insertRow();
    tr.insertCell().textContent = n.text ? n.title || n.id : 'Not transcribed yet';
    tr.insertCell().textContent = recorded(n.id);
    const tag = tr.insertCell();
    tag.className = 'tag';
    tag.textContent = !n.text ? 'waiting' : n.pushed ? 'on GitHub' : '';
    const act = tr.insertCell();
    act.className = 'act';
    if (n.text) {
      const dl = document.createElement('button');
      dl.className = 'secondary';
      dl.textContent = 'Download';
      dl.onclick = () => run('listStatus', async () => save(fileName(n), await notesRead(dev, n.id)));
      act.append(dl, ' ');
    }
    const del = document.createElement('button');
    del.className = 'secondary';
    del.textContent = 'Delete';
    del.onclick = () => {
      if (!confirm(`Delete "${n.title || n.id}" from the device?` + (n.pushed ? ' The GitHub copy stays.' : ''))) return;
      run('listStatus', async () => {
        await notesDelete(dev, n.id);
        shownNotes = shownNotes.filter(x => x !== n);
        renderNotes();
        say('listStatus', 'Deleted.', 'ok');
      });
    };
    act.append(del);
  }
  $('noteTable').hidden = !shownNotes.length;
}

$('refresh').onclick = () => run('listStatus', async () => {
  say('listStatus', 'Reading the card…');
  shownNotes = await notesList(dev);
  renderNotes();
  say('listStatus', shownNotes.length ? '' : 'No notes yet. Hold B in the Notes app to record one.');
});

$('downloadAll').onclick = () => run('listStatus', async () => {
  const text = shownNotes.filter(n => n.text);
  for (let i = 0; i < text.length; i++) {
    say('listStatus', `Downloading ${i + 1} of ${text.length}…`);
    save(fileName(text[i]), await notesRead(dev, text[i].id));
    await new Promise(r => setTimeout(r, 300));  // browsers drop downloads fired too close together
  }
  say('listStatus', `Downloaded ${text.length} note${text.length === 1 ? '' : 's'}.`, 'ok');
});

$('clearAll').onclick = () => {
  if (!confirm('Clear the WiFi, API keys, GitHub token and Notes settings from the device?')) return;
  run('clearStatus', async () => {
    await notesClear(dev, 'all');
    await load();
    say('clearStatus', 'Cleared.', 'ok');
  });
};

showCleanup();

// Let go of the port when the tab closes, so the Mac agent and flashing aren't blocked by a forgotten tab.
window.addEventListener('pagehide', () => { if (dev) dev.close().catch(() => {}); });
