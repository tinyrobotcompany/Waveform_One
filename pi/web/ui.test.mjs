import {test} from 'node:test';
import assert from 'node:assert/strict';
import {editName, presentation} from './ui.mjs';
test('screen keyboard handles Unicode deletion, spaces and maximum length',()=>{
 assert.equal(editName('Zoë','backspace'), 'Zo');
 assert.equal(editName('Simon','space'),'Simon ');
 assert.equal(editName('A'.repeat(40),'B'),'A'.repeat(40));
 assert.equal(editName('Simon','clear'),'');
});
test('unrecognised sound never receives a fictitious album title',()=>{
 const view=presentation('playing','Simon',null);
 assert.equal(view.title,'Music is playing.');assert.equal(view.artwork,null);
 assert.match(view.note,/recognition/);
});
test('idle uses personal greeting and hides previous album artwork',()=>{
 const view=presentation('idle','Simon',{title:'Old track',artwork_url:'https://example.org/a.jpg'});
 assert.equal(view.title,'Hello Simon.');assert.equal(view.artwork,null);
});
test('matched metadata is rendered only with safe artwork URL',()=>{
 const track={title:'Track',artist:'Artist',album:'Album',artwork_url:'javascript:alert(1)'};
 assert.equal(presentation('playing','Simon',track).artwork,null);
 assert.equal(presentation('playing','Simon',track).title,'Track');
});
test('recognition progress and no-match are distinct from a matched song',()=>{
 assert.match(presentation('playing','Simon',null,'recognizing').note,/Identifying/);
 assert.match(presentation('playing','Simon',null,'no_match').note,/recognise/);
 assert.equal(presentation('playing','Simon',{title:'Song',artist:'Artist',album:'Record'},'matched').eyebrow,'NOW PLAYING');
});

test('artwork stays visible until replacement loads and stale loads cannot overwrite it',async()=>{
 const {createArtworkLoader}=await import('./ui.mjs');
 const pending=new Map(),shown=[];
 const loader=createArtworkLoader(url=>new Promise((resolve,reject)=>pending.set(url,{resolve,reject})),url=>shown.push(url),()=>shown.push(null));
 loader('a');pending.get('a').resolve();await new Promise(r=>setImmediate(r));
 loader('b');assert.deepEqual(shown,['a']);
 loader('c');pending.get('c').resolve();await new Promise(r=>setImmediate(r));
 pending.get('b').resolve();await new Promise(r=>setImmediate(r));
 assert.deepEqual(shown,['a','c']);
 loader('d');pending.get('d').reject(new Error('offline'));await new Promise(r=>setImmediate(r));
 assert.deepEqual(shown,['a','c']);
 loader(null);assert.deepEqual(shown,['a','c',null]);
});
