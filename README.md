# gr-hle

Gold Rush! running on [dos-hle-kit](https://github.com/mitigd/dos-hle-kit) through function level HLE.

This is the simplest showcase project for the kit. It runs Sierra's Gold Rush! (1988, AGI interpreter) with a small set of native seams and nothing else. The original game logic, parser, script interpreter, and picture drawing code all still run from the original binary.

## What this is

A working example of how to get a DOS game running with seams.

The kit loads the original `agi` executable with no DOS and no BIOS underneath. Every game function that would have touched hardware is replaced by a small native function (a seam). Everything else is the original 16-bit code executing on the CPU.

This project exists to show that workflow end to end: find hardware touching functions, replace them, run until no unseamed hardware accesses remain.

## What this is not

This port is not needed to preserve or play the game. Gold Rush! already runs perfectly in ScummVM and DOSBox.

It also does not show off the hard parts of the kit. There is no 3D renderer to redirect, no protected mode extender, no MIDI driver, no VESA banking, no large decompiled subsystem.

Think of this as "hello world" for dos-hle-kit: small enough to read in one sitting, complete enough to actually play.

## How it works

1. The kit loads `agi`, a 16-bit real mode MZ program.
2. The game runs natively until it hits a seamed function address.
3. The seam runs host code instead: draw to an SDL window, queue audio, read host input, answer the clock, serve files from disk.
4. Game logic never knows the difference. It still thinks it set VGA mode 13h, programmed the PIT, and beeped the PC speaker.

Key points:

* Your game directory is treated as read only. All writes (save games, etc.) go to the overlay directory, which defaults to a per-user location and can be overridden with `--overlay DIR`.
* Seams are plain C++ functions with the disassembly evidence stored in a `doc` string. Run with `--seams` to print the full table.
* While porting, unseamed hardware access is reported at runtime as `[hw] unseamed hardware access`. You turn each report into a seam and repeat.

## Seams in this port

The full table is split by area under `src/seams/`. In total it is only a few dozen replacements:

* `video`: VGA mode queries, mode 13h setup for `vg_graf.ovl`, palette defaults. The overlay's blit and fill code stays original because it just writes to guest memory.
* `text`: BIOS text helpers redrawn as native 8x8 glyphs in the same 320x200 window. Used for full screen text such as the inventory screen.
* `input`: host keyboard queue wired to the game's key handler (`onKey` in `src/game.cpp`).
* `sound`: three leaf functions of the PC speaker sequencer. The sequencer itself stays original; only tone, volume, and silence go to host audio as a square wave.
* `timing`: 60 Hz host timer replacing the reprogrammed PIT, plus the game's random number helper seeded from the host clock.
* `file`: the game's C library file wrappers (open, read, write, seek, findfirst, etc.) mapped to the kit's sandboxed file layer, plus the overlay loader that loads `*.ovl` files into guest memory.
* `memory` and `platform`: startup memory layout and forcing the VGA machine path so the game picks `vg_graf.ovl`.

Because the AGI engine does almost all drawing in software to a linear buffer, the video side is tiny. That is exactly why this game was chosen as the first example.

## Requirements

* Linux with `/dev/kvm` available (or use `--cpu jit`)
* C++20 compiler, Meson, Ninja
* SDL2 and zlib (`libsdl2-dev`, `zlib1g-dev` on Debian/Ubuntu)
* A copy of Gold Rush! (Sierra On-Line, 1988). Game files are not included. Pass your copy with `--game` on first run (see below).
* `dos-hle-kit` as a Meson subproject (already wired in `subprojects/`)

## Setup

You do not need to edit `src/game.cpp` to set the game path. The `gameDir` field in there is only a compiled-in fallback hint. Just point the built program at your game folder on first run:

```bash
build/gr-hle --game "/path/to/Gold Rush"
```

That choice is remembered for next time, so later runs are just `build/gr-hle`.

How the game folder is found, first match wins:

1. `--game DIR`, or a path to the EXE on the command line
2. the `HLEKIT_GAME_DIR` environment variable
3. the folder remembered from the last successful start
4. the `gameDir` hint compiled into `src/game.cpp` (leave it empty in anything you share)
5. a folder next to the program containing the EXE (also checked: current directory, program directory, and a `game` subfolder of each)

A folder only counts if it contains the game executable (`agi` here, matched in any case).

Saves and other output never go into the game folder. They go to the overlay directory, which defaults to the per-user location for this game (on Linux, under `~/.local/share/hlekit/gr-hle/overlay`). Override it per run with:

```bash
build/gr-hle --overlay /tmp/ov
```

Optional: if you have function names from a decompiler, set `g.functionsCsv` in `src/game.cpp` to your `address,name,size` CSV for nicer logs. It is not required.

## Build and run

```bash
meson setup build
ninja -C build
```

Play the game:

```bash
build/gr-hle
```

Print the seam table:

```bash
build/gr-hle --seams
```

Run headless (no window, useful for testing):

```bash
build/gr-hle --headless --overlay /tmp/ov --seconds 10 --png shot.png
```

Useful flags:

```text
--game DIR       game folder (remembered, so you only need it once)
--overlay DIR    writable overlay (default: per-user location, see Setup)
--seams          print the seam table and exit
--log-services   log file calls
--cpu jit        run without KVM
```

Debug helper: `GR_WATCH=1` prints the guest position twice a second (see `src/support/debug.cpp`).

## Project layout

```text
src/
  game.cpp        GameInfo and module wiring only (game path, seam list, input callbacks)
  seams/
    seams.cpp     All(): concatenates the area modules
    seams.h       shared constants and entry points
    video.cpp     VGA modes and vg_graf.ovl hooks
    text.cpp      native 8x8 text console
    input.cpp     keyboard handler body
    sound.cpp     PC speaker to host audio
    timing.cpp    60 Hz timer and random
    file.cpp      sandboxed files and overlay loader
    memory.cpp    startup memory layout
    platform.cpp  force VGA machine path
  support/
    debug.cpp     GR_WATCH helper (not a seam)
  decompiled/     empty for now: where translated game logic would grow over time
docs/
  project.md         notes on this specific port
  seam-candidates.md every hardware touching function found in the exe
  ranked-seams.md    the same list ranked by replacement value
```

To add a seam: add it to the right area file, list the file in `meson.build` if it is new, and include it in `seams::All()`.

## Learning from this example

Suggested reading order:

1. `src/game.cpp`: the whole port in 30 lines. GameInfo plus seams plus input.
2. `src/seams/seams.cpp` and `seams.h`: how the table is composed from small modules.
3. `src/seams/timing.cpp`: the clearest single seam. One install function becomes a host timer that calls back into the original ISR step with `CallGuestNear16`.
4. `src/seams/video.cpp`: why HLE stays small. Only BIOS calls are replaced; the framebuffer work stays guest code.
5. `src/seams/file.cpp`: sandboxing and the overlay loader, the most code here and the pattern every port needs.

Then compare with `docs/seam-candidates.md` and `docs/ranked-seams.md` to see how the kit's analysis tools found those functions in the first place.

## Notes

* You need to own the game. This repo contains only native glue code, no Sierra data and no assets.
* No game directory is compiled in. The port finds the user's copy at startup (`--game`, `HLEKIT_GAME_DIR`, the remembered directory, or next to the program); keep `GameInfo::gameDir` empty in anything you share.
* Built against dos-hle-kit as a library. See the kit README for CPU backends, runner options, and the full seam API.
