# P4 display fonts

Generated with [LVGL's converter](https://github.com/lvgl/lv_font_conv), pinned to
`lv_font_conv@1.5.3`, from the Montserrat Medium and DejaVu Sans sources shipped
with our pinned LVGL component. Source fonts and licenses are retained under
`../../third_party/fonts`.

The same 1,871 glyphs are available at 16, 20, 24, 32, 40 and 48 pixels:
ASCII, Latin/extended Latin, combining marks, Greek/Cyrillic, general punctuation,
currency, letterlike symbols, arrows and musical/card symbols, where present in
the source fonts. Montserrat is preferred; DejaVu supplies missing characters.
Existing LVGL fonts remain the final fallback for UI icons. Plain 4bpp bitmaps keep
the existing P4 draw configuration, without introducing font decompression.
The generator turns FULL character maps with holes into sparse maps for the
pinned LVGL lookup, excludes rejected directional controls, and marks the plain
bitmaps static. Compiler Unicode/bidi warnings remain enabled.

Reproduce from this directory:

```sh
npm install --prefix /tmp/waveform-font-tools --ignore-scripts lv_font_conv@1.5.3
node generate.cjs /tmp/waveform-font-tools/node_modules/.bin/lv_font_conv
```

`coverage.json` records the selected codepoints. Repository Node tests verify
them against every generated C font's actual character maps, including straight
and curly apostrophes, French accents, smart quotes, dashes and music symbols.
Native rendering preserves valid UTF-8 metadata rather than substituting `?`.
`unicode_font.cpp` adds a shared, immutable fallback pack of 77,872 actual assigned
glyphs from GNU Unifont 17.0.05, selected against Unicode 17.0.0. It supplies CJK,
Hebrew, Arabic, Indic code points, spacing-modifier apostrophes and supplementary
musical/monochrome emoji symbols at all six sizes. The existing typeface is used
first, so ordinary labels retain their appearance. Source glyphs are 8×16 or
16×16 bitmaps scaled to the requested size. These fallback glyphs are less smooth
than the primary typeface; they are actual glyphs, not numbered diagnostic boxes.

The pack is 2,879,200 bytes, stored with the application, without mounting a file
system, downloading fonts or allocating a whole-font RAM cache. Draw workers
render into their own LVGL A8 buffers, with no shared mutable decoder state.
Controls, private-use and unassigned placeholder glyphs are excluded. Variation
selectors/joiners are invisible; this backend does not turn emoji sequences into
composite colour emoji. Bidi layout and LVGL Arabic/Persian shaping are enabled.

**Full Unicode is still an outstanding requirement**, not a claim of this patch.
Unifont lacks many rare CJK extensions, and LVGL's builtin shaping is not a
complete OpenType shaper for Indic and other complex scripts. Glyph availability
alone does not establish correct word shaping. Add the remaining font families,
normalization and a complete shaper, budget their storage/working memory, and
validate native-speaker samples before calling international rendering complete.
See the explicit acceptance item in the repository backlog.

The pinned sources, combining data and licenses are retained in
`../../third_party/fonts`; regenerate the pack offline with:

```sh
node generate_unicode.cjs
```

`unicode_coverage.json` records every included code point and SHA-256 hashes.
Repository tests compare every packed glyph byte-for-byte with its upstream
bitmap and verify bounds, ordering, coverage and the exclusion of placeholders.

For actual LVGL glyph and native-layout acceptance, use the host renderer under
`../../tests/native_display` with the pinned managed LVGL source. Its preparation
script copies the real native panel construction and label helpers from
`main.cpp`, so previews exercise the production font selection and layout:

```sh
# From the repository root; cmake and a host C/C++ compiler must be available.
node firmware/esp32/p4_display/tests/native_display/prepare.cjs /tmp/waveform-native-display-build
cmake -S firmware/esp32/p4_display/tests/native_display -B /tmp/waveform-native-display-build \
  -DLVGL_DIR="$PWD/firmware/esp32/p4_display/managed_components/lvgl__lvgl"
cmake --build /tmp/waveform-native-display-build -j 8
(cd /tmp/waveform-native-display-build && ./native_display_check)
```

It checks real glyph descriptors/bitmaps, missing-glyph fallback and long-text
separation from the status row, and writes PPM previews of an apostrophe title,
long accented metadata, international characters and initial-search feedback.
