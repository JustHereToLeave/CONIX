#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

// Set the base revision to 4, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(4);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_terminal_request terminal_request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

// Simple 8x8 bitmap font (partial)
// Each character is 8 bytes, each byte is a row of 8 pixels
static const uint8_t font_8x8[128][8] = {
    ['C'] = {
        0b00111100,
        0b01100110,
        0b01100000,
        0b01100000,
        0b01100000,
        0b01100110,
        0b00111100,
        0b00000000,
    },
    ['O'] = {
        0b00111100,
        0b01100110,
        0b01100110,
        0b01100110,
        0b01100110,
        0b01100110,
        0b00111100,
        0b00000000,
    },
    ['N'] = {
        0b01100110,
        0b01110110,
        0b01111110,
        0b01111110,
        0b01101110,
        0b01100110,
        0b01100110,
        0b00000000,
    },
    ['I'] = {
        0b00111100,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00111100,
        0b00000000,
    },
    ['X'] = {
        0b01100110,
        0b01100110,
        0b00111100,
        0b00011000,
        0b00111100,
        0b01100110,
        0b01100110,
        0b00000000,
    },
};

// Draw a single character at x, y position
void draw_char(struct limine_framebuffer *fb, char c, size_t x, size_t y, uint32_t color) {
    for (size_t row = 0; row < 8; row++) {
        uint8_t byte = font_8x8[(int)c][row];
        for (size_t col = 0; col < 8; col++) {
            if (byte & (1 << (7 - col))) {
                size_t pixel_x = x + col;
                size_t pixel_y = y + row;
                if (pixel_x < fb->width && pixel_y < fb->height) {
                    volatile uint32_t *fb_ptr = fb->address;
                    fb_ptr[pixel_y * (fb->pitch / 4) + pixel_x] = color;
                }
            }
        }
    }
}

// Draw a string at x, y position
void draw_string(struct limine_framebuffer *fb, const char *str, size_t x, size_t y, uint32_t color) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        draw_char(fb, str[i], x + (i * 8), y, color);
    }
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // Fill screen with background color
    uint32_t bg_color = 0x004447;
    for (size_t y = 0; y < framebuffer->height; y++) {
        for (size_t x = 0; x < framebuffer->width; x++) {
            volatile uint32_t *fb_ptr = framebuffer->address;
            fb_ptr[y * (framebuffer->pitch / 4) + x] = bg_color;
        }
    }

    // Draw "CONIX" at position (100, 100) in white
    draw_string(framebuffer, "CONIX KERNEL", 100, 100, 0xffffff);

    hcf();
}
