# Power of 10 — deviations

This project's own code (everything except the vendored `saturn_core/`
submodule, the vendored-by-copy `sharpdisp/` display library, and the
external `pico-sdk/` dependency) follows NASA/JPL's "Power of 10" rules
for safety-critical code:

- C: <https://github.com/Vhivi/Powerof10-NASA/blob/29f6f3975bba9c6d6430f8638d8d561786b04c26/rules/Powerof10-C.md>
- Python (adaptation): <https://github.com/Vhivi/Powerof10-NASA/blob/29f6f3975bba9c6d6430f8638d8d561786b04c26/rules/Powerof10-Python.md>

This is a hobbyist calculator replica, not flight software — but the
rules are a genuinely good discipline, so we follow them everywhere
they're actually achievable, the same posture soynut (this project's
structural template, see `CLAUDE.md`) took. A few places will likely
conflict with decisions this project makes deliberately (most probably
around whatever "black box" doctrine ends up governing the vendored
Saturn core, once it's chosen — see `CLAUDE.md`), or with the Pico SDK's
own design, which is out of our control. Rather than fake compliance or
silently ignore a rule, each such case gets listed here: which rule,
exactly what's excepted, why, and — critically — the boundary of the
exception, so it can't quietly expand to cover things it wasn't meant to.

This file starts empty — no code exists yet to deviate. Entries get added
as real exceptions are actually found during implementation, not
speculated in advance. Treat it as authoritative over any inline comment
if the two ever disagree, once entries exist.

## `firmware/saturn_compat/chf_compat.c` — `-Wvarargs` on `ChfGenerate()`

**Rule affected:** the project's `-Wall -Wextra -Wpedantic -Werror`
strict-compile posture (not a numbered Power-of-10 rule itself, but the
same "no silent laxity" spirit Rule 10 asks for).

**What's excepted:** a single `va_start(args, severity)` call inside
`ChfGenerate()`, wrapped in a `#pragma clang/GCC diagnostic ignored
"-Wvarargs"` push/pop.

**Why:** `ChfGenerate()`'s signature is fixed by the vendored
`saturn_core/src/libChf/src/Chf.h` — this shim must match it exactly
to stand in for real libChf. Its last named parameter before `...` is
`const ChfSeverity severity`, an enum; enums undergo default argument
promotion, which `-Wpedantic` flags as `va_start`-adjacent undefined
behavior per the letter of the C standard. Every real compiler handles
this correctly in practice (it's the same shape as any syslog-style
variadic logger) — the diagnostic is pedantry about an API shape this
project doesn't get to choose, not a real correctness risk.

**Boundary:** exactly that one `va_start` call. Nothing else in
`chf_compat.c` or elsewhere suppresses `-Wvarargs`, `-Wpedantic`, or
any other warning class.
