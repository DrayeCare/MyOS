#ifndef MYOS_FRAMEBUFFER_H
#define MYOS_FRAMEBUFFER_H

#include <stdint.h>
#include <limine.h>

/* The framebuffer backend is initialised once from the Limine response.
 * All drawing functions are no-ops until fb_init() has been called. */
void fb_init(struct limine_framebuffer *framebuffer);

uint64_t fb_width(void);
uint64_t fb_height(void);

void fb_put_pixel(uint64_t x, uint64_t y, uint32_t color);
void fb_fill_rect(uint64_t x, uint64_t y,
                  uint64_t w, uint64_t h, uint32_t color);

/* Copy h pixel rows of width w from one y position to another.
 * Used by the console for scrolling; honours the real framebuffer pitch. */
void fb_copy_rows(uint64_t dst_y, uint64_t src_y,
                  uint64_t x, uint64_t w, uint64_t h);

void fb_draw_char(uint64_t x, uint64_t y, char c, uint32_t color);
void fb_draw_string(uint64_t x, uint64_t y, const char *str, uint32_t color);

#endif
