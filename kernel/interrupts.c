#include <stdint.h>

#include "interrupts.h"

#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20
#define PIC1_OFFSET 32
#define PIC2_OFFSET 40

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256] __attribute__((aligned(16)));
static struct idt_ptr idtr;
static uint64_t gdt[5] __attribute__((aligned(16)));
static struct gdt_ptr gdtr;

/* Addresses of all 256 stubs, emitted by kernel/isr.S. */
extern uint64_t isr_stubs[256];

extern void gdt_flush(const struct gdt_ptr *ptr);
extern void idt_flush(const struct idt_ptr *ptr);

static void (*irq_handlers[IRQ_COUNT])(void);
static unsigned long unexpected_count;

static inline void io_outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %%al, %%dx" :: "a"(value), "d"(port));
}

static inline uint8_t io_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %%dx, %0" : "=a"(value) : "d"(port));
    return value;
}

static inline void io_wait(void) {
    io_outb(0x80, 0);
}

static void install_gdt(void) {
    /* Flat 64-bit kernel code (selector 0x08) and data (selector 0x10). */
    gdt[0] = 0;
    gdt[1] = 0x00AF9A000000FFFFULL;
    gdt[2] = 0x00CF92000000FFFFULL;
    gdt[3] = 0;
    gdt[4] = 0;

    gdtr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdtr.base = (uint64_t)&gdt;
    gdt_flush(&gdtr);
}

static void install_idt(void) {
    for (unsigned i = 0; i < 256; i++) {
        uint64_t offset = isr_stubs[i];

        idt[i].offset_low = (uint16_t)(offset & 0xffff);
        idt[i].selector = 0x08;
        idt[i].ist = 0;
        idt[i].attributes = 0x8e;   /* present, 64-bit interrupt gate */
        idt[i].offset_mid = (uint16_t)((offset >> 16) & 0xffff);
        idt[i].offset_high = (uint32_t)(offset >> 32);
        idt[i].reserved = 0;
    }

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base = (uint64_t)&idt;
    idt_flush(&idtr);
}

/* Remap the legacy 8259A pair to vectors 32-47 so CPU exceptions keep
 * vectors 0-31.  Both PICs start fully masked. */
static void pic_remap(void) {
    io_outb(PIC1_CMD, 0x11);
    io_wait();
    io_outb(PIC2_CMD, 0x11);
    io_wait();

    io_outb(PIC1_DATA, PIC1_OFFSET);
    io_wait();
    io_outb(PIC2_DATA, PIC2_OFFSET);
    io_wait();

    io_outb(PIC1_DATA, 0x04);        /* slave on IRQ2 */
    io_wait();
    io_outb(PIC2_DATA, 0x02);        /* cascade identity */
    io_wait();

    io_outb(PIC1_DATA, 0x01);        /* 8086 mode */
    io_wait();
    io_outb(PIC2_DATA, 0x01);
    io_wait();

    io_outb(PIC1_DATA, 0xff);
    io_outb(PIC2_DATA, 0xff);
}

static void pic_send_eoi(unsigned irq) {
    if (irq >= 8)
        io_outb(PIC2_CMD, PIC_EOI);
    io_outb(PIC1_CMD, PIC_EOI);
}

void interrupts_init(void) {
    interrupts_disable();

    install_gdt();
    install_idt();
    pic_remap();

    for (unsigned i = 0; i < IRQ_COUNT; i++)
        irq_handlers[i] = 0;

    unexpected_count = 0;
}

void interrupts_enable(void) {
    __asm__ volatile ("sti");
}

void interrupts_disable(void) {
    __asm__ volatile ("cli");
}

void irq_register(unsigned irq, void (*handler)(void)) {
    if (irq < IRQ_COUNT)
        irq_handlers[irq] = handler;
}

static uint8_t pic_read_mask(uint16_t port) {
    return io_inb(port);
}

void irq_mask(unsigned irq) {
    if (irq >= IRQ_COUNT)
        return;

    if (irq < 8) {
        io_outb(PIC1_DATA, (uint8_t)(pic_read_mask(PIC1_DATA) | (1u << irq)));
    } else {
        io_outb(PIC2_DATA,
                (uint8_t)(pic_read_mask(PIC2_DATA) | (1u << (irq - 8))));
    }
}

void irq_unmask(unsigned irq) {
    if (irq >= IRQ_COUNT)
        return;

    if (irq < 8) {
        io_outb(PIC1_DATA, (uint8_t)(pic_read_mask(PIC1_DATA) & ~(1u << irq)));
    } else {
        io_outb(PIC2_DATA,
                (uint8_t)(pic_read_mask(PIC2_DATA) & ~(1u << (irq - 8))));
    }
}

unsigned long interrupts_unexpected_count(void) {
    return unexpected_count;
}

void interrupt_dispatch(struct interrupt_frame *frame) {
    uint64_t vector = frame->vector;

    /* A CPU exception with no handler is not safe to resume. */
    if (vector < 32) {
        interrupts_disable();
        for (;;)
            __asm__ volatile ("hlt");
    }

    if (vector >= PIC1_OFFSET && vector < PIC1_OFFSET + IRQ_COUNT) {
        unsigned irq = (unsigned)(vector - PIC1_OFFSET);

        if (irq_handlers[irq] != 0)
            irq_handlers[irq]();

        /* Acknowledge the PIC before returning, cascaded IRQs first. */
        pic_send_eoi(irq);
        return;
    }

    /* Vectors 48-255 (including isr_unexpected) have no PIC source. */
    unexpected_count++;
}
