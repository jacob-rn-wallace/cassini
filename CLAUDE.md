# HP 50g Replica Project — Mercator

Real Saturn-CPU-family calculator emulation running on a Raspberry Pi
Pico 2, driving a real Sharp LS027B7DH01 memory LCD (400x240, on an
Adafruit breakout board, #4694) wired to show the real HP50G's native
131x80 graphics (confirmed — see "The Saturn CPU core" below) scaled 3x
(393x240) to exactly fill the display's pixel height, with keypresses
coming from a computer over USB serial for now (a physical keyboard is a
possible future step).

Structured as a sibling project to `soynut` (an HP-41CV replica) —
soynut is this project's structural template: same target MCU, same
"vendor an existing CPU emulation core, BYO ROM, never edit the vendored
core" approach, same directory shape, same coding-standard commitment.
Nothing in soynut is copied wholesale; its *conventions* are re-derived
here since the Saturn/50g-specific logic underneath is entirely
different from the HP-41's Nut CPU.

This is the stable reference doc: confirmed architecture, current
hardware/software state, and build/run instructions — read this first,
whether you're a person or a Claude instance picking up this repo cold.
Session-by-session development history belongs in `DEVLOG.md`, which is
gitignored and local-only (not yet created — this repo has no session
history yet).

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

**Scaffold plus a vendored Saturn CPU core.** No firmware has been
written yet and no hardware bring-up has happened, but Phase 1 of the
project plan (verifying and vendoring the Saturn core) is done: real
source for both candidates was cloned and read directly, `saturnng` was
selected, and it's vendored at `saturn_core/` as a pinned git submodule.
See "The Saturn CPU core" below for the confirmed facts this decision is
based on.

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
  driver for the Sharp LS027B7DH01 on a Pico 2. Not yet copied in.
- **`firmware/`** — will become the Pico SDK project: the real replica
  binary, wiring the Saturn core and `sharpdisp/` together.
  `firmware/saturn_compat/` will hold build-compatibility shims for the
  vendored core, mirroring soynut's `emu41gcc_compat/`.
- **`lcd_bringup/`** — will become a standalone Pico SDK project (no
  dependency on the Saturn core or a ROM) for isolated Sharp-display
  bring-up, mirroring soynut's `lcd_bringup/`.
- **`roms/`** — will hold a BYO Saturn ROM converter + README once the
  chosen core's expected ROM format is known. The ROM files themselves
  will never be in this repo — copyrighted Saturn-family firmware, not
  ours to redistribute.
- **`tests/`** — will hold native (host, no Pico SDK) tests, plain
  Makefile, mirroring soynut's `tests/Makefile`.
- **`sim/`** — will hold a host-native full simulator, later phase,
  mirroring soynut's `sim/`.
- **`tools/`** — will hold native diagnostic tools plus a Tkinter
  clickable-keyboard GUI, mirroring soynut's `tools/`.
- **`reference-material/`** — datasheets/mockups, not read by any build
  step. Gitignored (unlike soynut, where it's tracked) — pending a
  decision on what belongs here and whether any of it needs to be
  redistributable.
- **`pico-sdk/`** — official `raspberrypi/pico-sdk` checkout (dependency,
  not yet fetched), gitignored, same convention as soynut.
- **`toolchain/`** — extracted ARM GNU Toolchain (gitignored), same
  no-sudo `pkgutil --expand-full` workaround soynut's `CLAUDE.md`
  documents, once needed.

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
**not** initialized in this repo's vendoring — Mercator only needs
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
— doesn't yet, unlike 40g/48gx/48sx/49g). This means Mercator can target
the **real HP50G display geometry directly**, without needing a
different (49G-based) scale factor as a stand-in — see "Hardware" below.
Whether `saturnng`'s Flash-based ROM/RAM subsystem (shared between
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
architecture Mercator specifically needs.

## Hardware

- **Display:** Sharp LS027B7DH01, 400x240, on an Adafruit breakout board
  (#4694). Confirmed working already (not by this project — by prior
  work at `pico_sharpmem_display-main`, being vendored into
  `sharpdisp/`): 3 signal pins needed (CS/SCK/MOSI), board handles
  level-shifting/regulation from 3.3V logic on-board, default pinout
  CS=GP17/SCK=GP18/MOSI=GP19/spi0.
- **MCU:** Raspberry Pi Pico 2 (RP2350), raw Pico C SDK — same choice as
  soynut, for the same reason (RAM/flash headroom over an Uno-class
  board).
- **Scaling:** HP50G's native 131x80 (confirmed — see "The Saturn CPU
  core" above) scaled by an exact integer factor of 3x, giving
  393x240 — an exact fit against the Sharp display's 240-pixel height
  (0px vertical margin), with 7px of horizontal slack (393 of 400) to
  either center/letterbox or fill edge-to-edge, a decision for the
  display-bridge implementation phase, not blocking anything now.

## ROM images — bring your own

**The ROM files themselves are not in this repo, and never should be.**
Same posture as soynut's HP-41 ROMs: copyrighted Saturn-family firmware,
not ours to redistribute. `roms/*.ROM`/`.rom` and any generated C source
are gitignored (see `.gitignore`). A converter script and full
instructions aren't written yet (that's Phase 2 work), but the target
format is now confirmed from `saturn_core`'s own ROM-loading code (see
"The Saturn CPU core" above):

- Flat binary, no header, **2 MiB (2,097,152 bytes)** for a 49G/50G
  Flash ROM dump (512 KiB for HP48-series).
- Each on-disk byte unpacks to two 4-bit nibbles in memory, low nibble
  first.
- `roms/README.md` has the current placeholder text; it gets the full
  BYO instructions once the converter script exists.
