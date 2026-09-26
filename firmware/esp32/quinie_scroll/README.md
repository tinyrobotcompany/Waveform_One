# Quinie scrolling message

Standalone fun demo: `Aye, Aye Quinie, Fine Ta?` scrolls right to left,
repeating, with 10x14 pixel letters. Each visible character advances through
red, yellow, green, cyan, blue, magenta, white. Spaces do not consume a colour;
colours stay attached to their characters as they scroll. The seven-colour
palette repeats for this longer message.

Uses the same wiring, startup sequence, and scan timing as kovi_scroll.
No rewiring is needed. The Kovi and wiring-test sources remain separate.

From this directory in an activated ESP-IDF terminal:

```sh
idf.py build
idf.py -p /dev/cu.usbmodem1101 flash
```

Exit any running monitor with Ctrl-] first; update the port if it changed.
Keep the panel powered during flashing. The message starts automatically.
No monitor, music, microphone, or network connection is required.

To restore Kovi: `cd ../kovi_scroll` and run the same flash command.
