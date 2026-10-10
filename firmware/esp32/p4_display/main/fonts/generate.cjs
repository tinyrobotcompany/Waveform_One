// Run from this directory: node generate.cjs /path/to/lv_font_conv (version 1.5.3).
const fs = require('node:fs');
const { createRequire } = require('node:module');
const { execFileSync } = require('node:child_process');
const converter = fs.realpathSync(process.argv[2]);
if (!execFileSync(converter, ['--version'], { encoding: 'utf8' }).includes('1.5.3')) throw Error('Use lv_font_conv 1.5.3');
const opentype = createRequire(converter)('opentype.js');
process.chdir(__dirname);
const primary = '../../third_party/fonts/Montserrat-Medium.ttf';
const secondary = '../../third_party/fonts/DejaVuSans.ttf';
const fonts = [primary, secondary].map(file => opentype.loadSync(file));
// Latin/extended Latin, combining marks, Greek/Cyrillic, punctuation,
// currency, letterlike symbols, arrows and musical/card symbols.
const blocks = [[0x20,0x7e],[0xa0,0x24f],[0x300,0x52f],[0x1e00,0x1fff],
    [0x2000,0x206f],[0x20a0,0x20cf],[0x2100,0x214f],[0x2190,0x21ff],[0x2660,0x266f]];
const selected = [[], []];
for (const [start, end] of blocks) for (let cp = start; cp <= end; ++cp) {
    // Recognition rejects these directional controls; don't embed them in
    // converter comments or weaken the compiler's bidi-character checks.
    if ((cp >= 0x202a && cp <= 0x202e) || (cp >= 0x2066 && cp <= 0x2069)) continue;
    const index = fonts.findIndex(font => font.charToGlyphIndex(String.fromCodePoint(cp)) !== 0);
    if (index >= 0) selected[index].push(cp);
}
for (const size of [16,20,24,32,40,48]) {
    const fallback = size === 40 ? 32 : size;
    // 1.5.3 spreads its kerning table into Math.max; larger Unicode subsets
    // need more than Node's default JS stack, without disabling kerning.
    // Plain bitmaps preserve the P4's existing two-worker draw configuration;
    // no shared RLE decoder state or new font decompression path is needed.
    execFileSync(process.execPath, ['--stack-size=4096',converter,'--no-compress','--no-prefilter','--bpp','4','--size',String(size),
        '--font',primary,'-r',selected[0].join(','),
        '--font',secondary,'-r',selected[1].join(','),
        '--format','lvgl','--lv-include','lvgl.h','--lv-fallback',`lv_font_montserrat_${fallback}`,
        '--force-fast-kern-format','-o',`waveform_font_${size}.c`], { stdio: 'inherit' });
    // The converter's FULL cmaps encode holes with zero offsets, which our
    // LVGL lookup resolves to the first glyph. Sparse maps preserve true holes.
    const file = `waveform_font_${size}.c`;
    let source = fs.readFileSync(file, 'utf8');
    const lengths = new Map();
    source = source.replace(/static const uint8_t glyph_id_ofs_list_(\d+)\[\] =\s*\{([^}]+)\};/g, (_, id, body) => {
        const offsets = [...body.matchAll(/0x[\da-f]+|\d+/gi)].map(match => Number(match[0]));
        const positions = offsets.map((offset,index) => offset !== 0 || index === 0 ? index : -1).filter(index => index >= 0);
        if (positions.some((index,ordinal) => offsets[index] !== ordinal)) throw Error('Nonconsecutive FULL cmap');
        lengths.set(id, positions.length);
        return `static const uint16_t unicode_list_${id}[] = {${positions.join(', ')}};`;
    });
    source = source.replace(/\.unicode_list = NULL, \.glyph_id_ofs_list = glyph_id_ofs_list_(\d+), \.list_length = \d+, \.type = LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL/g,
        (_, id) => `.unicode_list = unicode_list_${id}, .glyph_id_ofs_list = NULL, .list_length = ${lengths.get(id)}, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY`);
    source = source.replace('.user_data = NULL,', '.user_data = NULL,\n    .static_bitmap = 1, /* Plain bitmaps, like the pinned LVGL built-in fonts. */');
    fs.writeFileSync(file, source);
}
const coverage = selected.flat().sort((a,b) => a-b);
fs.writeFileSync('coverage.json', JSON.stringify({ converter: 'lv_font_conv 1.5.3', sizes: [16,20,24,32,40,48], codepoints: coverage }, null, 2) + '\n');
console.log(`Generated ${coverage.length} glyphs at six sizes`);
