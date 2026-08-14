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
`sharpdisp/` into a real `firmware/` Pico SDK project) is in progress
and currently blocked on a real hardware memory constraint, not a
code bug.** The display-scaling decision from "Hardware" below is
resolved (3x nearest-neighbor, 393x192 centered in the 400x240 panel,
top 24px margin reserved for annunciator text labels — the user's
explicit direction). A real `firmware/` project exists, builds cleanly,
flashes successfully, and its `EmulatorInit()` cold-boot cascade
matches the host smoke test's own proven WARNING-then-cold-reset
behavior exactly — but it then panics with `Out of mem` while loading
the ROM. Root cause, confirmed precisely: the vendored core stores each
emulated nibble as a full byte (`typedef char Nibble`, deliberate but
memory-inefficient — fine for the desktop-scale RAM this core was
originally written against), so the unpacked 256 KiB HP48SX ROM alone
needs 512 KiB of RAM — **exactly 100% of the Pico 2's entire 512 KiB
SRAM** (confirmed directly from the linked binary's own memory map,
not estimated), before counting the emulator's internal RAM/Port 1
buffers, this project's own static data, the stack, or heap overhead.
See "Native firmware (`firmware/`)" below for the full technical
account — the newlib syscall shim used to embed the ROM, the build
fixes required, the debugging process, and the precise blocker with
the real options for resolving it (none yet chosen — paused for a
decision, since every real fix requires either editing vendored core
logic more deeply than this project has ever done, or different
hardware).

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
- **`tools/`** — will hold native diagnostic tools plus a Tkinter
  clickable-keyboard GUI, mirroring soynut's `tools/`.
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

Build:

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
pattern as `tests/saturn_smoke_test.c`), render once, idle. No
keyboard, no real-time timer-driven loop — there's no physical keyboard
hardware for this project yet, so real-time interactivity is its own
later phase.

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

### The `stdio_usb_connected()` gotcha

`main.c` originally used a fixed `sleep_ms(1500)` before its first
`printf()`, guessing that was "enough time" for a USB serial terminal
to attach. `pico_stdio_usb`'s CDC output is **silently dropped** when
written before a real terminal has connected — no fixed delay can
guarantee that in general, and it cost real debugging time (several
flash-wait-check cycles showing "nothing" over serial that were fully
explained by this, not a firmware bug). Fixed by blocking on
`stdio_usb_connected()` (`pico/stdio_usb.h`) in a loop before printing
anything, so no boot output is ever lost regardless of how long the
user takes to attach a terminal — a real, permanent fix, not a
debugging-only hack, kept in place.

### Current blocker: ROM memory footprint exceeds the Pico 2's RAM

**Hardware ordered to resolve this: a Pimoroni Pico Plus 2 (RP2350B +
8 MiB PSRAM), in transit as of 2026-08-13** — see "PSRAM research"
below for what's already been confirmed while waiting for it to
arrive, including a fifth option better than any of the original four.

The build is fully clean (strict warnings on this project's own files,
same split as everywhere else) and flashes successfully. At runtime it
correctly reproduces the exact same cold-boot WARNING cascade the host
smoke test already proved (`ERROR: Can't open file
[/nonexistent-cassini-state-path]` / `WARNING: Can't restore CPU status
from disk; resetting CPU`) — real evidence the newlib syscall shim and
compat layer are wired correctly. It then panics: `*** PANIC *** / Out
of mem`.

Root cause, confirmed precisely (not estimated): `romram48.c:117` does
one `malloc(sizeof(struct BusStatus_48))` call. That struct's fields
(`bus.h:183-196`) are ROM 1 MiB (`N_ROM_SIZE_48` — sized for the
*larger* HP48GX ROM regardless of which model is selected at runtime,
since model choice is a runtime `ui4x_config.model` value, not a
compile-time branch), RAM 256 KiB, Port 1 256 KiB, and **Port 2 8 MiB**
(`N_PORT_2_BANK_48` hardcoded to `32` — `bus.h`'s own comment says the
intended default was `8` (1 MiB), with a commented-out
model-conditional line directly above showing the original design,
`config.model == MODEL_48GX ? 32 : 1`, disabled at some point upstream
in favor of the unconditional value. HP48SX doesn't have this card slot
in real hardware at all.) Total: **~9.5 MiB requested in one call**,
against the Pico 2's actual total SRAM, confirmed directly from the
linked binary's own memory map (`RAM 0x20000000, length 0x00080000` =
exactly 512 KiB, not estimated from a datasheet).

Eliminating Port 2 entirely and right-sizing the other three constants
to real HP48SX-accurate values is **necessary but not sufficient**: the
vendored core stores every emulated nibble as a full byte
(`typedef char int4; typedef int4 Nibble;`, `types.h`) — deliberate,
but memory-inefficient, evidently fine for the desktop-scale RAM this
core was originally written against. The 256 KiB HP48SX ROM file,
unpacked at 1 byte per nibble, needs **512 KiB just for the ROM array
alone — exactly 100% of the Pico 2's entire 512 KiB SRAM**, before
counting the emulator's own RAM/Port 1 buffers, this project's own
static data (~18 KiB, confirmed via `arm-none-eabi-size`), the stack,
or heap overhead.

**Real options, none yet chosen — paused for a decision:**
1. Pack 2 nibbles per byte instead of 1 (halves ROM storage to a
   fitting 256 KiB) — touches how the core addresses memory throughout
   `bus_fetch_nibble()`/`bus_write_nibble()` and likely other call
   sites, not just size constants; a materially bigger, riskier edit to
   vendored logic than a constants-only patch.
2. Reference the ROM directly from flash (XIP) instead of copying it
   into RAM at all — the "right" embedded-systems answer (Pico has
   4 MiB of flash; ROM is read-only data), but requires changing `rom`
   from a fixed in-struct array to a pointer and adjusting how it's
   addressed — also a deeper structural change than constants alone.
3. Different/bigger hardware — a board with add-on PSRAM would sidestep
   this without touching vendored code at all.
4. Reconsider scope — accept that a full real-ROM boot isn't achievable
   on this specific board with the core's current architecture as-is.
5. **(Added after ordering PSRAM hardware, see "PSRAM research" below)**
   Relocate the newlib `malloc()` heap itself into PSRAM, instead of
   changing what or how `saturn_core` allocates. If it works, this is
   the only option of the five that requires **zero edits to the
   vendored core** — `romram48.c`/`romram49.c`'s existing `malloc()`
   calls would simply succeed unmodified. Not yet verified — needs the
   real board.

Whichever path is chosen, it will be **this project's first edit to
the vendored `saturn_core` submodule** (options 1-2) or a hardware
change (option 3) or a scope change (option 4) — a real departure from
the "vendored; never edited directly" posture stated since Phase 1, not
something to decide unilaterally. If an edit is chosen, the
recommended mechanism is a maintained patch file (e.g.
`saturn_core.patch`, tracked in this repo, applied as a build step)
rather than editing the submodule's checked-out files directly — that
keeps `saturn_core/`'s own git identity pristine (a fresh
`git submodule update --init` still gets the exact pinned upstream
commit) while the modification stays fully visible/auditable as a diff
in this repo, the same pattern Debian packages and Homebrew formulas
use for "vendor a dependency, need one small patch." Option 5 is the
one exception to that framing — if it works, there's no vendored-code
edit and no patch file to maintain at all.

### PSRAM research (while hardware is in transit)

Two things confirmed by direct source inspection, not assumption,
while waiting for the Pico Plus 2 to arrive:

**`x48` was re-checked and shares the identical memory problem — not a
viable escape hatch.** Re-cloned it fresh and read `romio.c` directly:
`unsigned char *rom` and its own unpacking logic (`*size = 2 *
st.st_size` when a packed-format ROM is detected) confirm the exact
same one-byte-per-nibble in-memory representation as `saturnng`.
Switching cores would hit the identical inflation, while also losing
49G/50G support entirely and `saturnng`'s cleaner core/UI separation.
This appears to be an inherited convention across the whole
Saturn-emulator lineage, not a `saturnng`-specific mistake — reinforces
that the fix has to be about *where*/*how* memory is stored, not which
core is vendored.

**The HP49G/50G Flash-ROM struct is actually smaller than the
48-series one, under today's unmodified code** — a real reframing of
which model is "harder," worth contrasting with the 48-series numbers
above. Pulled straight from `bus.h`:

```c
#define N_FLASH_SIZE_49  2048 * 1024 * 2   /* 4 MiB, 1 byte/nibble */
#define N_RAM_SIZE_49    512 * 1024 * 2    /* 1 MiB */

struct BusStatus_49 {
    Nibble flash[N_FLASH_SIZE_49];
    Nibble ram[N_RAM_SIZE_49];
    Nibble *ce2, *nce3;
};
```

`romram49.c:134` mallocs this whole struct in one call — **~5 MiB**,
confirmed, versus the 48-series' ~9.5 MiB. The difference is entirely
the 48-series' oversized, unconditional 8 MiB `N_PORT_2_BANK_48`
constant (a card slot the HP48SX doesn't even have in real hardware) —
the 49/50G bus table has no equivalent field. So under this core's
current memory design, HP50G is not the more memory-hungry target;
HP48SX/GX (as currently constant-sized) is.

**The local Pico SDK checkout (2.3.0) already has first-class,
mainline support for this exact board and for PSRAM in general** —
confirmed by reading the SDK tree directly:
- `src/boards/include/boards/pimoroni_pico_plus2_rp2350.h` already
  exists upstream, defining `PICO_PSRAM_CS_PIN` (GPIO 47) and
  `PICO_PSRAM_SIZE_BYTES` (8 MiB) via `pico_board_cmake_set_default` —
  no custom board file needed.
- `src/rp2_common/hardware_psram/` (`psram.c`/`psram.h`) is a real,
  mainline library: `psram_is_available()`, `psram_detect_size()`,
  `__in_psram`/`__uninitialized_psram` placement attributes, a
  `psram_or_malloc()` convenience macro. PSRAM init already runs
  automatically during `runtime_init` (stage `"11080"`) unless
  explicitly skipped.
- PSRAM is mapped as its own linker region:
  `pico_psram_region.template.ld` → `PSRAM(rwx): ORIGIN = 0x11000000,
  LENGTH = ${PICO_PSRAM_SIZE_BYTES}`.

**Critical nuance found the same way: PSRAM is not automatically part
of the `malloc()` heap.** `section_heap.incl` hardcodes the `.heap`
section to `> RAM` (ordinary on-chip SRAM), with an optional
`HEAP_LOC`/`HEAP_LIMIT` linker-symbol override for exactly where it
starts/ends — nothing merges PSRAM into it by default. So simply
swapping boards and enabling PSRAM will **not**, by itself, make
`saturn_core`'s existing `malloc()` call succeed; something has to
route that allocation into PSRAM. That something is Option 5 above:
override `HEAP_LOC`/`HEAP_LIMIT` at link time to point the whole heap
into the `0x11000000`-based PSRAM region. Real, honest caveat:
`bus_fetch_nibble()` runs on every single emulated instruction, so if
ROM/RAM end up living in PSRAM instead of on-chip SRAM, QMI PSRAM
access latency could meaningfully slow execution — this needs
benchmarking on the real board, not assumed to be free. None of this
can be verified without the physical chip (writes to `0x11000000`
bus-fault with nothing backing them right now), so Option 5 stays
recorded as the leading candidate to try first once hardware arrives,
not yet adopted.

`firmware/CMakeLists.txt` already has draft, off-by-default groundwork
for this (`CASSINI_PSRAM_HEAP` CMake option, default board updated to
`pimoroni_pico_plus2_rp2350`) — explicitly untested until the board is
in hand.

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
