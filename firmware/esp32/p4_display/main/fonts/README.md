# P4 display fonts

Generated with [LVGL's converter](https://github.com/lvgl/lv_font_conv), pinned to
`lv_font_conv@1.5.3`, from the Montserrat Medium and DejaVu Sans sources shipped
with our pinned LVGL component. Source fonts and licenses are retained under
`../../third_party/fonts`.

The same 1,871 glyphs are available at 16, 20, 24, 32, 40 and 48 pixels:
ASCII, Latin/extended Latin, combining marks, Greek/Cyrillic, general punctuation,
currency, letterlike symbols, arrows and musical/card symbols, where present in
the source fonts. Montserrat is preferred; DejaVu supplies missing characters.
Existing LVGL fonts remain the fallback for UI icons. Plain 4bpp bitmaps keep
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
Native rendering additionally checks the actual label's font before displaying
metadata. Unsupported characters get a visible `?`, preserving the original
metadata on the phone. This subset does not claim full Unicode coverage: CJK,
emoji and complex-script shaping need dedicated fonts/rendering acceptance.

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
long accented metadata and initial-search feedback.
