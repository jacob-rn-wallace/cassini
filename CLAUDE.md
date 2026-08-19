# Saturn-Family HP Calculator Replica Project — Cassini

Real Saturn-CPU-family calculator emulation running on a Raspberry Pi
Pico Plus 2 (RP2350B + 8 MiB PSRAM — the original target was a plain
Pico 2, until a real hardware memory ceiling forced the move; see
"Current status" below), driving a real Sharp LS027B7DH01 memory LCD
(400x240, on an Adafruit breakout board, #4694). Named for the Cassini
space probe,
which orbited Saturn — a nod to the Saturn CPU architecture shared by
HP's late-80s-through-2000s calculator lineage (48SX/48GX/40G/49G/50G,
whichever of these the vendored core actually supports — see "The
Saturn CPU core" below).

**Scope, deliberately broader than a single model.** This project
originally started as an HP 50g-only replica; it's now explicitly about
the whole Saturn-CPU calculator family, because the vendored emulation
core (`saturnng`) already treats them as close relatives sharing most of
their architecture, and because that gives a real sequencing advantage:
**start with whichever supported model is architecturally simplest, get
that working end-to-end on real hardware first, and only then take on
the more complex models** — the HP50G most of all, since it's the
newest, has the largest/tallest native display (131x80 vs. the 131x64
shared by the older models), and uses a Flash-based memory subsystem
rather than the simpler ROM-based one the 48-series uses. Which model is
genuinely simplest to bring up first is now confirmed (HP48SX/HP48GX,
tied — see "Hardware" below for the evidence), closing what was
originally a Phase 2 investigation item.

Structured as a sibling project to `soynut` (an HP-41CV replica) —
soynut is this project's structural template: same target MCU, same
"vendor an existing CPU emulation core, BYO ROM, never edit the vendored
core" approach, same directory shape, same coding-standard commitment.
Nothing in soynut is copied wholesale; its *conventions* are re-derived
here since the Saturn-family-specific logic underneath is entirely
different from the HP-41's Nut CPU.

This is the stable reference doc: confirmed architecture, current
hardware/software state, and build/run instructions — read this first,
whether you're a person or a Claude instance picking up this repo cold.
Session-by-session development history belongs in `DEVLOG.md`, which is
gitignored and local-only.

**License:** GPL-3.0 (see `LICENSE`), matching `saturnng`'s own top-level
`COPYING`/`LICENSE` choice exactly — same reasoning soynut applied when
it adopted GPL-2.0-or-later to match `emu41gcc`. Note `saturnng`'s
individual `src/core/*.c` files each carry a per-file "GPL-2.0 or (at
your option) any later version" grant (not "GPL-3.0-only"), so GPL-3.0
is a valid, compatible choice, not a mismatch — it's the version the
project as a whole is actually distributed under (its `COPYING` file is
full GPLv3 text), which is the more precise thing to mirror than the
permissive "or later" language alone. The vendored-by-copy `sharpdisp/`
library (once copied in) is separately LGPL-2.1 — compatible with, and
not a conflict with, this project's own GPL-3.0 terms.

## Current status

**Phases 1 through 3 are all done, including real hardware
confirmation.** Phase 1 (vendor the core) and Phase 2 (native host
smoke test, including a real ROM run) are done. Phase 3 (Sharp display
bring-up) is now fully verified end to end, including on physical
hardware — see below. `saturnng` was selected after cloning and reading
both
candidates directly, and is vendored at `saturn_core/` as a pinned git
submodule (see "The Saturn CPU core" below). On top of that,
`firmware/saturn_compat/` has real shim code — a from-scratch,
bare-metal-portable implementation of the five-function libChf surface
(`ChfStaticInit`/`ChfGenerate`/`ChfPushHandler`/`ChfSignal`/`ChfExit`)
`saturn_core/src/core/` calls through `chf_wrapper.h`'s macros, plus the
`ui4x_config` global and
`ui4x_make_filename_absolute_for_loading/saving()` callbacks that stand
in for the deliberately-uninitialized `ui4x` submodule — and
`tests/saturn_smoke_test.c` + `tests/Makefile` build and link the
vendored core against those shims into a real host binary
(`make -C tests run ROM=... MODEL=48sx`). It's been run end to end
against a missing-optional-state path (WARNING, falls through to a
clean `CpuReset()`), a missing-ROM path (FATAL, clean `exit(1)`), and
now a real HP48SX revision J ROM dump — full `PASS`, no bad opcode
across the whole instruction bound. See "Native (host) tests" below.

Separately, `sharpdisp/` (the Sharp memory-LCD framebuffer/font
library) is now vendored by copy from `pico_sharpmem_display-main` (see
`sharpdisp/README.md` for exactly what was and wasn't vendored, and the
two local patches already baked into the copy), and `lcd_bringup/` is a
real, standalone Pico SDK project — no dependency on `saturn_core/` or
a ROM — that links against it and builds/links cleanly under the
Pico 2 (RP2350) SDK 2.x + ARM GNU toolchain, producing a real
`lcd_bringup.uf2`. That `.uf2` has now been flashed to this project's
own physical Pico 2 (via `picotool load -f -x`, which force-rebooted
the board into BOOTSEL mode over its existing USB connection rather
than needing the physical BOOTSEL button held) and **visually
confirmed working**: "Cassini" centered plus a border, rendered
correctly on the real LS027B7DH01. Phase 3 is fully closed.

**Phase 4 (wiring `saturn_core` + `firmware/saturn_compat/` +
`sharpdisp/` into a real `firmware/` Pico SDK project) is done and
verified on real hardware, including a real memory-ceiling blocker
that forced a hardware change plus this project's first vendored-core
patch — both now resolved and confirmed working.** The display-scaling
decision from "Hardware" below is resolved (3x nearest-neighbor,
393x192 centered in the 400x240 panel, top 24px margin reserved for
annunciator text labels — the user's explicit direction). A real
`firmware/` project builds cleanly, flashes successfully, and — on the
Pico 2 — panicked with `Out of mem` while loading the ROM, root-caused
precisely to the vendored core's one-byte-per-nibble storage plus an
oversized `N_PORT_2_BANK_48` constant (~9.5 MiB in one `malloc()`
against the Pico 2's 512 KiB SRAM). That was resolved by moving to a
**Pimoroni Pico Plus 2 (RP2350B + 8 MiB PSRAM)**, relocating the
newlib heap into PSRAM via a custom linker-script override, and a
single, narrow, patch-file-tracked edit to `saturn_core/src/core/bus.h`
right-sizing that one constant. Real end-to-end confirmation: a full
HP48SX ROM boot + a bounded 2,000,000-instruction run completed
cleanly on the physical Pico Plus 2, landing at PC `0x0127D` with no
bad opcode — the exact same final PC the host smoke test reports as a
clean `PASS`. See "Native firmware (`firmware/`)" below, specifically
"Resolved: ROM memory footprint," for the full technical account — the
newlib syscall shim used to embed the ROM, the build fixes required,
the debugging process (including two real bugs found and fixed along
the way: a `hardware_psram`/`--gc-sections` linking trap, and a flash
byte-padding bug in this project's own on-device debug logging), and
exactly what was patched and why. Immediately after that, the same
session went further: the bring-up loop was found to be idling forever
because it never drove the real Saturn hardware timer registers
`Emulator()` normally would — fixed entirely in `main.c` (zero further
`saturn_core` changes), and the physical display now shows real,
legible HP48SX firmware UI (a genuine "Try to Recover Memory?" prompt)
for the first time. See "Timer-driven execution" (also under "Native
firmware" below) for that account. Immediately after that, again the
same session: a real interactive keyboard bridge (`main.c`'s loop is
now genuinely unbounded/continuous, a documented Rule 2 exception -
see `DEVIATIONS.md`; a wire protocol over USB serial drives
`saturn_core`'s own `KeybPress()`/`KeybRelease()`; a Tkinter GUI,
`tools/hp48_keyboard_gui.py`, presents a real, gridded-and-verified
clickable HP48GX keyboard photo). Confirmed working end to end for a
neutral key and for the recovery prompt's NO answer (including this
project's own bad-opcode safety net catching a real vendored-core
opcode gap cleanly); YES reaches the same gap but doesn't get caught
by that same safety net, left open as a real next-session
investigation. See "Interactive keyboard bridge" (also under "Native
firmware" below) for the full account.

## Coding standard: NASA/JPL "Power of 10"

Adopted from the start (not retrofitted later, unlike soynut's own
history), for the same reasons soynut gives: not certification, just a
strong discipline for long-term legibility. Applies to this project's
own original code only — never the vendored `saturn_core/` submodule
or the vendored-by-copy `sharpdisp/` display library. See
`DEVIATIONS.md` for the authoritative, currently-empty exception list,
and soynut's `CLAUDE.md` for the full rule-by-rule rationale this
project intends to mirror once real code exists to apply it to.

## Directory map

Scaffolded per the project's bootstrap plan, mirroring soynut's layout:

- **`saturn_core/`** — git submodule for the vendored Saturn CPU
  emulation core (`saturnng`, see "The Saturn CPU core" below). Vendored;
  never edited directly.
- **`sharpdisp/`** — vendored *by copy* (not submodule — no upstream git
  history exists to submodule against) from
  `pico_sharpmem_display-main`, an already-working, LGPL-2.1 framebuffer
  driver for the Sharp LS027B7DH01 on a Pico 2. Copied in: see
  `sharpdisp/README.md` for exactly what was vendored (library only —
  `include/`, `src/`, pre-generated `fonts/`, `LICENSE` — not upstream's
  own examples/tests/tools) and the two local patches already baked in
  (the `pico/platform.h` → `pico.h` swap SDK 2.x requires).
- **`firmware/`** — the real Pico SDK project wiring the Saturn core,
  `firmware/saturn_compat/`, and `sharpdisp/` together
  (`CMakeLists.txt`, `pico_sdk_import.cmake`, `main.c`, `pins.h`,
  `saturn_lcd.c`/`.h`, `rom_syscalls.c`/`.h`, `stubs/sys/ucontext.h`).
  Builds and flashes successfully; currently blocked at runtime on a
  real memory constraint, not a code bug — see "Current status" above
  and "Native firmware (`firmware/`)" below for the full account.
  `firmware/saturn_compat/` (`chf_compat.c`, `ui4x_compat.c`,
  `config_compat.c`, `saturn_compat.h`, `ui4x/src/api.h`) is confirmed
  to carry over completely unchanged from the host build, exactly as
  planned — mirroring soynut's `emu41gcc_compat/`, see "Native (host)
  tests" below for what they cover.
- **`lcd_bringup/`** — a standalone Pico SDK project (no dependency on
  the Saturn core or a ROM) for isolated Sharp-display bring-up,
  mirroring soynut's `lcd_bringup/` structurally (`CMakeLists.txt`,
  `main.c`, `pico_sdk_import.cmake`, `pins.h`) though its actual
  "driver" is the vendored `sharpdisp/` library, not a project-local
  controller driver like soynut's `st7920.c` (the Sharp display's own
  vendored library already covers that). Builds and links cleanly
  against `~/pico/pico-sdk` (master, SDK 2.x) and the ARM GNU toolchain,
  producing a real `lcd_bringup.uf2` — see "Current status" above for
  what's confirmed vs. still needing physical hardware.
- **`roms/`** — BYO Saturn ROM `README.md` with the confirmed format
  (see "ROM images" below) and native-smoke-test run instructions. No
  converter script exists yet (that's firmware-bring-up work, for
  embedding a ROM as C source on-target — the host smoke test just
  reads the ROM file directly). The ROM files themselves will never be
  in this repo — copyrighted Saturn-family firmware, not ours to
  redistribute.
- **`tests/`** — native (host, no Pico SDK) tests. `saturn_smoke_test.c`
  + `Makefile` exist and build/link cleanly against the vendored core;
  see "Native (host) tests" below.
- **`sim/`** — will hold a host-native full simulator, later phase,
  mirroring soynut's `sim/`.
- **`tools/`** — `hp48_keyboard_gui.py`, a Tkinter clickable-keyboard
  GUI modeled on soynut's own `tools/hp41_keyboard_gui.py`, driving the
  firmware over USB serial. See "Native firmware" below's "Interactive
  keyboard bridge" section for the full account. Native (non-Python)
  diagnostic tools not yet started.
- **`reference-material/`** — datasheets/mockups, not read by any build
  step. Gitignored (unlike soynut, where it's tracked) — pending a
  decision on what belongs here and whether any of it needs to be
  redistributable.
- **`pico-sdk/`** — official `raspberrypi/pico-sdk` checkout (dependency,
  gitignored, same convention as soynut). Not present as a project-local
  checkout on the machine this was developed on — `lcd_bringup/`'s
  `CMakeLists.txt` (mirroring soynut's own fallback) uses the
  `PICO_SDK_PATH` environment variable if set, and only falls back to a
  project-local `../pico-sdk` if it isn't, so a machine-wide checkout
  (e.g. `~/pico/pico-sdk`) works equally well and is what was actually
  used to confirm `lcd_bringup/` builds — see "Current status" above.
- **`toolchain/`** — extracted ARM GNU Toolchain (gitignored), same
  no-sudo `pkgutil --expand-full`-or-tarball workaround soynut's
  `CLAUDE.md` documents. Same story as `pico-sdk/` above: not
  project-local here, a machine-wide `~/pico/arm-gnu-toolchain` was used
  instead, just needs to be on `PATH` at build time.

Not yet present, and deliberately not scaffolded until confirmed
necessary: an `Arduino .../`-equivalent dormant-hardware directory (no
prior hardware-bridge history exists here, unlike soynut's), and a
`font-tables/`-equivalent (pending confirmation that the Saturn core's
display memory is already real pixels rather than something needing a
segment/font-style decode).

## The Saturn CPU core

**Chosen and vendored: `saturnng`**, at `saturn_core/` (git submodule,
pinned to commit `3f467d2`, tag `6.1.1`-adjacent). All facts below are
confirmed by directly cloning and reading the real source, not carried
over from secondhand research.

**Real upstream location:** `https://codeberg.org/gwh/saturnng.git` —
**not** `github.com/gwenhael-le-moine/saturnng`, which is now a dead
tombstone repo (its one commit, "Codeberg Migration", deletes the whole
tree and leaves a pointer to the Codeberg URL). Actively maintained:
most recent commit as of vendoring was 2026-07-27, with several tagged
releases (6.0.3 → 6.1.1) that same month.

**License:** effectively GPL-2.0-or-later at the per-file level (every
file under `src/core/` carries the standard "GPL v2, or (at your option)
any later version" header, copyright 1998-2000 Ivan Cibrario Bertolotti,
original author), but the project's own top-level `COPYING`/`LICENSE`
is full **GPL-3.0** text — this project mirrors that top-level choice
(see "License" above).

**Core/UI separation — real, confirmed by direct grep, but not
airtight.** `src/core/` (cpu.c, bus.c, disassembler.c, disk_io.c,
emulator.c, flash49.c, hdw.c, keyboard.c, monitor.c, romram48.c,
romram49.c, serial.c, types.h) has zero includes of glib/GTK/SDL/ncurses/
pthread/X11 — those only appear in `src/main.c`, `src/options.c`, and
`src/ui4x/` (a separate git submodule for the UI layer, deliberately
**not** initialized in this repo's vendoring — Cassini only needs
`src/core/`, never `src/ui4x/`). Two real, small dependencies the core
does have, both handled as bridge-shim work rather than edits to the
vendored code itself:
- `src/libChf/` — a small (~1745-line) condition/error-handling library
  every `src/core/*.c` file uses for its `FATAL`/`ERROR`/`WARNING`/
  `DEBUG` macros (via `chf_wrapper.h`). Its own `chf_msgc.c` uses POSIX
  `<nl_types.h>` message catalogs and `chf_sig.c` uses `setjmp.h`-based
  signal handling — neither portable to bare-metal RP2350 as-is. Public
  API is small (5 functions: `ChfGenerate`, `ChfStaticInit`,
  `ChfPushHandler`, `ChfSignal`, `ChfExit`) — a `firmware/saturn_compat/`
  shim reimplementing just that surface is the planned approach, not
  editing `libChf` itself.
- A handful of `ui4x_*`-named functions (e.g.
  `ui4x_make_filename_absolute_for_loading`) that `src/core/romram49.c`
  calls directly for ROM filename resolution — despite the name, these
  are a thin callback surface the core expects its embedder to supply,
  not a real dependency on the `ui4x` submodule's actual UI code.
  `firmware/saturn_compat/` will need to provide these too.

**HP49G/50G native display resolution — confirmed, and notably not
uniform across models.** Not hardcoded in `src/core/` (the core only
emulates I/O-port-level LCD controller registers, no pixel geometry) —
defined instead in the `ui4x` submodule's headers:
`#define LCD_WIDTH (131)` (`ui4x/src/api.h`), and
`#define LCD_HEIGHT (ui4x_config.model == MODEL_50G ? 80 : 64)`
(`ui4x/src/inner.h`). **HP49G is 131×64** (same as the 48-series) — the
project plan's original "3× scale to 393×240" framing was based on an
unverified assumption that HP49G matched the 50g's 131×80, which is
wrong. **HP50G is 131×80**, confirmed by the same header, and
`saturnng` already models `MODEL_50G` as a distinct configuration
(`dist/style-50g.css` exists, though `dist/saturn50g` — a launcher script
— doesn't yet, unlike 40g/48gx/48sx/49g). This means that whenever the
HP50G specifically gets tackled (see "Hardware" below on why it's
currently the *last* model in the planned sequence, not the first),
Cassini can target its **real display geometry directly**, without
needing a different (49G-based) scale factor as a stand-in. Whether
`saturnng`'s Flash-based ROM/RAM subsystem (shared between
49G/50G, differentiated mainly by this LCD-height config and by which
ROM file's actual content is loaded) is enough to run a real 50G-labeled
Saturn-mode ROM dump correctly hasn't been tested yet — that's Phase 2's
job (native host smoke test).

**Saturn ROM binary format — confirmed from `src/core/disk_io.c`'s
`bus_read_nibblesFromFile()` and `src/core/romram49.c`:**
- Flat binary, **no header** for the ROM image itself (a separate,
  differently-formatted save-state file format exists for
  suspend/resume, not relevant to initial ROM loading).
- Expected on-disk size for the 49G/50G Flash ROM:
  **2 MiB (2,097,152 bytes)** — half of `N_FLASH_SIZE_49`'s 4,194,304
  *nibbles* (`src/core/bus.h`), since one on-disk byte unpacks to two
  in-memory nibbles. HP48-series ROMs are 512 KiB instead
  (`N_ROM_SIZE_48`).
- Unpacking: each on-disk byte becomes two 4-bit nibbles in memory,
  **low nibble first** (bits 0-3 → the lower memory address, bits 4-7 →
  the next one) — matches the real Saturn CPU's own nibble-addressing
  convention.
- The Saturn CPU itself is a 4-bit nibble machine: `typedef char int4;
  typedef int4 Nibble;` (`src/core/types.h`), with 64-bit registers
  stored as 16 separate 4-bit `Nibble` cells
  (`NIBBLE_PER_REGISTER = 16`, `src/core/cpu.h`).
- ROM files aren't shipped with `saturnng` itself; its own
  `dist/ROMs/Makefile` fetches a `rom.49g` via `curl` from hpcalc.org at
  build time (no equivalent `rom.50g` target exists yet upstream) — this
  project will still need its own `roms/README.md`/converter and its own
  BYO posture regardless of what upstream does, since redistribution
  rights don't come from how upstream happens to fetch it.

**Core CPU file:** `src/core/cpu.c`, 2,562 lines (comparable in kind to
`emu41gcc`'s single-file `nutcpu.c` at 1,525 lines, though `saturnng`'s
"core" is spread across several files rather than one — `bus.c`
1,450 lines, `disassembler.c` 2,331 lines, `serial.c` 1,096 lines, etc.,
roughly 13,500 lines total under `src/core/`). Dispatch style: many small
`static void ExecXXX(...)` opcode-handler functions plus embedded
`switch` blocks, same overall spirit as a classic opcode-dispatch
emulator core.

**`x48` — evaluated and rejected as the primary choice**
(`github.com/czodroid/x48`, cloned and read directly for comparison).
Confirmed: uniform GPL-2.0-or-later across the tree (both top-level
`LICENSE` and per-file headers agree, unlike `saturnng`'s v2-vs-v3
split), core CPU file `src/emulate.c` is 2,491 lines. **Explicitly
HP48 S/SX and G/GX only** — its own README says so directly, and its
ROM loader (`src/romio.c`) actively detects and rejects/warns on HP49
ROMs by header signature, rather than supporting them. Its core/UI
separation is also weaker than `saturnng`'s: individual `.c` files
mostly avoid direct X includes, but the shared header every core file
depends on (`src/hp48_emu.h`) itself does `#include <X11/Xlib.h>` and
declares `Display*`/`Window`/`GC` externs, and `lcd.c` (which owns the
actual emulated LCD pixel buffer) has its Xlib rendering calls
intermixed with that buffer's own state, not cleanly separated. Kept
here as a documented, evaluated alternative in case `saturnng` proves
unworkable during Phase 2 — not because it's a strictly worse Saturn
core in general, but because it doesn't cover the HP49G/50G-family
architecture Cassini specifically needs.

## Hardware

- **Display:** Sharp LS027B7DH01, 400x240, on an Adafruit breakout board
  (#4694). Confirmed working already (not by this project — by prior
  work at `pico_sharpmem_display-main`, being vendored into
  `sharpdisp/`): 3 signal pins needed (CS/SCK/MOSI), board handles
  level-shifting/regulation from 3.3V logic on-board, default pinout
  CS=GP17/SCK=GP18/MOSI=GP19/spi0.
- **MCU:** Pimoroni Pico Plus 2 (RP2350B + 8 MiB PSRAM), raw Pico C SDK
  — same choice as soynut's own MCU family, for the same reason
  (RAM/flash headroom over an Uno-class board), upgraded from a plain
  Pico 2 once real hardware testing hit a memory ceiling the Pico 2's
  512 KiB SRAM couldn't clear even after right-sizing the vendored
  core's oversized constants — see "Native firmware" below's "Resolved:
  ROM memory footprint" section. Same physical footprint/pinout as a
  stock Pico 2 (confirmed directly from Pimoroni's own mechanical
  diagram and product claims), so the display wiring below is unchanged
  by the swap.
- **Scaling — per-model, not fixed.** Native resolution differs across
  the Saturn family: HP48SX/48GX/40G/49G are 131x64, HP50G is 131x80
  (see "The Saturn CPU core" above). The Sharp display's 240px height
  was originally chosen for the 50G specifically (3x → 393x240, an
  exact fit, 0px vertical margin). For a 131x64 model at the same 3x
  factor, that's 393x192 — a real 48px of unused vertical space (192 of
  240), which needs a real decision once a first target model is
  chosen: letterbox/center it, use a larger non-integer-friendly scale
  (unlikely to stay pixel-exact), or accept the smaller model just not
  filling the display. Not resolved yet — deliberately deferred to
  whichever model Phase 2 actually targets first, rather than guessed
  now.
- **First target model — confirmed by direct read of `romram48.c`,
  `bus.c`, `romram49.c`, and `flash49.c`.** The original guess ("HP48SX
  is simplest because 48GX has RAM-card complexity 48SX lacks") was
  **half wrong**: `bus.c:816-825` routes both `MODEL_48SX` and
  `MODEL_48GX` through the *same* `bus_hp48` module table, and the only
  model branch inside `romram48.c` (`RomInit48()`, lines 115-131) is how
  many bytes of ROM file to read — full `N_ROM_SIZE_48` for 48GX, half
  for 48SX. The Port 2 RAM/ROM card slot logic
  (`NCe3Init48`/`Read48`/`Write48`/`Save48`, `romram48.c:600-742`) is
  gated by the compile-time macro `N_PORT_2_BANK_48`, which **was**
  unconditionally defined to 32 (`bus.h:194`; the model-conditional
  version is commented out directly above it) — so it's fully compiled
  and exercised for **both** models regardless. 48SX and 48GX are
  therefore code-path-identical in this emulator core, differing only by
  ROM byte count. **HP48SX and HP48GX are tied for simplest** — pick
  either. (This constant is now patched to `1` in this project's own
  checkout via `saturn_core.patch` — see "Native firmware" below's
  "Resolved: ROM memory footprint" section. The code-path-identical
  analysis above is unaffected; only the Port 2 buffer's size changed.)

  The other half of the original guess was backwards: **HP40G is not
  simpler than 49G/50G — it shares their full complexity.**
  `bus.c:821-824` routes `MODEL_40G` through the exact same `bus_hp49`
  table as `MODEL_49G`/`MODEL_50G`, meaning 40G gets the full
  Flash-emulation state machine (`romram49.c`, `flash49.c`, 425 lines
  modeling 28F160S5/28F320S5-style command/erase behavior) — grepping
  both files for `MODEL_40G` returns zero hits, i.e. no cheaper
  40G-specific path exists anywhere in that code. 40G is exactly as
  complex as 49G/50G here, not a lighter alternative to them.

  **Ranking, confirmed: HP48SX ≈ HP48GX (tied, simplest) < HP40G ≈
  HP49G ≈ HP50G (tied, all routed through the Flash-based
  `romram49.c`/`flash49.c` path).** First target is now **HP48SX or
  HP48GX**, interchangeable in code-path terms — 48SX is the marginally
  smaller ROM (256 KiB vs 512 KiB effective read, see `romram48.c:124-128`)
  so it's the pick unless a reason emerges to prefer 48GX specifically.
  This closes the Phase 2 "which model first" investigation item.

## Native (host) tests

Mirrors soynut's own `tests/` — host-native binaries (system compiler,
no Pico/ARM toolchain) that prove core wiring without needing real
hardware. Currently one test, `tests/saturn_smoke_test.c`:

```
make -C tests run ROM=/path/to/your.rom MODEL=48sx   # or MODEL=48gx
```

`ROM=` accepts an absolute path, a path relative to `tests/` (since
`make -C tests` changes directory before running), or — the common
case, since `roms/` lives at the repo root — a path relative to the
repo root (e.g. `ROM=roms/hp48sx_revj.rom`); the `run` target's recipe
tries all three in that order before giving up.

It boots a real HP48SX/HP48GX ROM (BYO, see "ROM images" below)
against the vendored `saturn_core`, wired through
`firmware/saturn_compat/`'s shims, via `saturn_core/src/core/emulator.c`'s
own `EmulatorInit()` (not the top-level `src/emulator_api.c`/`main.c`
wrapper, which pulls in the un-vendored `ui4x` UI layer) — then calls
`cpu.c`'s `OneStep()` directly in a bounded loop (`MAX_INSTR`,
currently 2,000,000) rather than going through `Emulator()`'s
timer/throttle machinery, which doesn't matter for a host smoke test.
Pass/fail: whether the CPU core ever signals `CPU_E_BAD_OPCODE`/
`CPU_E_BAD_OPCODE2` (an unrecognized instruction — the simplest
concrete evidence the ROM is being decoded as real Saturn code, not
garbage), caught via a Chf handler pushed the same way `saturn_core`'s
own `Emulator()` catches `CPU_I_EMULATOR_INT` (`ChfPushHandler()` +
`setjmp()`, unwound with `longjmp()` on `CHF_UNWIND`) — a real exercise
of the shim's unwind path, not a shortcut around it.

`tests/Makefile` compiles `saturn_core/src/core/*.c` leniently (no
`-Wall`/`-Werror` — it's vendored, never edited to satisfy our own
warning level) and `firmware/saturn_compat/*.c` + `tests/*.c` strictly
(`-Wall -Wextra -Wpedantic -Werror`, per the Power-of-10 commitment),
same split soynut's `tests/Makefile` uses for `emu41gcc`/
`emu41gcc_compat`. One vendored-code accommodation was needed:
`serial.c` calls `read()`/`write()`/`close()` without including
`<unistd.h>` itself (works on whatever the original build's transitive
includes provided, not on macOS's libc) — handled with a `-include
unistd.h` force-include on just that one file's compile rule, the same
"make unmodified vendored source build under a modern compiler" trick
soynut uses for `nutcpu.c`. One deliberate `-Wpedantic` suppression
exists in `firmware/saturn_compat/chf_compat.c` (a `va_start()` call
whose last named parameter is an enum, forced by `Chf.h`'s own
`ChfGenerate()` signature) — see `DEVIATIONS.md` for the full
justification and exact boundary.

**Verified, including against a real ROM.** The harness builds and
links cleanly. Running it against a nonexistent ROM path exercises both
Chf severity paths correctly — a missing optional state file
(`cpu`/`mod`/...) signals `WARNING` and falls through to a clean cold
`CpuReset()`/`bus_reset()` exactly as `EmulatorInit()` intends, while a
missing ROM file signals `FATAL` and the shim's default
no-handler-registered policy cleanly `exit(1)`s — real evidence the
`ChfGenerate`/`ChfSignal`/handler-stack wiring behaves correctly in
both the "recoverable" and "unrecoverable" cases. Beyond that, a real
BYO HP48SX ROM dump (revision J, 262,144 bytes, matching the confirmed
size below) has now been run end to end: cold-init falls through
`WARNING`s for HDW/internal RAM/Port 1/Port 2 exactly as the
no-saved-state path predicts, then the CPU runs the full
`MAX_INSTR` = 2,000,000-instruction bound landing at PC `0x0127D`
without ever hitting `CPU_E_BAD_OPCODE`/`CPU_E_BAD_OPCODE2` — a real
`PASS`, and the first concrete evidence the vendored core is decoding
genuine Saturn machine code end to end through the shim layer, not just
exercising its error paths.

## Sharp display bring-up (`lcd_bringup/`)

Standalone Pico SDK project, deliberately decoupled from `saturn_core/`
and any ROM — proves the physical LS027B7DH01 + Adafruit breakout
(#4694) + Pico 2 + toolchain chain works before wiring the Saturn core
into `firmware/` at all. Mirrors soynut's `lcd_bringup/` structurally
(`CMakeLists.txt`/`main.c`/`pico_sdk_import.cmake`/`pins.h`), but its
actual driver is the vendored `sharpdisp/` library (see its own
`README.md`) rather than a project-local controller driver — the Sharp
display's own library already covers that layer.

Build (`PICO_BOARD=pico2` below reflects Phase 3's original hardware;
the project's board is now the Pico Plus 2 — see "Hardware" above and
"Native firmware"'s "Resolved: ROM memory footprint" section. This
exact project, unmodified, was re-run against
`-DPICO_BOARD=pimoroni_pico_plus2_rp2350` as a real diagnostic step
during that PSRAM debugging, in its own separate `build_plus2/`
directory, and confirmed the same "Cassini" text + border rendering
correctly on the new board too — real evidence the physical wiring
swap didn't need any code changes):

```
export PICO_SDK_PATH=~/pico/pico-sdk      # or wherever your checkout is
export PICO_BOARD=pico2
export PATH=~/pico/arm-gnu-toolchain/bin:$PATH
cd lcd_bringup && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

Produces `lcd_bringup.uf2`. Flash with BOOTSEL held (drag-and-drop to
the `RP2350` mass-storage volume), or `picotool load -f -x
lcd_bringup.uf2` — the `-f` forces the board into BOOTSEL mode over its
existing USB connection first if it isn't already there (confirmed
working even when the board was running unrelated previously-flashed
firmware enumerating as USB-serial, no physical button press needed),
and `-x` reboots straight into the newly-flashed app afterward.

`main.c` draws "Cassini" centered plus a border, once, then idles —
deliberately the same shape as the vendored library's own `hello_world`
example, which was already built/flashed/visually confirmed working on
this exact display/breakout/Pico 2 combination before `sharpdisp/` was
vendored (see `sharpdisp/README.md`). `pins.h` names the CS/SCK/MOSI
pins explicitly (GP17/18/19, `spi0`) rather than relying on
`sharpdisp_init_default()`'s internal default, even though they
currently match — see "Hardware" below. Note `main.c` never calls
`stdio_init_all()`, so despite `CMakeLists.txt` enabling
`pico_enable_stdio_usb`, no USB serial console actually comes up at
runtime (harmless — the display path is pure SPI, independent of USB
stdio — but it does mean `picotool info` can't be used to confirm the
board is running this program after flashing; only the physical display
output can).

**Fully verified, including on physical hardware:** builds and links
cleanly (strict warnings on `main.c`, vendored `sharpdisp/` sources left
at their own warning level, same split `tests/Makefile` uses) against
`~/pico/pico-sdk` (master, SDK 2.x) and the ARM GNU toolchain, producing
a real `.uf2`. That `.uf2` has been flashed to this project's own
Pico 2 + LS027B7DH01 + Adafruit breakout (#4694) and visually
confirmed: "Cassini" centered plus a border render correctly on the
real display. Phase 3 has no open items.

## Native firmware (`firmware/`)

Phase 4: wires `saturn_core` + `firmware/saturn_compat/` +
`sharpdisp/` into one real Pico SDK executable (`cassini`) that boots a
compiled-in HP48SX ROM and renders its emulated LCD to the physical
Sharp display. Scope, confirmed with the user: a *static bring-up*
milestone — run the CPU for a bounded instruction count (same safety
pattern as `tests/saturn_smoke_test.c`), render once, idle. Still no
keyboard (no physical keyboard hardware for this project yet, so real
interactivity is its own later phase) and still not wall-clock
real-time (deliberately bounded by instruction count, not throttled to
real time), but the loop now *does* drive the real Saturn hardware
timer registers (instruction-count-paced rather than wall-clock-paced)
— see "Timer-driven execution" below for why that turned out necessary
and what it unlocked. **Done and verified on real hardware, including
real, legible HP48SX firmware UI actually rendering** — see "Resolved:
ROM memory footprint" and "Timer-driven execution" below for the full
story of what that took.

Build (needs a real ROM already embedded — see "ROM embedding" below
and `roms/README.md` first):

```
export PICO_SDK_PATH=~/pico/pico-sdk      # or wherever your checkout is
export PATH=~/pico/arm-gnu-toolchain/bin:$PATH
cd firmware && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release        # PICO_BOARD defaults to pimoroni_pico_plus2_rp2350,
                                            # CASSINI_PSRAM_HEAP defaults ON - both required for
                                            # this board, see "Resolved: ROM memory footprint" below
make -j
picotool load -f -x cassini.uf2            # -f forces BOOTSEL automatically, no button needed
```

Reading back the on-device debug log after a run (see "Resolved: ROM
memory footprint" below for what this is):

```
picotool save -r 0x10FF0000 0x11000000 -t bin out.bin -f
strings -a out.bin
```

**Display layout**, per the user's explicit direction: the native
HP48SX 131x64 LCD scaled 3x (393x192), centered in the 400x240 panel —
3px left / 4px right margin (400-393=7, uneven by construction), 24px
top / 24px bottom (240-192=48, split evenly). The top 24px band holds a
row of small text annunciator labels (L/R/A/BAT/BSY/IO, matching
`emulator_api.c`'s `ANN_LEFT`/`ANN_RIGHT`/`ANN_ALPHA`/`ANN_BATTERY`/
`ANN_BUSY`/`ANN_IO` bits) rather than being left blank — the user's
explicit reason for preferring 3x over a larger scale that would fill
the panel.

### ROM embedding

`roms/rom_to_c.py` (new, mirrors soynut's own converter) embeds a raw
ROM file as `const unsigned char cassini_rom_data[]` — no nibble
unpacking or other transform, unlike soynut's converter, since
`saturn_core`'s own `disk_io.c` (`bus_read_nibblesFromFile()`, called
unmodified from `romram48.c`'s `RomInit48()`) already does that
unpacking itself via `fopen()`/`getc()`/`fclose()`. Run manually/
offline (`python3 roms/rom_to_c.py roms/hp48sx_revj.rom
roms/rom_images.c`); output gitignored, never committed (BYO ROM
policy, same as everywhere else in this project).

Rather than touch `disk_io.c`'s call chain, `firmware/rom_syscalls.c`
overrides the newlib syscalls `fopen`/`getc`/`fclose` are themselves
built on — `_open()`/`_read()`/`_close()`, all declared
`__attribute__((weak))` in `~/pico/pico-sdk/src/rp2_common/
pico_clib_interface/newlib_interface.c` (confirmed directly; a strong
definition cleanly overrides a weak one at link time, standard newlib
retargeting, no duplicate-symbol risk). It recognizes one magic
filename (`EMBEDDED_ROM_PATH`, shared with `main.c` via
`rom_syscalls.h`, passed to the existing `saturn_compat_set_rom_path()`
API) and serves bytes from the compiled-in array; every other path
fails cleanly, exactly preserving the WARNING-then-cold-reset fallback
`firmware/saturn_compat/ui4x_compat.c`'s `Resolve()` already creates
for RAM/Port1/Port2. Also provides `_stat()` — discovered as a real
link error, not speculative: newlib bundles `_stat_r` together with
`_fstat_r` in the same object file, so referencing the SDK's
already-weakly-stubbed `_fstat()` still pulls in `_stat_r`'s own body,
which calls the never-stubbed `_stat()`. `_fstat()`/`_lseek()` stay as
the SDK's existing weak stubs — confirmed harmless (`fopen`'s
`fstat`-based buffer sizing just falls back to a default size on
failure; `bus_read_nibblesFromFile()` never calls `lseek()`).

### LCD decode (`firmware/saturn_lcd.c`/`.h`)

Reimplemented directly against the vendored core's global `hdw`
(`extern hdw_t hdw;`, `hdw.h:84`) and `bus_fetch_nibble()`, deliberately
**not** reusing `saturn_core/src/emulator_api.c` (it depends on the
un-vendored `ui4x` submodule) — but copied verbatim as the reference
for the exact decode algorithm rather than re-derived: rows
`0..hdw.lcd_vlc` come from `hdw.lcd_base_addr` advancing
`hdw.lcd_line_offset` nibbles between rows; remaining rows to full
height (64, hardcoded — confirmed HP48SX/GX standard, not present
anywhere in vendored source) come from `hdw.lcd_menu_addr`. Bit 0
(LSB) of each nibble = leftmost of its 4 pixels, bit 3 (MSB) =
rightmost. Blits via `bitmap_filled_rect()`
(`sharpdisp/include/sharpdisp/bitmapshapes.h`) — a real filled rect,
distinct from the outline-only `bitmap_rect()` `lcd_bringup/main.c`
uses.

### Two build accommodations found on this toolchain (neither needed by
the host build)

- `saturn_core/src/core/bus.c:105` unconditionally `#include
  <sys/ucontext.h>` — confirmed completely unused in the file (no
  `ucontext_t`/`mcontext_t`/`sigaction` reference anywhere), present on
  macOS's libc (why the host build never hit this) but absent from the
  ARM/newlib toolchain. Fixed with an empty stub
  (`firmware/stubs/sys/ucontext.h`) on the include path — same
  "make unmodified vendored source build under a different compiler"
  posture as the next item.
- `serial.c` calls `read()`/`write()`/`close()` without including
  `<unistd.h>` itself — same issue `tests/Makefile` already handles for
  the host build, fixed identically via a `-include unistd.h`
  force-include scoped to just that one file.

### The `stdio_usb_connected()` gotcha — and why blocking on it was later removed

`main.c` originally used a fixed `sleep_ms(1500)` before its first
`printf()`, guessing that was "enough time" for a USB serial terminal
to attach. `pico_stdio_usb`'s CDC output is **silently dropped** when
written before a real terminal has connected — no fixed delay can
guarantee that in general, and it cost real debugging time (several
flash-wait-check cycles showing "nothing" over serial that were fully
explained by this, not a firmware bug). First fixed by blocking on
`stdio_usb_connected()` (`pico/stdio_usb.h`) in a loop before printing
anything, so no boot output was ever lost regardless of how long a
terminal took to attach.

**That block was later removed once it started actively hurting
debugging, once the host machine's serial terminal turned out to be
unreliable enough to matter (see "Resolved: ROM memory footprint"
below).** Gating literally all progress — including the on-screen log
and physical display — on a serial terminal successfully connecting
defeated the entire point of adding a no-terminal-required status
channel: a run would sit at "Booting..." on the physical panel
indefinitely if nothing ever managed to open the serial port, even
though the board itself was fine. `stdio_init_all()` is still called
(so a terminal can still attach at any point and see everything from
then on), but `main()` no longer waits for it - `printf()` output is
simply dropped if nothing is attached yet, same as it always was
before a terminal connects; the panel and the flash-persisted log are
now the primary, always-available channels.

### Resolved: ROM memory footprint exceeded the Pico 2's RAM

**Fully resolved and confirmed working on real hardware, 2026-08-19.**
The board is now a **Pimoroni Pico Plus 2 (RP2350B + 8 MiB PSRAM)**,
physically wired identically to the earlier Pico 2 (same pinout, see
"Hardware" above), and a real HP48SX ROM boot + a bounded
2,000,000-instruction run now completes cleanly on it — landing at PC
`0x0127D` with no bad opcode, the exact same final PC the host smoke
test (`tests/saturn_smoke_test.c`) reports as a clean `PASS`. This
section keeps the full original debugging history (root cause, options
considered, PSRAM research done while the board was in transit) below,
followed by exactly what was actually done to resolve it and the real
bugs hit along the way.

**Original root cause, on the Pico 2 (confirmed precisely, not
estimated):** `romram48.c:117` does one
`malloc(sizeof(struct BusStatus_48))` call. That struct's fields
(`bus.h:183-196`) were ROM 1 MiB (`N_ROM_SIZE_48` — sized for the
*larger* HP48GX ROM regardless of which model is selected at runtime),
RAM 256 KiB, Port 1 256 KiB, and **Port 2 8 MiB** (`N_PORT_2_BANK_48`
hardcoded to `32` — `bus.h`'s own comment says the intended default was
`8` (1 MiB), with a commented-out model-conditional line directly
above showing the original design, `config.model == MODEL_48GX ? 32 :
1`, disabled at some point upstream in favor of the unconditional
value. HP48SX doesn't have this card slot in real hardware at all.)
Total: ~9.5 MiB requested in one call, against the Pico 2's actual
512 KiB SRAM. Separately, the vendored core stores every emulated
nibble as a full byte (`typedef char int4; typedef int4 Nibble;`,
`types.h`) — deliberate but memory-inefficient — so even after
right-sizing the other three constants, the 256 KiB HP48SX ROM alone
still needs 512 KiB unpacked, **100% of the Pico 2's entire SRAM**
before counting anything else. Confirmed non-viable escape hatches at
the time: `x48` shares the identical one-byte-per-nibble
representation (re-checked directly in `romio.c`), so switching cores
wouldn't have helped and would have cost 49G/50G support.

**What was actually done — three pieces, all now confirmed working
together:**

1. **Hardware: Pimoroni Pico Plus 2 (RP2350B + 8 MiB PSRAM).** Same
   physical footprint/pinout as a stock Pico 2 (Pimoroni's own product
   claim, mechanically confirmed from their dimensioned diagram), so no
   rewiring was needed when the board physically arrived — see
   "Hardware" above for the exact pin mapping used, and its own
   sourcing note for how that was verified before trusting it.

2. **Relocate the newlib `malloc()` heap into PSRAM, via a linker
   script override — not a plain `--defsym`.** The Pico SDK maps PSRAM
   as its own region (`PSRAM(rwx): ORIGIN = 0x11000000, LENGTH =
   PICO_PSRAM_SIZE_BYTES`, `pico_psram_region.template.ld`), but its
   own `section_heap.incl` hardcodes the `.heap` output section to
   `> RAM` regardless of any `HEAP_LOC`/`HEAP_LIMIT` `--defsym`
   symbols — those only move the heap's *address*, not which memory
   *region* the linker checks it against. First attempt at the
   `--defsym`-only approach failed at link time with a real error
   (`address 0x11000800 of cassini.elf section .heap' is not within
   region RAM'`), proving this the hard way rather than by reading
   ahead. The actual fix uses the SDK's own supported override
   mechanism, `pico_add_linker_script_override_path()`, to substitute
   this project's own `firmware/linker_overrides/section_heap.incl`
   for the SDK's version — targeting `PSRAM` instead of `RAM`, placed
   at `__psram_end__` (the symbol `sections_psram.incl` exports for
   exactly this purpose). Wired up in `firmware/CMakeLists.txt` via the
   `CASSINI_PSRAM_HEAP` option (now the default-on path for this
   board). This is this project's zero-vendored-edit option — it alone
   would not have been sufficient, though (see next item).

3. **One narrow, patch-file-tracked edit to `saturn_core`** — this
   project's first — changing `bus.h`'s `N_PORT_2_BANK_48` from `32`
   to `1`. Necessary because even with the heap correctly in PSRAM,
   `struct BusStatus_48`'s unmodified ~9.5 MiB request still exceeded
   the Pico Plus 2's 8 MiB PSRAM (confirmed by direct arithmetic before
   ever flashing a build that would have failed) — the oversized Port 2
   card-slot buffer (a slot HP48SX doesn't have in real hardware) was
   still the entire problem, just now against an 8 MiB ceiling instead
   of 512 KiB. The commented-out model-conditional replacement
   (`config.model == MODEL_48GX ? 32 : 1`) doesn't compile as a fixed
   struct-array size (a runtime-conditional macro can't size a
   compile-time array), so the constant itself was reduced directly.
   New total: ~1.75 MiB, comfortably inside 8 MiB. Tracked as
   `saturn_core.patch` (repo root) rather than edited in place —
   `firmware/CMakeLists.txt` applies it automatically and idempotently
   at configure time (`git apply --reverse --check` first, to detect
   whether it's already applied) via `execute_process()`, so
   `saturn_core/`'s own pinned-submodule git identity stays pristine
   and a fresh `git submodule update --init` still gets the exact
   upstream commit — the patch file is the only thing this repo
   actually commits, exactly the Debian/Homebrew-style mechanism
   recorded as the plan before any of this was attempted.

**Two real bugs found and fixed while getting real-hardware
confirmation, both worth keeping on record:**

- **`hardware_psram` must actually be linked, or PSRAM is silently
  never configured at all.** `hardware/psram.h`'s own doc comment
  states plainly that the static `PICO_PSRAM_SIZE_BYTES`/
  `PICO_PSRAM_CS_PIN` config path (what this board's header sets) is
  **never verified against real hardware** — only
  `PICO_AUTO_DETECT_PSRAM` does that. Worse: this project's first
  real-hardware attempt got a `malloc()` pointer inside the PSRAM
  address range and a write that didn't crash, but read-back
  verification failed — root-caused to `firmware/CMakeLists.txt` not
  yet linking `hardware_psram` at all at that point, meaning
  `psram.c`'s `runtime_init_setup_psram()` (which actually configures
  the QMI peripheral's quad-SPI read/write command format for real
  PSRAM protocol) was never compiled into the binary in the first
  place — the linker region existed, but nothing had ever told the
  QMI hardware how to actually talk to the chip. Fixed by adding
  `hardware_psram` to `target_link_libraries`. Once linked,
  `psram_detect_size()` (a real SPI READ-ID handshake, not just
  config) confirmed a genuine 8 MiB chip, and the malloc/write/read-back
  test passed cleanly. **Because this project's link uses
  `--gc-sections`, and static linking only pulls a `.o` out of an
  archive if some symbol from it is referenced, `main.c` keeps one
  permanent call to `psram_is_available()`/`psram_get_size()`** —
  removing it would silently regress PSRAM back to unconfigured, even
  though the underlying `runtime_init` registration mechanism
  (`.preinit_array.*`-family sections) is itself gc-section-safe once
  linked in.
- **A flash-based on-device debug log (added this session, see below)
  had a real byte-padding bug on first attempt.** Padding an
  in-progress flash write out to a page boundary using the buffer's
  default zero-fill (`0x00`) permanently zeroed those trailing bytes
  in flash, because `flash_range_program()` can only clear bits (1 →
  0) — a later, longer flush could never write real content into
  those same positions again, since they were already fully cleared.
  Fixed by padding with `0xFF` (the erased state) instead, via an
  explicit `memset()` before first use.

**Debugging infrastructure added this session, kept as standing
tools in `firmware/main.c` (not removed once the blocker was found —
proved broadly useful):**
- An LED heartbeat (`PICO_DEFAULT_LED_PIN`, toggled once a second by a
  hardware repeating-timer callback, independent of whatever `main()`
  is doing) — real, physical proof the board is alive versus genuinely
  wedged, which matters because this project's host-machine serial
  terminal has been seriously unreliable (scripted `pyserial`/`stty`
  opens hung on `open()` roughly 2 times out of 3, for reasons never
  fully root-caused on the host side).
- An on-screen scrolling log (`LogLine()`, `sharpdisp`-rendered
  directly to the physical panel) showing exactly what `main()` is
  doing at all times, replacing a fixed `stdio_usb_connected()` wait
  that used to block *all* progress (screen included) on a serial
  terminal actually attaching — removed once it became clear that
  defeated the entire point of a no-terminal-required status channel.
- A flash-persisted full log (`FlashLogAppend()`/`FlashLogFlush()`,
  reserving the last 64 KiB of the Pico Plus 2's 16 MiB flash) readable
  after the fact — even across a reset, or a run that scrolled past too
  fast to read live — via `picotool save -r 0x10FF0000 0x11000000 -t
  bin out.bin -f`. The `-f` forces the board out of application mode
  automatically; no physical BOOTSEL button press needed for this or
  for routine reflashing (`picotool load -f -x`/`picotool reboot -f
  -u`) in the common case, confirmed working repeatedly once the board
  stopped genuinely hanging (see the LED heartbeat bullet above)
  — occasional manual BOOTSEL intervention was only ever needed while
  chasing the actual PSRAM-config bug above, not as a standing
  requirement.

**Confirmed real-hardware LCD state, at the time:** after the
2,000,000-instruction run, `hdw.lcd_base_addr`/`lcd_vlc`/`lcd_menu_addr`
all held real, sane values (not zero/garbage) — the LCD controller
state was genuinely initialized by the ROM's boot code. The physical
panel showed no visible content at that point, but a direct nibble
probe of the emulated LCD memory confirmed 0 of 340 sampled nibbles
were nonzero — the display was correctly rendering a genuinely blank
emulator LCD buffer, not hitting a render bug. Consistent with the CPU
sitting at a near-constant `pc=0x01281` for nearly the entire run
(instruction 20000 onward) — timer-idling, as confirmed and then fixed
immediately below.

**Real, still-open, deliberately deferred question:** whether QMI
PSRAM access latency (`bus_fetch_nibble()` runs on every emulated
instruction) meaningfully slows execution versus on-chip SRAM was
flagged as a real risk before hardware arrived. Not yet benchmarked —
the 2,000,000-instruction run completed well within a few seconds
either way, so it clearly isn't *prohibitively* slow, but no
side-by-side on-chip-SRAM-vs-PSRAM timing comparison has been done.

### Timer-driven execution: real HP48SX UI now rendering, 2026-08-19

Same day as the PSRAM resolution above, immediately following it. The
constant `pc=0x01281` idling noted above was root-caused precisely (not
just guessed): `saturn_core/src/core/emulator.c`'s real `Emulator()`/
`EmulatorLoop()` (never called by `main.c` — Power of 10, Rule 2 means
this project's own bounded `OneStep()` loop is used instead) drives the
real Saturn/HP48 hardware T1/T2 timer registers (`hdw.t1_val`/
`t2_val`/`t1_ctrl`/`t2_ctrl`, `hdw.h:52-57`) between batches of
`OneStep()` calls — real 16 Hz / 8192 Hz hardware timer rates
(`T1_MULTIPLIER = 8192/16 = 512`), decrementing `t2_val` while
`T2_CTRL_TRUN` is set (T1 has no such gate, always ticks), and on
underflow setting a service-request bit plus calling `CpuWake()`/
`CpuIntRequest(INT_REQUEST_IRQ)` per the `T*_CTRL_WAKE`/`T*_CTRL_INT`
bits — all file-local to `emulator.c` (`T1_CTRL_*`/`T2_CTRL_*`
constants, not exported via any header). `main.c`'s static bring-up
loop was never calling any of this, so `hdw.t1_val`/`t2_val` simply sat
frozen at their cold-reset values forever, and the ROM — correctly,
per real hardware behavior — never got the timer interrupt it was
waiting on.

**Fix, entirely in `firmware/main.c`, zero `saturn_core` changes**:
replicated the exact same decrement/underflow/wake logic
`EmulatorLoop()` uses (same control-bit values, same `T1_MULTIPLIER`
real-hardware ratio), copied as a comment-documented block the same
"reference behavior, not vendored logic" way `saturn_lcd.c`'s LCD
decode already was — these are real HP48 hardware register meanings,
not this core's own implementation details. The one place this
necessarily diverges from real hardware: `EmulatorLoop()` paces T2
ticks by wall-clock time (`T2_INTERVAL` ≈ 122 µs); this project's
harness is deliberately bounded by instruction count instead (Rule 2
again — no unbounded/wall-clock-throttled loop), so ticks are paced by
a plain `INSTR_PER_T2_TICK = 10` instruction-count cadence with no
real-hardware equivalent, chosen only to tick "often enough" within
`MAX_INSTR` to give the ROM's own timer setup a real chance to fire.

**Result, confirmed both from the flash-persisted log and physically
on the real display:** the CPU's PC now roams across a wide, varied
range of addresses instead of sitting frozen at one — with a real,
visible periodic pattern later in the run (near-identical PC sequences
recurring roughly every 1,200,000 instructions), consistent with the
calculator settling into a genuine idle/keyboard-scan loop woken by
each timer tick, exactly the real intended HP48 OS behavior. Final
`pc` at instruction 2,000,000 changed run-to-run (`0x0092C` observed),
confirming real, varied execution rather than a fixed landing point.
**The physical Sharp display now shows real, correctly-rendered,
legible HP48SX firmware UI**: "Try to Recover Memory?" with YES/NO
softkey labels at the bottom — the calculator's genuine memory-loss
recovery prompt, real and expected behavior for a cold boot against
completely fresh/empty emulated RAM (there is no saved calculator
state anywhere in this setup, so the ROM correctly detects that and
asks). This is real confirmation, for the first time, of the entire
pipeline working correctly end to end on physical hardware: CPU
execution, the PSRAM-backed memory subsystem, the timer/interrupt
plumbing, and `saturn_lcd.c`'s LCD decode (bit ordering, 3x scaling,
softkey label positioning) all producing genuine, readable calculator
output.

Actually answering the YES/NO prompt needed keyboard input - see
"Interactive keyboard bridge" immediately below for how that was built
(a host-side virtual keyboard over USB serial, not physical GPIO
buttons - the user's explicit direction once soynut's own equivalent
tool was pointed to as the template).

### Interactive keyboard bridge: `firmware/main.c` + `tools/hp48_keyboard_gui.py`, 2026-08-19

Same day again, immediately following "Timer-driven execution" above.
Modeled on soynut's own `tools/hp41_keyboard_gui.py` (a Tkinter
clickable-keyboard photo GUI over USB serial) at the user's explicit
direction, adapted for the real HP48SX/GX keyboard and this project's
own keyboard-injection mechanism - not a straight copy, since the two
projects' underlying key-delivery mechanisms turned out to be
genuinely different (see below).

**Real architecture change, not just an addition**: `firmware/main.c`
previously ran a bounded 2,000,000-instruction loop once, then froze
forever. Interactive use needs a genuinely continuous loop instead -
`main()`'s instruction loop is now unbounded (`while (true) { ... }`),
running until powered off like a real calculator. This is a
deliberate, documented exception to Power of 10's Rule 2 (bounded
loops) - see `DEVIATIONS.md`'s new entry for the full justification
and boundary (only that one loop; everything nested inside it, e.g.
`PollKeyboardInput()`'s per-call byte-reading, stays genuinely
bounded). `MAX_INSTR` is gone; the loop now polls for incoming serial
key commands and redraws the display on a wall-clock cadence
(`REDRAW_INTERVAL_MS = 100`) instead of stopping and rendering once.

**Keyboard-injection mechanism - researched directly from
`saturn_core/src/core/keyboard.c` before writing any code**: the
embedder API is exactly two functions, `KeybPress(int keycode)` and
`KeybRelease(int keycode)` (`keyboard.h`). `keycode` packs a real
HP48 keyboard-matrix row/column as `(OUT_bit << 4) | IN_bit`; the ON
key is a special case (`0x8000`, sets/clears a bit across all OUT
lines at once rather than a normal row/col code). Real HP48SX/GX
keycodes for all 49 keys are copied directly from
`saturn_core/src/emulator_api.c`'s `keyboard48[]` table (the vendored
core's own reference embedder, itself never included by this
project - the same "copy the reference values, don't include the
file" posture `saturn_lcd.c`'s LCD decode already established).
`KeybPress()` fires a real Saturn hardware NMI
(`CpuIntRequest(INT_REQUEST_NMI)`); `KeybRelease()` posts nothing.
Critically, **this turned out simpler than soynut's HP-41 equivalent**:
`KeybPress()`/`KeybRelease()` persist real state directly in the
emulator's own keyboard matrix between calls, so a key stays "held"
for exactly as long as it takes to call `KeybRelease()` - no
sustain/re-assert plumbing is needed the way soynut's
`hp41_key_hold_bridge.c` needed for the HP-41's `dokey()` polling
architecture, and no tap-vs-hold threshold-timing protocol is needed
in the GUI either. A mouse-down sends a press immediately; a mouse-up
sends a release immediately - full stop.

**Wire protocol**, correspondingly simpler than soynut's: exactly 3
raw bytes per message, `'P'`/`'R'` followed by 2 uppercase-hex-digit
ASCII chars encoding the keycode. Every real row/col keycode fits in
one byte (max `0xBF`); the wire byte `0xFF` is reserved to mean the ON
key, since its real code (`0x8000`) doesn't fit. No newline or framing
byte needed - `firmware/main.c`'s `PollKeyboardInput()` parses exactly
3 bytes per message and resyncs cleanly on any unrecognized byte
(Power of 10, Rule 2/3: bounded work per call, no misinterpreted
partial input), the same recovery posture soynut's own
`hp41_key_bridge.c` established for its own escape-sequence protocol.
Keypress logging during interactive use uses plain `printf()` (serial-
only), never `LogLine()` - `LogLine()`'s `LogRedraw()` unconditionally
overwrites the physical panel with the debug log, which was exactly
the point during boot but would otherwise clobber the real calculator
UI on every single keypress once rendering has started.

**`tools/hp48_keyboard_gui.py`'s `KEY_MAP` (49 real keys, not
eyeballed)**: derived the same diligent way soynut's own image-based
hit-boxes were - a gridded overlay of `HP48GXkeyboard.jpg` (50px minor
gridlines, 200px major gridlines with coordinate labels burned in) was
used to read every key's real edges directly off the photo, then every
computed hit-box was rendered back onto the full photo and visually
confirmed to land tightly on its own key with zero overlaps across all
49 keys before being trusted. Two real column pitches were found and
used: a tighter 280px pitch across the top 5 rows (the A-F softkeys
through the ENTER row, 6 columns - ENTER itself spans the physical
width of two columns), and a wider ~350px pitch across the bottom 4
rows (the digit/operator block, 5 columns) - the same "more columns =
tighter pitch" pattern soynut's own HP-41 photo showed, just with the
tight/wide rows in the opposite order (HP48's tight block is on top;
HP-41's was on the bottom).

**`HP48GXkeyboard.jpg`'s license**: a photograph by Clemens Pfeiffer
(uploaded by Panoramafotos.net) via Wikimedia Commons -
<https://commons.wikimedia.org/wiki/File:HP48GX.jpg> - licensed CC
BY-SA 3.0, confirmed directly from the Commons file page rather than
assumed. Cropped to just the keyboard region by this project's own
user before being added to the repo; redistributing it (or this repo)
must keep this attribution and the CC BY-SA 3.0 license per its terms.

**One-click launcher**: `tools/run_keyboard_gui.sh`, modeled on
soynut's own `sim/run_with_gui.sh` but much simpler - that script
starts a host-native simulator process and auto-discovers its virtual
serial port before launching the GUI; Cassini has no host-native
simulator yet (`sim/` is still empty), and the emulator runs on the
real physical board, so this just runs
`python3 hp48_keyboard_gui.py` directly - `find_port()` already
auto-detects the board's USB serial port on its own. Confirmed
working: launched cleanly, found the port, connected, and opened the
window with no errors.

**Verified working, with one real bug found and left open**:
- A neutral key (N7, `0x33`) presses and releases cleanly, confirmed
  via a real scripted serial test (not the GUI itself, which needs a
  human to click) - `cassini: key P 0x0033` / `cassini: key R 0x0033`,
  no adverse effects.
- **NO** (`0x80`) on the "Try to Recover Memory?" prompt works
  correctly end to end, including this project's own safety net: it
  reaches a real bad opcode in the ROM's own memory-recovery code at
  `PC=0x010E6` (a genuine gap in the vendored core's opcode coverage,
  not something this project can fix without editing `saturn_core`),
  and `BadOpcodeHandler`'s `ChfSignal`/`longjmp` unwind catches it
  correctly on the very first hit - `"hit bad opcode 0x12D at
  pc=0x010E6, halted"`, a clean, controlled stop.
- **YES** (`0x14`) reaches the exact same `PC=0x010E6` address, but the
  unwind does **not** engage - instead of stopping on the first hit,
  execution keeps running through a long, exactly-repeating cycle of
  bad-opcode errors (confirmed reproducible across multiple clean
  reboots, same PC sequence every time) for as long as observed (a
  fixed, several-second test window), never reaching the "halted"
  message. The condition code matches `CPU_E_BAD_OPCODE` (301)
  identically in both the YES and NO cases, so this isn't an obviously
  different failure mode being hit - something about the real code
  path YES's softkey dispatch takes causes this project's own
  `ChfPushHandler`/`ChfSignal` safety net to stop engaging partway
  through, not yet root-caused. **Left open, deliberately paused here
  at the user's request** rather than continuing to dig blind - a real
  next-session investigation, not a wire-protocol or architecture
  problem (both of those are independently confirmed working via the
  N7 and NO tests above).

### YES-path bad-opcode investigation, 2026-08-19 - not yet root-caused

Same day again. A real, deep root-cause investigation, not just more
reproduction - real progress was made (several false leads found and
correctly ruled out rather than assumed, two genuinely distinct
failure modes discovered, one of them fully captured with real
register-level state), but the actual root cause is still open,
paused at the user's request rather than continuing to dig without the
hardware debugging tools (a real SWD/GDB probe) this now likely needs.

**Host reproduction never worked, and the reason why turned out to be
the first real finding.** A standalone host-side repro tool
(`/private/tmp/.../keypress_debug.c`, not committed - scratch, built
against `tests/build/`'s object files the same way
`tests/saturn_smoke_test.c` is) replicated `main.c`'s exact timer-tick
logic, warmed up to a fixed 2,000,000 instructions (landing at
`pc=0x0092C`, matching the *old*, since-removed bounded harness's own
snapshot), then called `KeybPress(0x14)` - and swept both press-only
and press+release timing (1 to 10,000 instructions apart) without ever
reproducing a single bad opcode. **The flaw was the fixed 2,000,000
warmup itself**: once real hardware CPU-state tracing was added (see
below) and captured what `firmware/main.c` was actually doing right
before a real keypress arrived, its `before-P` snapshot showed
`pc=0x04930` - not `0x0092C`. The old bounded harness stopped at
exactly 2,000,000 instructions; the new continuous one doesn't stop at
all, so by the time a real USB keypress physically arrives (however
many real seconds after boot), the CPU has run for an genuinely
unpredictable number of instructions beyond that, cycling through
different idle-loop phases. The host tool's fixed-instruction-count
methodology was measuring a state that real hardware never actually
occupies at keypress time - a real, instructive dead end, not the bug
itself.

**Real theories investigated and correctly ruled out, not assumed
away:**
- **Stale `tests/build/` objects** (predating `saturn_core.patch`) -
  real, found, fixed (full rebuild), but not the cause of this bug.
- **Periodic LCD-memory reads** (matching `main.c`'s real redraw
  cadence) as a possible source of read-side-effects - added to the
  host repro, no change.
- **`char` signedness** - ARM EABI (`arm-none-eabi-gcc`, this
  project's actual firmware target) defaults `char` to *unsigned*;
  macOS clang (the host, where `tests/saturn_smoke_test.c` has always
  passed cleanly) defaults it to *signed* - confirmed directly via
  each compiler's own `__CHAR_UNSIGNED__` predefine, not assumed.
  `Nibble` is `typedef char int4` (`types.h`), so this looked like a
  strong, well-evidenced candidate. **Disproven by direct test**:
  forcing `-funsigned-char` on a from-scratch host rebuild (matching
  ARM's default) still could not reproduce the bug across the same
  timing sweep. A real near-miss - a `-fsigned-char` fix was drafted
  and then reverted once the validation test failed, rather than
  shipped on the strength of the theory alone.
- **Corrupted/duplicated wire-protocol delivery** from the host's own
  flaky serial connection - checked directly against a real failure
  log; exactly one clean `P`/`R` pair, no duplication.
- **ROM content mismatch** between the raw `.rom` file used for host
  testing and the array actually embedded in `roms/rom_images.c` -
  MD5-verified byte-identical.
- **Uninitialized-variable bugs**, checked for directly via a one-off
  strict-warnings (`-Wall -Wextra -Wuninitialized -Wmaybe-uninitialized`)
  compile of the relevant `saturn_core` files under the real ARM
  toolchain - nothing surfaced.

**Real hardware CPU-state tracing, built once host repro was
understood to be unrepresentative** (all "TEMPORARY diagnostic" in
`firmware/main.c`, kept in place rather than removed - see below for
why): `DumpCpuState()` records `pc`/`p`/`hst`/carry/`int_enable`/
`int_service`/`int_pending`/the full 8-entry return stack/both data
pointers - not just PC - to a trace, callable cheaply and often enough
to actually catch a divergence. Wired into `PollKeyboardInput()`
(before/after every press and release) and the main loop (every 20
instructions for 3,000 instructions after a keypress,
`TRACE_STEP_INTERVAL`/`TRACE_LENGTH_INSTR`).

**First tracing attempt wrote straight to flash (like the boot log) -
and that choice itself made the bug stop reproducing.** Real
`flash_range_erase()`/`flash_range_program()` calls take real
wall-clock time; interleaving ~150 of them into the timing-sensitive
window after a keypress shifted that same timing enough to dodge the
race being investigated (0 reproductions across another full sweep,
this time with the *fix* for the false char-signedness lead already
reverted, so genuinely the tracing itself was the perturbation).
**Reworked to trace into a plain heap buffer instead** (this project's
heap is PSRAM-backed - see `CASSINI_PSRAM_HEAP` - so this costs a fast
RAM write, no erase, no program-time delay) via `PsramTraceAppend()`
into a 512 KiB circular buffer (`psram_trace`), with the *only* flash
write deferred to `FlushTraceToFlash()` - called once, reactively,
after the observation window has already fully elapsed
(`trace_remaining` reaching 0), never interleaved with the
timing-sensitive window itself.

**This surfaced a second, genuinely distinct failure mode.** Repeated
press/release attempts within a single boot session (sending several
in a row, since single isolated attempts kept landing on lucky timing)
did catch a real failure - but on that occasion `picotool`'s own
force-BOOTSEL reset failed outright, and the flash log came back with
only the original boot messages, meaning `FlushTraceToFlash()` never
ran. This is different from the earlier-observed failure mode (a long,
looping bad-opcode cascade where the main loop keeps running, main
loop progress keeps advancing, and `FlushTraceToFlash()` fires
normally at the end of the window) - this was a **true, total lockup**.

**Added a watchdog to try to catch the true-lockup case too**:
`main_loop_progress` (a `volatile long`, incremented once per main
loop iteration) is watched by the existing `LedHeartbeat()` repeating-
timer callback (already running independently, in an alarm IRQ
context, once a second) - if it goes 2+ heartbeat ticks without
changing, `LedHeartbeat()` itself calls `FlushTraceToFlash()` directly
from IRQ context (flash operations are already interrupt-safe
regardless of caller - both `flash_range_erase()`/`_program()`
disable interrupts for their own duration). **This did not capture
data either** - a repeated true-lockup reproduction still came back
with only the original boot log, meaning even the independent hardware
timer interrupt never got to run its stall-detection check.

**Working conclusion, not yet confirmed**: since a hardware timer IRQ
failing to fire at all - not just the main loop failing to progress -
implies something is masking interrupts globally, not just occupying
the main thread of execution, this strongly suggests a genuine ARM
Cortex-M33 fault (a hard fault from an invalid memory/instruction
access, most plausibly reached via the same corrupted execution the
bad-opcode cascade already shows) with **no fault handler installed**
- on this architecture, an unhandled fault typically raises execution
priority to the point that ordinary interrupts (including this
project's own LED-heartbeat timer) can't preempt it, matching exactly
what was observed. The natural next diagnostic - installing a minimal
`HardFault_Handler` that captures the fault status registers and PC
directly, since fault handlers run at a priority high enough to still
execute even when everything else is masked - was proposed and
**deliberately not done, paused at the user's explicit request**
rather than continuing without confirming this theory first.

**Kept in place, not reverted, once the session paused**: the LED-
heartbeat stall watchdog, `main_loop_progress`, the PSRAM trace buffer
and its `DumpCpuState()`/`PsramTraceAppend()`/`FlushTraceToFlash()`
machinery, and the `before-P`/`after-P`/`before-R`/`after-R`/`trc`
call sites in `PollKeyboardInput()` and the main loop - all still
marked `TEMPORARY diagnostic` in comments, but real, working
infrastructure that took real effort to get right (especially the
flash-timing-perturbation lesson) and will very likely be needed again
for whatever comes next on this bug, the same "kept as a standing
debug facility" precedent the flash-persisted boot log itself already
established.

## ROM images — bring your own

**The ROM files themselves are not in this repo, and never should be.**
Same posture as soynut's HP-41 ROMs: copyrighted Saturn-family firmware,
not ours to redistribute. `roms/*.ROM`/`.rom` and any generated C source
are gitignored (see `.gitignore`). The target format is confirmed from
`saturn_core`'s own ROM-loading code (see "The Saturn CPU core" above),
now down to the exact per-model byte counts (`romram48.c:124-128`'s
`RomInit48()` reads a different amount for each 48-series model, not a
single shared "HP48-series" figure as earlier phrasing here implied):

- Flat binary, no header, **2 MiB (2,097,152 bytes)** for an
  HP40G/49G/50G Flash ROM dump.
- **256 KiB (262,144 bytes)** for an HP48SX ROM dump.
- **512 KiB (524,288 bytes)** for an HP48GX ROM dump.
- Each on-disk byte unpacks to two 4-bit nibbles in memory, low nibble
  first.
- `roms/README.md` has the full current BYO instructions, including how
  to run `tests/saturn_smoke_test.c` against a real ROM once you have
  one (see "Native (host) tests" above). A converter that embeds a ROM
  as C source for the actual Pico firmware build doesn't exist yet -
  that's later firmware-bring-up work, not needed for the host smoke
  test, which reads the ROM file directly.
