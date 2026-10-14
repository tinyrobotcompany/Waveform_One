// node generate_unicode.cjs; sources are vendored, so regeneration is offline.
const fs = require('node:fs');
const zlib = require('node:zlib');
const crypto = require('node:crypto');
const path = require('node:path');
const root = path.join(__dirname, '../../third_party/fonts');
const data = fs.readFileSync(path.join(root, 'UnicodeData-17.txt'), 'utf8');
const assigned = new Set();
const diagnostics = new Set(['bmp', 'smp', 'plane14'].flatMap(plane =>
    fs.readFileSync(path.join(root, `unifont-unassigned-${plane}.hex`), 'utf8')
        .trim().split('\n').map(line => parseInt(line.split(':')[0], 16))));
const invisible = cp => cp === 0x34f || (cp >= 0xfe00 && cp <= 0xfe0f)
    || (cp >= 0xe0100 && cp <= 0xe01ef);
let first;
for (const line of data.trim().split('\n')) {
    const fields = line.split(';'), cp = parseInt(fields[0], 16);
    if (fields[1].includes(', First>')) { first = cp; continue; }
    if (!['Cc', 'Cf', 'Cs', 'Co'].includes(fields[2])) {
        for (let p = fields[1].includes(', Last>') ? first : cp; p <= cp; ++p) assigned.add(p);
    }
}
const combining = new Map(fs.readFileSync(path.join(root, 'unifont-combining-17.txt'), 'utf8')
    .trim().split('\n').map(line => { const [cp, offset] = line.split(':'); return [parseInt(cp, 16), Number(offset)]; }));
const source = fs.readFileSync(path.join(root, 'unifont_all-17.0.05.hex.gz'));
const rows = zlib.gunzipSync(source).toString().trim().split('\n')
    .map(line => { const [cp, hex] = line.split(':'); return { cp: parseInt(cp, 16), hex }; })
    .filter(row => assigned.has(row.cp) && !diagnostics.has(row.cp) && !invisible(row.cp))
    .sort((a, b) => a.cp - b.cp);
const records = Buffer.alloc(rows.length * 8), bitmaps = [], offsets = new Map();
let length = 0;
rows.forEach(({cp, hex}, index) => {
    if (!/^(?:[0-9A-F]{32}|[0-9A-F]{64})$/i.test(hex)) throw Error(`Invalid glyph ${cp}`);
    if (index && rows[index - 1].cp >= cp) throw Error('Duplicate code point');
    if (!offsets.has(hex)) { offsets.set(hex, length); const bytes = Buffer.from(hex, 'hex'); bitmaps.push(bytes); length += bytes.length; }
    // 21-bit Unicode scalar plus width/combining flags. Offset is into bitmap area.
    const flags = (hex.length === 64 ? 0x80000000 : 0) | (combining.has(cp) ? 0x40000000 : 0);
    records.writeUInt32LE((cp | flags) >>> 0, index * 8);
    records.writeUInt32LE(offsets.get(hex), index * 8 + 4);
});
const header = Buffer.alloc(16); header.write('WFU1');
header.writeUInt32LE(rows.length, 4); header.writeUInt32LE(16 + records.length, 8);
header.writeUInt32LE(length, 12);
const binary = Buffer.concat([header, records, ...bitmaps]);
fs.writeFileSync(path.join(__dirname, 'waveform_unicode.bin'), binary);
fs.writeFileSync(path.join(__dirname, 'unicode_coverage.json'), JSON.stringify({
    source: 'GNU Unifont 17.0.05; Unicode 17.0.0',
    sourceSha256: crypto.createHash('sha256').update(source).digest('hex'),
    binarySha256: crypto.createHash('sha256').update(binary).digest('hex'),
    glyphs: rows.length, bytes: binary.length, codepoints: rows.map(row => row.cp),
}, null, 2) + '\n');
console.log(`Packed ${rows.length} actual assigned glyphs in ${binary.length} bytes; no private-use or unassigned placeholders`);
