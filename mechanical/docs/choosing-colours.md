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
   colour: lines with the same colour print from one filament, so one spool.
   Black and white need two spools, whichever lines are white.

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
2. Check: the filament list shows one filament per colour the print uses
   (black and white for the shells; black only for the other prints), and the
   shell's parts (in the object list) show the stripe and name colours.
3. Load the CANVAS with a spool of each of those colours, slice and print.

The printer (Centauri Carbon, 0.4 nozzle), print profile (0.16 mm Optimal)
and filament presets are already set: each colour takes the preset of the
first template slot with that colour (Elegoo PETG @BLACK for the body,
@WHITE for the white parts). The purge amounts between colours are worked out for your
colours, the way ElegooSlicer does it.

## Print at an online service

Print services (PrintYourModel, Zbotic and others) don't read the colour
parts in the ElegooSlicer projects. They read colours **painted on the
model**, as Bambu Studio and OrcaSlicer save them. The rebuild also writes
those, in `mechanical/exports/print_shop/`.

`print_shop/` holds the same six print jobs as `print_jobs/`, with the same
names and layout. Each part is one closed mesh. Every colour is painted on,
the body colour (filament 1) too, so services that count the painted colours
(PrintYourModel) count all of them; slots that share a colour share one
filament. With black and
white, the shells (files 2 and 3) use filament 1 = black and filament 2 =
white; all other files are black only. Upload these and, on the service's
colour step, set filament 1 to black and filament 2 to white.

Settings to ask for: 0.2 mm layers (the stripes are designed for them), 3
walls or more (the heat-set inserts melt into them), 20–25% infill.

Ask for PETG. If the service offers to "repair" the model, say no.

On the CANVAS, keep using the ElegooSlicer projects in `print_jobs/`; the
print-shop files also open in ElegooSlicer if ever needed.

## Change the printer, print profile or filament presets

These come from a project saved once in ElegooSlicer, kept in
`mechanical/slicer/ElegooSlicer/template.json`. To change them:

1. In ElegooSlicer, open any file from `mechanical/exports/print_jobs/`.
2. Choose the printer, print profile and the four filament presets you want
   (four slots, in the order body, stripe 1, stripe 2, name; their colours
   don't matter).
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
