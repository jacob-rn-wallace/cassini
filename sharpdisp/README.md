# sharpdisp — vendored by copy

This is a trimmed, vendored-by-copy snapshot of `pico_sharpmem_display`,
a Pico SDK framebuffer/font library for Sharp memory LCDs (LS013B4DN04,
LS027B7DH01, ...). See the root `CLAUDE.md`'s "Directory map" section
for why it's vendored by copy rather than as a git submodule (no
upstream git history was available to submodule against on the machine
this was vendored from).

**License:** LGPL-2.1 (see `LICENSE`), separate from and compatible
with this project's own GPL-3.0 — see the root `CLAUDE.md`'s "License"
section.

**What's vendored, and what's deliberately left out:** only the library
itself — `include/sharpdisp/`, `src/`, and `fonts/` (pre-generated
`.c`/`.h` font tables only, not the `.yaml` sources or the Python
codegen tooling that built them, since regenerating fonts isn't
something this project needs to do). Not vendored: the upstream
project's own `examples/`, `test/`, `tools/`, `images/`, and top-level
build scaffolding (`bootstrap.sh`, its own `CMakeLists.txt`) — those are
upstream's demo/dev infrastructure, not the library `lcd_bringup/`
actually links against.

**Two local patches already baked in**, carried over as part of the
copy rather than something Cassini had to rediscover: every generated
font file's `#include <pico/platform.h>` was changed to
`#include <pico.h>`, since Pico SDK 2.x hard-errors on including
`pico/platform.h` directly. `pico.h` still transitively includes it
(`_PICO_H` is defined first, then `#include "pico/platform.h"`), so
nothing that depended on `platform.h`'s contents (e.g. `__in_flash()`)
broke. This project's target board (Pico 2 / RP2350) requires SDK 2.x
regardless (RP2350 support starts at SDK 2.0.0), so this patch isn't
optional here the way it might be for an RP2040-only project pinning to
SDK 1.5.1 instead.

**Confirmed hardware fit:** default pin choice in
`sharpdisp_init_freq_hz()` (`include/sharpdisp/sharpdisp.h`) is CS=GP17,
SCK=GP18, MOSI=GP19, `spi0` — exactly the pinout the root `CLAUDE.md`'s
"Hardware" section documents for the Adafruit LS027B7DH01 breakout
(#4694). This combination (this library, this display, this breakout,
a Pico 2, SDK 2.x with the patch above) was already built, flashed, and
visually confirmed working (centered text + border rendered correctly
on the physical display) prior to vendoring — not by this project, but
by the prior work this was copied from. `lcd_bringup/` re-proves the
same thing standalone, inside this repo.

**Never edited directly beyond the two patches above**, same "vendored,
treat as a black box" posture this project applies to `saturn_core/`.
