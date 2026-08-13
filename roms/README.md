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

## Embedding a ROM for the real firmware build

The actual Pico firmware (`firmware/`) can't do host-style `fopen()`,
so `rom_to_c.py` embeds a raw ROM file as a C byte array instead:

```
python3 rom_to_c.py hp48sx_revj.rom rom_images.c
```

Gitignored output (`roms/rom_images.c`), same BYO-ROM policy as
everywhere else — never committed. See `CLAUDE.md`'s "Native firmware"
section for how `firmware/rom_syscalls.c` serves this array back to
`saturn_core`'s unmodified ROM-loading code, and for the current status
of the firmware build itself (builds and flashes, currently blocked at
runtime on a real memory constraint, not a code bug).

## Verifying a dump

Confirmed working against a real HP48SX revision J dump (262,144 bytes,
matching the format above): drop it anywhere and point `ROM=` at it —
`ROM=` accepts an absolute path, a path relative to `tests/`, or (the
common case) a path relative to the repo root, e.g. if the file lives
at `roms/hp48sx_revj.rom`:

```
make -C tests run ROM=roms/hp48sx_revj.rom MODEL=48sx
```

A correct dump cold-boots (WARNING-level "can't restore state, resetting"
messages for HDW/internal RAM/Port 1/Port 2 are expected and harmless —
there's no saved state file, by design, see `CLAUDE.md`'s "Native (host)
tests" section) and then reports `PASS: no bad opcode in <N>
instructions` after running the full instruction bound. A `FATAL: Can't
initialize internal ROM` instead means the file wasn't found at the
resolved path, or doesn't match the expected size for the model given.
