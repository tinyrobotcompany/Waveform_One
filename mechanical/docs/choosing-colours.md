# Choosing the case colours

The colours of the printed case are set in one file. The print files then
open in ElegooSlicer already coloured, so nothing is coloured by hand in the
slicer.

## Change the colours

1. Open `mechanical/colours.toml` in a text editor. It has four lines:

   | Line | What it colours | Filament slot |
   |---|---|---|
   | `body` | the cabinet, the record holders and all small parts | 1 |
   | `stripe_1` | the first band, 10 mm behind the front | 2 |
   | `stripe_2` | the second band, 40 mm behind the front | 3 |
   | `name` | WAVEFORM ONE on the front | 4 |

2. Write each colour as `"#RRGGBB"`, with the quotes. Some common ones:

   | Colour | Code |
   |---|---|
   | White | `#FFFFFF` |
   | Black | `#000000` |
   | Red | `#C12E1F` |
   | Blue | `#0A2989` |
   | Orange | `#FF6A13` |
   | Yellow | `#F4EE2A` |
   | Green | `#00AE42` |
   | Grey | `#8E9089` |

   Any other colour: copy the code shown by a colour picker (ElegooSlicer's
   filament colour picker shows it). The two stripes and the name may share a
   colour.

3. Rebuild the print files. In Terminal, from the project folder (or the
   feature's worktree, `.worktrees/<name>/`, when working on a branch):

   ```sh
   cd mechanical/cad
   uv run python -m waveform_one.build
   ```

   It takes about a minute and rewrites `mechanical/exports/print_jobs/`.

## Print

1. In ElegooSlicer, **File → Open Project** and choose a file from
   `mechanical/exports/print_jobs/`. If it asks, choose **Load Whole File**.
2. Check: the four filament slots show your colours, and the shell's parts
   (in the object list) show the stripe and name colours.
3. Load the CANVAS with filaments matching slots 1–4, slice and print.

The printer (Centauri Carbon, 0.4 nozzle), print profile (0.16 mm Optimal)
and filament presets (Elegoo PETG @BLACK in slot 1, @WHITE in slots 2–4) are
already set. The purge amounts between colours are worked out for your
colours, the way ElegooSlicer does it.

## Change the printer, print profile or filament presets

These come from a project saved once in ElegooSlicer, kept in
`mechanical/slicer/ElegooSlicer/template.json`. To change them:

1. In ElegooSlicer, open any file from `mechanical/exports/print_jobs/`.
2. Choose the printer, print profile and the four filament presets you want
   (four slots; their colours don't matter).
3. **File → Save Project As…**, anywhere (for example the worktree's
   `mechanical/` folder).
4. In Terminal:

   ```sh
   cd mechanical/cad
   uv run python -m waveform_one.slicer path/to/the/saved.3mf
   uv run python -m waveform_one.build
   ```

The saved project can then be deleted; its settings are now in
`template.json`. The stripes were designed for 0.2 mm layers. With
0.16 mm layers their edges move by less than 0.1 mm, which doesn't show.
