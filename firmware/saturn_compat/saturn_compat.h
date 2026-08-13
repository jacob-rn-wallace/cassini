/**
 * @file saturn_compat.h
 * @brief Entry points the compat shim exposes to callers (test
 * harnesses, and eventually firmware/main.c) that the vendored
 * saturn_core itself never declares.
 */
#ifndef SATURN_COMPAT_H
#define SATURN_COMPAT_H 1

/**
 * @brief Set the real on-disk path ui4x_make_filename_absolute_for_loading()
 * returns for saturn_core's ROM_FILE_NAME ("rom") lookup.
 *
 * Must be called before bus_init() (i.e. before EmulatorInit()) so
 * RomInit48()/RomInit49() resolve to a real, user-supplied ROM file
 * rather than the shim's default "doesn't exist" placeholder. Cassini
 * never ships ROM files itself (see roms/README.md) - the caller is
 * always something that received the path from the user (a test
 * harness's argv, an env var, ...).
 *
 * @param path Absolute or relative path to a raw Saturn ROM dump.
 *             The string is not copied; it must stay valid for as
 *             long as the shim might be asked to resolve "rom" again.
 */
void saturn_compat_set_rom_path( const char* path );

#endif /* !SATURN_COMPAT_H */
