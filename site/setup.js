// The Setup page (setup.html). Talks to the device over Web Serial only.
import { supported, connect, asleepHint, notesStatus, notesSet, notesSetLong, notesClear, notesTest, notesMic, notesList,
         notesRead, notesDelete } from './serial.js';

const $ = id => document.getElementById(id);
const DEFAULT_MODEL = { openai: 'gpt-4o-mini', anthropic: 'claude-haiku-4-5', off: '' };
let dev = null, busy = false;

// Up to five WiFi networks: slot 0 is the original wifi_* settings, slots 1 to 4 are wifi2_* to wifi5_*.
const SLOTS = 5;
const key = (slot, part) => (slot === 0 ? `wifi_${part}` : `wifi${slot + 1}_${part}`);
let nets = [];       // saved networks: { slot, ssid, enterprise, hasCa, hasPass }
let editing = -1;    // the slot shown in the form, or -1 for a new network

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
const transcribeChoice = () => document.querySelector('input[name=transcribe]:checked').value;
const enterprise = () => document.querySelector('input[name=wifiKind]:checked').value === 'enterprise';

let caSaved = false;
function showWifiKind() {
  $('userBox').hidden = $('caBox').hidden = !enterprise();
  $('enterpriseNote').hidden = !enterprise() || caSaved;
  $('caNote').hidden = !enterprise() || !caSaved;
  $('removeCa').hidden = !caSaved || editing < 0;
  if ($('wifi_ssid').value === '' && enterprise()) $('wifi_ssid').value = 'eduroam';
}
document.querySelectorAll('input[name=wifiKind]').forEach(r => (r.onchange = showWifiKind));

// The saved networks, each with Edit and Remove.
function renderNets() {
  const ul = $('netList');
  ul.textContent = '';
  for (const n of nets) {
    const li = document.createElement('li');
    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = n.ssid;
    const kind = document.createElement('span');
    kind.className = 'kind';
    kind.textContent = n.enterprise ? 'eduroam / work' : 'home';
    const edit = document.createElement('button');
    edit.className = 'secondary';
    edit.textContent = 'Edit';
    edit.onclick = () => { editNet(n.slot); say('wifiStatus', ''); };
    const del = document.createElement('button');
    del.className = 'secondary';
    del.textContent = 'Remove';
    del.onclick = () => {
      if (!confirm(`Remove "${n.ssid}" from the device?`)) return;
      run('wifiStatus', async () => {
        for (const part of ['ssid', 'user', 'pass', 'ca']) await notesSet(dev, key(n.slot, part), '');
        if (editing === n.slot) editing = -1;
        await load();
        say('wifiStatus', 'Removed.', 'ok');
      });
    };
    li.append(name, kind, edit, del);
    ul.append(li);
  }
  if (!nets.length) {
    const li = document.createElement('li');
    li.className = 'kind';
    li.textContent = 'No network saved yet.';
    ul.append(li);
  }
  $('wifiSaved').textContent = nets.length ? `· ${nets.length} saved` : '';
}

// Puts a saved network in the form (slot), or clears it for a new one (-1).
function editNet(slot) {
  const n = nets.find(x => x.slot === slot);
  editing = n ? slot : -1;
  $('wifi_ssid').value = n ? n.ssid : '';
  $('wifi_user').value = n ? n.user : '';
  $('wifi_pass').value = '';
  $('wifi_pass').placeholder = n && n.hasPass ? 'saved' : '';
  $('wifi_ca').value = '';
  caSaved = !!(n && n.hasCa);
  $('wifi_ca').placeholder = caSaved ? 'saved' : '-----BEGIN CERTIFICATE-----';
  document.querySelector(`input[name=wifiKind][value=${n && n.enterprise ? 'enterprise' : 'home'}]`).checked = true;
  $('netFormTitle').textContent = n ? `Edit ${n.ssid}` : 'Add a network';
  $('saveNet').textContent = n ? 'Save changes' : 'Save network';
  $('cancelEdit').hidden = !n;
  showWifiKind();
}

function showTranscribe() {
  const eleven = transcribeChoice() === 'elevenlabs';
  $('elevenlabsBox').hidden = $('testElevenLabs').hidden = !eleven;
  $('whisperNote').hidden = eleven;
}
document.querySelectorAll('input[name=transcribe]').forEach(r => (r.onchange = showTranscribe));

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
  for (const name of ['gh_repo', 'cleanup_model']) $(name).value = s(name).set ? s(name).value : '';
  nets = [];
  for (let i = 0; i < SLOTS; i++)
    if (s(key(i, 'ssid')).set)
      nets.push({ slot: i, ssid: s(key(i, 'ssid')).value, user: s(key(i, 'user')).value, enterprise: s(key(i, 'user')).set,
                  hasCa: s(key(i, 'ca')).set, hasPass: s(key(i, 'pass')).set });
  renderNets();
  editNet(editing);
  for (const name of ['gh_branch', 'gh_dir']) $(name).value = s(name).set ? s(name).value : '';
  $('gh_on').checked = s('gh_on').value === '1';
  const cleanup = s('cleanup').value || 'openai';
  document.querySelector(`input[name=cleanup][value=${cleanup}]`).checked = true;
  showCleanup();
  const transcribe = s('transcribe').value === 'elevenlabs' ? 'elevenlabs' : 'openai';  // older firmware: Whisper
  document.querySelector(`input[name=transcribe][value=${transcribe}]`).checked = true;
  showTranscribe();
  for (const name of ['openai_key', 'elevenlabs_key', 'anthropic_key', 'gh_token', 'cal_url']) {
    $(name).value = '';
    $(name).placeholder = s(name).set ? (s(name).value ? `saved (…${s(name).value})` : 'saved') : '';
  }
  $('openaiSaved').textContent = s('openai_key').set ? '· saved' : '';
  $('elevenlabsSaved').textContent = s('elevenlabs_key').set ? '· saved' : '';
  $('anthropicSaved').textContent = s('anthropic_key').set ? '· saved' : '';
  $('transcribeSaved').textContent = transcribe === 'elevenlabs' ? '· ElevenLabs' : '· OpenAI';
  $('githubSaved').textContent = s('gh_on').value === '1' ? '· on' : '· off';
  $('calendarSaved').textContent = s('cal_url').set ? '· saved' : '';
  $('cleanupSaved').textContent = cleanup === 'off' ? '· off' : cleanup === 'anthropic' ? '· Claude' : '· OpenAI';
  $('device').textContent = st.card
    ? `Connected. ${st.notes} note${st.notes === 1 ? '' : 's'} on the card${st.waiting ? `, ${st.waiting} waiting to sync` : ''}.`
    : "Connected. There's no SD card, so notes only go to GitHub and nothing is kept on the device.";
  $('device').textContent += " Disconnect when you're done, so the Mac sync and updates can reach it.";
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
  openai: { status: 'openaiStatus', plain: [], secret: ['openai_key'] },
  transcribe: { status: 'transcribeStatus', plain: [], secret: ['elevenlabs_key'], extra: () => ({ transcribe: transcribeChoice() }) },
  cleanup: { status: 'cleanupStatus', plain: ['cleanup_model'], secret: ['anthropic_key'], extra: () => ({ cleanup: cleanupChoice() }) },
  github: { status: 'githubStatus', plain: ['gh_repo', 'gh_branch', 'gh_dir'], secret: ['gh_token'],
            extra: () => ({ gh_on: $('gh_on').checked ? '1' : '0' }) },
  calendar: { status: 'calendarStatus', plain: [], secret: ['cal_url'] },
};

// The base64 body of the first certificate in a pasted PEM, or null if there isn't one.
function caBody() {
  const m = $('wifi_ca').value.match(/-----BEGIN CERTIFICATE-----([\s\S]*?)-----END CERTIFICATE-----/);
  const body = m && m[1].replace(/\s+/g, '');
  return body && /^[A-Za-z0-9+/]+=*$/.test(body) && body.length <= 3000 ? body : null;
}

function check(group) {
  if (group === 'openai' && $('openai_key').value && !$('openai_key').value.trim().startsWith('sk-'))
    return 'OpenAI keys start with sk-.';
  if (group === 'cleanup' && /^sk-/.test($('cleanup_model').value.trim()))
    return 'That looks like an API key. The model box takes a model name, like gpt-4o-mini.';
  if (group === 'calendar' && $('cal_url').value && !/^(webcals?|https?):\/\/\S+$/i.test($('cal_url').value.trim()))
    return 'The link should start with https:// or webcal://.';
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
      if (!Object.keys(values).length) return say(g.status, 'Nothing to save yet. Type the key first.');
      for (const [key, value] of Object.entries(values)) await notesSet(dev, key, value);
      await load();
      say(g.status, 'Saved on the device.', 'ok');
    });
  };
});

// Saves the form as network `editing`, or as the first empty slot.
function checkNet() {
  if ($('wifi_ca').value.trim() && enterprise() && !caBody())
    return 'The CA certificate should be PEM text, from -----BEGIN CERTIFICATE----- to -----END CERTIFICATE-----.';
  if (!$('wifi_ssid').value.trim()) return 'Type the network name.';
  if (enterprise() && !$('wifi_user').value.trim()) return 'Type your username.';
  if (editing < 0 && nets.length >= SLOTS) return `${SLOTS} networks are already saved. Remove one first.`;
  const ssid = $('wifi_ssid').value.trim();
  if (nets.some(n => n.ssid === ssid && n.slot !== editing)) return `"${ssid}" is already saved, so edit that one instead.`;
  return '';
}

$('saveNet').onclick = () => {
  const problem = checkNet();
  if (problem) return say('wifiStatus', problem, 'bad');
  const slot = editing >= 0 ? editing : [...Array(SLOTS).keys()].find(i => !nets.some(n => n.slot === i));
  run('wifiStatus', async () => {
    say('wifiStatus', 'Saving…');
    try {
      await notesSet(dev, key(slot, 'ssid'), $('wifi_ssid').value.trim());
      await notesSet(dev, key(slot, 'user'), enterprise() ? $('wifi_user').value.trim() : '');  // '' = home network
      if ($('wifi_pass').value.trim()) await notesSet(dev, key(slot, 'pass'), $('wifi_pass').value.trim());
      else if (editing < 0) await notesSet(dev, key(slot, 'pass'), '');  // a new open network
      if (enterprise() && caBody()) await notesSetLong(dev, key(slot, 'ca'), caBody());
      if (!enterprise() && caSaved) await notesSet(dev, key(slot, 'ca'), '');  // home: no CA
    } catch (e) {
      if (slot > 0) throw new Error('Saving a second network needs newer firmware. Update the device on the Install page first.');
      throw e;
    }
    editing = -1;
    await load();
    say('wifiStatus', 'Saved on the device.', 'ok');
  });
};

$('cancelEdit').onclick = () => { editNet(-1); say('wifiStatus', ''); };

$('removeCa').onclick = () =>
  run('wifiStatus', async () => {
    await notesSet(dev, key(editing, 'ca'), '');
    await load();
    say('wifiStatus', 'Certificate removed.', 'ok');
  });

// What to do about a Test failure, by the reason the device gives.
const HINTS = [
  ['key lacks permission', " The key is restricted: make one with every permission, or give it Speech to Text and User."],
  ['free plan blocked', " ElevenLabs stops free accounts it thinks are shared: another free account on your network, or a VPN. A different network or a paid plan clears it."],
];

document.querySelectorAll('[data-test]').forEach(btn => {
  btn.onclick = () => {
    const what = btn.dataset.test;
    const status = { wifi: 'wifiStatus', openai: 'openaiStatus', elevenlabs: 'transcribeStatus', anthropic: 'cleanupStatus',
                     github: 'githubStatus', calendar: 'calendarStatus' }[what];
    run(status, async () => {
      say(status, 'Testing on the device… (a few seconds)');
      const r = await notesTest(dev, what);
      say(status, r.ok ? (r.detail ? `Works: ${r.detail}.` : 'Works.') : r.reason + (HINTS.find(h => r.reason.includes(h[0]))?.[1] ?? ''), r.ok ? 'ok' : 'bad');
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
  say('micStatus', r.peak < 300 ? 'Very quiet. Is anything covering the microphone?' : `Peak ${r.peak}, average ${r.rms}.`,
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

showTranscribe();
showCleanup();

// Let go of the port when the tab closes, so the Mac agent and flashing aren't blocked by a forgotten tab.
window.addEventListener('pagehide', () => { if (dev) dev.close().catch(() => {}); });
