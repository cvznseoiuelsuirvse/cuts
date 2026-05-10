#ifndef CUTS_WAYLAND_UTIL_H
#define CUTS_WAYLAND_UTIL_H

#include <stdint.h>
#include <stdio.h>

void calc_popup_coords(struct c_xdg_surface *surface, int32_t *x, int32_t *y);

int32_t read_i32(char *buffer, uint32_t *offset);
uint32_t read_u32(char *buffer, uint32_t *offset);
uint16_t read_u16(char *buffer, uint32_t *offset);
void read_string(char *buffer, uint32_t *offset, char *out, size_t out_size);
void *read_array(char *buffer, uint32_t *offset, size_t size);

void write_i32(char *buffer, uint32_t *offset, int32_t val);
void write_u32(char *buffer, uint32_t *offset, uint32_t val);
void write_u16(char *buffer, uint32_t *offset, uint16_t val);
void write_string(char *buffer, uint32_t *offset, const char *val);
void write_array(char *buffer, uint32_t *offset, const uint8_t *array, size_t array_size);

#endif
