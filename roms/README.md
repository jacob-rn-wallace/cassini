# ROM images — bring your own

**Not yet fully written — the converter script doesn't exist yet
(Phase 2 work).** The expected format is confirmed, though, from
`saturn_core`'s own ROM-loading code
(`src/core/disk_io.c`'s `bus_read_nibblesFromFile()` and
`src/core/romram49.c`; see the root `CLAUDE.md`'s "The Saturn CPU core"
section for the full citation):

- Flat binary, **no header**.
- **2 MiB (2,097,152 bytes)** for an HP49G/HP50G Flash ROM dump
  (HP48-series ROMs are 512 KiB instead — not this project's target,
  see `CLAUDE.md`).
- Each on-disk byte unpacks to two 4-bit Saturn nibbles, **low nibble
  first**.

Once the converter script exists, this file will also cover:

- Where to legally obtain a Saturn-native HP50G ROM dump (e.g. from a
  physical calculator you own).
- How to verify a dump matches the expected format before use.
- How to run the converter that turns a raw ROM dump into the C source
  the build actually compiles.

The ROM files themselves, and any generated C source derived from them,
will never be committed to this repository — see `.gitignore`.
