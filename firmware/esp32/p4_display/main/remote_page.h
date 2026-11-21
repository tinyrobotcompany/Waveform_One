#pragma once

inline constexpr char kRemotePage[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Waveform One · Now playing</title><style>
@property --bg{syntax:'<color>';inherits:true;initial-value:#080d0a}
@property --panel{syntax:'<color>';inherits:true;initial-value:#121a13}
:root{transition:--bg .7s,--panel .7s;color-scheme:dark;font-family:Arial,sans-serif;--bg:#080d0a;--panel:#121a13;--line:#374838;--ink:#edf3e8;--ink2:#edf3e8;--muted:#a7b2a7;--muted2:#a7b2a7;--green:#a8d57a;--lavender:#a79ae8}
.field{position:fixed;inset:0;z-index:-1}.field canvas{position:absolute;inset:0;width:100%;height:100%;transition:opacity .7s}
*{box-sizing:border-box}[hidden]{display:none!important}body{margin:0;background:var(--bg);color:var(--ink);min-height:100svh;padding:env(safe-area-inset-top) 22px env(safe-area-inset-bottom)}
button,input{font:inherit}button{min-height:48px;border:1px solid var(--line);border-radius:12px;background:var(--panel);color:var(--ink);padding:10px 16px;cursor:pointer}button:disabled{opacity:.5;cursor:default}button:focus-visible,input:focus-visible{outline:2px solid var(--lavender);outline-offset:4px}
header{max-width:900px;margin:auto;min-height:84px;display:flex;align-items:center;justify-content:space-between;gap:12px}.brand{font-size:12px;letter-spacing:2px;color:var(--green);font-weight:bold}.settings-button{background:transparent;font-size:14px}
main{max-width:900px;margin:18px auto 40px}.player{display:grid;grid-template-columns:minmax(0,1fr);gap:28px}.cover{position:relative;aspect-ratio:1;overflow:hidden;border:2px solid var(--line);border-radius:20px;background:var(--panel);display:grid;place-items:center}.cover.known{border-color:var(--green)}.cover.searching:not(.known){border-color:var(--lavender)}
.cover img{position:absolute;width:100%;height:100%;object-fit:cover}.placeholder{text-align:center;color:var(--muted);font-size:clamp(34px,9vw,64px);letter-spacing:1px;line-height:1.2}.wave{display:flex;justify-content:center;align-items:center;gap:7px;height:50px;margin-top:26px}.wave i{display:block;width:7px;height:12px;border-radius:8px;background:var(--lavender);animation:wave 1.3s ease-in-out infinite alternate;animation-delay:calc(var(--i)*-.12s)}@keyframes wave{to{height:48px;opacity:.4}}
.eyebrow{margin:0 0 14px;color:var(--green);font-size:16px;letter-spacing:3px;font-weight:bold}h1{font-size:clamp(30px,7vw,48px);font-weight:500;line-height:1.14;margin:0 0 16px;overflow-wrap:anywhere}.artist{font-size:23px;line-height:1.35;margin:0 0 8px;overflow-wrap:anywhere;color:var(--ink2)}.album{font-size:17px;color:var(--muted);line-height:1.5;margin:0;overflow-wrap:anywhere}.activity{color:var(--muted2);font-size:14px;margin:24px 0 0;min-height:20px}.dot{display:inline-block;background:var(--green);width:6px;height:6px;border-radius:50%;margin-right:8px}.activity.searching .dot{background:var(--lavender)}
.connection{font-size:13px;color:var(--muted);margin:24px 0 0;line-height:1.5}.connection:empty{display:none}.overlay{position:fixed;inset:0;background:#0009;z-index:2;display:grid;place-items:center;padding:20px}.sheet{width:100%;max-width:520px;max-height:calc(100svh - 40px);overflow:auto;background:var(--panel);border:1px solid var(--line);border-radius:22px;padding:24px}.sheet-heading{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:24px}.sheet-heading h2{font-size:28px;margin:0}.section{border-top:1px solid var(--line);padding:24px 0 0;margin:24px 0 0}.section h3{font-size:13px;letter-spacing:2px;color:var(--green);margin:0 0 16px}.styles{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px}.styles button{padding:10px 4px;font-size:14px}.styles button[aria-pressed=true]{border-color:var(--green);color:var(--green)}label{display:block;font-size:15px;color:var(--muted)}input[type=text]{width:100%;margin:10px 0 12px;padding:13px;border:1px solid var(--line);border-radius:9px;background:var(--bg);color:var(--ink);font-size:18px}input[type=range]{width:100%;height:44px;accent-color:var(--green)}.save{width:100%}.feedback{font-size:14px;color:var(--green);min-height:20px;margin:20px 0 0;line-height:1.5}
@media(min-width:700px){main{margin-top:44px}.player{grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:42px;align-items:center}.eyebrow{font-size:20px}.connection{margin-top:32px}}
body,.cover,.dot,.eyebrow,.brand,h1,.artist,.album,.activity,button{transition:background-color .7s,border-color .7s,color .7s}
@media(prefers-reduced-motion:reduce){:root,body,.cover,.dot,.eyebrow,.brand,h1,.artist,.album,.activity,button,.field canvas{transition:none}.wave i{animation:none;height:28px}}
#controls:disabled #display-brightness{display:none}
</style></head><body>
<div class="field" aria-hidden="true"><canvas id="field-a" width="8" height="5"></canvas><canvas id="field-b" width="8" height="5" style="opacity:0"></canvas></div>
<header><span class="brand">WAVEFORM ONE</span><button class="settings-button" id="open-settings" aria-controls="settings" aria-expanded="false">Settings</button></header>
<main><div class="player"><div class="cover" id="cover"><div class="placeholder" id="placeholder">WAVEFORM<br>ONE<div class="wave" id="wave" hidden aria-hidden="true"><i style="--i:0"></i><i style="--i:1"></i><i style="--i:2"></i><i style="--i:3"></i><i style="--i:4"></i><i style="--i:5"></i><i style="--i:6"></i><i style="--i:7"></i><i style="--i:8"></i></div></div><img id="artwork" alt="" hidden referrerpolicy="no-referrer"></div>
<div class="metadata"><p class="eyebrow" id="eyebrow">YOUR MUSIC</p><h1 id="title">Connecting…</h1><p class="artist" id="artist"></p><p class="album" id="album"></p><p class="activity" id="activity"><span class="dot"></span><span id="activity-text">Connecting to Waveform One</span></p></div></div><p class="connection" id="connection" role="status"></p></main>
<div class="overlay" id="settings" hidden><section class="sheet" role="dialog" aria-modal="true" aria-labelledby="settings-title"><div class="sheet-heading"><h2 id="settings-title">Settings</h2><button id="close-settings">Done</button></div>
<fieldset id="controls" disabled style="border:0;padding:0;margin:0;min-width:0"><label for="name">Your name</label><input id="name" type="text" maxlength="40" autocomplete="given-name"><button class="save" id="save-name">Save name</button>
<div class="section"><h3>LED STYLE</h3><div class="styles"><button data-style="classic" aria-pressed="false">Classic</button><button data-style="mirrored" aria-pressed="false">Mirrored</button><button data-style="waterfall" aria-pressed="false">Waterfall</button></div></div>
<div class="section"><h3>APPEARANCE</h3><div class="styles"><button data-appearance="album" aria-pressed="true">Album colours</button><button data-appearance="classic" aria-pressed="false">Classic black</button></div></div>
<div class="section"><h3>DISPLAY</h3><label for="brightness">Brightness <output id="brightness-value">100%</output></label><input id="brightness" type="range" min="10" max="100" value="100"><p id="display-brightness" role="status"></p></div>
<div class="section"><h3>SOFTWARE UPDATES</h3>
<p id="update-version"></p><p id="update-result" role="status" hidden></p><p id="update-message" role="status">Checking update status…</p>
<progress id="update-progress" max="100" value="0" aria-label="Download progress"></progress>
<p><a id="update-notes" target="_blank" rel="noopener noreferrer" hidden>What’s new</a></p>
<div class="styles"><button data-update="check">Check now</button><button data-update="install">Install now</button><button data-update="postpone">Postpone 24h</button></div>
<p><label><input id="update-auto" type="checkbox"> Install automatically</label></p>
<label>Check frequency <select id="update-hours"><option value="720">Every 30 days</option><option value="1">Hourly (testing)</option></select></label>
<p><label>Installation window starts <input id="update-start" type="time" value="03:00"></label><label>Ends <input id="update-end" type="time" value="04:00"></label></p>
<button id="save-updates">Save update settings</button><p id="update-next"></p>
</div></fieldset><p class="feedback" id="feedback" role="status"></p></section></div>
<script>
const $=id=>document.getElementById(id);
let updateSettingsLoaded=false,updateSettingsDirty=false,activeUpdate=false;
let csrf='',pollTimer,artworkUrl='',requestActive=false,settingsOpen=false,failures=0,fieldKey='',fieldFront=false;
function pairedMessage(){return 'Scan the QR code on Waveform One to connect.'}
function loseSession(){activeUpdate=false;csrf='';$('controls').disabled=true;updateSettingsLoaded=false;$('connection').textContent=pairedMessage();$('feedback').textContent=pairedMessage();$('activity-text').textContent='Connection required';$('update-version').textContent='';$('update-message').textContent='Not connected. Scan the device QR code to see live update status.';$('update-next').textContent='';$('update-result').hidden=true;$('update-progress').value=0;$('update-notes').hidden=true}
function reconnecting(){$('connection').textContent='Reconnecting to Waveform One…';$('update-message').textContent='Update status unavailable. Reconnecting…'}
function showSettings(open){settingsOpen=open;$('settings').hidden=!open;$('open-settings').setAttribute('aria-expanded',String(open));document.body.style.overflow=open?'hidden':'';(open?$('close-settings'):$('open-settings')).focus()}
$('open-settings').onclick=()=>showSettings(true);$('close-settings').onclick=()=>showSettings(false);
$('settings').onclick=e=>{if(e.target===$('settings'))showSettings(false)};
document.addEventListener('keydown',e=>{if(!settingsOpen)return;if(e.key==='Escape')showSettings(false);if(e.key==='Tab'){const nodes=[...$('settings').querySelectorAll('button,input')].filter(n=>!n.disabled&&!n.closest('fieldset[disabled]'));const first=nodes[0],last=nodes[nodes.length-1];if(e.shiftKey&&document.activeElement===first){e.preventDefault();last.focus()}else if(!e.shiftKey&&document.activeElement===last){e.preventDefault();first.focus()}}});
function showPlaceholder(){$('artwork').hidden=true;$('placeholder').hidden=false}
$('artwork').onload=()=>{$('artwork').hidden=false;$('placeholder').hidden=true};$('artwork').onerror=showPlaceholder;
// The album's colour field: 8 x 5 cells the browser smooths when it scales them up.
// The new field is drawn on the hidden canvas, which then fades in over the other.
function showField(cells){const key=cells.join();if(key===fieldKey)return;fieldKey=key;const [shown,hidden]=fieldFront?[$('field-b'),$('field-a')]:[$('field-a'),$('field-b')];const context=hidden.getContext('2d');cells.forEach((cell,i)=>{context.fillStyle=cell;context.fillRect(i%8,Math.floor(i/8),1,1)});hidden.style.opacity='1';shown.style.opacity='0';fieldFront=!fieldFront}
function render(state){
 const keys={background:'--bg',panel:'--panel',line:'--line',text:'--ink',secondary:'--ink2',tertiary:'--muted',quaternary:'--muted2',accent:'--green',search:'--lavender'};
 const defaults={background:'#080d0a',panel:'#121a13',line:'#374838',text:'#edf3e8',secondary:'#edf3e8',tertiary:'#a7b2a7',quaternary:'#a7b2a7',accent:'#a8d57a',search:'#a79ae8'};
 const colour=value=>/^#[0-9a-f]{6}$/i.test(value||'');
 for(const [key,css] of Object.entries(keys)){const value=state.palette?.[key];document.documentElement.style.setProperty(css,colour(value)?value:defaults[key])}
 document.documentElement.style.colorScheme=state.palette?.light===true?'light':'dark';
 const field=Array.isArray(state.palette?.field)&&state.palette.field.length===40&&state.palette.field.every(colour)?state.palette.field:Array(40).fill(colour(state.palette?.background)?state.palette.background:defaults.background);
 showField(field);
 document.querySelectorAll('[data-appearance]').forEach(button=>button.setAttribute('aria-pressed',String(button.dataset.appearance===(state.appearance||'album'))));
 const track=state.status==='muted'?null:state.track,searching=['capturing','identifying'].includes(state.status);$('cover').classList.toggle('known',!!track);$('cover').classList.toggle('searching',searching);$('activity').classList.toggle('searching',searching);$('wave').hidden=!searching;
 $('eyebrow').textContent=track?'NOW PLAYING':'YOUR MUSIC';$('title').textContent=track?track.title:'Listening for music';$('artist').textContent=track?track.artist:'';$('album').textContent=track?track.album:'';
 $('activity-text').textContent=state.status==='muted'?'Microphone muted':searching?(track?'Checking what’s playing…':'Finding your song…'):state.status==='configuration_required'?'ACRCloud configuration required':state.status==='unavailable'?'Trying again shortly':track?'Enjoy the music':'Play a song to get started';
 const url=track&&track.artwork||'';if(url!==artworkUrl){artworkUrl=url;showPlaceholder();if(url){$('artwork').alt=track.album?track.album+' album artwork':'Album artwork';$('artwork').src=url}else{$('artwork').removeAttribute('src');$('artwork').alt=''}}
 if(!settingsOpen){$('name').value=state.name;$('brightness').value=state.brightness;$('brightness-value').textContent=state.brightness+'%'}
 const actual=state.display_brightness??state.brightness;$('display-brightness').textContent=actual<state.brightness?'Automatic dimming: screen '+actual+'% (selected '+state.brightness+'%)':'';
 document.querySelectorAll('[data-style]').forEach(button=>button.setAttribute('aria-pressed',String(button.dataset.style===state.style)));
}
async function post(path,data){if(!csrf){loseSession();return false}try{const response=await fetch(path,{method:'POST',credentials:'same-origin',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Waveform-CSRF':csrf},body:new URLSearchParams(data)});if(response.status===403){loseSession();return false}if(!response.ok)throw Error();$('feedback').textContent=path==='/api/style'?'Applying LED style…':'Saved';return true}catch{$('feedback').textContent='Could not save. Please try again.';return false}}
$('save-name').onclick=()=>{const name=$('name').value.trim();if(name)post('/api/name',{name});else $('feedback').textContent='Enter your name first.'};document.querySelectorAll('[data-style]').forEach(button=>button.onclick=()=>post('/api/style',{style:button.dataset.style}));
document.querySelectorAll('[data-appearance]').forEach(button=>button.onclick=async()=>{if(await post('/api/appearance',{value:button.dataset.appearance}))poll()});
// Commit a slider change once; dragging never creates overlapping device requests.
$('brightness').oninput=()=>{$('brightness-value').textContent=$('brightness').value+'%'};$('brightness').onchange=()=>post('/api/brightness',{value:$('brightness').value});
function renderUpdates(value){
 activeUpdate=value.countdown_seconds>0||[1,3,4,5].includes(value.state);
 $('update-version').textContent='Installed: '+value.installed+(value.available?' · Available: '+value.available:'');
 $('update-message').textContent=value.message;
 $('update-result').textContent=value.last_install_message||'';$('update-result').hidden=!value.last_install_message;
 $('update-progress').value=value.progress;
 const notes=$('update-notes');
 const safe=/^https:\/\/github\.com\/tinyrobotcompany\/Waveform_One\/releases\/tag\/v[0-9]+\.[0-9]+\.[0-9]+$/.test(value.notes_url||'');
 notes.hidden=!safe;if(safe)notes.href=value.notes_url;else notes.removeAttribute('href');
 $('update-next').textContent=value.next_check?'Next check: '+new Date(value.next_check*1000).toLocaleString():'';
 if(!updateSettingsLoaded||!updateSettingsDirty){
  const clock=n=>String(Math.floor(n/60)).padStart(2,'0')+':'+String(n%60).padStart(2,'0');
  $('update-auto').checked=value.automatic;$('update-hours').value=String(value.interval_hours);
  $('update-start').value=clock(value.window_start);$('update-end').value=clock(value.window_end);updateSettingsLoaded=true;
 }
 const busy=[1,3,4,5].includes(value.state);
 document.querySelector('[data-update="install"]').disabled=!value.available||busy;
 document.querySelector('[data-update="check"]').disabled=busy||value.countdown_seconds>0;
 document.querySelector('[data-update="postpone"]').disabled=busy;
}
async function pollUpdates(){
 const response=await fetch('/api/updates',{credentials:'same-origin',cache:'no-store',signal:AbortSignal.timeout(8000)});
 if(response.status===401){loseSession();return}if(!response.ok)throw Error();const value=await response.json();if(csrf)renderUpdates(value);
}
for(const id of ['update-auto','update-hours','update-start','update-end'])$(id).addEventListener('change',()=>{updateSettingsDirty=true});
document.querySelectorAll('[data-update]').forEach(button=>button.onclick=async()=>{if(await post('/api/updates/action',{action:button.dataset.update})){$('feedback').textContent='Request queued…';poll()}});
$('save-updates').onclick=async()=>{
 const minutes=id=>{const parts=$(id).value.split(':').map(Number);return parts.length===2?parts[0]*60+parts[1]:NaN};
 const start=minutes('update-start'),end=minutes('update-end');
 if(!Number.isInteger(start)||!Number.isInteger(end)||start===end){$('feedback').textContent='Choose different start and end times.';return}
 if(await post('/api/updates/config',{value:[$('update-hours').value,$('update-auto').checked?1:0,start,end].join(',')})){updateSettingsDirty=false;$('feedback').textContent='Update settings queued…';poll()}
};
async function connect(){
 const response=await fetch('/api/session',{credentials:'same-origin',cache:'no-store',signal:AbortSignal.timeout(8000)});
 if(response.status===401){loseSession();return false}if(!response.ok)throw Error();
 $('display-brightness').textContent='';
 csrf=await response.text();$('controls').disabled=false;$('feedback').textContent='';await post('/api/time',{epoch:Math.floor(Date.now()/1000)});return !!csrf;
}
async function poll(){
 clearTimeout(pollTimer);if(requestActive||document.hidden)return;requestActive=true;
 try{
  if(!csrf&&!await connect())return;
  // Both views read the same device worker; a metadata error must not stop OTA status.
  const results=await Promise.allSettled([
   (async()=>{const response=await fetch('/api/state',{credentials:'same-origin',cache:'no-store',signal:AbortSignal.timeout(8000)});
    if(response.status===401){loseSession();return}if(!response.ok)throw Error();const state=await response.json();if(csrf)render(state)})(),
   pollUpdates()
  ]);
  if(!csrf){loseSession();return}
  if(results.some(result=>result.status==='rejected')){
   failures++;$('connection').textContent='Reconnecting to Waveform One…';
   if(results[1].status==='rejected')$('update-message').textContent='Update status unavailable. Reconnecting…';
  }else{failures=0;$('connection').textContent=''}
 }catch{failures++;reconnecting()}
 finally{requestActive=false;if(!document.hidden)pollTimer=setTimeout(poll,failures?Math.min(10000,2000*(failures+1)):(activeUpdate?1000:2000))}
}
document.addEventListener('visibilitychange',()=>{if(!document.hidden)poll();else clearTimeout(pollTimer)});
// History is explicitly enabled per boot and is never sent to another service here.
const historySection=document.createElement('div');historySection.className='section';
historySection.innerHTML='<h3>LISTENING HISTORY</h3><p>Optional: last 50 confirmed tracks, in device memory until restart. No audio. Disabling deletes this session.</p><button id="history-refresh">View / refresh</button> <button id="history-toggle">Enable for this session</button> <button id="history-clear">Delete history</button> <button id="history-export">Export JSON</button><p id="history-status" role="status"></p><ol id="history-list"></ol>';
$('controls').append(historySection);
let historyData=null,historyBusy=false;
function resetHistory(){historyData=null;$('history-list').replaceChildren();$('history-status').textContent='Connect to view history.';$('history-toggle').textContent='Enable for this session'}
new MutationObserver(()=>{if($('controls').disabled)resetHistory()}).observe($('controls'),{attributes:true,attributeFilter:['disabled']});
function renderHistory(value){
 historyData=value;$('history-toggle').textContent=value.enabled?'Disable and delete':'Enable for this session';
 $('history-status').textContent=value.enabled?'Enabled until device restart. Recognition evidence is not a complete playback log.':'History is off.';
 $('history-list').replaceChildren();
 for(const item of value.entries){
  const row=document.createElement('li'),text=document.createElement('p');
  text.textContent=item.artist+' — '+item.title+(item.album?' · '+item.album:'');row.append(text);
  const detail=document.createElement('small');detail.textContent=(item.started?new Date(item.started*1000).toLocaleString():'Time unavailable')+' · '+Math.floor(item.observed_seconds)+' seconds of repeated recognition';row.append(detail);
  const link=document.createElement('a');link.textContent=' Find on Discogs';link.href='https://www.discogs.com/search/?'+new URLSearchParams({q:item.artist+' '+(item.album||item.title),type:'release'});link.target='_blank';link.rel='noopener noreferrer';row.append(link);$('history-list').append(row);
 }
}
async function historyAction(action){
 if(historyBusy||!csrf)return null;historyBusy=true;
 try{const response=await fetch('/api/history',{method:'POST',credentials:'same-origin',cache:'no-store',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Waveform-CSRF':csrf},body:new URLSearchParams({action}),signal:AbortSignal.timeout(8000)});
 if(response.status===401||response.status===403){loseSession();resetHistory();return null}if(!response.ok)throw Error();const value=await response.json();if(!csrf)return null;renderHistory(value);return value;
 }catch{$('history-status').textContent='History unavailable. Try again.';return null}finally{historyBusy=false}
}
$('history-refresh').onclick=()=>historyAction('read');
$('history-toggle').onclick=async()=>{const current=await historyAction('read');if(current)await historyAction(current.enabled?'disable':'enable')};
$('history-clear').onclick=()=>historyAction('clear');
$('history-export').onclick=async()=>{const value=await historyAction('read');if(!value)return;const url=URL.createObjectURL(new Blob([JSON.stringify(value,null,2)],{type:'application/json'}));const link=document.createElement('a');link.href=url;link.download='waveform-history.json';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000)};
window.addEventListener('pageshow',()=>poll());
window.addEventListener('online',()=>poll());
poll();
</script></body></html>)HTML";
