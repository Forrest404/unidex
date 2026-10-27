#pragma once

// The whole phone portal page: one reply, everything inline (no internet, no outside files). Its look and
// layout follow the Pala Note 2.0 portal (warm paper, ink, coral accent, lilac and sand cards, a floating
// pill navigation, big headlines). Three tabs: Notes, drawn on the phone from /notes.json and /note.json so
// moving around is instant; Settings and Setup, from /setup.json, saved with POST /settings, /time and /set.
static const char PHONE_PAGE[] = R"PAGE(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="color-scheme" content="light dark"><title>unidex</title><link rel="icon" href="data:,">
<style>
:root{--ink:#22211f;--muted:#6b675f;--line:#d9d4c9;--paper:#f6f4ef;--surface:#fffdfa;--accent:#c9402a;
--lilac:#ddd7e9;--sand:#f1e7cc;--soft:#ebe9e3;--navbg:rgba(232,230,223,.94);--shadow:rgba(49,44,36,.045);
--ok:#2d6a39;--segon:#fffdfa;--warn:#c9402a;--okbg:#e1eddf;--track:rgba(0,0,0,.08);--warnbg:#f6e0d9;--ease:cubic-bezier(.32,.72,0,1)}
@media(prefers-color-scheme:dark){:root{--ink:#f2efe8;--muted:#9c978e;--line:#3b3833;--paper:#151412;
--surface:#211f1c;--ok:#8fcf9b;--segon:#45413a;--warn:#ff8f78;--okbg:#1f3424;--track:rgba(255,255,255,.14);--warnbg:#3b2521;--lilac:#36324a;--sand:#3d3524;--soft:#2a2825;--navbg:rgba(38,36,33,.94);--shadow:rgba(0,0,0,.3)}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html{background:var(--paper);touch-action:manipulation;-webkit-text-size-adjust:100%}
:focus-visible{outline:2px solid var(--accent);outline-offset:3px}
body{margin:0;padding:0 24px 64px;background:var(--paper);color:var(--ink);
font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Arial,Helvetica,sans-serif;font-synthesis:none;-webkit-font-smoothing:antialiased}
button,input,textarea{font-family:inherit}
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
.rec{display:flex;align-items:center;gap:12px}
.rec audio{flex:1;min-width:0;height:44px}
.rec .btn{flex:none}
/* Settings and Setup */
.stack{display:grid;grid-template-columns:minmax(0,1fr);gap:14px;max-width:640px}
section:not(.hidden) .stack>*{animation:rise .32s var(--ease) both}
section:not(.hidden) .stack>:nth-child(2){animation-delay:.03s}section:not(.hidden) .stack>:nth-child(3){animation-delay:.06s}
section:not(.hidden) .stack>:nth-child(n+4){animation-delay:.09s}
@keyframes rise{from{opacity:0;transform:translateY(8px)}}
.card h2{font-size:20px;line-height:1.2;letter-spacing:-.02em;font-weight:780;margin:0 0 4px}
.card h3{font-size:16px;font-weight:760;margin:18px 0 12px}
.hint{display:block;font-size:14px;line-height:1.45;color:var(--muted);margin:0 0 14px;font-weight:400}
.grow{flex:1;min-width:0}
.ch{display:flex;align-items:flex-start;gap:14px;margin:0 0 16px}.hrow{display:flex;align-items:center;justify-content:space-between;gap:10px;min-height:28px}
.hrow h2{margin:0}.hrow+.hint{margin-top:4px}.ch>div{flex:1;min-width:0}.ch .hint{margin:0}
.ic{flex:none;width:44px;height:44px;border-radius:14px;display:flex;align-items:center;justify-content:center;background:var(--soft);color:var(--ink)}
.ic.lilac{background:var(--lilac)}.ic.sand{background:var(--sand)}.ic.warn{background:var(--warnbg);color:var(--warn)}
.ic svg{width:22px;height:22px;fill:none;stroke:currentColor;stroke-width:1.9;stroke-linecap:round;stroke-linejoin:round}
.tag{flex:none;display:inline-flex;align-items:center;gap:5px;font-size:12px;font-weight:750;padding:6px 10px;border-radius:999px;
background:var(--soft);color:var(--muted);white-space:nowrap}
.tag.ok{background:var(--okbg);color:var(--ok)}.tag.ok::before{content:"";width:6px;height:6px;border-radius:50%;background:currentColor}
.field{display:block;margin:0 0 14px}
.field>span,.field>label>span{display:block;font-size:13px;font-weight:720;margin:0 0 6px}.field i{font-style:normal;font-weight:500;color:var(--muted)}
.input{display:block;width:100%;font:inherit;font-size:16px;color:var(--ink);border:1px solid var(--line);border-radius:14px;padding:12px 14px;
background:var(--paper);min-height:48px;outline:0;-webkit-appearance:none;transition:border-color .15s,box-shadow .15s}
.input:focus{border-color:var(--accent);box-shadow:0 0 0 3px rgba(201,64,42,.14)}
.input::placeholder{color:var(--muted);opacity:.85}
textarea.input{min-height:110px;font:12px/1.4 ui-monospace,Menlo,monospace;resize:vertical}
.pw{position:relative}.pw .input{padding-right:72px}
.peek{position:absolute;right:4px;top:4px;min-height:40px;border:0;background:none;color:var(--warn);font:inherit;font-size:14px;font-weight:700;padding:0 12px;cursor:pointer}
.pair{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.seg{display:flex;gap:4px;padding:4px;border-radius:16px;background:var(--soft);margin:0 0 14px}
.seg label{flex:1;position:relative;display:flex}
.seg input{position:absolute;inset:0;opacity:0;margin:0;cursor:pointer}
.seg span{flex:1;display:flex;align-items:center;justify-content:center;text-align:center;min-height:44px;padding:4px 6px;border-radius:12px;
font-size:14px;font-weight:700;color:var(--muted);transition:background .2s var(--ease),color .2s,box-shadow .2s}
.seg input:checked+span{background:var(--segon);color:var(--ink);box-shadow:0 2px 8px var(--shadow)}
.seg input:focus-visible+span{outline:2px solid var(--accent)}
.row{display:flex;align-items:center;justify-content:space-between;gap:14px}
.toggle{cursor:pointer;align-items:center;margin:0}.toggle b{display:block;font-size:20px;letter-spacing:-.02em;font-weight:780}.toggle .hint{margin:2px 0 0}
#cGithub .toggle{margin:0 0 16px}
.switch{appearance:none;-webkit-appearance:none;flex:none;width:52px;height:32px;margin:0;border-radius:999px;background:var(--line);
position:relative;cursor:pointer;transition:background .2s}
.switch::after{content:"";position:absolute;top:3px;left:3px;width:26px;height:26px;border-radius:50%;background:#fff;
box-shadow:0 1px 3px rgba(0,0,0,.3);transition:transform .25s var(--ease)}
.switch:checked{background:var(--accent)}.switch:checked::after{transform:translateX(20px)}
.stepper{display:flex;align-items:center;gap:12px}
input[type=range]{flex:1;min-width:0;display:block;height:44px;margin:0;accent-color:var(--accent);cursor:pointer}
.btn.round{width:44px;padding:0;font-size:22px;font-weight:500;flex:none}
.big{font-size:24px;font-weight:800;letter-spacing:-.03em;line-height:1;font-variant-numeric:tabular-nums}
.btns{display:flex;flex-wrap:wrap;gap:8px}
.btn:disabled{opacity:.55;cursor:default}
.btn.saved{background:var(--okbg);color:var(--ok);border-color:transparent}
.card.dirty [data-save]{background:var(--accent);color:#fff;border-color:var(--accent)}
.say{font-size:14px;font-weight:650;margin:10px 0 0}.say:empty{display:none}.say.ok{color:var(--ok)}.say.bad{color:var(--warn)}
.net{display:flex;align-items:center;gap:8px;padding:12px 0;border-top:1px solid var(--line)}
.net:last-child{margin-bottom:14px;border-bottom:1px solid var(--line)}
.net div{flex:1;min-width:0}.net b{display:block;overflow-wrap:anywhere}.net small{color:var(--muted);font-size:13px}
.near{display:flex;flex-wrap:wrap;gap:8px;margin:-6px 0 14px}.near .topic{min-height:40px;padding:8px 12px;font-size:13px}.near:empty{display:none}
.btn.danger{color:var(--warn);border-color:var(--warn)}
.btn.wide{width:100%;min-height:56px;font-size:16px;border-radius:20px;box-shadow:0 10px 24px rgba(201,64,42,.22)}
.finish{margin:10px 0 0}.finish .hint{text-align:center;margin:10px 0 0}
/* the cards at the top: the device, and how far setup has got */
.hero{display:flex;align-items:center;gap:18px;background:var(--sand)}
.hero .hint{margin:0 0 10px;color:var(--ink);opacity:.78}.hero .btn{background:var(--surface)}
.mini{flex:none;width:84px;padding:8px 8px 10px;border-radius:18px;background:var(--surface);box-shadow:0 6px 16px var(--shadow)}
.scr{aspect-ratio:1;border-radius:7px;background:#e9e6da;color:#22211f;display:flex;flex-direction:column;align-items:center;justify-content:center;
gap:2px;border:1px solid rgba(0,0,0,.12);transition:background .3s,color .3s}
.scr b{font-size:13px;font-weight:900;letter-spacing:-.05em}.scr span{font-size:10px;font-weight:700;font-variant-numeric:tabular-nums}
.mini.inv .scr{background:#22211f;color:#e9e6da}
.mbtn{display:flex;gap:6px;margin-top:7px}.mbtn i{flex:1;height:7px;border-radius:4px;background:var(--line)}
.bat{height:8px;border-radius:4px;background:var(--track);overflow:hidden;max-width:220px}
.bat i{display:block;height:100%;width:0;border-radius:4px;background:var(--ink);transition:width .6s var(--ease)}
.ring{--p:0;flex:none;width:76px;height:76px;border-radius:50%;display:flex;align-items:center;justify-content:center;
background:conic-gradient(var(--accent) calc(var(--p)*1turn),var(--track) 0);transition:--p .6s}
.ring b{width:58px;height:58px;border-radius:50%;background:var(--sand);display:flex;align-items:center;justify-content:center;
font-size:17px;font-weight:820;letter-spacing:-.02em}
/* finished, and the device out of reach */
#bye{position:fixed;inset:0;z-index:60;background:var(--paper);display:flex;align-items:center;justify-content:center;padding:24px;
animation:fade .3s ease-out}
.byecard{max-width:420px;text-align:center}.byecard .hint{font-size:16px}
.tick{width:84px;height:84px;margin:0 auto 18px;display:block;fill:none;stroke-linecap:round;stroke-linejoin:round}
.tick circle{stroke:var(--okbg);stroke-width:4}.tick path{stroke:var(--ok);stroke-width:4.5;stroke-dasharray:40;stroke-dashoffset:40;animation:draw .5s .2s var(--ease) forwards}
@keyframes draw{to{stroke-dashoffset:0}}@keyframes fade{from{opacity:0}}
#lost{position:fixed;left:12px;right:12px;top:calc(12px + env(safe-area-inset-top));z-index:55;max-width:560px;margin:0 auto;display:flex;
align-items:center;gap:12px;padding:12px 12px 12px 18px;border-radius:20px;background:var(--ink);color:var(--paper);font-size:14px;font-weight:650;
box-shadow:0 14px 34px rgba(0,0,0,.25);animation:rise .3s var(--ease)}
#lost span{flex:1}#lost .btn{flex:none;background:var(--paper);color:var(--ink);border:0}
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
.brand{display:none}.nav{padding:6px}.navlinks{flex:1;justify-content:space-around;gap:4px}
.navlink{flex:1;flex-direction:column;justify-content:center;gap:3px;padding:8px 4px;font-size:12px;border-radius:20px}
.top{display:block;margin-top:34px;padding-top:0}.top>.pill{margin-top:18px}
.grid{grid-template-columns:1fr}.card{border-radius:22px}.title{font-size:21px}
#open{padding:0 14px 120px}}
</style></head><body>
<div class="wrap">
<nav class="nav"><div class="brand">unidex</div><div class="navlinks">
<a class="navlink" id="nNotes" href="#"><svg viewBox="0 0 24 24"><path d="M6 6h14M6 12h14M6 18h14"/><circle cx="3" cy="6" r=".5"/><circle cx="3" cy="12" r=".5"/><circle cx="3" cy="18" r=".5"/></svg><span class="navlabel">Notes</span></a>
<a class="navlink" id="nSettings" href="#settings"><svg viewBox="0 0 24 24"><path d="M4 7h9M19 7h1M4 17h3M13 17h7"/><circle cx="16" cy="7" r="2.5"/><circle cx="10" cy="17" r="2.5"/></svg><span class="navlabel">Settings</span></a>
<a class="navlink" id="nSetup" href="#setup"><svg viewBox="0 0 24 24"><path d="M2.5 9a14 14 0 0119 0M5.5 12.5a9.5 9.5 0 0113 0M8.5 16a5 5 0 017 0"/><circle cx="12" cy="19.5" r=".9"/></svg><span class="navlabel">Setup</span></a>
</div></nav>
<section id="tabNotes">
<div class="top"><div><div class="sub">Voice notes on your unidex</div><h1>Notes.</h1></div><div class="pill" id="count">&nbsp;</div></div>
<div class="toolbar"><div class="find"><input id="q" class="search" type="search" placeholder="Search notes…" aria-label="Search notes" autocomplete="off" autocorrect="off" enterkeyhint="search">
<button id="qc" class="clear hidden" aria-label="Clear search" onclick="setQuery('')">&times;</button></div>
<a class="btn" href="/export.txt" download><svg viewBox="0 0 24 24"><path d="M12 4v11M7 10l5 5 5-5M5 20h14"/></svg>Export</a></div>
<div id="grid" class="grid"></div><div id="empty" class="empty hidden"></div>
</section>
<section id="tabSettings" class="hidden">
<div class="top"><div><div class="sub">How your unidex behaves</div><h1>Settings.</h1></div></div>
<div class="stack">
<div class="card hero"><div class="mini" id="mini" aria-hidden="true"><div class="scr"><b>unidex</b><span id="miniT">--:--</span></div><div class="mbtn"><i></i><i></i></div></div>
<div class="grow"><h2 id="devName">Your unidex</h2><p class="hint" id="devInfo">&nbsp;</p>
<div class="bat" role="img" id="batBox"><i id="batI"></i></div></div></div>
<div class="card"><div class="ch"><i class="ic lilac"><svg viewBox="0 0 24 24"><path d="M20 14.5A8 8 0 019.5 4a8 8 0 1010.5 10.5z"/></svg></i><div><h2>Sleep after</h2><p class="hint">How long it stays on after the last button press.</p></div></div>
<div class="seg" role="radiogroup" aria-label="Sleep after" style="margin:0"><label><input type="radio" name="sleep" value="10"><span>10 s</span></label><label><input type="radio" name="sleep" value="20"><span>20 s</span></label><label><input type="radio" name="sleep" value="30"><span>30 s</span></label><label><input type="radio" name="sleep" value="60"><span>1 min</span></label></div>
<p class="say" id="sleepSay" role="status"></p></div>
<div class="card"><div class="ch"><i class="ic sand"><svg viewBox="0 0 24 24"><path d="M4 9.5h3.5L12 6v12l-4.5-3.5H4z"/><path d="M15.5 9a4.5 4.5 0 010 6M18 6.5a8 8 0 010 11"/></svg></i><div><div class="hrow"><h2>Volume</h2><b class="big" id="volv"></b></div><p class="hint">For playing notes and the reminder chime. It chimes at the new level.</p></div></div>
<div class="stepper"><button class="btn round" id="volDown" aria-label="Quieter">&minus;</button><input type="range" id="vol" min="0" max="10" step="1" aria-label="Volume"><button class="btn round" id="volUp" aria-label="Louder">+</button></div>
<p class="say" id="volSay" role="status"></p></div>
<div class="card"><label class="ch toggle"><i class="ic"><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="8.5"/><path d="M12 3.5a8.5 8.5 0 010 17z" fill="currentColor"/></svg></i><span class="grow"><b>Invert the screen</b><span class="hint">White on black.</span></span><input type="checkbox" class="switch" id="inv"></label>
<p class="say" id="invSay" role="status"></p></div>
<div class="card"><div class="ch"><i class="ic lilac"><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="8.5"/><path d="M12 7.5V12l3 2"/></svg></i><div><div class="hrow"><h2>Date and time</h2><span class="tag" id="timeTag"></span></div><p class="hint" id="clock">&nbsp;</p></div></div>
<button class="btn" id="setTime">Use this phone’s time</button><p class="say" id="timeSay" role="status"></p></div>
<div class="finish"><button class="btn primary wide" onclick="done(this)">Done</button><p class="hint">Closes this page and your unidex’s WiFi.</p></div>
</div></section>
<section id="tabSetup" class="hidden">
<div class="top"><div><div class="sub">WiFi, keys and GitHub</div><h1>Setup.</h1></div></div>
<div class="stack">
<div class="card hero"><div class="ring" id="ring"><b id="ringN">0/3</b></div><div class="grow"><h2 id="heroT">&nbsp;</h2><p class="hint" id="heroH">&nbsp;</p><button class="btn hidden" id="heroGo">Go</button></div></div>
<div class="card" id="cWifi"><div class="ch"><i class="ic lilac"><svg viewBox="0 0 24 24"><path d="M2.5 9a14 14 0 0119 0M5.5 12.5a9.5 9.5 0 0113 0M8.5 16a5 5 0 017 0"/><circle cx="12" cy="19.5" r=".9"/></svg></i><div><div class="hrow"><h2>WiFi</h2><span class="tag" id="wifiTag"></span></div><p class="hint">For the time, notes and reminders. Save up to 5: it joins whichever is in range.</p></div></div>
<div id="nets"></div><button class="btn" id="addNet">+ Add a network</button>
<div id="netForm" class="hidden"><h3 id="netTitle">Add a network</h3>
<div class="seg" role="radiogroup" aria-label="Kind of network"><label><input type="radio" name="kind" value="home"><span>Home or hotspot</span></label><label><input type="radio" name="kind" value="ent"><span>eduroam or work</span></label></div>
<label class="field"><span>Network name</span><input class="input" id="ssid" maxlength="32" autocomplete="off" autocapitalize="off" autocorrect="off" spellcheck="false"></label>
<div class="near" id="near"></div>
<label class="field" id="userBox"><span>Username</span><input class="input" id="wuser" maxlength="128" placeholder="you@uni.ac.uk" autocomplete="off" autocapitalize="off" autocorrect="off" spellcheck="false"></label>
<div class="field"><label for="wpass"><span>Password</span></label><div class="pw"><input class="input" id="wpass" type="password" maxlength="128" autocomplete="off" autocapitalize="off" autocorrect="off" spellcheck="false"><button type="button" class="peek" id="peek">Show</button></div></div>
<label class="field" id="caBox"><span>Certificate <i>(optional, from your IT pages)</i></span><textarea class="input" id="wca" spellcheck="false" placeholder="-----BEGIN CERTIFICATE-----"></textarea></label>
<div class="btns"><button class="btn primary" id="saveNet">Save network</button><button class="btn" id="cancelNet">Cancel</button></div></div>
<p class="say" id="wifiSay" role="status"></p></div>
<div class="card" id="cTranscribe"><div class="ch"><i class="ic sand"><svg viewBox="0 0 24 24"><path d="M3 12h2M7 8v8M11 4.5v15M15 8.5v7M19 11v2"/></svg></i><div><div class="hrow"><h2>Transcription</h2><span class="tag" id="transcribeTag"></span></div><p class="hint">Who writes down what you say.</p></div></div>
<div class="seg" role="radiogroup" aria-label="Transcription"><label><input type="radio" name="transcribe" value="openai"><span>OpenAI Whisper</span></label><label><input type="radio" name="transcribe" value="elevenlabs"><span>ElevenLabs</span></label></div>
<label class="field" id="oaBox"><span>OpenAI key <i>(platform.openai.com)</i></span><input class="input" id="openai_key" type="password" autocomplete="off" autocapitalize="off" spellcheck="false"></label>
<label class="field" id="elBox"><span>ElevenLabs key <i>(free plan: about 30 min a month)</i></span><input class="input" id="elevenlabs_key" type="password" autocomplete="off" autocapitalize="off" spellcheck="false"></label>
<button class="btn" data-save="transcribe">Save</button><p class="say" id="transcribeSay" role="status"></p></div>
<div class="card" id="cCleanup"><div class="ch"><i class="ic lilac"><svg viewBox="0 0 24 24"><path d="M11 3.5l1.7 4.3 4.3 1.7-4.3 1.7L11 15.5l-1.7-4.3L5 9.5l4.3-1.7z"/><path d="M18 14.5l.8 1.9 1.9.8-1.9.8-.8 1.9-.8-1.9-1.9-.8 1.9-.8z"/></svg></i><div><div class="hrow"><h2>Tidy-up</h2><span class="tag" id="cleanupTag"></span></div><p class="hint">Gives each note a title, a summary and topics.</p></div></div>
<div class="seg" role="radiogroup" aria-label="Tidy-up"><label><input type="radio" name="cleanup" value="openai"><span>OpenAI</span></label><label><input type="radio" name="cleanup" value="anthropic"><span>Claude</span></label><label><input type="radio" name="cleanup" value="off"><span>Off</span></label></div>
<label class="field" id="oaBox2"><span>OpenAI key</span><input class="input" id="openai_key2" type="password" autocomplete="off" autocapitalize="off" spellcheck="false"></label>
<label class="field" id="anBox"><span>Anthropic key</span><input class="input" id="anthropic_key" type="password" autocomplete="off" autocapitalize="off" spellcheck="false"></label>
<label class="field" id="modelBox"><span>Model <i>(optional)</i></span><input class="input" id="cleanup_model" maxlength="64" placeholder="The default" autocomplete="off" autocapitalize="off" autocorrect="off" spellcheck="false"></label>
<button class="btn" data-save="cleanup">Save</button><p class="say" id="cleanupSay" role="status"></p></div>
<div class="card" id="cGithub"><label class="ch toggle"><i class="ic sand"><svg viewBox="0 0 24 24"><circle cx="6" cy="5.5" r="2"/><circle cx="6" cy="18.5" r="2"/><circle cx="18" cy="8" r="2"/><path d="M6 7.5v9M18 10c0 4.5-6 3.5-11 7"/></svg></i><span class="grow"><b>Send notes to GitHub</b><span class="hint">Each note as a Markdown file in your repo, for Obsidian.</span></span><input type="checkbox" class="switch" id="gh_on"></label>
<div id="ghBox">
<label class="field"><span>Repo</span><input class="input" id="gh_repo" maxlength="100" placeholder="owner/name" autocomplete="off" autocapitalize="off" autocorrect="off" spellcheck="false"></label>
<div class="pair"><label class="field"><span>Branch</span><input class="input" id="gh_branch" maxlength="64" autocomplete="off" autocapitalize="off" autocorrect="off" spellcheck="false"></label>
<label class="field"><span>Folder</span><input class="input" id="gh_dir" maxlength="64" autocomplete="off" autocapitalize="off" autocorrect="off" spellcheck="false"></label></div>
<label class="field"><span>Token <i>(fine-grained, Contents: read and write)</i></span><input class="input" id="gh_token" type="password" autocomplete="off" autocapitalize="off" spellcheck="false"></label></div>
<button class="btn" data-save="github">Save</button><p class="say" id="githubSay" role="status"></p></div>
<div class="card hidden" id="calCard"><div class="ch"><i class="ic lilac"><svg viewBox="0 0 24 24"><rect x="3.5" y="5" width="17" height="15" rx="3"/><path d="M3.5 10h17M8 3v4M16 3v4"/></svg></i><div><div class="hrow"><h2>Calendar link</h2><span class="tag" id="calendarTag"></span></div><p class="hint">Your calendar’s private address (iCal, .ics or webcal://), so your unidex fetches your timetable itself.</p></div></div>
<label class="field"><span>Link</span><input class="input" id="cal_url" type="password" maxlength="256" autocomplete="off" autocapitalize="off" spellcheck="false"></label>
<button class="btn" data-save="calendar">Save</button><p class="say" id="calendarSay" role="status"></p></div>
<div class="card"><div class="ch"><i class="ic warn"><svg viewBox="0 0 24 24"><path d="M4 7h16M9.5 7V4.5h5V7M6 7l1 12.5h10L18 7"/></svg></i><div><h2>Remove everything</h2><p class="hint">Forgets every WiFi network, key and link on this unidex. Your notes stay.</p></div></div>
<button class="btn danger" id="clearAll">Remove everything</button><p class="say" id="clearSay" role="status"></p></div>
<div class="finish"><button class="btn primary wide" onclick="done(this)">Done</button><p class="hint">Closes this page. If you changed WiFi or keys, your unidex then checks they work and shows how it went.</p></div>
</div></section>
<div id="bye" class="hidden"><div class="byecard"><svg class="tick" viewBox="0 0 52 52" aria-hidden="true"><circle cx="26" cy="26" r="24"/><path d="M15 27l7.5 7.5L37.5 19"/></svg>
<h2 class="title">All done.</h2><p class="hint">Your unidex has closed its WiFi, so this phone goes back to its usual one. If you changed WiFi or keys, it’s checking them now: the results are on its screen.</p></div></div>
<div id="lost" class="hidden" role="alert"><span>Can’t reach your unidex. Is this phone still on its WiFi?</span><button class="btn" onclick="location.reload()">Retry</button></div>
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
<div class="card" id="oa"><div class="eyebrow">Recording</div><div class="rec">
<audio id="au" controls preload="none"></audio>
<a class="btn" id="owav" download aria-label="Download the recording"><svg viewBox="0 0 24 24"><path d="M12 4v11M7 10l5 5 5-5M5 20h14"/></svg></a>
</div></div>
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
<a id="sWav" download><i><svg viewBox="0 0 24 24"><path d="M9 18V6l10-2v12"/><circle cx="6.5" cy="18" r="2.5"/><circle cx="16.5" cy="16" r="2.5"/></svg></i>Recording</a>
<button id="sMore" class="hidden" onclick="nativeShare()"><i><svg viewBox="0 0 24 24"><circle cx="5" cy="12" r="1.3"/><circle cx="12" cy="12" r="1.3"/><circle cx="19" cy="12" r="1.3"/></svg></i>More</button>
</div></div><button class="btn cancel" onclick="closeSheet()">Cancel</button></div>
<div id="toast" role="status" aria-live="polite"></div><textarea id="clip" readonly aria-hidden="true" tabindex="-1"></textarea>
<script>
var NOTES=/*NOTES*/null,TAB=/*TAB*/'notes';
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
$('otxt').href='/note.txt?id='+encodeURIComponent(o.id);$('otxt').classList.toggle('hidden',!!o.p);
var src='/audio?id='+encodeURIComponent(o.id),au=$('au');if(au.getAttribute('src')!=src){au.pause();au.setAttribute('src',src);$('oa').classList.remove('hidden')}
$('owav').href=src+'&dl=1'}
function route(){var h=decodeURIComponent(location.hash.slice(1)),t=h=='settings'||h=='setup'?h:'notes',id=t=='notes'?h:'',box=$('open');
showTab(t);closeSheet();
if(!id){document.body.classList.remove('reading');box.classList.add('gone');cur=null;$('au').pause();return}
var stub=byId[id]||cache[id];if(!stub&&!cache[id]){load(id).then(function(){route()},function(){toast('Couldn’t open that note');history.back()});return}
show(cache[id]||stub,!!cache[id]);box.scrollTop=0;box.classList.remove('gone');document.body.classList.add('reading');
if(!cache[id])load(id).then(function(o){if(location.hash.slice(1)==encodeURIComponent(o.id)||decodeURIComponent(location.hash.slice(1))==o.id)show(o,true)},
function(){toast('Couldn’t load the text. Try again.')})}
function copyNote(){if(!cur)return toast('One moment…');var t=$('clip'),ok=false;t.value=cur.c;t.readOnly=false;t.focus();t.select();t.setSelectionRange(0,t.value.length);
try{ok=document.execCommand('copy')}catch(e){}t.readOnly=true;t.blur();toast(ok?'Copied':'Couldn’t copy here')}
function openSheet(){if(!cur)return toast('One moment…');var e=encodeURIComponent;$('st').textContent=cur.t;$('sMsg').href='sms:&body='+e(cur.c);
$('sMail').href='mailto:?subject='+e(cur.t)+'&body='+e(cur.c);$('sWa').href='whatsapp://send?text='+e(cur.c);
$('sFile').href='/note.txt?id='+e(cur.id);$('sWav').href='/audio?id='+e(cur.id)+'&dl=1';$('sMore').classList.toggle('hidden',!navigator.share);document.body.classList.add('sharing')}
function closeSheet(){document.body.classList.remove('sharing')}
function nativeShare(){navigator.share({title:cur.t,text:cur.c}).catch(function(){});closeSheet()}
document.onkeydown=function(e){if(e.key!='Escape')return;if(document.body.classList.contains('sharing'))closeSheet();else if(document.body.classList.contains('reading'))history.back()};
$('q').oninput=render;window.onhashchange=route;$('au').onerror=function(){if(this.getAttribute('src'))$('oa').classList.add('hidden')};
// --- Settings and Setup: what /setup.json says, saved with a POST each ---
var S=null,asked=0,offset=null,editing=-1,SLOTS=5;
var CHECK='<svg viewBox="0 0 24 24"><path d="M5 12.5l4.5 4.5L19 7.5"/></svg>';
function showTab(t){[['notes','Notes'],['settings','Settings'],['setup','Setup']].forEach(function(x){var on=x[0]==t,a=$('n'+x[1]);
$('tab'+x[1]).classList.toggle('hidden',!on);a.classList.toggle('active',on);if(on)a.setAttribute('aria-current','page');else a.removeAttribute('aria-current')});
document.title=t=='notes'?'unidex':t=='settings'?'Settings · unidex':'Setup · unidex';if(t!='notes'&&!asked)loadSetup()}
function lost(on){$('lost').classList.toggle('hidden',!on)}
function post(path,data){var b=Object.keys(data).map(function(k){return encodeURIComponent(k)+'='+encodeURIComponent(data[k])}).join('&');
return fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})
.catch(function(){lost(true);throw new Error('Can’t reach your unidex. Is this phone still on its WiFi?')})
.then(function(r){lost(false);return r.json().catch(function(){return{}}).then(function(o){if(!r.ok||o.error)throw new Error(o.error||'That didn’t work. Try again.');return o})})}
function tell(id,text,kind){var e=$(id);e.textContent=text||'';e.className='say'+(kind?' '+kind:'')}
function busy(b,on,text){if(on){b.dataset.t=b.innerHTML;b.textContent=text||'Saving…'}else if(b.dataset.t)b.innerHTML=b.dataset.t;b.disabled=on}
// A save that worked: the button says so for a moment (and a short buzz, where the phone does that).
function saved(b){b.classList.add('saved');b.innerHTML=CHECK.replace('<svg','<svg style="width:16px;height:16px;fill:none;stroke:currentColor;stroke-width:2.4"')+'Saved';
b.disabled=false;if(navigator.vibrate)navigator.vibrate(8);clearTimeout(b.st);b.st=setTimeout(function(){b.classList.remove('saved');if(b.dataset.t)b.innerHTML=b.dataset.t},1800)}
function obj(k,v){var o={};o[k]=v;return o}
function C(n){return S&&S.cred[n]||{set:0,v:''}}
function radio(n){var r=document.querySelector('input[name='+n+']:checked');return r?r.value:''}
function setRadio(n,v){var r=document.querySelectorAll('input[name='+n+']');for(var i=0;i<r.length;i++)r[i].checked=r[i].value==String(v)}
function tag(id,on,yes,no){var e=$(id);if(!e)return;e.textContent=on?yes:no;e.classList.toggle('ok',!!on)}
// Remove buttons: the first tap asks, a second within 4 s does it.
function twoTap(b,ask,go){if(b.dataset.armed){delete b.dataset.armed;clearTimeout(b.tm);return go()}
b.dataset.armed=1;b.dataset.t0=b.textContent;b.textContent=ask;b.tm=setTimeout(function(){delete b.dataset.armed;b.textContent=b.dataset.t0},4000)}
function loadSetup(){asked=1;return fetch('/setup.json').then(function(r){return r.json()}).then(function(o){var first=!S;S=o;lost(false);drawSettings();drawSetup();if(first)fixTime()},
function(){lost(true)})}
// Settings: each change is saved as it's made.
function drawSettings(){setRadio('sleep',S.sleep);$('vol').max=S.volumeMax;$('vol').value=S.volume;$('volv').textContent=S.volume?S.volume:'Off';
$('inv').checked=!!S.invert;$('mini').classList.toggle('inv',!!S.invert);$('devName').textContent=S.name||'Your unidex';
$('devInfo').textContent=(S.battery>=0?(S.charging?'Charging, ':'Battery ')+S.battery+'%':'On USB')+' · '+S.version.replace(/-g[0-9a-f]+/,'').replace('-dirty','');
$('batI').style.width=(S.battery>=0?S.battery:100)+'%';$('batBox').setAttribute('aria-label',S.battery>=0?'Battery '+S.battery+'%':'No battery');clock()}
function saveSetting(k,v,say){post('/settings',obj(k,v)).then(function(){S[k]=+v;tell(say,'');drawSettings();toast('Saved')},function(e){tell(say,e.message,'bad');drawSettings()})}
document.querySelectorAll('input[name=sleep]').forEach(function(r){r.onchange=function(){saveSetting('sleep',r.value,'sleepSay')}});
function vol(v){v=Math.max(0,Math.min(+$('vol').max,v));$('vol').value=v;$('volv').textContent=v?v:'Off';if(v!=S.volume)saveSetting('volume',v,'volSay')}
$('vol').oninput=function(){$('volv').textContent=+this.value?this.value:'Off'};$('vol').onchange=function(){vol(+this.value)};
$('volDown').onclick=function(){vol(+$('vol').value-1)};$('volUp').onclick=function(){vol(+$('vol').value+1)};
$('inv').onchange=function(){$('mini').classList.toggle('inv',this.checked);saveSetting('invert',this.checked?1:0,'invSay')};
// The device's clock: set from this phone's when it's unset or more than a minute out.
function clock(){var t=new Date(Date.now()+(offset||0)*1000);$('clock').textContent=offset==null?'Not set yet':t.toLocaleString(undefined,
{weekday:'long',day:'numeric',month:'long',hour:'2-digit',minute:'2-digit'});$('miniT').textContent=offset==null?'--:--':
('0'+t.getHours()).slice(-2)+':'+('0'+t.getMinutes()).slice(-2);tag('timeTag',offset!=null&&Math.abs(offset)<60,'Right','Not set')}
function setTime(auto){var b=$('setTime');busy(b,true,'Setting…');post('/time',{t:Math.round(Date.now()/1000)}).then(function(){offset=0;clock();
if(auto)tell('timeSay','Set from this phone’s clock.','ok');busy(b,false);if(!auto)saved(b)},function(e){busy(b,false);tell('timeSay',e.message,'bad')})}
function fixTime(){offset=S.time?S.time-Date.now()/1000:null;clock();if(offset==null||Math.abs(offset)>60)setTime(true)}
$('setTime').onclick=function(){setTime(false)};setInterval(function(){if(S)clock()},15000);
// WiFi: network n is wifi_* (the first) or wifi<n+1>_*; a name saved means a network.
function key(n,part){return n?'wifi'+(n+1)+'_'+part:'wifi_'+part}
function nets(){var a=[];for(var i=0;i<SLOTS;i++)if(C(key(i,'ssid')).set)a.push({slot:i,ssid:C(key(i,'ssid')).v,user:C(key(i,'user')).v,ca:C(key(i,'ca')).set});return a}
// How far setup has got: WiFi, who writes notes down (with its key), and the tidy-up (off counts).
function steps(){var t=C('transcribe').v||'openai',c=C('cleanup').v||'openai';
return[{done:nets().length>0,card:'cWifi',next:'Add your WiFi so it can set its clock and send notes.'},
{done:C(t=='elevenlabs'?'elevenlabs_key':'openai_key').set,card:'cTranscribe',next:'Add a key so your voice notes get written down.'},
{done:c=='off'||C(c=='anthropic'?'anthropic_key':'openai_key').set,card:'cCleanup',next:'Add a key for the tidy-up, or turn it off.'}]}
function drawSetup(){var box=$('nets'),a=nets();box.textContent='';
a.forEach(function(n){var r=el('div','net'),t=el('div');t.appendChild(el('b','',n.ssid));t.appendChild(el('small','',n.user?'eduroam · '+n.user:'Home or hotspot'));r.appendChild(t);
var e=el('button','btn','Edit');e.onclick=function(){editNet(n.slot)};var x=el('button','btn','Remove');x.onclick=function(){twoTap(x,'Sure?',function(){removeNet(n,x)})};
r.appendChild(e);r.appendChild(x);box.appendChild(r)});
if(!a.length&&editing==-1)editNet(-1,1);
$('addNet').classList.toggle('hidden',a.length>=SLOTS||editing!=-1);
['openai_key','openai_key2','elevenlabs_key','anthropic_key','gh_token','cal_url'].forEach(function(n){var c=C(n.replace('2',''));$(n).value='';
$(n).placeholder=c.set?'Saved, ends in '+c.v+'. Blank keeps it':'Paste it here'});
setRadio('transcribe',C('transcribe').v||'openai');setRadio('cleanup',C('cleanup').v||'openai');$('cleanup_model').value=C('cleanup_model').v;
$('gh_on').checked=C('gh_on').v=='1';$('gh_repo').value=C('gh_repo').v;$('gh_branch').value=C('gh_branch').v||'main';$('gh_dir').value=C('gh_dir').v||'VoiceNotes';
$('calCard').classList.toggle('hidden',!('cal_url' in S.cred));
var near=$('near');near.textContent='';(S.nearby||[]).filter(function(n){return!a.some(function(x){return x.ssid==n})}).slice(0,8).forEach(function(n){
var b=el('button','topic',n);b.type='button';b.onclick=function(){$('ssid').value=n;$('wpass').focus()};near.appendChild(b)});
var st=steps(),n=st.filter(function(x){return x.done}).length,next=st.filter(function(x){return!x.done})[0];
$('ring').style.setProperty('--p',n/st.length);$('ringN').textContent=n+'/'+st.length;
$('heroT').textContent=n==st.length?'All set up.':n?'Almost there.':'Let’s get it online.';
$('heroH').textContent=next?next.next:'Your unidex has WiFi and its keys. Press Done to check they work.';
$('heroGo').classList.toggle('hidden',!next);$('heroGo').onclick=function(){$(next.card).scrollIntoView({behavior:'smooth',block:'start'})};
tag('wifiTag',a.length,a.length+' saved','None yet');tag('transcribeTag',st[1].done,'Ready','Needs a key');tag('cleanupTag',st[2].done,C('cleanup').v=='off'?'Off':'Ready','Needs a key');
tag('calendarTag',C('cal_url').set,'Linked','Not linked');
document.querySelectorAll('.card.dirty').forEach(function(c){c.classList.remove('dirty')});choices()}
function choices(){var t=radio('transcribe'),c=radio('cleanup');$('oaBox').classList.toggle('hidden',t!='openai');$('elBox').classList.toggle('hidden',t!='elevenlabs');
$('oaBox2').classList.toggle('hidden',c!='openai'||t=='openai');$('anBox').classList.toggle('hidden',c!='anthropic');
$('modelBox').classList.toggle('hidden',c=='off');$('ghBox').classList.toggle('hidden',!$('gh_on').checked)}
document.querySelectorAll('input[name=transcribe],input[name=cleanup],#gh_on').forEach(function(r){r.addEventListener('change',choices)});
// A card with a change not saved yet: its Save lights up.
document.querySelectorAll('[data-save]').forEach(function(b){var card=b.closest('.card');['input','change'].forEach(function(ev){
card.addEventListener(ev,function(){card.classList.add('dirty')})})});
function kind(){var e=radio('kind')=='ent';$('userBox').classList.toggle('hidden',!e);$('caBox').classList.toggle('hidden',!e)}
document.querySelectorAll('input[name=kind]').forEach(function(r){r.onchange=kind});
function editNet(slot,quiet){var n=nets().filter(function(x){return x.slot==slot})[0];editing=n?slot:-2;
setRadio('kind',n&&n.user?'ent':'home');$('ssid').value=n?n.ssid:'';$('wuser').value=n?n.user:'';$('wpass').value='';$('wca').value='';
$('wpass').placeholder=n?'Saved. Blank keeps it':'';$('wca').placeholder=n&&n.ca?'One is saved. Paste another to replace it':'-----BEGIN CERTIFICATE-----';
$('netTitle').textContent=n?'Edit “'+n.ssid+'”':nets().length?'Add a network':'Your first network';$('near').classList.toggle('hidden',!!n);kind();
$('netForm').classList.remove('hidden');$('addNet').classList.add('hidden');tell('wifiSay','');$('cancelNet').classList.toggle('hidden',!nets().length);
if(!quiet)$(n?'wpass':'ssid').focus()}
function closeNet(){editing=-1;$('netForm').classList.add('hidden');drawSetup()}
$('addNet').onclick=function(){editNet(-1)};$('cancelNet').onclick=function(){closeNet();tell('wifiSay','')};
$('peek').onclick=function(){var p=$('wpass'),show=p.type=='password';p.type=show?'text':'password';this.textContent=show?'Hide':'Show'};
function caBody(){var m=$('wca').value.match(/-----BEGIN CERTIFICATE-----([\s\S]*?)-----END CERTIFICATE-----/),b=m&&m[1].replace(/\s+/g,'');
return b&&/^[A-Za-z0-9+\/]+=*$/.test(b)&&b.length<=3000?b:null}
$('saveNet').onclick=function(){var b=this,ent=radio('kind')=='ent',ssid=$('ssid').value.trim(),a=nets(),slot=editing,v={};
if(ent&&$('wca').value.trim()&&!caBody())return tell('wifiSay','The certificate should be the text from -----BEGIN CERTIFICATE----- to -----END CERTIFICATE-----.','bad');
if(!ssid){tell('wifiSay','Type the network’s name.','bad');return $('ssid').focus()}
if(ent&&!$('wuser').value.trim()){tell('wifiSay','Type your username.','bad');return $('wuser').focus()}
if(a.some(function(n){return n.ssid==ssid&&n.slot!=editing}))return tell('wifiSay','“'+ssid+'” is saved already. Edit that one instead.','bad');
if(slot<0){for(slot=0;slot<SLOTS&&a.some(function(n){return n.slot==slot});slot++);if(slot>=SLOTS)return tell('wifiSay',SLOTS+' networks are saved already. Remove one first.','bad')}
v[key(slot,'ssid')]=ssid;v[key(slot,'user')]=ent?$('wuser').value.trim():'';
if($('wpass').value)v[key(slot,'pass')]=$('wpass').value;else if(editing<0)v[key(slot,'pass')]='';
if(ent&&caBody())v[key(slot,'ca')]=caBody();if(!ent)v[key(slot,'ca')]='';
busy(b,true);post('/set',v).then(function(){editing=-1;$('netForm').classList.add('hidden');return loadSetup()})
.then(function(){busy(b,false);tell('wifiSay','Saved “'+ssid+'”. Your unidex tries it when you press Done.','ok');if(navigator.vibrate)navigator.vibrate(8)},
function(e){busy(b,false);tell('wifiSay',e.message,'bad')})};
function removeNet(n,b){var v={};['ssid','pass','user','ca'].forEach(function(p){v[key(n.slot,p)]=''});busy(b,true,'Removing…');
post('/set',v).then(loadSetup).then(function(){tell('wifiSay','Removed “'+n.ssid+'”.','ok')},function(e){busy(b,false);tell('wifiSay',e.message,'bad')})}
// Keys and GitHub: which fields each Save sends. Keys only when typed (blank keeps the saved one).
function typed(id){return $(id).value.trim()}
var GROUPS={transcribe:function(){var t=radio('transcribe'),v={transcribe:t},k=t=='openai'?'openai_key':'elevenlabs_key';if(typed(k))v[k]=typed(k);return v},
cleanup:function(){var c=radio('cleanup'),v={cleanup:c};if(c!='off')v.cleanup_model=typed('cleanup_model');
if(c=='anthropic'&&typed('anthropic_key'))v.anthropic_key=typed('anthropic_key');if(c=='openai'&&typed('openai_key2'))v.openai_key=typed('openai_key2');return v},
github:function(){var v={gh_on:$('gh_on').checked?'1':'0',gh_repo:typed('gh_repo'),gh_branch:typed('gh_branch'),gh_dir:typed('gh_dir')};
if(typed('gh_token'))v.gh_token=typed('gh_token');return v},
calendar:function(){return typed('cal_url')?{cal_url:typed('cal_url')}:{}}};
function check(g){var k=typed('openai_key')||typed('openai_key2');
if((g=='transcribe'||g=='cleanup')&&k&&k.indexOf('sk-'))return'OpenAI keys start with sk-.';
if(g=='cleanup'&&/^sk-/.test(typed('cleanup_model')))return'That looks like a key. This box takes a model name, like gpt-4o-mini.';
if(g=='github'&&typed('gh_repo')&&!/^[\w.-]+\/[\w.-]+$/.test(typed('gh_repo')))return'The repo should look like owner/name.';
if(g=='github'&&$('gh_on').checked&&!typed('gh_repo'))return'Type the repo first.';
if(g=='calendar'&&typed('cal_url')&&!/^(webcals?|https?):\/\/\S+$/i.test(typed('cal_url')))return'The link should start with https:// or webcal://.';return''}
document.querySelectorAll('[data-save]').forEach(function(b){b.onclick=function(){var g=b.dataset.save,say=g+'Say',p=check(g),v=GROUPS[g]();
if(p)return tell(say,p,'bad');if(!Object.keys(v).length)return tell(say,'Nothing to save yet. Type it in first.','bad');
busy(b,true);post('/set',v).then(loadSetup).then(function(){tell(say,'');saved(b)},function(e){busy(b,false);tell(say,e.message,'bad')})}});
$('clearAll').onclick=function(){var b=this;twoTap(b,'Tap again to remove it all',function(){busy(b,true,'Removing…');
post('/clear',{}).then(loadSetup).then(function(){busy(b,false);tell('clearSay','Removed. WiFi and keys are gone from this unidex.','ok')},function(e){busy(b,false);tell('clearSay',e.message,'bad')})})};
function done(b){busy(b,true,'Closing…');post('/done',{}).then(function(){$('bye').classList.remove('hidden')},function(e){busy(b,false);toast(e.message)})}
if(!location.hash&&TAB!='notes')history.replaceState(null,'','#'+TAB);
loadSetup();
function start(d){notes=d;byId={};notes.forEach(function(o){byId[o.id]=o});render();route();setTimeout(prefetch,250)}
if(NOTES)start(NOTES);
else fetch('/notes.json').then(function(r){return r.json()}).then(start)
.catch(function(){$('empty').classList.remove('hidden');$('empty').textContent='Couldn’t load the notes. Reload the page to try again.'});
</script></body></html>)PAGE";
