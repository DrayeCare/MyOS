/* x86_64 interrupt entry stubs.
 *
 * Each stub pushes a vector number (and a zero error-code placeholder for
 * exceptions that do not push one) and jumps to isr_common.  Vectors 48-255
 * share isr_unexpected: they are counted and ignored rather than dispatched.
 */
    .section .text

.macro ISR_NOERR n
    .global isr\n
    .type isr\n, @function
isr\n:
    pushq $0
    pushq $\n
    jmp isr_common
.endm

.macro ISR_ERR n
    .global isr\n
    .type isr\n, @function
isr\n:
    pushq $\n
    jmp isr_common
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

/* IRQs 0-15 are mapped to vectors 32-47; the CPU never pushes an error
 * code for external interrupts. */
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47

    .global isr_unexpected
    .type isr_unexpected, @function
isr_unexpected:
    pushq $0
    pushq $255
    jmp isr_common

/* Build the struct interrupt_frame on the stack, call the C dispatcher,
 * restore registers, discard vector/error and return from the interrupt. */
    .global isr_common
    .type isr_common, @function
isr_common:
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    movq %rsp, %rdi
    call interrupt_dispatch

    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    addq $16, %rsp
    iretq

/* Load a flat GDT and reload the segment registers, including CS. */
    .global gdt_flush
    .type gdt_flush, @function
gdt_flush:
    lgdt (%rdi)
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    pushq $0x08
    leaq 1f(%rip), %rax
    pushq %rax
    lretq
1:
    ret

    .global idt_flush
    .type idt_flush, @function
idt_flush:
    lidt (%rdi)
    ret

/* Address of every installed stub, indexed by vector.  Keeping this table
 * in assembly avoids 48 hand-written C extern declarations. */
    .section .rodata
    .global isr_stubs
isr_stubs:
    .quad isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
    .quad isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
    .quad isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    .quad isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    .quad isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39
    .quad isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
    .rept 208
    .quad isr_unexpected
    .endr

    .section .note.GNU-stack,"",@progbits
