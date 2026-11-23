#ifndef TEST_MOCK_LIMINE_H
#define TEST_MOCK_LIMINE_H

/* Minimal stand-in for include/limine.h used only by host-side tests.
 * It must appear before -Iinclude on the compiler command line so that
 * kernel/framebuffer.c compiles against this struct without pulling in the
 * real bootloader request definitions. */
#include <stdint.h>

struct limine_framebuffer {
    void *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
};

#endif
