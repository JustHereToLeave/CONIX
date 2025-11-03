#ifndef FONT_H
#define FONT_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

// Initialize the font
void font_init(void);

// Draw a character at x, y
void font_draw_char(struct limine_framebuffer *fb, char c, size_t x, size_t y, uint32_t fg_color, uint32_t bg_color);

// Draw a string at x, y
void font_draw_string(struct limine_framebuffer *fb, const char *str, size_t x, size_t y, uint32_t fg_color, uint32_t bg_color);

// Get font dimensions
uint32_t font_get_width(void);
uint32_t font_get_height(void);

// Terminal functions
void terminal_init(struct limine_framebuffer *fb, uint32_t fg, uint32_t bg);
void terminal_putchar(char c);
void terminal_write(const char *str);
void terminal_clear(void);

#endif