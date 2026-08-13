# HP 50g Replica Project — Mercator

Real Saturn-CPU-family calculator emulation running on a Raspberry Pi
Pico 2, driving a real Sharp LS027B7DH01 memory LCD (400x240, on an
Adafruit breakout board, #4694) wired to show the calculator's native
131x80 graphics scaled 3x (393x240) to exactly fill the display's pixel
height, with keypresses coming from a computer over USB serial for now
(a physical keyboard is a possible future step).

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

**License:** not yet finalized — depends on which Saturn CPU core ends
up vendored (see "The Saturn CPU core" below) plus the vendored
`sharpdisp/` display library's confirmed LGPL-2.1 terms. Will be set
once the core's real license is read directly from its source, the same
diligence soynut applied before adopting GPL-2.0-or-later to match
`emu41gcc`'s own terms.

## Current status

**Not yet started.** This file exists only as a scaffold — no firmware
has been written, no Saturn core has been vendored yet, and no hardware
bring-up has happened. See the project plan for the phased roadmap this
repo is following.

## Coding standard: NASA/JPL "Power of 10"

Adopted from the start (not retrofitted later, unlike soynut's own
history), for the same reasons soynut gives: not certification, just a
strong discipline for long-term legibility. Applies to this project's
own original code only — never the vendored Saturn core (once chosen)
or the vendored-by-copy `sharpdisp/` display library. See
`DEVIATIONS.md` for the authoritative, currently-empty exception list,
and soynut's `CLAUDE.md` for the full rule-by-rule rationale this
project intends to mirror once real code exists to apply it to.

## Directory map

Scaffolded per the project's bootstrap plan, mirroring soynut's layout:

- **`saturn_core/`** — will become a git submodule for the vendored
  Saturn CPU emulation core, once one is chosen and verified (see
  below). Currently empty.
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

**Not yet chosen or vendored.** Leading candidate: `saturnng`
(`github.com/gwenhael-le-moine/saturnng`), which explicitly emulates
HP49G + HP48GX/SX + HP40G and has already had its UI decoupled once
(X11 → SDL2/ncurses). Documented fallback: `x48`
(`github.com/czodroid/x48`, GPL-2.0 confirmed, older/simpler, HP48SX/GX
only as far as currently known). Neither has had its actual license text,
real core/UI separation, HP49G's true native resolution, or its exact
Saturn ROM binary format confirmed yet by reading real source — this is
the single biggest open risk in the project and the first real
implementation step. This section gets filled in with confirmed facts
once that reading happens; nothing here should be treated as settled
until it cites something actually read from the chosen core's own
source.

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
- **Scaling:** target calculator's native graphics (expected 131x80,
  unconfirmed — see "The Saturn CPU core" above) scaled by an exact
  integer factor (3x, giving 393x240) to exactly fill the Sharp
  display's 240-pixel height. Final scale factor depends on confirming
  the real native resolution first.

## ROM images — bring your own

Not yet documented — depends on the chosen Saturn core's exact expected
ROM format (size, endianness, word width, header presence), unconfirmed.
Will follow the same posture as soynut's HP-41 ROMs regardless of
format specifics: user-supplied, gitignored, never redistributed in
this repo.
