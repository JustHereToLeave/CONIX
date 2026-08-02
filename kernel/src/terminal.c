#include "terminal.h"
#include "font.h"
#include "string.h"
#include "heap.h"

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

void terminal_print_boot_banner(void) {
    terminal_write("CONIX Kernel ");
    terminal_writeline(CONIX_VERSION);
    terminal_writeline("Now with PSF support!");
    terminal_writeline("");
    terminal_print_prompt();
}

// --- Command dispatch ---

#define MAX_ARGS 8

typedef void (*cmd_fn)(int argc, char *argv[]);

// Splits str in place on spaces. argv[i] points into str itself —
// no extra buffer needed, but it mutates the string (spaces -> '\0').
static int tokenize(char *str, char *argv[], int max_args) {
    int argc = 0;
    char *p = str;

    while (*p != '\0' && argc < max_args) {
        while (*p == ' ') p++;          // skip leading spaces
        if (*p == '\0') break;

        argv[argc++] = p;

        while (*p != '\0' && *p != ' ') p++;
        if (*p == ' ') { *p = '\0'; p++; }
    }
    return argc;
}

static void cmd_conix(int argc, char *argv[]) {
    (void)argc; (void)argv;
    terminal_write("CONIX Kernel ");
    terminal_write(CONIX_VERSION);
    terminal_writeline("\nby Coen Buck\nReleased November 3rd, 2025");
}

static void cmd_fortnite(int argc, char *argv[]) {
    (void)argc; (void)argv;
    terminal_writeline("timmy this is NOT how the terminal works");
}

static void cmd_shimboot(int argc, char *argv[]) {
    (void)argc; (void)argv;
    terminal_writeline("no");
}

static void cmd_clear(int argc, char *argv[]) {
    (void)argc; (void)argv;
    terminal_clear();
}

static void cmd_echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        terminal_write(argv[i]);
        if (i < argc - 1) terminal_write(" ");
    }
    terminal_putchar('\n');
}

// Parses a hex string like "1000" or "0x1000" into a value.
// Returns 0 on failure (no error signaling here, keep it simple for now).
static uint64_t parse_hex(const char *s) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    uint64_t val = 0;
    while (*s != '\0') {
        char c = *s;
        uint8_t digit;

        if (c >= '0' && c <= '9')      digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else break; // stop at first non-hex char

        val = (val << 4) | digit;
        s++;
    }
    return val;
}

// Prints a byte as two hex digits.
static void print_hex_byte(uint8_t b) {
    static const char *digits = "0123456789abcdef";
    terminal_putchar(digits[(b >> 4) & 0xF]);
    terminal_putchar(digits[b & 0xF]);
}

// Prints an address as 0x + 16 hex digits.
static void print_hex_addr(uint64_t addr) {
    terminal_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        static const char *digits = "0123456789abcdef";
        terminal_putchar(digits[(addr >> shift) & 0xF]);
    }
}

static void cmd_mem(int argc, char *argv[]) {
    if (argc < 2) {
        terminal_writeline("usage: mem <hex_addr> [byte_count]");
        return;
    }

    uint64_t addr = parse_hex(argv[1]);
    uint64_t count = (argc >= 3) ? parse_hex(argv[2]) : 16;
    if (count == 0) count = 16;
    if (count > 256) count = 256; // sanity cap so a typo doesn't spam the screen forever

    volatile uint8_t *ptr = (volatile uint8_t *)addr;

    for (uint64_t i = 0; i < count; i += 16) {
        print_hex_addr(addr + i);
        terminal_write(": ");

        // hex bytes
        for (uint64_t j = 0; j < 16 && (i + j) < count; j++) {
            print_hex_byte(ptr[i + j]);
            terminal_write(" ");
        }

        terminal_write(" ");

        // ascii repr
        for (uint64_t j = 0; j < 16 && (i + j) < count; j++) {
            uint8_t b = ptr[i + j];
            terminal_putchar((b >= 32 && b < 127) ? (char)b : '.');
        }

        terminal_putchar('\n');
    }
}

static void cmd_heaptest(int argc, char *argv[]) {
    (void)argc; (void)argv;

    void *a = kmalloc(64);
    void *b = kmalloc(128);
    void *c = kmalloc(32);

    terminal_write("a: "); print_hex_addr((uint64_t)a); terminal_putchar('\n');
    terminal_write("b: "); print_hex_addr((uint64_t)b); terminal_putchar('\n');
    terminal_write("c: "); print_hex_addr((uint64_t)c); terminal_putchar('\n');

    kfree(b);
    void *d = kmalloc(100); // should be able to reuse b's freed space

    terminal_write("d: "); print_hex_addr((uint64_t)d); terminal_putchar('\n');
    terminal_writeline(d == b ? "reused freed block correctly!" : "did not reuse (check logic)");

    kfree(a);
    kfree(c);
    kfree(d);
}

static void cmd_help(int argc, char *argv[]); // needs a forward decl, table references it below

typedef struct {
    const char *name;
    const char *help;
    cmd_fn fn;
} command_t;

static const command_t commands[] = {
    { "conix",    "kernel info",             cmd_conix },
    { "fortnite", "???",                     cmd_fortnite },
    { "shimboot", "???",                     cmd_shimboot },
    { "clear",    "clear the screen",        cmd_clear },
    { "echo",     "print arguments back",    cmd_echo },
    { "mem",      "dump memory: mem <addr> [count]", cmd_mem },
    { "help",     "list available commands", cmd_help },
    { "heaptest", "test the heap allocator", cmd_heaptest },
};
#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

static void cmd_help(int argc, char *argv[]) {
    (void)argc; (void)argv;
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        terminal_write(commands[i].name);
        terminal_write(" - ");
        terminal_writeline(commands[i].help);
    }
}

static void dispatch(char *line) {
    char *argv[MAX_ARGS];
    int argc = tokenize(line, argv, MAX_ARGS);
    if (argc == 0) return;

    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].fn(argc, argv);
            return;
        }
    }

    terminal_write("command not found: ");
    terminal_writeline(argv[0]);
}

// Handle keyboard input with proper buffering
void terminal_handle_input(char c) {
    if (c == '\n') {
        // Enter pressed - null terminate the buffer and move to next line
        input_buffer[input_pos] = '\0';
        terminal_putchar('\n');
        
if (input_pos > 0) {
    dispatch(input_buffer);
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
