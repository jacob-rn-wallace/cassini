# Cassini

A ground-up hardware-and-software replica of HP's Saturn-CPU-family
calculators (48SX/48GX/40G/49G/50G, whichever the vendored core actually
supports) — real Saturn CPU emulation, running a real Saturn-native ROM,
driving a real Sharp memory LCD. Named for the Cassini space probe,
which orbited Saturn.

Started as an HP 50g-only replica; broadened to cover the whole
Saturn-family lineage, since they're close architectural relatives and
that opens up a real sequencing advantage — bring up whichever supported
model is simplest first (currently a tentative guess: HP48SX, no
Flash-based memory subsystem to model, no RAM-card complexity), and only
tackle the HP50G — newest, tallest native display, Flash-based memory —
once something is actually working end to end. See `CLAUDE.md` for the
full reasoning.

Structured as a sibling project to
[Soynut](https://github.com/) (an HP-41CV replica) — Cassini reuses
Soynut's structural conventions (vendor an existing CPU emulation core
as a "never edit" black box, BYO ROM, pure-logic/hardware-split bridge
files, the same coding-standard commitment) but nothing else; the
Saturn-family-specific logic underneath is entirely new.

## Status

**Repo scaffolded, Saturn CPU core selected and vendored.** No firmware
exists yet and nothing has run on real hardware, but the emulation core
question — the single biggest open risk at project start — is resolved:
`saturnng` (`codeberg.org/gwh/saturnng`) is vendored as a pinned git
submodule at `saturn_core/`. See `CLAUDE.md` for the full current state
and `DEVLOG.md` (gitignored, local) for the research trail that led here.

## How it's meant to work, roughly

```
Computer (USB serial)
   │  keypresses in / debug text out
   ▼
Raspberry Pi Pico 2   ──SPI──▶   Sharp LS027B7DH01 memory LCD
(real Saturn CPU core,           (400x240, on an Adafruit breakout,
 real Saturn-native ROM)          scaled calculator graphics)
```

See `CLAUDE.md` for the full architecture plan, current confirmed
status, and directory map.

## A note on what is and isn't included

- **Saturn-family ROM firmware will not be in this repo.** It's
  copyrighted calculator firmware, not open source. See `roms/README.md`
  for the confirmed binary format and (once written) full BYO
  instructions.
- **The Saturn CPU emulation core** (`saturn_core/`, `saturnng`) is
  vendored as an unmodified git submodule, never edited directly —
  mirroring Soynut's treatment of its own vendored Nut CPU core.
- **The Sharp-display driver** (`sharpdisp/`) will be vendored by copy
  from prior work (`pico_sharpmem_display-main`), LGPL-2.1 licensed —
  not yet copied in.

## Code quality

This project's own original code (not the vendored emulation core, the
vendored display library, or the Pico SDK) follows NASA/JPL's "Power of
10" rules for safety-critical C and Python, adopted from the start. See
`CLAUDE.md`'s "Coding standard" section and `DEVIATIONS.md` for details.
