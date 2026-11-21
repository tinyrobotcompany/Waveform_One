// PLAYWRIGHT_MODULE=/path/to/playwright CHROME_BIN=/path/to/chrome node .../remote_browser_check.cjs
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const source = fs.readFileSync(path.join(__dirname, '../main/remote_page.h'), 'utf8');
const pageHtml = source.match(/R"HTML\(([\s\S]*)\)HTML";/)[1];
const output = process.env.WAVEFORM_SCREENSHOTS || '/tmp/waveform-phone-preview';
fs.mkdirSync(output, { recursive: true });
const track = { title: 'In My Time of Dying (Live from Earls Court, 1975)', artist: 'Led Zeppelin', album: 'Physical Graffiti', artwork: 'https://is1-ssl.mzstatic.com/test.svg' };
const standard={background:'#080d0a',panel:'#121a13',line:'#374838',text:'#edf3e8',secondary:'#edf3e8',tertiary:'#a7b2a7',quaternary:'#a7b2a7',accent:'#a8d57a',search:'#a79ae8',light:false,field:Array(40).fill('#080d0a')};
// A light album palette, as the P4 extracts it from a yellow cover: dark text on a light colour field.
const albumPalette={background:'#f7dc72',panel:'#e3c85e',line:'#ba8826',text:'#411a0b',secondary:'#8f3003',tertiary:'#6c4622',quaternary:'#8f3003',accent:'#bd4716',search:'#411a0b',light:true,
    field:Array.from({length:40},(_,i)=>i<20?'#e6c062':'#eed884')};
let state = { appearance:'album', status: 'capturing', track: null, name: 'Simon', brightness: 65, style: 'classic' };
let unauthorized = false, failing = false, sessionFailing = false;
const posts = [];
let history={schema:1,session:'fixture-session',enabled:false,entries:[]};
let update = {state:2,installed:'0.0.0',available:'1.2.3',message:'Update available',progress:0,countdown_seconds:0,interval_hours:720,automatic:true,window_start:180,window_end:240,next_check:1800000000,notes_url:'https://github.com/tinyrobotcompany/Waveform_One/releases/tag/v1.2.3'};
const server = http.createServer((req, res) => {
    res.setHeader('Cache-Control', 'no-store');
    if (req.url === '/pair') {
        res.writeHead(303, { 'Set-Cookie': 'wf1_session=fixture; Path=/; Max-Age=7776000; HttpOnly; SameSite=Strict', Location: '/' });
        return res.end();
    }
    if (req.url === '/') {
        res.setHeader('Content-Security-Policy', "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src https://*.mzstatic.com; frame-ancestors 'none'; base-uri 'none'; form-action 'none'");
        res.setHeader('Content-Type', 'text/html');
        return res.end(pageHtml);
    }
    const paired = req.headers.cookie?.includes('wf1_session=fixture');
    if (req.url === '/api/session') {
        res.writeHead(sessionFailing ? 503 : paired && !unauthorized ? 200 : 401);
        return res.end(paired ? 'fixture-csrf' : 'Pairing required');
    }
    if (req.url === '/api/state') {
        res.writeHead(!paired || unauthorized ? 401 : failing ? 503 : 200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({...state,palette:state.track&&state.appearance!=='classic'?albumPalette:standard}));
    }
    if (req.url === '/api/updates') {
        res.writeHead(!paired || unauthorized ? 401 : 200, {'Content-Type':'application/json'});
        return res.end(JSON.stringify(update));
    }
    if (req.method === 'POST') {
        if (!paired || req.headers['x-waveform-csrf'] !== 'fixture-csrf') { res.writeHead(403); return res.end(); }
        let body = '';
        req.on('data', chunk => { body += chunk; });
        req.on('end', () => {
            posts.push({ path: req.url, body });
            const form = new URLSearchParams(body);
            if (req.url === '/api/brightness') state.brightness = Number(form.get('value'));
            if (req.url === '/api/name') state.name = form.get('name');
            if (req.url === '/api/style') state.style = form.get('style');
            if (req.url === '/api/appearance') state.appearance = form.get('value');
            if(req.url==='/api/updates/action' && form.get('action')==='install') update={...update,state:3,countdown_seconds:0,message:'Downloading update… Keep Waveform One powered on.'};
            if(req.url==='/api/updates/action' && form.get('action')==='postpone') update={...update,state:2,countdown_seconds:0,message:'Installation postponed for 24 hours.'};
            if(req.url==='/api/updates/config') { const [hours,automatic,start,end]=form.get('value').split(',').map(Number); update={...update,interval_hours:hours,automatic:!!automatic,window_start:start,window_end:end}; }
            if(req.url==='/api/history') {
                const action=form.get('action');
                if(action==='enable')history.enabled=true;
                if(action==='disable'){history.enabled=false;history.entries=[]}
                if(action==='clear')history.entries=[];
                res.setHeader('Content-Type','application/json');return res.end(JSON.stringify(history));
            }
            res.end('OK');
        });
        return;
    }
    res.writeHead(404); res.end();
});

(async () => {
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const base = `http://127.0.0.1:${server.address().port}`;
    const browser = await chromium.launch({ executablePath: process.env.CHROME_BIN, headless: true });
    try {
        const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
        await page.addInitScript(() => {
            window.pollDelays = [];
            const original = window.setTimeout;
            window.setTimeout = (callback, delay, ...args) => {
                if (callback && callback.name === 'poll') window.pollDelays.push(delay);
                return original(callback, delay, ...args);
            };
        });
        const errors = [];
        page.on('pageerror', error => errors.push(error.message));
        await page.route('https://is1-ssl.mzstatic.com/test.svg', route => route.fulfill({ contentType: 'image/svg+xml', body: '<svg xmlns="http://www.w3.org/2000/svg" width="400" height="400"><rect width="400" height="400" fill="#baa988"/><path d="M80 120h100v140H80zm140 0h100v140H220z" fill="#eee9dc"/><path d="M95 135h70v110H95zm140 0h70v110h-70z" fill="#758c8b"/><text x="200" y="320" text-anchor="middle" font-size="20" fill="#29332d">LED ZEPPELIN</text></svg>' }));
        await page.goto(base);
        await page.getByText('Scan the QR code on Waveform One to connect.').first().waitFor();
        assert(await page.locator('#settings').isHidden());
        assert(await page.locator('#save-name').isDisabled());
        await page.goto(base + '/pair');
        await page.getByText('Finding your song…').waitFor();
        assert(await page.locator('#wave').isVisible());
        assert(!(await page.evaluate(() => document.cookie)).includes('wf1_session'));
        await page.screenshot({ path: path.join(output, 'phone-search.png'), fullPage: true });
        state = { ...state, status: 'matched', track };
        await page.getByRole('heading', { name: track.title }).waitFor();
        await page.locator('#artwork:visible').waitFor();
        assert(await page.locator('#settings').isHidden());
        assert(await page.locator('#cover.known').count());
        await page.screenshot({ path: path.join(output, 'phone-album.png'), fullPage: true });
        for (const viewport of [{ width: 844, height: 390 }, { width: 768, height: 1024 }, { width: 320, height: 640 }]) {
            await page.setViewportSize(viewport);
            assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
            await page.screenshot({ path: path.join(output, `album-${viewport.width}.png`), fullPage: true });
        }
        await page.setViewportSize({ width: 390, height: 844 });
        state.status = 'identifying';
        await page.getByText('Checking what’s playing…').waitFor();
        assert(await page.locator('#artwork').isVisible());
        assert(await page.locator('#title').textContent() === track.title);
        await page.getByRole('button', { name: 'Settings', exact: true }).click();
        assert(await page.getByRole('dialog').isVisible());
        assert.equal(await page.locator('#brightness').inputValue(), '65');
        assert.equal(await page.evaluate(()=>document.documentElement.style.getPropertyValue('--green')),albumPalette.accent);
        // Every text role follows the album, and the light field is drawn behind the page.
        assert.equal(await page.evaluate(()=>getComputedStyle(document.querySelector('#title')).getPropertyValue('--ink').trim()),albumPalette.text);
        assert.equal(await page.evaluate(()=>document.documentElement.style.getPropertyValue('--ink2')),albumPalette.secondary);
        assert.equal(await page.evaluate(()=>document.documentElement.style.getPropertyValue('--muted2')),albumPalette.quaternary);
        assert.equal(await page.evaluate(()=>document.documentElement.style.colorScheme),'light');
        assert.deepEqual(await page.evaluate(()=>{const canvas=[...document.querySelectorAll('.field canvas')].find(c=>c.style.opacity==='1');return [...canvas.getContext('2d').getImageData(0,4,1,1).data]}),[0xee,0xd8,0x84,255]);
        await page.getByRole('button',{name:'Classic black',exact:true}).click();
        await page.locator('[data-appearance=classic][aria-pressed=true]').waitFor();
        assert.equal(await page.evaluate(()=>document.documentElement.style.getPropertyValue('--bg')),standard.background);
        assert.equal(await page.evaluate(()=>document.documentElement.style.getPropertyValue('--ink')),standard.text);
        assert.equal(await page.evaluate(()=>document.documentElement.style.colorScheme),'dark');
        assert(posts.some(post=>post.path==='/api/appearance'&&post.body==='value=classic'));
        await page.getByRole('button',{name:'Album colours',exact:true}).click();
        await page.locator('[data-appearance=album][aria-pressed=true]').waitFor();
        await page.locator('#name').fill('Zoë');
        await page.getByRole('button', { name: 'Save name' }).click();
        await page.getByText('Saved', { exact: true }).waitFor();
        await page.getByRole('button', { name: 'Mirrored', exact: true }).click();
        await page.locator('[data-style=mirrored][aria-pressed=true]').waitFor();
        state.display_brightness = 10;
        await page.waitForFunction(() => document.getElementById('display-brightness').textContent.includes('screen 10%'));
        assert.match(await page.locator('#display-brightness').textContent(), /selected 65%/);
        delete state.display_brightness;
        await page.waitForFunction(() => document.getElementById('display-brightness').textContent === '');
        await page.locator('#brightness').fill('42');
        await page.locator('#brightness').dispatchEvent('change');
        await page.waitForFunction(() => document.querySelector('#feedback').textContent === 'Saved');
        assert(posts.some(post => post.path === '/api/brightness' && post.body === 'value=42'));
        await page.screenshot({ path: path.join(output, 'phone-settings.png'), fullPage: true });
        await page.waitForFunction(()=>document.querySelector('#update-version').textContent.includes('1.2.3'));
        assert.equal(await page.locator('#update-hours').inputValue(),'720');
        assert.equal(await page.locator('#update-notes').getAttribute('href'), update.notes_url);
        await page.route('**/api/updates/action', route => route.fulfill({status:503,body:'Busy'}));
        await page.locator('[data-update=check]').click();
        await page.getByText('Request failed. Check the connection and try again.', {exact:true}).waitFor();
        assert(await page.locator('#update-wave').isHidden());
        assert(!await page.locator('[data-update=check]').isDisabled());
        await page.unroute('**/api/updates/action');
        let releaseInstall;
        const installGate = new Promise(resolve => { releaseInstall = resolve; });
        await page.route('**/api/updates/action', async route => {
            if (route.request().postData() === 'action=install') await installGate;
            await route.continue();
        });
        await page.locator('[data-update=install]').click();
        assert(await page.locator('#update-wave').isVisible(), 'OTA wave appears before HTTP response');
        assert(await page.locator('[data-update=install]').isDisabled(), 'duplicate install blocked while pending');
        assert.match(await page.locator('#update-message').textContent(), /Requesting installation/);
        releaseInstall();

        await page.getByText('Downloading update… Keep Waveform One powered on.',{exact:true}).waitFor();
        assert(await page.locator('[data-update=install]').isDisabled());
        await page.waitForFunction(()=>window.pollDelays.includes(1000));
        // A failed install remains visible even when a new check reports availability.
        update={...update,state:2,last_install_message:'Download interrupted. Current firmware is unchanged.',message:'Update available'};
        await page.getByText(update.last_install_message,{exact:true}).waitFor();
        // Device-originated scheduled countdown is visible without a phone action.
        update={...update,state:2,countdown_seconds:51,message:'Installing in 51 seconds. Choose Postpone to cancel.'};
        await page.getByText(update.message,{exact:true}).waitFor();
        assert(!await page.locator('[data-update=install]').isDisabled());
        await page.locator('[data-update=postpone]').click();
        await page.getByText('Installation postponed for 24 hours.',{exact:true}).waitFor();
        await page.locator('#update-hours').selectOption('1');
        await page.locator('#update-auto').uncheck();
        await page.locator('#save-updates').click();
        await page.waitForFunction(()=>document.querySelector('#feedback').textContent==='Update settings queued…');
        assert(posts.some(post=>post.path==='/api/updates/config'&&post.body==='value=1%2C0%2C180%2C240'));
        await page.locator('#history-refresh').click();
        await page.waitForFunction(()=>document.getElementById('history-status').textContent==='History is off.');
        await page.locator('#history-toggle').click();
        await page.waitForFunction(()=>document.getElementById('history-toggle').textContent==='Disable and delete');
        history.entries=[{id:1,title:'<img src=x onerror=alert(1)>',artist:'Artist & Name',album:'Album',started:1800000000,observed_seconds:65,duration_seconds:120}];
        await page.locator('#history-refresh').click();
        await page.waitForFunction(()=>document.querySelectorAll('#history-list li').length===1);
        assert.equal(await page.locator('#history-list img').count(),0);
        assert((await page.locator('#history-list').textContent()).includes('<img src=x'));
        assert((await page.locator('#history-list a').getAttribute('href')).startsWith('https://www.discogs.com/search/?'));
        await page.locator('#history-clear').click();
        await page.waitForFunction(()=>document.querySelectorAll('#history-list li').length===0);
        await page.locator('#history-toggle').click();
        await page.waitForFunction(()=>document.getElementById('history-status').textContent==='History is off.');

        update={...update,notes_url:'javascript:alert(1)',message:'<img src=x onerror=alert(1)>'};
        await page.waitForFunction(()=>document.querySelector('#update-message').textContent.includes('<img'));
        assert(await page.locator('#update-notes').isHidden());
        assert.equal(await page.locator('#update-message img').count(),0);
        await page.screenshot({path:path.join(output,'phone-updates.png'),fullPage:true});
        await page.keyboard.press('Escape');
        assert(await page.locator('#settings').isHidden());
        assert(await page.locator('#open-settings').evaluate(el => el === document.activeElement));
        state.track = { ...track, title: '<img src=x onerror=alert(1)> "Live"', artwork: '' };
        await page.getByRole('heading', { name: state.track.title }).waitFor();
        assert.equal(await page.locator('#title img').count(), 0);
        assert(await page.locator('#placeholder').isVisible());
        state.track = { ...track, artwork: 'https://is1-ssl.mzstatic.com/missing.jpg' };
        await page.route('https://is1-ssl.mzstatic.com/missing.jpg', route => route.abort());
        await page.getByRole('heading', { name: track.title }).waitFor();
        assert(await page.locator('#placeholder').isVisible());
        failing = true;
        update={...update,installed:'1.2.3',available:'',state:0,message:'Waveform One is up to date.'};
        await page.waitForFunction(()=>document.querySelector('#update-version').textContent==='Installed: 1.2.3');
        assert.equal(await page.locator('#update-message').textContent(),'Waveform One is up to date.');
        await page.getByText('Reconnecting to Waveform One…').waitFor();
        assert.equal(await page.locator('#title').textContent(), track.title);
        failing = false;
        state = { ...state, track, status: 'muted' };
        await page.getByText('Microphone muted', {exact:true}).waitFor();
        assert.notEqual(await page.locator('#title').textContent(),track.title);
        state = { ...state, track: null, status: 'waiting' };
        await page.getByRole('heading', { name: 'Listening for music' }).waitFor({ timeout: 10000 });
        assert(await page.locator('#wave').isHidden());
        unauthorized = true;
        await page.getByText('Scan the QR code on Waveform One to connect.').first().waitFor();
        assert(await page.locator('#save-name').isDisabled());
        assert.equal(await page.locator('#update-version').textContent(),'');
        assert.equal(await page.locator('#update-next').textContent(),'');
        unauthorized=false;
        await page.getByRole('heading',{name:'Listening for music'}).waitFor({timeout:15000});
        await page.waitForFunction(()=>!document.querySelector('#controls').disabled);
        await page.waitForFunction(()=>document.querySelector('#update-version').textContent==='Installed: 1.2.3');
        // A temporary /api/session outage on page load must recover without a reload.
        sessionFailing=true;await page.reload();
        await page.getByText('Reconnecting to Waveform One…').waitFor();
        sessionFailing=false;
        await page.getByRole('heading',{name:'Listening for music'}).waitFor({timeout:15000});
        await page.waitForFunction(()=>document.querySelector('#update-version').textContent==='Installed: 1.2.3');
        await page.waitForFunction(()=>window.pollDelays.includes(2000));
        assert.deepEqual(errors, []);
        console.log('PASS: native phone album-first layouts, artwork fallback, search/check retention, settings/CSRF, safe text and session loss');
        console.log('Screenshots: ' + output);
    } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; }).finally(() => server.close());
