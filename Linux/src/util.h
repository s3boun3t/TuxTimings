#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stddef.h>

uint32_t bit_slice(uint32_t val, int hi, int lo);
int      read_file_string(const char *path, char *buf, size_t sz);
int      read_file_bytes(const char *path, uint8_t *buf, size_t sz);
float    read_float_file(const char *path);
int      read_int_file(const char *path);
char    *run_command(const char *cmd);
/* Discard output after a stream error, failed pclose, or unsuccessful command. */
char    *run_command_checked(const char *cmd);
int      file_exists(const char *path);
int      dir_exists(const char *path);

#endif
