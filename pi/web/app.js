'use strict';
const $ = id => document.getElementById(id);
const fragment = new URLSearchParams(location.hash.slice(1));
let token = fragment.get('token') || localStorage.getItem('waveform-token') || '';
if (fragment.has('token')) { localStorage.setItem('waveform-token', token); history.replaceState(null, '', location.pathname); }
let busy = false, latest = null, pollTimer;
async function api(path, options = {}) {
  const response = await fetch(path, {...options, headers: {...options.headers, Authorization: `Bearer ${token}`}, signal: AbortSignal.timeout(18000)});
  if (response.status === 401) { if (!$('pairing').open) $('pairing').showModal(); throw new Error('Connect this browser with your device token.'); }
  if (!response.ok) throw new Error(await response.text());
  return response.json();
}
async function loadQr() {
  try {
    const response=await fetch('/api/pairing-qr',{headers:{Authorization:`Bearer ${token}`}});
    if(!response.ok)throw new Error('Pairing code unavailable');
    $('pairQr').src='data:image/svg+xml;charset=utf-8,'+encodeURIComponent(await response.text());
  }catch(e){message(e.message);}
}
function message(text) { $('message').textContent = text; }
function update(data) {
  latest = data;
  const d = data.device, name = data.preferences.name;
  $('scene').dataset.phase = d.phase;
  $('connection').textContent = d.connected ? '●  Device connected' : '○  Reconnecting';
  $('currentStyle').textContent = d.mode || 'No style received';
  const playing = d.phase === 'playing';
  $('eyebrow').textContent = playing ? 'IN THE MOMENT' : 'YOUR LISTENING SPACE';
  const headlines = {playing:'Let the music move you.',idle:name ? `Hello ${name}.` : 'Welcome home.',disconnected:'Let’s reconnect.',waiting:'Waiting for sound data.',calibrating:'A moment of quiet.'};
  const subtitles = {playing:'Your room. Your records. Your rhythm.',idle:'What would you like to listen to today?',disconnected:'The ESP is disconnected. We’ll reconnect automatically.',waiting:'Your ESP is connected. Waiting for fresh microphone readings.',calibrating:'Keep the room quiet while your microphone calibrates.'};
  $('headline').textContent = headlines[d.phase]; $('subtitle').textContent = subtitles[d.phase];
  $('sceneStatus').textContent = ({playing:'Listening to your music',idle:'Listening for music',disconnected:'Checking the USB connection…',waiting:'Awaiting microphone telemetry',calibrating:'Calibrating microphone'})[d.phase];
  $('clock').hidden = d.phase !== 'idle';
  document.querySelectorAll('[data-mode]').forEach(button => { const selected = button.dataset.mode === d.mode; button.classList.toggle('active',selected); button.setAttribute('aria-pressed',String(selected)); button.querySelector('.selected').textContent = selected ? '●' : '○'; button.disabled = busy || !d.connected; });
  if (!$('settings').open) { $('name').value = name; $('screenBrightness').value=data.preferences.screen_brightness; } $('brightnessControl').hidden=!data.screen_brightness_supported;
}
async function poll() {
  try { update(await api('/api/state')); if (!busy) message(''); }
  catch(e) { message(e.message); $('connection').textContent='○  Pi unreachable'; document.querySelectorAll('[data-mode]').forEach(b=>b.disabled=true); latest=null;$('scene').dataset.phase='disconnected';$('headline').textContent='Connecting to your Pi.';$('subtitle').textContent='We’ll reconnect automatically when your Pi is available.';$('sceneStatus').textContent='Pi connection unavailable';$('clock').hidden=true; }
  pollTimer = setTimeout(poll, 650);
}
document.querySelectorAll('[data-mode]').forEach(button => button.addEventListener('click',async()=>{
  if(busy)return; busy=true; if(latest)update(latest); message('Changing style…');
  try { await api(`/api/mode/${button.dataset.mode}`,{method:'POST'}); update(await api('/api/state')); message('Style updated.'); }
  catch(e){message(e.message);} finally{busy=false;if(latest)update(latest);}
}));
$('pairForm').addEventListener('submit',async e=>{e.preventDefault();token=$('token').value.trim();try{const state=await api('/api/state');localStorage.setItem('waveform-token',token);$('pairing').close();$('pairError').textContent='';update(state);}catch(e){$('pairError').textContent=e.message;}});
$('settingsToggle').addEventListener('click',()=>{$('pairCode').textContent=token;loadQr();$('settings').showModal();$('settingsToggle').setAttribute('aria-expanded','true');});
$('closeSettings').addEventListener('click',()=>$('settings').close());
$('settings').addEventListener('close',()=>$('settingsToggle').setAttribute('aria-expanded','false'));
$('nameForm').addEventListener('submit',async e=>{e.preventDefault();try{await api('/api/preferences',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:$('name').value.trim(),screen_brightness:Number($('screenBrightness').value)})});$('settings').close();update(await api('/api/state'));}catch(e){message(e.message);}});
function clock(){ $('clock').textContent=new Date().toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'}); }clock();setInterval(clock,1000);
const canvas=$('spectrum'), ctx=canvas.getContext('2d'), levels=Array(24).fill(0);
function draw(){ctx.clearRect(0,0,720,160);for(let i=0;i<24;i++){const target=latest?latest.device.bands[i]/9:0;levels[i]+=(target-levels[i])*.15;const height=Math.max(2,levels[i]*150);ctx.fillStyle=`hsl(${80+i*3},30%,${45+levels[i]*25}%)`;const mirrored=latest?.device.mode==='mirrored';ctx.fillRect(i*30+5,mirrored?80-height/2:160-height,20,height);}requestAnimationFrame(draw);}draw();poll();
