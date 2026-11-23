ENTRY(kmain)

SECTIONS {
    . = 0xffffffff80000000;
    _kernel_start = .;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(.bss*)
        *(COMMON)
    }

    .limine_requests : {
        KEEP(*(.limine_requests))
    }

    _kernel_end = .;
}