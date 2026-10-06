#pragma once

// The whole "Open on phone" page: one reply, everything inline (no internet, no outside files). Its look and
// layout follow the Pala Note 2.0 portal (warm paper, ink, coral accent, lilac and sand cards, a floating
// pill navigation, big headlines); the notes are drawn on the phone from /notes.json and /note.json, so
// moving around is instant.
static const char PHONE_PAGE[] = R"PAGE(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="color-scheme" content="light dark"><title>unidex</title><link rel="icon" href="data:,">
<style>
:root{--ink:#22211f;--muted:#6b675f;--line:#d9d4c9;--paper:#f6f4ef;--surface:#fffdfa;--accent:#c9402a;
--lilac:#ddd7e9;--sand:#f1e7cc;--soft:#ebe9e3;--navbg:rgba(232,230,223,.94);--shadow:rgba(49,44,36,.045);
--ease:cubic-bezier(.32,.72,0,1)}
@media(prefers-color-scheme:dark){:root{--ink:#f2efe8;--muted:#9c978e;--line:#3b3833;--paper:#151412;
--surface:#211f1c;--lilac:#36324a;--sand:#3d3524;--soft:#2a2825;--navbg:rgba(38,36,33,.94);--shadow:rgba(0,0,0,.3)}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html{background:var(--paper);touch-action:manipulation;-webkit-text-size-adjust:100%}
:focus-visible{outline:2px solid var(--accent);outline-offset:3px}
body{margin:0;padding:0 24px 64px;background:var(--paper);color:var(--ink);
font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Arial,Helvetica,sans-serif;font-synthesis:none;-webkit-font-smoothing:antialiased}
button,input{font-family:inherit}
.wrap{max-width:1080px;margin:0 auto}
.nav{position:sticky;top:12px;z-index:20;min-height:68px;display:flex;align-items:center;justify-content:space-between;
margin:12px 0 52px;padding:8px 10px 8px 20px;border:1px solid rgba(31,31,28,.06);border-radius:28px;background:var(--navbg);
-webkit-backdrop-filter:blur(18px);backdrop-filter:blur(18px);box-shadow:0 16px 36px rgba(31,31,28,.08)}
.brand{font-weight:900;letter-spacing:-.055em;font-size:24px}
.navlinks{display:flex;gap:5px}
.navlink{display:flex;align-items:center;gap:7px;color:var(--muted);text-decoration:none;border-radius:18px;padding:11px 14px;font-size:13px;font-weight:760}
.navlink.active{background:var(--accent);color:#fff}
.navlink svg{width:17px;height:17px;fill:none;stroke:currentColor;stroke-width:1.9;stroke-linecap:round;stroke-linejoin:round}
.top{display:flex;align-items:flex-end;justify-content:space-between;gap:24px;margin:0 0 28px;padding-top:18px}
h1{font-size:clamp(56px,8vw,92px);letter-spacing:-.048em;line-height:.95;margin:0;font-weight:820}
.sub{font-size:12px;text-transform:uppercase;letter-spacing:.14em;color:var(--muted);margin:0 0 14px;font-weight:750}
.pill{display:inline-flex;align-items:center;border:1px solid var(--line);border-radius:999px;padding:8px 12px;font-size:12px;
background:var(--surface);white-space:nowrap}
.toolbar{display:flex;align-items:center;gap:10px;margin:18px 0 22px}
.find{flex:1;position:relative}
.search{width:100%;font:inherit;font-weight:600;font-size:16px;color:var(--ink);border:1px solid var(--line);border-radius:16px;
padding:14px 44px 14px 15px;background:var(--surface);min-height:52px;outline:0;-webkit-appearance:none}
.search::-webkit-search-cancel-button{display:none}
.search:focus{border-color:var(--accent);box-shadow:0 0 0 3px rgba(201,64,42,.14)}
.clear{position:absolute;right:6px;top:50%;transform:translateY(-50%);width:40px;height:40px;border:0;border-radius:50%;
background:none;color:var(--muted);font-size:22px;line-height:1;cursor:pointer}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}
.card{background:var(--surface);border-radius:26px;padding:22px;box-shadow:0 10px 32px var(--shadow)}
.card.lilac{background:var(--lilac)}.card.sand{background:var(--sand)}
.note{cursor:pointer;transition:transform .15s ease-out}.note:active{transform:scale(.985)}
.num{font-size:11px;letter-spacing:.11em;text-transform:uppercase;color:var(--muted);margin-bottom:9px;font-weight:700}
.title{font-size:23px;line-height:1.14;letter-spacing:-.025em;font-weight:780;margin:0 0 12px}
.text{font-size:15px;line-height:1.5;color:var(--ink);opacity:.86;margin:0 0 14px}
.topics{display:flex;flex-wrap:wrap;gap:6px}
.topic{border:1px solid var(--line);border-radius:999px;padding:6px 10px;font:inherit;font-size:12px;white-space:nowrap;background:var(--soft);
color:var(--ink)}
button.topic{cursor:pointer;min-height:32px}
.state{float:right;margin-left:10px}
.btn{appearance:none;display:inline-flex;align-items:center;justify-content:center;gap:7px;font:inherit;line-height:1.25;color:var(--ink);
text-decoration:none;border:1px solid var(--line);border-radius:999px;padding:10px 15px;background:var(--surface);font-size:13px;
font-weight:650;cursor:pointer;min-height:44px;transition:transform .12s ease-out}
.btn:active{transform:scale(.97)}
.btn svg{width:16px;height:16px;fill:none;stroke:currentColor;stroke-width:1.9;stroke-linecap:round;stroke-linejoin:round}
.btn.primary{background:var(--accent);color:#fff;border-color:var(--accent)}
.empty{border:1px dashed var(--line);border-radius:24px;padding:42px;text-align:center;color:var(--muted)}
.hidden{display:none!important}
.foot{color:var(--muted);text-align:center;font-size:12px;margin:34px 0 0}
/* an open note: a full card over the page */
#open{position:fixed;inset:0;z-index:30;background:var(--paper);overflow-y:auto;-webkit-overflow-scrolling:touch;
transform:translateY(100%);transition:transform .22s cubic-bezier(.4,0,1,1);padding:0 24px 130px;visibility:hidden}
.reading #open{transform:none;transition:transform .32s cubic-bezier(.2,.8,.2,1);visibility:visible}.reading{overflow:hidden}
#open.gone{transition:transform .22s cubic-bezier(.4,0,1,1),visibility 0s .22s}
.sk{height:15px;border-radius:8px;background:var(--soft);margin:12px 0;animation:pulse 1.1s ease-in-out infinite}
.sk:nth-child(2){width:92%}.sk:nth-child(3){width:70%}
@keyframes pulse{50%{opacity:.45}}
.dock{position:fixed;left:0;right:0;bottom:0;z-index:3;display:flex;justify-content:center;gap:8px;
padding:22px 16px calc(14px + env(safe-area-inset-bottom));background:linear-gradient(rgba(0,0,0,0),var(--paper) 38%)}
.dock .btn{box-shadow:0 6px 18px var(--shadow)}
.openbar{position:sticky;top:0;display:flex;justify-content:space-between;align-items:center;padding:16px 0;background:var(--paper);z-index:2}
.close{width:44px;height:44px;border-radius:50%;border:1px solid var(--line);background:var(--surface);color:var(--ink);font-size:22px;line-height:1;cursor:pointer}
.reader{max-width:720px;margin:0 auto}
.reader .title{font-size:clamp(32px,6vw,48px);letter-spacing:-.04em;line-height:1.02;margin:6px 0 16px}
.reader .card{margin-bottom:14px}
.body p{font-size:17px;line-height:1.6;margin:0 0 12px}.body .li{padding-left:18px;text-indent:-14px}.body .h{font-weight:760;margin-top:18px}
.eyebrow{font-size:10px;letter-spacing:.14em;text-transform:uppercase;color:var(--muted);margin:0 0 8px;font-weight:750}
details summary{cursor:pointer;list-style:none;font-weight:700}details summary::-webkit-details-marker{display:none}
details .body p{font-size:15px;color:var(--muted)}
@media(prefers-reduced-motion:reduce){*{transition-duration:.01ms!important;animation:none!important}}
/* share sheet */
#scrim{position:fixed;inset:0;background:rgba(20,18,15,.38);opacity:0;pointer-events:none;transition:opacity .3s;z-index:40}
#sheet{position:fixed;left:12px;right:12px;bottom:calc(12px + env(safe-area-inset-bottom));z-index:41;max-width:520px;margin:0 auto;
transform:translateY(130%);transition:transform .4s var(--ease)}
.sharing #scrim{opacity:1;pointer-events:auto}.sharing #sheet{transform:none}
#sheet .card{padding:20px 14px 16px}
#sheet .title{font-size:19px;margin:0 8px 16px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.apps{display:flex;justify-content:space-around;flex-wrap:wrap;gap:10px 0}
.apps a,.apps button{display:flex;flex-direction:column;align-items:center;gap:7px;width:76px;font-size:12px;font-weight:650;color:var(--ink);
text-decoration:none;background:none;border:0;padding:0;cursor:pointer}
.apps i{width:56px;height:56px;border-radius:18px;display:flex;align-items:center;justify-content:center;background:var(--soft);color:var(--ink)}
.apps i svg{width:26px;height:26px;fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round}
.apps .hot i{background:var(--accent);color:#fff}
.cancel{display:block;width:100%;margin-top:10px;min-height:54px;border-radius:22px}
#toast{position:fixed;left:50%;bottom:calc(104px + env(safe-area-inset-bottom));transform:translate(-50%,10px);background:var(--ink);
color:var(--paper);font-size:14px;font-weight:700;padding:11px 18px;border-radius:999px;opacity:0;transition:.25s;pointer-events:none;z-index:50}
#toast.on{opacity:1;transform:translate(-50%,0)}
#clip{position:fixed;top:0;left:0;width:1px;height:1px;opacity:0;border:0;padding:0}
@media(max-width:760px){body{padding:0 14px 108px}
.nav{position:fixed;top:auto;bottom:calc(14px + env(safe-area-inset-bottom));left:50%;transform:translateX(-50%);width:calc(100% - 28px);
max-width:560px;margin:0;padding:7px 9px 7px 16px;min-height:66px}
.brand{font-size:20px}.navlinks{flex:1;justify-content:flex-end}.navlink{padding:10px 12px}.navlabel{display:none}
.top{display:block;margin-top:34px;padding-top:0}.top>.pill{margin-top:18px}
.grid{grid-template-columns:1fr}.card{border-radius:22px}.title{font-size:21px}
#open{padding:0 14px 120px}}
</style></head><body>
<div class="wrap">
<nav class="nav"><div class="brand">unidex</div><div class="navlinks">
<a class="navlink active" href="#" aria-label="Notes" aria-current="page"><svg viewBox="0 0 24 24"><path d="M6 6h14M6 12h14M6 18h14"/><circle cx="3" cy="6" r=".5"/><circle cx="3" cy="12" r=".5"/><circle cx="3" cy="18" r=".5"/></svg><span class="navlabel">Notes</span></a>
</div></nav>
<div class="top"><div><div class="sub">Voice notes on your unidex</div><h1>Notes.</h1></div><div class="pill" id="count">&nbsp;</div></div>
<div class="toolbar"><div class="find"><input id="q" class="search" type="search" placeholder="Search notes…" aria-label="Search notes" autocomplete="off" autocorrect="off" enterkeyhint="search">
<button id="qc" class="clear hidden" aria-label="Clear search" onclick="setQuery('')">&times;</button></div>
<a class="btn" href="/export.txt" download><svg viewBox="0 0 24 24"><path d="M12 4v11M7 10l5 5 5-5M5 20h14"/></svg>Export</a></div>
<div id="grid" class="grid"></div><div id="empty" class="empty hidden"></div>
<div class="foot">Straight from your unidex. Nothing goes to the internet.</div>
</div>
<div id="open" class="gone" role="dialog" aria-modal="true" aria-labelledby="ot"><div class="reader">
<div class="openbar"><div class="num" id="od"></div><button class="close" onclick="history.back()" aria-label="Close">&times;</button></div>
<h2 class="title" id="ot"></h2>
<div class="card lilac" id="os"><div class="eyebrow">Summary</div><div id="ost"></div></div>
<div class="dock">
<button class="btn primary" onclick="openSheet()"><svg viewBox="0 0 24 24"><path d="M12 3v12M8 7l4-4 4 4M6 11H5a1 1 0 00-1 1v8a1 1 0 001 1h14a1 1 0 001-1v-8a1 1 0 00-1-1h-1"/></svg>Share</button>
<button class="btn" onclick="copyNote()"><svg viewBox="0 0 24 24"><rect x="8" y="8" width="12" height="13" rx="2"/><path d="M16 8V5a2 2 0 00-2-2H6a2 2 0 00-2 2v10a2 2 0 002 2h2"/></svg>Copy</button>
<a class="btn" id="otxt" download aria-label="Download as text"><svg viewBox="0 0 24 24"><path d="M12 4v11M7 10l5 5 5-5M5 20h14"/></svg>TXT</a>
</div>
<div class="card"><div class="body" id="ob"></div></div>
<details class="card" id="oq"><summary>Original transcript</summary><div class="body"></div></details>
<div class="topics" id="ok"></div>
</div></div>
<div id="scrim" onclick="closeSheet()"></div>
<div id="sheet"><div class="card"><div class="title" id="st"></div><div class="apps">
<a id="sMsg" class="hot"><i><svg viewBox="0 0 24 24"><path d="M12 3.5c-5 0-9 3.4-9 7.6 0 2.3 1.2 4.4 3.2 5.8L5.5 20l3.6-1.6c.9.2 1.9.3 2.9.3 5 0 9-3.4 9-7.6s-4-7.6-9-7.6z"/></svg></i>Messages</a>
<a id="sMail"><i><svg viewBox="0 0 24 24"><rect x="3" y="5" width="18" height="14" rx="3"/><path d="M4 7l8 6 8-6"/></svg></i>Mail</a>
<a id="sWa"><i><svg viewBox="0 0 24 24"><path d="M4 20l1.3-4A8.5 8.5 0 1112 20.5a8.4 8.4 0 01-4-1z"/><path d="M9 9.5c.3 2 2.5 4.2 4.5 4.5"/></svg></i>WhatsApp</a>
<button onclick="copyNote();closeSheet()"><i><svg viewBox="0 0 24 24"><rect x="8" y="8" width="12" height="13" rx="2"/><path d="M16 8V5a2 2 0 00-2-2H6a2 2 0 00-2 2v10a2 2 0 002 2h2"/></svg></i>Copy</button>
<a id="sFile" download><i><svg viewBox="0 0 24 24"><path d="M12 4v11M7 10l5 5 5-5M5 20h14"/></svg></i>Save as file</a>
<button id="sMore" class="hidden" onclick="nativeShare()"><i><svg viewBox="0 0 24 24"><circle cx="5" cy="12" r="1.3"/><circle cx="12" cy="12" r="1.3"/><circle cx="19" cy="12" r="1.3"/></svg></i>More</button>
</div></div><button class="btn cancel" onclick="closeSheet()">Cancel</button></div>
<div id="toast" role="status" aria-live="polite"></div><textarea id="clip" readonly></textarea>
<script>
var NOTES=/*NOTES*/null;
var $=function(s){return document.getElementById(s)},notes=NOTES||[],byId={},cur=null,cache={},loading={};
function el(t,c,x){var e=document.createElement(t);if(c)e.className=c;if(x!=null)e.textContent=x;return e}
function toast(t){var e=$('toast');e.textContent=t;e.classList.add('on');clearTimeout(e.t);e.t=setTimeout(function(){e.classList.remove('on')},2200)}
// "Today, 13:04" / "Yesterday, 21:50" / "Fri 2 Oct, 21:50" / "2 Oct 2025": from the id (YYYYMMDD-HHMMSS), with the phone's own today.
function when(id){var m=/^(\d{4})(\d\d)(\d\d)-(\d\d)(\d\d)/.exec(id);if(!m)return'';var d=new Date(+m[1],m[2]-1,+m[3]),t=new Date(),hm=m[4]+':'+m[5];
t.setHours(0,0,0,0);var days=Math.round((t-d)/864e5);if(days==0)return'Today, '+hm;if(days==1)return'Yesterday, '+hm;
var o=d.getFullYear()==t.getFullYear()?{weekday:'short',day:'numeric',month:'short'}:{day:'numeric',month:'short',year:'numeric'};
return d.toLocaleDateString(undefined,o)+', '+hm}
// Topics as pills; in the list they filter the notes by that topic.
function topics(box,list,filter){box.textContent='';(list?list.split(' · '):[]).forEach(function(x){
var p=el(filter?'button':'span','topic',x);if(filter)p.onclick=function(e){e.stopPropagation();setQuery(x)};box.appendChild(p)})}
// A note's full text, fetched once and kept; asking again while it's on its way waits for the same answer.
function load(id){if(cache[id])return Promise.resolve(cache[id]);if(loading[id])return loading[id];
return loading[id]=fetch('/note.json?id='+encodeURIComponent(id)).then(function(r){if(!r.ok)throw r.status;return r.json()})
.then(function(o){cache[id]=o;delete loading[id];return o},function(e){delete loading[id];throw e})}
// Fetch the newest notes in the background, one after another, so opening them is instant.
function prefetch(){var q=notes.slice(0,40).map(function(o){return o.id});(function next(){var id=q.shift();if(id)load(id).then(next,next)})()}
function setQuery(v){$('q').value=v;render();if(!v)$('q').blur();window.scrollTo(0,0)}
function render(){var raw=$('q').value.trim(),q=raw.toLowerCase(),g=$('grid'),n=0,f=document.createDocumentFragment();
notes.forEach(function(o){if(q&&(o.t+' '+o.s+' '+o.x+' '+o.k).toLowerCase().indexOf(q)<0)return;n++;
var c=el('article','card note');c.tabIndex=0;c.setAttribute('role','button');
c.onclick=function(){location.hash=o.id};c.onkeydown=function(e){if(e.key=='Enter')location.hash=o.id};
c.onpointerdown=function(){load(o.id).catch(function(){})};
if(o.p)c.appendChild(el('span','topic state','Waiting'));
c.appendChild(el('div','num',when(o.id)));c.appendChild(el('h2','title',o.t));
if(o.s)c.appendChild(el('p','text',o.s));var k=el('div','topics');topics(k,o.k,true);c.appendChild(k);f.appendChild(c)});
g.textContent='';g.appendChild(f);g.classList.toggle('hidden',!n);$('empty').classList.toggle('hidden',!!n);$('qc').classList.toggle('hidden',!raw);
$('empty').textContent=notes.length?'No notes match “'+raw+'”.':'No notes yet. Hold B on the Notes screen to record one.';
var all=notes.length+(notes.length==1?' note':' notes');$('count').textContent=q?n+' of '+all:all}
function body(box,text){box.textContent='';(text||'').split('\n').forEach(function(l){l=l.trim().replace(/\*\*/g,'');if(!l)return;var c='';
if(/^[-*] /.test(l)){l='• '+l.slice(2);c='li'}else if(/^#/.test(l)){l=l.replace(/^#+\s*/,'');c='h'}box.appendChild(el('p',c,l))})}
// Opens at once with what the list already knows (title, date, summary); the text follows when it's in.
function show(o,full){cur=full?o:null;$('od').textContent=when(o.id);$('ot').textContent=o.t;$('os').classList.toggle('hidden',!o.s);$('ost').textContent=o.s||'';
if(full)body($('ob'),o.b||(o.p?'This note is transcribed the next time the device has WiFi.':''));
else $('ob').innerHTML='<div class="sk"></div><div class="sk"></div><div class="sk"></div>';
$('oq').classList.toggle('hidden',!(full&&o.q));$('oq').open=false;if(full)body($('oq').lastChild,o.q);topics($('ok'),o.k);
$('otxt').href='/note.txt?id='+encodeURIComponent(o.id);$('otxt').classList.toggle('hidden',!!o.p)}
function route(){var id=decodeURIComponent(location.hash.slice(1)),box=$('open');closeSheet();
if(!id){document.body.classList.remove('reading');box.classList.add('gone');cur=null;return}
var stub=byId[id]||cache[id];if(!stub&&!cache[id]){load(id).then(function(){route()},function(){toast('Couldn’t open that note');history.back()});return}
show(cache[id]||stub,!!cache[id]);box.scrollTop=0;box.classList.remove('gone');document.body.classList.add('reading');
if(!cache[id])load(id).then(function(o){if(location.hash.slice(1)==encodeURIComponent(o.id)||decodeURIComponent(location.hash.slice(1))==o.id)show(o,true)},
function(){toast('Couldn’t load the text. Try again.')})}
function copyNote(){if(!cur)return toast('One moment…');var t=$('clip'),ok=false;t.value=cur.c;t.readOnly=false;t.focus();t.select();t.setSelectionRange(0,t.value.length);
try{ok=document.execCommand('copy')}catch(e){}t.readOnly=true;t.blur();toast(ok?'Copied':'Couldn’t copy here')}
function openSheet(){if(!cur)return toast('One moment…');var e=encodeURIComponent;$('st').textContent=cur.t;$('sMsg').href='sms:&body='+e(cur.c);
$('sMail').href='mailto:?subject='+e(cur.t)+'&body='+e(cur.c);$('sWa').href='whatsapp://send?text='+e(cur.c);
$('sFile').href='/note.txt?id='+e(cur.id);$('sMore').classList.toggle('hidden',!navigator.share);document.body.classList.add('sharing')}
function closeSheet(){document.body.classList.remove('sharing')}
function nativeShare(){navigator.share({title:cur.t,text:cur.c}).catch(function(){});closeSheet()}
document.onkeydown=function(e){if(e.key!='Escape')return;if(document.body.classList.contains('sharing'))closeSheet();else if(location.hash)history.back()};
$('q').oninput=render;window.onhashchange=route;
function start(d){notes=d;byId={};notes.forEach(function(o){byId[o.id]=o});render();route();setTimeout(prefetch,250)}
if(NOTES)start(NOTES);
else fetch('/notes.json').then(function(r){return r.json()}).then(start)
.catch(function(){$('empty').classList.remove('hidden');$('empty').textContent='Couldn’t load the notes. Reload the page to try again.'});
</script></body></html>)PAGE";
