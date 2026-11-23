#ifndef MYOS_INTERRUPTS_H
#define MYOS_INTERRUPTS_H

#include <stdint.h>

#define IRQ_COUNT 16

/* Layout created by kernel/isr.S before the C dispatcher is entered. */
struct interrupt_frame {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);

/* irq is 0-15, mapped to IDT vectors 32-47. */
void irq_register(unsigned irq, void (*handler)(void));
void irq_mask(unsigned irq);
void irq_unmask(unsigned irq);

unsigned long interrupts_unexpected_count(void);
void interrupt_dispatch(struct interrupt_frame *frame);

#endif
