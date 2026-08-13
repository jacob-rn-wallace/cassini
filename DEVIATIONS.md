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
