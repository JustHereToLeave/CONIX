#include "idt.h"
#include "terminal.h"

// IDT entry structure
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

// IDT pointer structure
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// IDT with 256 entries
static struct idt_entry idt[256];
static struct idt_ptr idtp;

// Port I/O functions
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Scancode to ASCII mapping (US keyboard layout)
static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

// Keyboard interrupt handler (defined in assembly below)
extern void keyboard_handler_asm(void);

// The actual C handler called by the assembly stub
void keyboard_handler_c(void) {
    uint8_t scancode = inb(0x60);
    
    // Only handle key presses (scancodes < 0x80)
    if (scancode < 0x80) {
        if (scancode < sizeof(scancode_to_ascii)) {
            char c = scancode_to_ascii[scancode];
            if (c != 0) {
                terminal_handle_input(c);  // Changed this line
            }
        }
    }
    
    // Send EOI (End Of Interrupt) to PIC
    outb(0x20, 0x20);
}

// Set an IDT entry
static void idt_set_gate(uint8_t num, uint64_t handler) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].selector = 0x28;  // Kernel code segment (from limine)
    idt[num].ist = 0;
    idt[num].type_attr = 0x8E;  // Present, ring 0, interrupt gate
    idt[num].offset_mid = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].zero = 0;
}

// Initialize PIC (Programmable Interrupt Controller)
static void pic_init(void) {
    // ICW1: Initialize PIC
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    
    // ICW2: Remap IRQs to 0x20-0x2F
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    
    // ICW3: Setup cascade
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    
    // ICW4: 8086 mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    
    // Unmask all interrupts except keyboard (IRQ1)
    outb(0x21, 0xFD);  // 11111101 - only keyboard enabled
    outb(0xA1, 0xFF);
}

void idt_init(void) {
    // Clear IDT
    for (int i = 0; i < 256; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attr = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].zero = 0;
    }
    
    // Set keyboard interrupt (IRQ1 = interrupt 0x21)
    idt_set_gate(0x21, (uint64_t)keyboard_handler_asm);
    
    // Load IDT
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint64_t)&idt;
    asm volatile("lidt %0" : : "m"(idtp));
    
    // Initialize PIC
    pic_init();
    
    // Enable interrupts
    asm volatile("sti");
}

// Assembly stub for keyboard handler
// This saves registers, calls the C handler, then restores and returns
asm(
    ".global keyboard_handler_asm\n"
    "keyboard_handler_asm:\n"
    "    pushq %rax\n"
    "    pushq %rbx\n"
    "    pushq %rcx\n"
    "    pushq %rdx\n"
    "    pushq %rsi\n"
    "    pushq %rdi\n"
    "    pushq %rbp\n"
    "    pushq %r8\n"
    "    pushq %r9\n"
    "    pushq %r10\n"
    "    pushq %r11\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    call keyboard_handler_c\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %r11\n"
    "    popq %r10\n"
    "    popq %r9\n"
    "    popq %r8\n"
    "    popq %rbp\n"
    "    popq %rdi\n"
    "    popq %rsi\n"
    "    popq %rdx\n"
    "    popq %rcx\n"
    "    popq %rbx\n"
    "    popq %rax\n"
    "    iretq\n"
);