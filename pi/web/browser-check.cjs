// Run with PLAYWRIGHT_MODULE and CHROME_BIN set; see pi/README.md.
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const state={device:{connected:true,phase:'playing',mode:'mirrored',bands:Array(24).fill(3)},preferences:{name:'Simon',screen_brightness:100},screen_brightness_supported:true};
const server=http.createServer((req,res)=>{
 const pathname=new URL(req.url,'http://localhost').pathname;
 if(pathname==='/api/state'){res.setHeader('Content-Type','application/json');res.end(JSON.stringify(state));return;}
 if(pathname==='/api/preferences'){let body='';req.on('data',d=>body+=d);req.on('end',()=>{state.preferences=JSON.parse(body);res.end('{}');});return;}
 if(pathname.startsWith('/api/mode/')){state.device.mode=pathname.split('/').pop();res.end('{}');return;}
 if(pathname==='/api/wake'){res.end('{}');return;}
 if(pathname==='/api/pairing-qr'){res.setHeader('Content-Type','image/svg+xml');res.end('<svg xmlns="http://www.w3.org/2000/svg" width="180" height="180"><rect width="180" height="180" fill="white"/></svg>');return;}
 const file={'/':'index.html','/style.css':'style.css','/app.js':'app.js','/ui.mjs':'ui.mjs'}[pathname];
 if(!file){res.writeHead(404);res.end();return;}
 res.setHeader('Content-Type',file.endsWith('.html')?'text/html':file.endsWith('.css')?'text/css':'text/javascript');res.end(fs.readFileSync(path.join(__dirname,file)));
});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const browser=await chromium.launch({executablePath:process.env.CHROME_BIN,headless:true});
 try{
 for(const [width,height,kiosk] of [[800,480,true],[390,844,false],[844,390,false]]){
  state.device.phase='playing';state.preferences.name='Simon';
  const page=await browser.newPage({viewport:{width,height},hasTouch:true});
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}/?kiosk=${kiosk?1:0}#token=test`);
  await page.getByRole('heading',{name:'Music is playing.'}).waitFor();
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'horizontal overflow');
  assert(!(await page.locator('[data-mode="classic"]').isVisible()),'styles must not occupy main screen');
  await page.screenshot({path:`/tmp/waveform-playing-${width}.png`});
  await page.locator('#settingsToggle').click();
  if(kiosk)await page.locator('#name').click();else await page.locator('#keyboardToggle').click();
  await page.locator('#keyboard').waitFor({state:'visible'});
  await page.locator('[data-key="clear"]').click();
  for(const key of ['a','l','e','x'])await page.locator(`[data-key="${key}"]`).click();
  await page.screenshot({path:`/tmp/waveform-keyboard-${width}.png`});
  await page.locator('#keyboardDone').click();
  assert.equal((await page.locator('#name').inputValue()).toLowerCase(),'alex');
  await page.getByRole('button',{name:'Save & close'}).click();
  await page.locator('#settings').waitFor({state:'hidden'});
  assert.equal(state.preferences.name.toLowerCase(),'alex');
  await page.locator('#settingsToggle').click();await page.locator('[data-tab="styles"]').click();
  await page.locator('[data-mode="classic"]').click();
  await page.waitForFunction(()=>document.querySelector('[data-mode="classic"]').getAttribute('aria-pressed')==='true');
  await page.locator('[data-tab="phone"]').click();
  await page.locator('summary').click();
  await page.locator('.settings-scroll').evaluate(e=>e.scrollTop=e.scrollHeight);
  assert(await page.locator('.settings-scroll').evaluate(e=>e.scrollTop>0),'settings should scroll');
  const save=await page.getByRole('button',{name:'Save & close'}).boundingBox();assert(save.y+save.height<=height,'save not reachable');
  await page.locator('#closeSettings').click();
  state.device.phase='idle';
  await page.waitForFunction(()=>document.querySelector('#scene').dataset.phase==='idle');
  assert(await page.locator('#clock').isVisible());assert(!(await page.locator('#record').isVisible()));
  assert((await page.locator('#headline').textContent()).toLowerCase().includes('alex'));
  await page.screenshot({path:`/tmp/waveform-idle-${width}.png`});
  assert.deepEqual(errors,[]);await page.close();console.log(`PASS ${width}x${height}: layout, keyboard, saved name, styles, settings scrolling, quiet clock`);
 }
 }finally{await browser.close();server.close();}
})().catch(e=>{console.error(e);server.close();process.exitCode=1;});
