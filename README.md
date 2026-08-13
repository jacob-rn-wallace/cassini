# Mercator

A ground-up hardware-and-software replica of the HP 50g family of
calculators — real Saturn-CPU-family emulation, running a real
Saturn-native ROM, driving a real Sharp memory LCD scaled to match the
calculator's native display resolution.

Structured as a sibling project to
[Soynut](https://github.com/) (an HP-41CV replica) — Mercator reuses
Soynut's structural conventions (vendor an existing CPU emulation core
as a "never edit" black box, BYO ROM, pure-logic/hardware-split bridge
files, the same coding-standard commitment) but nothing else; the
Saturn/50g-specific logic underneath is entirely new.

## Status

**Not yet started.** This repo currently holds only its initial
scaffold — directory structure, coding-standard tooling config, and a
`CLAUDE.md` describing the plan. No Saturn CPU core has been vendored
yet, no firmware exists, and nothing has run on real hardware.

## How it's meant to work, roughly

```
Computer (USB serial)
   │  keypresses in / debug text out
   ▼
Raspberry Pi Pico 2   ──SPI──▶   Sharp LS027B7DH01 memory LCD
(real Saturn CPU core,           (400x240, on an Adafruit breakout,
 real Saturn-native ROM)          3x-scaled calculator graphics)
```

See `CLAUDE.md` for the full architecture plan, current confirmed
status, and directory map.

## A note on what is and isn't included

- **The Saturn-family ROM firmware will not be in this repo.** It's
  copyrighted calculator firmware, not open source. Once a Saturn core
  is vendored, this section (and a `roms/README.md`) will explain how to
  supply your own.
- **The Saturn CPU emulation core** will be vendored as an unmodified
  git submodule, never edited directly — mirroring Soynut's treatment of
  its own vendored Nut CPU core.
- **The Sharp-display driver** (`sharpdisp/`) is vendored by copy from
  prior work (`pico_sharpmem_display-main`), LGPL-2.1 licensed.

## Code quality

This project's own original code (not the vendored emulation core, the
vendored display library, or the Pico SDK) follows NASA/JPL's "Power of
10" rules for safety-critical C and Python, adopted from the start. See
`CLAUDE.md`'s "Coding standard" section and `DEVIATIONS.md` for details.
