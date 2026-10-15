# Time Escape (C port)

A C rewrite of the post-compo version of "Stuff From The Past", a Ludum Dare 21 game from 2011.
It is built on raylib 6.0. CMake downloads raylib on the first configure, checks it against a pinned SHA-256 hash, and links it statically. Every build is self-contained, with no raylib library to ship beside it.

Arrow keys move, X jumps, and C swings the sword once you have it.
Take the sword from the cave and escape back through the portal.

## Where the code came from

The post-compo SWF has no surviving source.
The code was rebuilt from a decompile of `escape_postCompo.swf`.
The compo source in `ld21_escape_stuffFromThePast_aukyr_SRC.zip` served as a readable reference.

## Layout

- `src/flx.c`, `src/flx.h`: a small Flixel 2.5 style layer with sprites, collision, auto-tiling, camera, text and input.
- `src/actors.c`: the player, cave people, the carried sword, the sword on its pedestal, portals and sparkles.
- `src/game.c`: the five screens, ported from the original FlxStates.
- `src/main.c`: window, fixed 60 Hz loop, slow motion, screen shake and pixel-perfect scaling.
- `tests/check.c`: logic checks that run without a window.
- `assets/`: sprites, tiles and level layout images taken from the original SWF.
- `web/site-content.html`: this game's text for the GitHub Pages cover page.

## Web page and touch controls

On a touch screen, a control bar sits under the game: left and right, X to jump and C to swing the sword.
The game fills the screen width, and the page never zooms or scrolls.
With a keyboard, the bar stays hidden. Add `?touch=1` to the address to test the touch layout on a desktop.

The bar calls `flx_touch_key()` in `src/flx.c`, which feeds the same input as the keyboard.

The web page and the touch controls are shared with Kurban Bayrami, which keeps them in its `port/web/`.
On every configure, CMake downloads `shell.html.in`, `site.html.in`, `touch-controls.js` and `touch-controls.css` from that repo's `main` branch.
It then fills in this game's title, tagline, colours, game shape and touch keys.
The cover page's text comes from `web/site-content.html`.

So a change pushed to Kurban reaches this game on its next configure. GitHub may take up to 5 minutes to serve the new files.
For a build that must not change, pin a Kurban commit:

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release -DSHARED_WEB_REF=<commit hash>
```

Levels are PNG images with one pixel per 8x8 tile.
Black pixels are solid ground. Colored pixels mark the spawn point, portals, the sword and the cave people.

## Build

Desktop (macOS, Windows, Linux) needs CMake 3.24 or newer, a C compiler, and network access on the first configure.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/timeescape
```

On Linux, install the X11 and OpenGL headers first.
On Debian or Ubuntu that is `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.

Web needs Emscripten.

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
python3 -m http.server 8765 --bind 127.0.0.1 --directory build-web
```

Then open http://127.0.0.1:8765/.

## Publish on itch.io

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --target itch
```

Upload `build-web/timeescape-itch.zip`. On the itch.io project page, use these settings:

- Kind of project: HTML.
- Tick "This file will be played in the browser".
- Viewport dimensions: 480 x 320.
- Leave "Mobile friendly" unticked, since the game needs a keyboard.

## Publish on GitHub Pages

The site is live at https://eralpkaraduman.github.io/time-escape/.
It has a cover page at the root, from `web/site.html`, and the game itself under `play/`.

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --target pages
```

This assembles the whole site in `build-web/pages/`.
Commit that folder's contents to the `gh-pages` branch and push it. GitHub serves that branch.

Logic checks run from this folder:

```sh
./build/timeescape_check
```

## Code checks

These tools are deterministic: the same code gives the same result every run.

Static analysis reads the code without running it.
Run it after a desktop configure, which downloads the raylib headers into `build/`.

```sh
cppcheck --enable=warning,performance,portability,style --std=c99 \
    -D__STDC__=1 -D__STDC_VERSION__=199901L --quiet \
    -I src -I build/_deps/raylib-src/src --suppress=missingIncludeSystem src tests
for f in src/*.c tests/*.c; do clang --analyze -I src -I build/_deps/raylib-src/src "$f" -o /dev/null; done
```

AddressSanitizer catches bad memory access, and UndefinedBehaviorSanitizer catches undefined behaviour.
Both run the logic checks with extra checking compiled in.

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan --target time
./build-asan/time
```

On macOS, the built-in `leaks` tool finds memory that was never freed.
On Linux, AddressSanitizer reports leaks itself.

```sh
leaks --atExit -- ./build/time
```
