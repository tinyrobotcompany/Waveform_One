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
let state = { status: 'capturing', track: null, name: 'Simon', brightness: 65, style: 'classic' };
let unauthorized = false, failing = false;
const posts = [];
const server = http.createServer((req, res) => {
    res.setHeader('Cache-Control', 'no-store');
    if (req.url === '/pair') {
        res.writeHead(303, { 'Set-Cookie': 'wf1_session=fixture; Path=/; HttpOnly; SameSite=Strict', Location: '/' });
        return res.end();
    }
    if (req.url === '/') {
        res.setHeader('Content-Security-Policy', "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src https://*.mzstatic.com; frame-ancestors 'none'; base-uri 'none'; form-action 'none'");
        res.setHeader('Content-Type', 'text/html');
        return res.end(pageHtml);
    }
    const paired = req.headers.cookie?.includes('wf1_session=fixture');
    if (req.url === '/api/session') {
        res.writeHead(paired ? 200 : 401);
        return res.end(paired ? 'fixture-csrf' : 'Pairing required');
    }
    if (req.url === '/api/state') {
        res.writeHead(!paired || unauthorized ? 401 : failing ? 503 : 200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify(state));
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
        await page.locator('#name').fill('Zoë');
        await page.getByRole('button', { name: 'Save name' }).click();
        await page.getByText('Saved', { exact: true }).waitFor();
        await page.getByRole('button', { name: 'Mirrored', exact: true }).click();
        await page.locator('[data-style=mirrored][aria-pressed=true]').waitFor();
        await page.locator('#brightness').fill('42');
        await page.locator('#brightness').dispatchEvent('change');
        await page.waitForFunction(() => document.querySelector('#feedback').textContent === 'Saved');
        assert(posts.some(post => post.path === '/api/brightness' && post.body === 'value=42'));
        await page.screenshot({ path: path.join(output, 'phone-settings.png'), fullPage: true });
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
        await page.getByText('Reconnecting to Waveform One…').waitFor();
        assert.equal(await page.locator('#title').textContent(), track.title);
        failing = false;
        state = { ...state, track: null, status: 'waiting' };
        await page.getByRole('heading', { name: 'Listening for music' }).waitFor({ timeout: 10000 });
        assert(await page.locator('#wave').isHidden());
        unauthorized = true;
        await page.getByText('Scan the QR code on Waveform One to connect.').first().waitFor();
        assert(await page.locator('#save-name').isDisabled());
        assert.deepEqual(errors, []);
        console.log('PASS: native phone album-first layouts, artwork fallback, search/check retention, settings/CSRF, safe text and session loss');
        console.log('Screenshots: ' + output);
    } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; }).finally(() => server.close());
