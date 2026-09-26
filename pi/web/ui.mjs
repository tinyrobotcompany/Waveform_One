export function editName(value,key){
 const chars=Array.from(value);
 if(key==='backspace')return chars.slice(0,-1).join('');
 if(key==='clear')return '';
 return [...chars,...Array.from(key==='space'?' ':key)].slice(0,40).join('');
}
export function presentation(phase,name,track,recognition){
 const base={title:'Waiting for sound data.',subtitle:'Your ESP is connected. Waiting for fresh microphone readings.',album:'',note:'',eyebrow:'YOUR LISTENING SPACE',artwork:null};
 if(phase==='idle')return {...base,title:name?`Hello ${name}.`:'Welcome home.',subtitle:'What would you like to listen to today?'};
 if(phase==='disconnected')return {...base,title:'Let’s reconnect.',subtitle:'The ESP is disconnected. We’ll reconnect automatically.'};
 if(phase==='calibrating')return {...base,title:'A moment of quiet.',subtitle:'Keep the room quiet while the microphone calibrates.'};
 if(phase==='playing'){
  if(track && typeof track.title==='string' && track.title.trim()){
   let artwork=null;
   try{const url=new URL(track.artwork_url);if(url.protocol==='https:'&&!url.username&&!url.password)artwork=url.href;}catch{}
   return {...base,eyebrow:'NOW PLAYING',title:track.title,subtitle:typeof track.artist==='string'?track.artist:'',album:typeof track.album==='string'?track.album:'',artwork};
  }
  return {...base,eyebrow:'LISTENING NOW',title:'Music is playing.',subtitle:'Enjoy the moment.',note:({listening:'Listening for the song…',recognizing:'Identifying your music…',no_match:'Couldn’t recognise this passage. Listening again shortly.',unavailable:'Music recognition is temporarily unavailable. Retrying automatically.',waiting:'Waiting to identify your music.'})[recognition]||'Music recognition is getting ready.'};
 }
 return base;
}
