# Time Escape: Stuff From The Past

My Ludum Dare 21 Compo game, from August 2011. The theme was "Escape".
Steal back a sword you left in the Stone Age, then outrun the cave people back to the portal.

The post-compo version only survived as a compiled SWF. In October 2026 it was rebuilt in C with raylib, from a decompile of that SWF, using the compo source as a reference.

## Play it

- [Game page](https://eralpkaraduman.github.io/time-escape/), with the description, controls and screenshots.
- [Game only](https://eralpkaraduman.github.io/time-escape/play/), full window.

It runs in the browser on desktop with a keyboard, and on phones and tablets with on-screen controls.

## What is here

- `escape_postCompo.swf`, `postCompo.html`: the post-compo Flash build from 28 August 2011. The C port follows this version.
- `escape.swf`, `index.html`: the compo build submitted on 22 August 2011.
- `escape_0.swf` to `escape_3.swf`: work-in-progress builds from during the jam, 20 and 21 August 2011.
- `ld21_escape_stuffFromThePast_aukyr_SRC.zip`: the compo source code, in ActionScript 3 with Flixel 2.55.
- `21.7z`: an earlier snapshot of the project, from 21 August 2011.
- `ld21_escape_stuffFromThePast_aukyr_1.png` to `_4.png`: screenshots from the original entry.
- `port/`: the C port. See `port/README.md` to build it for web, macOS, Windows and Linux.
- `port/ITCH.txt`, `port/timeescape-cover-630x500.png`, `port/screenshots/`: the itch.io page text and images.
- `dist/`: ready-made builds of the port.

## Builds in dist/

- `timeescape-web-itch.zip`: the web version. Upload it to itch.io as an HTML game with a 480 x 320 viewport.
- `timeescape-macos-arm64.zip`: for Macs with Apple silicon. The build is unsigned, so the first time, right-click `timeescape` and choose Open.

## Controls

Arrow keys move. X jumps, and holding it jumps higher. C swings the sword once you have it.
