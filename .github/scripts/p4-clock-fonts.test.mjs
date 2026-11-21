import {readFileSync} from 'node:fs';
import {test} from 'node:test';
import assert from 'node:assert/strict';
const base=new URL('../../firmware/esp32/p4_display/main/fonts/',import.meta.url);
for(const size of [128,224]) test(`${size}px clock digits remain large and fit every displayed time`,()=>{
    const source=readFileSync(new URL(`waveform_clock_${size}.c`,base),'utf8');
    assert.match(source,/\.range_start = 45, .range_length = 14, .glyph_id_start = 1/);
    const descriptors=source.match(/glyph_dsc\[\] =\s*\{([\s\S]*?)\n\};/)[1];
    const advances=[...descriptors.matchAll(/\.adv_w = (\d+)/g)].map(m=>Math.ceil(Number(m[1])/16));
    assert.equal(advances.length,15);
    const height=Number(source.match(/\.line_height = (\d+)/)[1]);
    assert(height >= (size===224?150:80));
    assert(height+100<=504, 'clock and separate AM/PM fit vertically');
    const width=text=>[...text].reduce((sum,c)=>sum+advances[c.charCodeAt(0)-44]+2,0);
    for(let h=0;h<24;h++)for(let m=0;m<60;m++) {
        for(const text of [`${String(h).padStart(2,'0')}:${String(m).padStart(2,'0')}`,`${h%12||12}:${String(m).padStart(2,'0')}`]) {
            assert(width(text)<=928,`${text} fits with screen margins`);
            assert(width(text)>=(size===224?300:170),`${text} is readable at a distance`);
        }
    }
    assert(width('--:--')<=928);
    assert.match(source,/\.bitmap_format = 0,/);
    assert.match(source,/\.static_bitmap = 1,/);
});
