#include <stdint.h>
#include <limine.h>

#include "font8x8_basic.h"
#include "framebuffer.h"

static struct limine_framebuffer *fb;

void fb_init(struct limine_framebuffer *framebuffer) {
    fb = framebuffer;
}

uint64_t fb_width(void) {
    return fb == 0 ? 0 : fb->width;
}

uint64_t fb_height(void) {
    return fb == 0 ? 0 : fb->height;
}

/* The console and UI store colours as 0x00RRGGBB, which matches Limine's
 * 32bpp RGB framebuffers on x86_64 (little endian).  Byte writes keep this
 * correct even when the framebuffer pitch is not a multiple of 4. */
static volatile uint8_t *pixel_address(uint64_t x, uint64_t y) {
    return (volatile uint8_t *)fb->address + y * fb->pitch + x * 4;
}

void fb_put_pixel(uint64_t x, uint64_t y, uint32_t color) {
    if (fb == 0 || x >= fb->width || y >= fb->height)
        return;

    volatile uint8_t *p = pixel_address(x, y);
    p[0] = (uint8_t)(color & 0xff);
    p[1] = (uint8_t)((color >> 8) & 0xff);
    p[2] = (uint8_t)((color >> 16) & 0xff);
    p[3] = (uint8_t)((color >> 24) & 0xff);
}

void fb_fill_rect(uint64_t x, uint64_t y,
                  uint64_t w, uint64_t h, uint32_t color) {
    for (uint64_t dy = 0; dy < h; dy++) {
        for (uint64_t dx = 0; dx < w; dx++) {
            fb_put_pixel(x + dx, y + dy, color);
        }
    }
}

void fb_copy_rows(uint64_t dst_y, uint64_t src_y,
                  uint64_t x, uint64_t w, uint64_t h) {
    if (fb == 0 || w == 0 || h == 0 || x >= fb->width)
        return;

    for (uint64_t dy = 0; dy < h; dy++) {
        uint64_t dst = dst_y + dy;
        uint64_t src = src_y + dy;

        if (dst >= fb->height || src >= fb->height)
            continue;
        if (x + w > fb->width)
            w = fb->width - x;
        if (w == 0)
            break;

        volatile uint8_t *dst_row = pixel_address(x, dst);
        volatile uint8_t *src_row = pixel_address(x, src);

        for (uint64_t i = 0; i < w * 4; i++)
            dst_row[i] = src_row[i];
    }
}

void fb_draw_char(uint64_t x, uint64_t y, char c, uint32_t color) {
    unsigned char glyph = (unsigned char)c;
    if (glyph >= 128)
        return;

    for (uint8_t row = 0; row < 8; row++) {
        uint8_t bits = (uint8_t)font8x8_basic[glyph][row];

        for (uint8_t col = 0; col < 8; col++) {
            if (bits & (1U << col))
                fb_put_pixel(x + col, y + row, color);
        }
    }
}

void fb_draw_string(uint64_t x, uint64_t y,
                    const char *str, uint32_t color) {
    uint64_t start_x = x;

    while (*str) {
        if (*str == '\n') {
            x = start_x;
            y += 10;
        } else {
            fb_draw_char(x, y, *str, color);
            x += 8;
        }
        str++;
    }
}
