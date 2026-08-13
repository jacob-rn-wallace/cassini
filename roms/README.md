# ROM images — bring your own

The ROM files themselves, and any generated C source derived from them,
will never be committed to this repository — see `.gitignore`. Same
posture as soynut's HP-41 ROMs: copyrighted Saturn-family firmware,
not ours to redistribute.

## Format

Confirmed directly from `saturn_core`'s own ROM-loading code
(`src/core/disk_io.c`'s `bus_read_nibblesFromFile()`,
`src/core/romram48.c`'s `RomInit48()`, `src/core/romram49.c`; see the
root `CLAUDE.md`'s "The Saturn CPU core" section for the full
citation):

- Flat binary, **no header**.
- **2 MiB (2,097,152 bytes)** for an HP40G/HP49G/HP50G Flash ROM dump.
- **256 KiB (262,144 bytes)** for an **HP48SX** ROM dump.
- **512 KiB (524,288 bytes)** for an **HP48GX** ROM dump — note this
  differs from HP48SX even though `saturn_core` treats the two as
  code-path-identical otherwise (see `CLAUDE.md`'s "Hardware" section);
  the only runtime branch between them is exactly this byte count.
- Each on-disk byte unpacks to two 4-bit Saturn nibbles, **low nibble
  first**.

## Native (host) smoke test — no converter needed yet

`tests/saturn_smoke_test.c` (see `CLAUDE.md`'s "Native (host) tests"
section) reads a ROM file directly with real host file I/O — no C-array
conversion step exists or is needed for this host-native build. Once
you have a real HP48SX or HP48GX ROM dump matching the sizes above:

```
make -C tests run ROM=/path/to/your.rom MODEL=48sx   # or MODEL=48gx
```

This boots the ROM against the vendored `saturn_core` (via
`firmware/saturn_compat/`'s shims) and runs it for a bounded number of
instructions, reporting PASS/FAIL based on whether the CPU core ever
hits an unrecognized opcode.

A converter that embeds a ROM as C source (for the actual Pico
firmware build, which can't do host-style `fopen()`) doesn't exist yet
— that's later firmware-bring-up work, not needed for this host smoke
test.

## Verifying a dump

Not yet written — pending a first real ROM dump to test the size/format
checks above against.
