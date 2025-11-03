#include "font.h"
#include "../font_data.h"  // This is the xxd output

// PSF1 header structure
struct psf1_header {
    uint8_t magic[2];       // Magic bytes: 0x36, 0x04
    uint8_t mode;           // PSF font mode
    uint8_t charsize;       // Character size (bytes per glyph)
};

#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

static struct psf1_header *font_header = NULL;
static uint8_t *font_glyphs = NULL;
static uint32_t font_width = 8;   // PSF1 is always 8 pixels wide
static uint32_t font_height = 16; // Will be set from header

void font_init(void) {
    // The xxd output creates a variable like: unsigned char Goha_16_psf[]
    font_header = (struct psf1_header *)Goha_16_psf;
    
    // Verify it's actually a PSF1 font
    if (font_header->magic[0] != PSF1_MAGIC0 || font_header->magic[1] != PSF1_MAGIC1) {
        // Not a valid PSF1 font, halt
        for(;;) asm("hlt");
    }
    
    // PSF1 is always 8 pixels wide, height is charsize
    font_height = font_header->charsize;
    
    // Glyphs start right after the 4-byte header
    font_glyphs = (uint8_t *)Goha_16_psf + sizeof(struct psf1_header);
}

uint32_t font_get_width(void) {
    return font_width;
}

uint32_t font_get_height(void) {
    return font_height;
}

void font_draw_char(struct limine_framebuffer *fb, char c, size_t x, size_t y, uint32_t fg_color, uint32_t bg_color) {
    if (!font_header || !font_glyphs) return;
    
    // Get the glyph for this character
    int glyph_index = (unsigned char)c;
    
    // PSF1 can have 256 or 512 glyphs depending on mode
    // For safety, wrap around if out of range
    glyph_index = glyph_index % 256;
    
    uint8_t *glyph = font_glyphs + (glyph_index * font_header->charsize);
    
    // Draw the glyph - PSF1 is always 8 pixels wide, one byte per row
    for (uint32_t row = 0; row < font_height; row++) {
        uint8_t byte = glyph[row];
        
        for (uint32_t col = 0; col < 8; col++) {
            // Check if this pixel is set (MSB first)
            int pixel_set = (byte >> (7 - col)) & 1;
            
            // Draw the pixel
            size_t pixel_x = x + col;
            size_t pixel_y = y + row;
            
            if (pixel_x < fb->width && pixel_y < fb->height) {
                volatile uint32_t *fb_ptr = fb->address;
                fb_ptr[pixel_y * (fb->pitch / 4) + pixel_x] = pixel_set ? fg_color : bg_color;
            }
        }
    }
}

void font_draw_string(struct limine_framebuffer *fb, const char *str, size_t x, size_t y, uint32_t fg_color, uint32_t bg_color) {
    if (!font_header) return;
    
    for (size_t i = 0; str[i] != '\0'; i++) {
        font_draw_char(fb, str[i], x + (i * font_width), y, fg_color, bg_color);
    }
}