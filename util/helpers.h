#ifndef CUTS_UTIL_HELPERS_H
#define CUTS_UTIL_HELPERS_H

#include <stdint.h>
#include <stdio.h>

#include "backend/drm.h"

void print_buffer(char *buffer, size_t buffer_len);
float read_f32(char *buffer, uint32_t *offset);
int32_t read_i32(char *buffer, uint32_t *offset);
uint32_t read_u32(char *buffer, uint32_t *offset);
uint16_t read_u16(char *buffer, uint32_t *offset);
void read_string(char *buffer, uint32_t *offset, char *out, size_t out_size);

void write_f32(char *buffer, uint32_t *offset, float val);
void write_i32(char *buffer, uint32_t *offset, int32_t val);
void write_u32(char *buffer, uint32_t *offset, uint32_t val);
void write_u16(char *buffer, uint32_t *offset, uint16_t val);
void write_string(char *buffer, uint32_t *offset, const char *val);
void draw_rect(struct c_drm_dumb_framebuffer *fb, 
			   uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) ;

#endif
