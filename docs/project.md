# Gold Rush! — function-level HLE

The original `agi` runs on dos-hle-kit; only the functions at the hardware seams are replaced
by native code. Use any decompiler you like for names and function boundaries; the kit does not
require a particular workspace format.

The seam table is split by area under `src/seams/` (video, input, sound, timing). `src/game.cpp`
only wires the modules together.

| | |
| :-- | :-- |
| Game directory | `/mnt/usb_sdxc_2/Games/DOS/Gold Rush (1987)(Sierra On-Line)` (never written: writes go to `overlay/`) |
| Function labels | `(none)` (optional CSV: address,name,size) |
| Seam candidates | [seam-candidates.md](seam-candidates.md) (generated from the executable) |
| Ranked candidates | [ranked-seams.md](ranked-seams.md) (heuristic video/sound/timing/input ranking) |
| Seam modules | `src/seams/` (by area): `file` (DOS file wrappers + overlay loader, sandboxed to the game dir, writes to `overlay/`), `memory` (startup block layout), `platform` (selects the VGA path), `timing` (60 Hz tick task, random), `input` (host key queue), `video` + `text` (VGA overlay BIOS calls, native 8x8 text console), `sound` (PC-speaker voice to host audio) |
| Support | `src/support/`: `debug` (`GR_WATCH=1`) |
| Decompiled code | `src/decompiled/` (grows over time) |

## Layout

```text
src/
  game.cpp        GameInfo and module wiring only
  seams/          seam modules by area + All(), which concatenates them
  decompiled/     translated game code, by subsystem (grows over time)
```

The seam table is a plain `std::vector<Seam>`: add new source files to `meson.build` and
concatenate their vectors in `seams::All()`. Start with HLE seams, split them by area, then
translate game code under `src/decompiled/`, calling the original through
`Machine::CallGuest`/`CallGuestFar` while a module is incomplete. A full decomp keeps the seam
boundary and the original executable as the reference/fallback.

## Run

```bash
meson setup build && ninja -C build
build/gr-hle                                   # window + sound
build/gr-hle --seams                           # the seam table, all modules
build/gr-hle --headless --overlay /tmp/ov --seconds 10 --png shot.png   # scripted, hermetic
```

## How it runs (real mode, no DOS)

`agi` is a 16-bit MZ program; the kit loads it with no DOS or BIOS underneath. Nothing answers an
interrupt: each game function that would have used one is replaced by a native one (`src/seams/`),
and everything else - the interpreter, the AGI logic, the graphics overlay's blit and fill code - is
the original code running on the CPU. Native code can call game functions back with
`Machine::CallGuestNear16` (e.g. `init_memory` runs the game's own `FUN_7c09`).

- Graphics: `select_machine` makes the game choose `vg_graf.ovl` (mode 13h, 320x200). `load_overlay`
  re-arms three BIOS-facing seams inside the overlay each time it loads (`OverlaySeams()`).
- Text: the game's BIOS text helpers draw 8x8 glyphs natively (the kit's `hlekit/font.h`: data fonts, resolved per mode).
- Saves: `overlay/` (default `./overlay`, `--overlay DIR`) - the game directory is never written.

Debug: `GR_WATCH=1` prints the guest position twice a second; `--log-services` logs file calls.
