#ifndef MYOS_CONSOLE_H
#define MYOS_CONSOLE_H

#include <stdint.h>

/* The 8x8 font is drawn with a 2 pixel vertical gap, so a text row is
 * 10 pixels tall.  A console owns a rectangular pixel region; nothing is
 * ever drawn outside it. */
#define CONSOLE_CELL_WIDTH  8
#define CONSOLE_LINE_HEIGHT 10

struct console {
    uint64_t x;         /* region left edge, pixels */
    uint64_t y;         /* region top edge, pixels */
    uint64_t width;     /* region width, pixels */
    uint64_t height;    /* region height, pixels */
    uint64_t cols;      /* usable columns (width / 8) */
    uint64_t rows;      /* usable rows (height / 10) */
    uint64_t col;       /* cursor column, 0-based */
    uint64_t row;       /* cursor row, 0-based */
    uint32_t fg;
    uint32_t bg;
};

/* Region is clamped to whole character cells.  A zero-sized region is
 * valid: writes are then silently discarded. */
void console_init(struct console *c, uint64_t x, uint64_t y,
                  uint64_t width, uint64_t height,
                  uint32_t fg, uint32_t bg);

void console_clear(struct console *c);
void console_set_color(struct console *c, uint32_t color);

void console_putc(struct console *c, char ch);
void console_write(struct console *c, const char *str);
void console_newline(struct console *c);

/* Erase the previous character cell without scrolling. */
void console_backspace(struct console *c);

/* Move every text row up by one and clear the vacated bottom row. */
void console_scroll(struct console *c);

#endif
