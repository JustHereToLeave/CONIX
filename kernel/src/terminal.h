#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <limine.h>

void terminal_init(struct limine_framebuffer *fb, uint32_t fg_color, uint32_t bg_color);
void terminal_putchar(char c);
void terminal_write(const char *str);
void terminal_writeline(const char *str);
void terminal_clear(void);
void terminal_newline(void);
void terminal_handle_input(char c);  // New function for input handling
const char* terminal_get_input(void);  // Get the current input buffer

#endif