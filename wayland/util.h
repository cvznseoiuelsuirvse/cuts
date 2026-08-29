#ifndef CUTS_WAYLAND_UTIL_H
#define CUTS_WAYLAND_UTIL_H

#include <stdint.h>
#include <stdio.h>

int32_t read_i32(uint8_t *buffer, size_t *offset);
uint32_t read_u32(uint8_t *buffer, size_t *offset);
uint16_t read_u16(uint8_t *buffer, size_t *offset);
void read_string(uint8_t *buffer, size_t *offset, char *out, size_t out_size);
void *read_array(uint8_t *buffer, size_t *offset, size_t size);

void write_i32(uint8_t *buffer, size_t *offset, int32_t val);
void write_u32(uint8_t *buffer, size_t *offset, uint32_t val);
void write_u16(uint8_t *buffer, size_t *offset, uint16_t val);
void write_string(uint8_t *buffer, size_t *offset, const char *val);
void write_array(uint8_t *buffer, size_t *offset, const uint8_t *array, size_t array_size);

#endif
