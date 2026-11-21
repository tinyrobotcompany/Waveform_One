// node generate_clock.cjs /path/to/lv_font_conv (1.5.3)
const fs=require('node:fs');
const {execFileSync}=require('node:child_process');
const converter=fs.realpathSync(process.argv[2]);
if(execFileSync(converter,['--version'],{encoding:'utf8'}).trim()!=='1.5.3')throw Error('Use lv_font_conv 1.5.3');
process.chdir(__dirname);
for(const size of [128,224]) {
    const file=`waveform_clock_${size}.c`;
    // Keep a tiny contiguous ASCII range: hyphen, digits, colon and intervening punctuation.
    // Plain, static bitmaps support the existing concurrent P4 draw workers.
    execFileSync(converter,['--size',String(size),'--bpp','4','--no-compress','--no-prefilter','--no-kerning',
        '--font','../../third_party/fonts/Montserrat-Medium.ttf','-r','45-58',
        '--format','lvgl','--lv-include','lvgl.h','-o',file],{stdio:'inherit'});
    const source=fs.readFileSync(file,'utf8').replace('.user_data = NULL,','.user_data = NULL,\n    .static_bitmap = 1, /* Plain immutable glyph bitmaps. */');
    fs.writeFileSync(file,source);
}
