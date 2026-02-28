#ifndef CUTS_UTIL_HELPERS_H
#define CUTS_UTIL_HELPERS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define STREQ(s1, s2) (strcmp((s1), (s2)) == 0)
#define LENGTH(s) (sizeof((s)) /  sizeof(*(s)))

void print_buffer(char *buffer, size_t buffer_len);
float read_f32(char *buffer, uint32_t *offset);
int32_t read_i32(char *buffer, uint32_t *offset);
uint32_t read_u32(char *buffer, uint32_t *offset);
uint16_t read_u16(char *buffer, uint32_t *offset);
void read_string(char *buffer, uint32_t *offset, char *out, size_t out_size);
void *read_array(char *buffer, uint32_t *offset, size_t size);

void write_f32(char *buffer, uint32_t *offset, float val);
void write_i32(char *buffer, uint32_t *offset, int32_t val);
void write_u32(char *buffer, uint32_t *offset, uint32_t val);
void write_u16(char *buffer, uint32_t *offset, uint16_t val);
void write_string(char *buffer, uint32_t *offset, const char *val);
void write_array(char *buffer, uint32_t *offset, const uint8_t *array, size_t array_size);

uint32_t drm_format_to_bpp(uint32_t format);

#endif
