#include "terminal.h"
#include "font.h"
#include "string.h"

static struct limine_framebuffer *term_fb = NULL;
static uint32_t term_fg_color = 0xffffff;
static uint32_t term_bg_color = 0x000000;
static size_t term_x = 0;
static size_t term_y = 0;
static size_t term_max_cols = 0;
static size_t term_max_rows = 0;

// Input buffer
#define INPUT_BUFFER_SIZE 256
static char input_buffer[INPUT_BUFFER_SIZE];
static size_t input_pos = 0;
static size_t prompt_x = 0;  // Where the prompt started

void terminal_init(struct limine_framebuffer *fb, uint32_t fg_color, uint32_t bg_color) {
    term_fb = fb;
    term_fg_color = fg_color;
    term_bg_color = bg_color;
    term_x = 0;
    term_y = 0;
    
    // Calculate how many characters fit on screen
    term_max_cols = fb->width / font_get_width();
    term_max_rows = fb->height / font_get_height();
    
    // Clear input buffer
    for (size_t i = 0; i < INPUT_BUFFER_SIZE; i++) {
        input_buffer[i] = 0;
    }
    input_pos = 0;
    
    terminal_clear();
}

void terminal_clear(void) {
    if (!term_fb) return;
    
    // Fill entire screen with background color
    for (size_t y = 0; y < term_fb->height; y++) {
        for (size_t x = 0; x < term_fb->width; x++) {
            volatile uint32_t *fb_ptr = term_fb->address;
            fb_ptr[y * (term_fb->pitch / 4) + x] = term_bg_color;
        }
    }
    
    term_x = 0;
    term_y = 0;
}

void terminal_newline(void) {
    term_x = 0;
    term_y++;
    
    // Simple scroll: if we go past bottom, wrap to top
    // TODO: implement proper scrolling later
    if (term_y >= term_max_rows) {
        term_y = 0;
        terminal_clear();
    }
}

void terminal_putchar(char c) {
    if (!term_fb) return;
    
    // Handle special characters
    if (c == '\n') {
        terminal_newline();
        return;
    }
    
    if (c == '\r') {
        term_x = 0;
        return;
    }
    
    if (c == '\b') {
        // Backspace
        if (term_x > 0) {
            term_x--;
            // Clear the character
            size_t pixel_x = term_x * font_get_width();
            size_t pixel_y = term_y * font_get_height();
            font_draw_char(term_fb, ' ', pixel_x, pixel_y, term_fg_color, term_bg_color);
        }
        return;
    }
    
    // Draw the character
    size_t pixel_x = term_x * font_get_width();
    size_t pixel_y = term_y * font_get_height();
    font_draw_char(term_fb, c, pixel_x, pixel_y, term_fg_color, term_bg_color);
    
    term_x++;
    
    // Wrap to next line if we reach the edge
    if (term_x >= term_max_cols) {
        terminal_newline();
    }
}

void terminal_write(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        terminal_putchar(str[i]);
    }
}

void terminal_writeline(const char *str) {
    terminal_write(str);
    terminal_newline();
}

// Print the prompt and remember where it is
static void terminal_print_prompt(void) {
    terminal_write("> ");
    prompt_x = term_x;
}

// Handle keyboard input with proper buffering
void terminal_handle_input(char c) {
    if (c == '\n') {
        // Enter pressed - null terminate the buffer and move to next line
        input_buffer[input_pos] = '\0';
        terminal_putchar('\n');
        
if (input_pos > 0) {
    // Check what command they typed
    if (strcmp(input_buffer, "conix") == 0) {
        terminal_writeline("CONIX Kernel v0.1.1\nby Coen Buck\nReleased November 3rd, 2025");
    } else if (strcmp(input_buffer, "fortnite") == 0) {
        terminal_writeline("timmy this is NOT how the terminal works");
    }  else if (strcmp(input_buffer, "shimboot") == 0) {
        terminal_writeline("no");
    } else {
        terminal_write("command not found: ");
        terminal_writeline(input_buffer);
    }
}
        
        // Clear buffer for next input
        input_pos = 0;
        for (size_t i = 0; i < INPUT_BUFFER_SIZE; i++) {
            input_buffer[i] = 0;
        }
        
        // Print new prompt
        terminal_print_prompt();
        
    } else if (c == '\b') {
        // Backspace - remove from buffer and screen
        if (input_pos > 0) {
            input_pos--;
            input_buffer[input_pos] = '\0';
            
            // Handle visual backspace (can go to previous line)
            if (term_x > prompt_x || term_y > 0) {
                if (term_x == 0 && term_y > 0) {
                    // Go to end of previous line
                    term_y--;
                    term_x = term_max_cols - 1;
                } else if (term_x > 0) {
                    term_x--;
                }
                
                // Clear the character
                size_t pixel_x = term_x * font_get_width();
                size_t pixel_y = term_y * font_get_height();
                font_draw_char(term_fb, ' ', pixel_x, pixel_y, term_fg_color, term_bg_color);
            }
        }
        
    } else {
        // Regular character - add to buffer and display
        if (input_pos < INPUT_BUFFER_SIZE - 1) {
            input_buffer[input_pos] = c;
            input_pos++;
            terminal_putchar(c);
        }
    }
}

const char* terminal_get_input(void) {
    return input_buffer;
}