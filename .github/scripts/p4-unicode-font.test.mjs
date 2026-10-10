import { readFileSync } from 'node:fs';
import { gunzipSync } from 'node:zlib';
import { createHash } from 'node:crypto';
import { test } from 'node:test';
import assert from 'node:assert/strict';
const root = new URL('../../firmware/esp32/p4_display/', import.meta.url);
const binary = readFileSync(new URL('main/fonts/waveform_unicode.bin', root));
const manifest = JSON.parse(readFileSync(new URL('main/fonts/unicode_coverage.json', root), 'utf8'));
const fontSource = readFileSync(new URL('third_party/fonts/unifont_all-17.0.05.hex.gz', root));
const source = new Map(gunzipSync(fontSource).toString().trim().split('\n')
    .map(line => { const [cp, hex] = line.split(':'); return [parseInt(cp, 16), Buffer.from(hex, 'hex')]; }));
const hash = buffer => createHash('sha256').update(buffer).digest('hex');

test('every packed Unicode glyph matches the licensed source bitmap and recorded coverage', () => {
    assert.equal(binary.toString('ascii', 0, 4), 'WFU1');
    assert.equal(hash(fontSource), manifest.sourceSha256);
    assert.equal(hash(binary), manifest.binarySha256);
    assert.equal(binary.length, manifest.bytes);
    const count = binary.readUInt32LE(4), bitmapStart = binary.readUInt32LE(8);
    assert.equal(count, manifest.glyphs);
    assert.equal(bitmapStart, 16 + count * 8);
    assert.equal(bitmapStart + binary.readUInt32LE(12), binary.length);
    let previous = -1;
    for (let i = 0; i < count; ++i) {
        const flags = binary.readUInt32LE(16 + i * 8), cp = flags & 0x1fffff;
        const offset = bitmapStart + binary.readUInt32LE(20 + i * 8);
        const length = flags & 0x80000000 ? 32 : 16;
        assert(cp > previous && cp <= 0x10ffff); previous = cp;
        assert.equal(cp, manifest.codepoints[i]);
        assert(offset >= bitmapStart && offset + length <= binary.length);
        assert.deepEqual(binary.subarray(offset, offset + length), source.get(cp));
        assert(!(cp >= 0xd800 && cp <= 0xf8ff), 'Surrogate/private-use diagnostic glyph');
    }
});

test('international music characters use genuine fallback glyphs; gaps are explicitly recorded', () => {
    const coverage = new Set(manifest.codepoints);
    for (const sample of ['Donʼt', '東京 · 世界の音楽', '서울', 'العربية', 'שלום', 'हिन्दी', '🎵 𝄞 ♫']) {
        for (const char of sample) assert(coverage.has(char.codePointAt(0)), `Missing ${char}`);
    }
    assert(!coverage.has(0x378)); // unassigned; never count a numbered diagnostic box as coverage
    assert(!coverage.has(0xe000)); // private use
    assert(!coverage.has(0x10ffff)); // noncharacter
    assert(!coverage.has(0x20838)); // uncommon CJK extension is an acknowledged gap
    assert(!coverage.has(0x1342f)); // upstream diagnostic box for an assigned but unavailable glyph
    assert(!coverage.has(0xfe0f)); // invisible variation selector, not a displayable glyph
    const text = readFileSync(new URL('main/display_text.h', root), 'utf8');
    assert(!text.includes("result += '?'"));
});
