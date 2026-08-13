/**
 * @file rom_syscalls.h
 * @brief Shared magic filename between main.c and rom_syscalls.c.
 */
#ifndef ROM_SYSCALLS_H
#define ROM_SYSCALLS_H 1

/**
 * @brief Sentinel filename recognized by rom_syscalls.c's _open()
 * override.
 *
 * Passed to saturn_compat_set_rom_path() at startup, so
 * firmware/saturn_compat/ui4x_compat.c's Resolve() returns this exact
 * string for saturn_core's ROM_FILE_NAME ("rom") lookup, which then
 * reaches _open() unchanged via disk_io.c's unmodified
 * bus_read_nibblesFromFile() -> fopen() call.
 */
#define EMBEDDED_ROM_PATH "EMBEDDED_ROM"

#endif /* !ROM_SYSCALLS_H */
