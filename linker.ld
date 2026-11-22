OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

PHDRS
{
    kernel PT_LOAD FLAGS(7);
}

SECTIONS
{
    . = 0xffffffff80000000;

    .limine_requests : {
        KEEP(*(.limine_requests))
    } :kernel

    . = ALIGN(4096);

    .text : {
        *(.text .text.*)
    } :kernel

    . = ALIGN(4096);

    .rodata : {
        *(.rodata .rodata.*)
    } :kernel

    . = ALIGN(4096);

    .data : {
        *(.data .data.*)
    } :kernel

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :kernel

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
    }
}
