# Font and Unicode data sources

- Montserrat Medium and DejaVu Sans: copied from the project's pinned LVGL
  9.5.0 component; their original license files are adjacent.
- GNU Unifont 17.0.05 `unifont_all-17.0.05.hex.gz`:
  <https://unifoundry.com/pub/unifont/unifont-17.0.05/font-builds/>.
- `unifont-combining-17.txt`: unchanged
  `font/precompiled/unifont-combining-17.0.05.txt` from the same version's source
  archive: <https://unifoundry.com/pub/unifont/unifont-17.0.05/>.
- `unifont-unassigned-*.hex`: unchanged `plane00`, `plane01` and `plane0E`
  diagnostic glyph lists from that archive, used to exclude numbered boxes
  even where a code point is assigned in the Unicode database.
- Font attribution: Roman Czyborra, Paul Hardy, Qianqian Fang, Andrew Miller,
  Johnnie Weaver and the GNU Unifont contributors. Original terms are retained
  in `Unifont-LICENSE.txt`. The font portions are used under their offered
  SIL Open Font License 1.1. No GPL utility source is incorporated. The derived
  embedded pack is named Waveform Unicode and retains the font's license.
  The original `Unifont-README.txt` retains detailed contributor attribution.
- `UnicodeData-17.txt`: unchanged Unicode 17.0.0 character database from
  <https://www.unicode.org/Public/17.0.0/ucd/UnicodeData.txt>; terms retained in
  `Unicode-LICENSE.txt`.

`main/fonts/generate_unicode.cjs` selects assigned visible characters and
deduplicates identical bitmaps. Original source glyph shapes remain unchanged.
Coverage and source/generated hashes are recorded in `unicode_coverage.json`.
This data is a build-time input; the device does not fetch fonts or Unicode data.
