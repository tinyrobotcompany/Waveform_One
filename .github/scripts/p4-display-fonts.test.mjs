import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import assert from 'node:assert/strict';
const base = new URL('../../firmware/esp32/p4_display/main/fonts/', import.meta.url);
const coverage = JSON.parse(readFileSync(new URL('coverage.json', base), 'utf8'));
const numbers = text => [...text.matchAll(/0x[\da-f]+|\d+/gi)].map(match => Number(match[0]));

function glyphs(source) {
    const lists = new Map([...source.matchAll(/static const uint16_t unicode_list_(\d+)\[\] =\s*\{([^}]+)\}/g)]
        .map(match => [match[1], numbers(match[2])]));
    const cmaps = source.match(/static const lv_font_fmt_txt_cmap_t cmaps\[\] =\s*\{([\s\S]*?)\n\};/)[1];
    const result = new Set();
    for (const match of cmaps.matchAll(/\{([^}]+)\}/g)) {
        const item = match[1];
        const start = Number(item.match(/\.range_start = (\d+)/)[1]);
        const length = Number(item.match(/\.range_length = (\d+)/)[1]);
        assert(Number(item.match(/\.glyph_id_start = (\d+)/)[1]) > 0);
        const list = item.match(/\.unicode_list = unicode_list_(\d+)/);
        if (list) for (const offset of lists.get(list[1])) result.add(start + offset);
        else for (let i = 0; i < length; ++i) result.add(start + i);
    }
    return result;
}

for (const size of coverage.sizes) test(`P4 ${size}px font covers declared Unicode and musical metadata`, () => {
    const source = readFileSync(new URL(`waveform_font_${size}.c`, base), 'utf8');
    const available = glyphs(source);
    assert.deepEqual([...available].sort((a,b) => a-b), coverage.codepoints);
    for (const sample of ["Don't You Want Me", 'Don’t You Want Me', 'Beyoncé · Björk · Sigur Rós',
        'Cœur — Noël… “Live” «Été»', 'Łódź · Dvořák · Straße', 'Ελληνικά · Музыка', '© ℗ € £ ♫']) {
        for (const char of sample) assert(available.has(char.codePointAt(0)), `Missing ${char} in ${size}px font`);
    }
    assert.match(source, /\.bitmap_format = 0,/); // P4 draw workers use plain bitmaps.
    assert.match(source, /\.fallback = &lv_font_montserrat_/); // Existing Wi-Fi/icon glyphs retained.
});
