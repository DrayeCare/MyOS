#include "console.h"
#include "framebuffer.h"

void console_init(struct console *c, uint64_t x, uint64_t y,
                  uint64_t width, uint64_t height,
                  uint32_t fg, uint32_t bg) {
    c->x = x;
    c->y = y;
    c->width = width;
    c->height = height;
    c->cols = width / CONSOLE_CELL_WIDTH;
    c->rows = height / CONSOLE_LINE_HEIGHT;
    c->fg = fg;
    c->bg = bg;
    console_clear(c);
}

void console_clear(struct console *c) {
    c->col = 0;
    c->row = 0;

    if (c->cols == 0 || c->rows == 0)
        return;

    fb_fill_rect(c->x, c->y,
                 c->cols * CONSOLE_CELL_WIDTH,
                 c->rows * CONSOLE_LINE_HEIGHT,
                 c->bg);
}

void console_set_color(struct console *c, uint32_t color) {
    c->fg = color;
}

void console_scroll(struct console *c) {
    if (c->rows == 0)
        return;

    uint64_t region_w = c->cols * CONSOLE_CELL_WIDTH;
    uint64_t moved_h = (c->rows - 1) * CONSOLE_LINE_HEIGHT;

    /* Copy whole pixel rows inside the console region only.  The logo and
     * the UI outside the region are never touched. */
    if (moved_h != 0) {
        fb_copy_rows(c->y, c->y + CONSOLE_LINE_HEIGHT,
                     c->x, region_w, moved_h);
    }

    /* The vacated bottom line must be repainted with the panel colour so
     * stale glyphs do not remain after scrolling. */
    fb_fill_rect(c->x, c->y + moved_h,
                 region_w, CONSOLE_LINE_HEIGHT, c->bg);
}

/* Advance to the next row, scrolling when already at the bottom.  Called
 * before any glyph is drawn, so the cursor can never leave the region. */
static void console_next_line(struct console *c) {
    c->col = 0;

    if (c->rows == 0)
        return;

    if (c->row + 1 >= c->rows)
        console_scroll(c);
    else
        c->row++;
}

void console_newline(struct console *c) {
    console_next_line(c);
}

void console_putc(struct console *c, char ch) {
    if (c->cols == 0 || c->rows == 0)
        return;

    if (ch == '\n') {
        console_next_line(c);
        return;
    }

    if (ch == '\r') {
        c->col = 0;
        return;
    }

    unsigned char glyph = (unsigned char)ch;
    if (glyph < 32 || glyph >= 127)
        return;

    if (c->col >= c->cols)
        console_next_line(c);

    fb_draw_char(c->x + c->col * CONSOLE_CELL_WIDTH,
                 c->y + c->row * CONSOLE_LINE_HEIGHT,
                 ch, c->fg);

    c->col++;

    /* Wrap at the right edge.  If that was the bottom row, scroll now so
     * the next character starts on a freshly cleared line in the region. */
    if (c->col >= c->cols)
        console_next_line(c);
}

void console_backspace(struct console *c) {
    if (c->cols == 0 || c->rows == 0)
        return;

    if (c->col > 0) {
        c->col--;
    } else if (c->row > 0) {
        c->row--;
        c->col = c->cols - 1;
    } else {
        return;                     /* already at the region origin */
    }

    fb_fill_rect(c->x + c->col * CONSOLE_CELL_WIDTH,
                 c->y + c->row * CONSOLE_LINE_HEIGHT,
                 CONSOLE_CELL_WIDTH, CONSOLE_LINE_HEIGHT, c->bg);
}

void console_write(struct console *c, const char *str) {
    if (str == 0)
        return;

    while (*str)
        console_putc(c, *str++);
}
