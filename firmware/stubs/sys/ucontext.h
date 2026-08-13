/* Stub for the ARM/newlib toolchain, which (unlike the host libc the
 * native smoke test builds against) doesn't ship <sys/ucontext.h>.
 *
 * saturn_core/src/core/bus.c unconditionally #includes it (bus.c:105)
 * but never actually references anything from it - no ucontext_t,
 * mcontext_t, sigaction, or related symbol appears anywhere in that
 * file (confirmed by grep). Vendored code is never edited, so rather
 * than touch bus.c, this empty stub just satisfies the #include -
 * same "make unmodified vendored source build under a different
 * compiler" posture as firmware/CMakeLists.txt's -include unistd.h
 * force-include for serial.c (mirroring tests/Makefile's identical
 * trick there). See CLAUDE.md's "Native firmware" section.
 */
#ifndef CASSINI_STUB_SYS_UCONTEXT_H
#define CASSINI_STUB_SYS_UCONTEXT_H
#endif
