# Saturn-Family HP Calculator Replica Project — Cassini

Real Saturn-CPU-family calculator emulation running on a Raspberry Pi
Pico 2, driving a real Sharp LS027B7DH01 memory LCD (400x240, on an
Adafruit breakout board, #4694). Named for the Cassini space probe,
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

**Phase 1 (vendor the core) and most of Phase 2 (native host smoke
test) are done; no hardware bring-up has happened yet.** `saturnng` was
selected after cloning and reading both candidates directly, and is
vendored at `saturn_core/` as a pinned git submodule (see "The Saturn
CPU core" below). On top of that, `firmware/saturn_compat/` now has
real shim code — a from-scratch, bare-metal-portable implementation of
the five-function libChf surface (`ChfStaticInit`/`ChfGenerate`/
`ChfPushHandler`/`ChfSignal`/`ChfExit`) `saturn_core/src/core/` calls
through `chf_wrapper.h`'s macros, plus the `ui4x_config` global and
`ui4x_make_filename_absolute_for_loading/saving()` callbacks that stand
in for the deliberately-uninitialized `ui4x` submodule — and
`tests/saturn_smoke_test.c` + `tests/Makefile` build and link the
vendored core against those shims into a real host binary
(`make -C tests run ROM=... MODEL=48sx`). It's been run end to end
against both a missing-optional-state path (WARNING, falls through to
a clean `CpuReset()`, exactly as intended) and a missing-ROM path
(FATAL, clean `exit(1)`) — real evidence the wiring is correct — but
not yet against an actual HP48SX/HP48GX ROM dump, since none exists on
this machine and ROMs are BYO by policy (see `roms/README.md`). See
"Native (host) tests" below for how to finish that validation once a
real ROM is available.

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
  binary, wiring the Saturn core and `sharpdisp/` together. Not yet a
  Pico SDK project (no `CMakeLists.txt`/pico-sdk wiring exists), but
  `firmware/saturn_compat/` already holds real build-compatibility
  shims for the vendored core (`chf_compat.c`, `ui4x_compat.c`,
  `config_compat.c`, `saturn_compat.h`, `ui4x/src/api.h`), mirroring
  soynut's `emu41gcc_compat/` — see "Native (host) tests" below and
  "Current status" above for what they cover and how they were
  verified. Written host-portable on purpose (POSIX libc only, no Pico
  SDK calls) so the same shims should carry over largely unchanged once
  firmware bring-up starts; not yet exercised on-target, so treat that
  as an assumption, not a confirmed fact.
- **`lcd_bringup/`** — will become a standalone Pico SDK project (no
  dependency on the Saturn core or a ROM) for isolated Sharp-display
  bring-up, mirroring soynut's `lcd_bringup/`.
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
- **MCU:** Raspberry Pi Pico 2 (RP2350), raw Pico C SDK — same choice as
  soynut, for the same reason (RAM/flash headroom over an Uno-class
  board).
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
  gated by the compile-time macro `N_PORT_2_BANK_48`, which is
  unconditionally defined to 32 (`bus.h:194`; the model-conditional
  version is commented out directly above it) — so it's fully compiled
  and exercised for **both** models regardless. 48SX and 48GX are
  therefore code-path-identical in this emulator core, differing only by
  ROM byte count. **HP48SX and HP48GX are tied for simplest** — pick
  either.

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

**Verified so far, without a real ROM:** the harness builds and links
cleanly, and running it against a nonexistent ROM path exercises both
Chf severity paths correctly — a missing optional state file
(`cpu`/`mod`/...) signals `WARNING` and falls through to a clean cold
`CpuReset()`/`bus_reset()` exactly as `EmulatorInit()` intends, while a
missing ROM file signals `FATAL` and the shim's default
no-handler-registered policy cleanly `exit(1)`s — real evidence the
`ChfGenerate`/`ChfSignal`/handler-stack wiring behaves correctly in
both the "recoverable" and "unrecoverable" cases. **Not yet verified:**
an actual pass/fail run against a real HP48SX/HP48GX ROM dump, since
none exists on the machine this was developed on and ROMs are BYO by
policy. That's the immediate next step whenever a real ROM is
available.

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
