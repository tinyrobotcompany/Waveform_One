import {editName,presentation} from '/ui.mjs';
const $=id=>document.getElementById(id);
const fragment=new URLSearchParams(location.hash.slice(1));
const kiosk=new URLSearchParams(location.search).get('kiosk')==='1';
let token=fragment.get('token')||localStorage.getItem('waveform-token')||'';
if(fragment.has('token')){localStorage.setItem('waveform-token',token);history.replaceState(null,'',location.pathname+location.search);}
let busy=false,latest=null,lastWake=0,messageUntil=0,artworkUrl=null;
async function api(path,options={}){
 const response=await fetch(path,{...options,headers:{...options.headers,Authorization:`Bearer ${token}`},signal:AbortSignal.timeout(18000)});
 if(response.status===401){if(!$('pairing').open)$('pairing').showModal();throw new Error('Connect this browser with your device token.');}
 if(!response.ok)throw new Error(await response.text());return response.json();
}
function message(text){$('message').textContent=text;$('settingsMessage').textContent=text;messageUntil=Date.now()+6000;}
async function wake(){if(Date.now()-lastWake<10000)return;lastWake=Date.now();try{await api('/api/wake',{method:'POST'});}catch{}}
document.addEventListener('pointerdown',wake,{passive:true});
document.addEventListener('keydown',wake);
async function loadQr(){try{const r=await fetch('/api/pairing-qr',{headers:{Authorization:`Bearer ${token}`}});if(!r.ok)throw new Error('Pairing code unavailable');$('pairQr').src='data:image/svg+xml;charset=utf-8,'+encodeURIComponent(await r.text());}catch(e){message(e.message);}}
function update(data){
 latest=data;const d=data.device,name=data.preferences.name;
 $('scene').dataset.phase=d.phase;
 $('connection').textContent=d.connected?'● Connected':'○ Reconnecting';
 $('headerGreeting').textContent=name?`Hello ${name}`:'Your listening space';
 const view=presentation(d.phase,name,data.track,data.recognition);
 $('eyebrow').textContent=view.eyebrow;$('headline').textContent=view.title;$('subtitle').textContent=view.subtitle;
 $('album').textContent=view.album;$('recognitionNote').textContent=view.note;
 $('record').setAttribute('aria-label',view.artwork?'Album artwork':'Record illustration; album artwork unavailable');
 if(artworkUrl!==view.artwork){artworkUrl=view.artwork;$('artwork').hidden=true;if(artworkUrl)$('artwork').src=artworkUrl;else $('artwork').removeAttribute('src');}
 $('sceneStatus').textContent=({playing:'Listening to your music',idle:'Listening for music',disconnected:'Checking USB…',waiting:'Awaiting microphone',calibrating:'Calibrating microphone'})[d.phase];
 $('clock').hidden=d.phase!=='idle';$('quietStatus').textContent=d.phase==='idle'?'Quiet mode is active.':d.phase==='playing'?`Quiet mode follows ${d.idle_in_seconds ?? 30} seconds without substantial sound.`:'Quiet-mode detection is waiting for the microphone.';
 document.querySelectorAll('[data-mode]').forEach(button=>{const selected=button.dataset.mode===d.mode;button.classList.toggle('active',selected);button.setAttribute('aria-pressed',String(selected));button.querySelector('.selected').textContent=selected?'●':'○';button.disabled=busy||!d.connected;});
 if(!$('settings').open){$('name').value=name;$('screenBrightness').value=data.preferences.screen_brightness;}
 $('brightnessValue').textContent=$('screenBrightness').value+'%';$('brightnessControl').hidden=!data.screen_brightness_supported;
}
$('artwork').addEventListener('load',()=>{$('artwork').hidden=false;});
$('artwork').addEventListener('error',()=>{$('artwork').hidden=true;});
async function poll(){
 try{update(await api('/api/state'));if(!busy&&Date.now()>messageUntil){$('message').textContent='';$('settingsMessage').textContent='';}}
 catch(e){message(e.message);$('connection').textContent='○ Pi unreachable';document.querySelectorAll('[data-mode]').forEach(b=>b.disabled=true);latest=null;$('scene').dataset.phase='disconnected';$('headline').textContent='Connecting to your Pi.';$('subtitle').textContent='We’ll reconnect automatically when your Pi is available.';$('album').textContent='';$('recognitionNote').textContent='';$('artwork').hidden=true;$('sceneStatus').textContent='Pi connection unavailable';$('clock').hidden=true;}
 setTimeout(poll,650);
}
document.querySelectorAll('[data-mode]').forEach(button=>button.addEventListener('click',async()=>{
 if(busy)return;busy=true;if(latest)update(latest);message('Changing style…');
 try{await api(`/api/mode/${button.dataset.mode}`,{method:'POST'});update(await api('/api/state'));message('Style updated.');}catch(e){message(e.message);}finally{busy=false;if(latest)update(latest);}
}));
$('pairForm').addEventListener('submit',async e=>{e.preventDefault();token=$('token').value.trim();try{const state=await api('/api/state');localStorage.setItem('waveform-token',token);$('pairing').close();$('pairError').textContent='';update(state);}catch(e){$('pairError').textContent=e.message;}});
function tab(name){document.querySelectorAll('[data-panel]').forEach(p=>p.hidden=p.dataset.panel!==name);document.querySelectorAll('[data-tab]').forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.tab===name)));document.querySelector('.settings-scroll').scrollTop=0;if(name==='phone'){loadQr();$('pairCode').textContent=token;}}
document.querySelectorAll('[data-tab]').forEach(b=>b.addEventListener('click',()=>tab(b.dataset.tab)));
$('settingsToggle').addEventListener('click',()=>{tab('profile');$('settings').showModal();$('settingsToggle').setAttribute('aria-expanded','true');$('closeSettings').focus();});
$('closeSettings').addEventListener('click',()=>$('settings').close());
$('settings').addEventListener('close',()=>$('settingsToggle').setAttribute('aria-expanded','false'));
$('screenBrightness').addEventListener('input',()=>$('brightnessValue').textContent=$('screenBrightness').value+'%');
$('nameForm').addEventListener('submit',async e=>{e.preventDefault();try{await api('/api/preferences',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:$('name').value.trim(),screen_brightness:Number($('screenBrightness').value)})});$('settings').close();update(await api('/api/state'));message('Settings saved.');}catch(e){message(e.message);}});
let draft='',shift=true;
function keys(){
 $('namePreview').textContent=draft||'Enter your name';$('keys').replaceChildren();
 for(const row of ['qwertyuiop'.split(''),'asdfghjkl'.split(''),['shift',...'zxcvbnm'.split(''),'backspace'],['clear',"'",'-','space']]){
  const div=document.createElement('div');div.className='key-row';
  for(const key of row){const b=document.createElement('button');b.type='button';b.dataset.key=key;b.textContent=({shift:shift?'⇧ ON':'⇧',backspace:'⌫',space:'Space',clear:'Clear'})[key]||(shift?key.toUpperCase():key);if(key.length>1)b.className=key==='space'?'space':'wide';b.addEventListener('click',()=>{if(key==='shift')shift=!shift;else{draft=editName(draft,key.length===1&&shift?key.toUpperCase():key);if(key.length===1)shift=false;}keys();});div.append(b);}
  $('keys').append(div);
 }
}
function openKeyboard(){draft=$('name').value;shift=!draft;keys();$('keyboard').showModal();$('keyboardDone').focus();}
if(kiosk){$('name').readOnly=true;$('name').inputMode='none';$('name').addEventListener('click',openKeyboard);}
$('keyboardToggle').addEventListener('click',openKeyboard);
$('keyboardDone').addEventListener('click',()=>{$('name').value=draft;$('keyboard').close();});
$('keyboard').addEventListener('keydown',e=>{if(e.key==='Backspace'){e.preventDefault();draft=editName(draft,'backspace');keys();}else if(e.key.length===1&&!e.ctrlKey&&!e.metaKey){e.preventDefault();draft=editName(draft,e.key);keys();}});
function clock(){const text=new Date().toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});$('clock').textContent=text;$('headerClock').textContent=text;}clock();setInterval(clock,1000);
poll();
