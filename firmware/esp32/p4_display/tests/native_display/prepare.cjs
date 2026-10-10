// Copy the actual native panel and label helpers into the host renderer.
const fs = require('node:fs');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../../main/main.cpp'), 'utf8');
const out = process.argv[2];
fs.mkdirSync(out, { recursive: true });
const helpers = source.slice(source.indexOf('void set_label_text('), source.indexOf('void on_open_settings('));
const panel = source.slice(source.indexOf('    lv_obj_t *information ='), source.indexOf('    artwork_image = lv_image_create(artwork);'));
if (!helpers || !panel) throw Error('Native source blocks not found');
fs.writeFileSync(path.join(out, 'native_helpers.inc'), helpers);
fs.writeFileSync(path.join(out, 'native_panel.inc'), panel);
