# ROM images — bring your own

Not yet written. Depends on which Saturn CPU core gets vendored (see the
root `CLAUDE.md`'s "The Saturn CPU core" section) and its exact expected
ROM binary format (size, endianness, word width, header presence),
which is unconfirmed as of this scaffold.

Once confirmed, this file will explain, mirroring Soynut's
`roms/README.md`:

- Where to legally obtain a Saturn-native ROM dump (e.g. from a
  physical HP49G you own).
- The exact binary format expected and how to verify it.
- How to run the (not-yet-written) converter script that turns a raw
  ROM dump into the C source the build actually compiles.

The ROM files themselves, and any generated C source derived from them,
will never be committed to this repository — see `.gitignore`.
