/* Host-side regression tests for the console and framebuffer modules.
 *
 * These tests compile the real kernel/console.c and kernel/framebuffer.c
 * against a mock Limine framebuffer.  No bootloader, QEMU, libc
 * beyond freestanding headers, or hosted runtime services are needed to run them.
 */
#include <stdint.h>
#include <stdio.h>

#include "limine.h"
#include "console.h"
#include "framebuffer.h"

/* Defined once by font8x8_basic.h inside framebuffer.c.  Do not include
 * that header here: under -fno-common a second definition would collide. */
extern char font8x8_basic[128][8];

#define W 32
#define H 40
#define PITCH 130            /* deliberately NOT a multiple of 4 */
#define SENTINEL 0xDEADBEEFu
#define FG 0x00FF00u
#define FG2 0xFF0000u
#define BG 0x112233u

static uint8_t pixels[H * PITCH];
static struct limine_framebuffer mock;
static int checks;
static int failures;

static void check(int condition, const char *label) {
    checks++;
    printf("%s  %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition)
        failures++;
}

/* Rows are not 4-byte aligned when pitch is 130, so a flat byte fill would
 * read back rotated.  Fill pixel by pixel using the same addressing the
 * framebuffer module uses. */
static void reset_fb(void) {
    for (uint64_t y = 0; y < H; y++) {
        for (uint64_t x = 0; x < W; x++) {
            uint8_t *p = pixels + y * PITCH + x * 4;
            p[0] = (uint8_t)(SENTINEL & 0xff);
            p[1] = (uint8_t)((SENTINEL >> 8) & 0xff);
            p[2] = (uint8_t)((SENTINEL >> 16) & 0xff);
            p[3] = (uint8_t)((SENTINEL >> 24) & 0xff);
        }
    }

    mock.address = pixels;
    mock.width = W;
    mock.height = H;
    mock.pitch = PITCH;
    mock.bpp = 32;
    fb_init(&mock);
}

static uint32_t read_px(uint64_t x, uint64_t y) {
    const uint8_t *p = pixels + y * PITCH + x * 4;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void expect_px(uint64_t x, uint64_t y, uint32_t want, const char *label) {
    uint32_t got = read_px(x, y);
    if (got != want) {
        printf("       (%llu,%llu): got %08x, expected %08x\n",
               (unsigned long long)x, (unsigned long long)y,
               (unsigned)got, (unsigned)want);
    }
    check(got == want, label);
}

static int glyph_matches(uint64_t x, uint64_t y, char ch, uint32_t color) {
    unsigned char g = (unsigned char)ch;
    for (uint8_t row = 0; row < 8; row++) {
        uint8_t bits = (uint8_t)font8x8_basic[g][row];
        for (uint8_t col = 0; col < 8; col++) {
            uint32_t want = (bits & (1U << col)) ? color : BG;
            if (read_px(x + col, y + row) != want)
                return 0;
        }
    }
    return 1;
}

/* Standard region: x=8..23, y=10..29 -> 2 columns x 2 rows. */
static void init_console(struct console *c) {
    console_init(c, 8, 10, 16, 20, FG, BG);
}

static void test_init_clamps_region(void) {
    reset_fb();
    struct console c;
    console_init(&c, 8, 10, 17, 23, FG, BG);   /* 17x23 -> 2x2 cells */
    check(c.cols == 2 && c.rows == 2, "init clamps region to whole cells");
    check(c.col == 0 && c.row == 0, "init resets cursor");
    check(c.x == 8 && c.y == 10 && c.width == 17 && c.height == 23,
          "init stores the requested region");
}

static void test_init_clears_region(void) {
    reset_fb();
    struct console c;
    init_console(&c);
    check(read_px(8, 10) == BG && read_px(23, 29) == BG,
          "init clears console region to background colour");
    check(read_px(7, 10) == SENTINEL && read_px(24, 10) == SENTINEL &&
          read_px(8, 9) == SENTINEL && read_px(8, 30) == SENTINEL,
          "init does not touch pixels outside the region");
}

static void test_character_rendering(void) {
    reset_fb();
    struct console c;
    init_console(&c);
    console_putc(&c, 'A');
    check(glyph_matches(8, 10, 'A', FG),
          "character rendering matches the 8x8 font bitmap and foreground");
    expect_px(8, 9, SENTINEL, "glyph pixels stay within the console region");
}

static void test_set_color(void) {
    reset_fb();
    struct console c;
    init_console(&c);
    console_set_color(&c, FG2);
    console_putc(&c, 'A');
    check(glyph_matches(8, 10, 'A', FG2),
          "console_set_color changes the colour of newly drawn glyphs");
}

static void test_newline(void) {
    reset_fb();
    struct console c;
    init_console(&c);
    console_write(&c, "A\nB");
    check(c.row == 1 && c.col == 1, "newline advances row and resets column");
    check(glyph_matches(8, 10, 'A', FG) && glyph_matches(8, 20, 'B', FG),
          "newline places the next character on the following row");
}

static void test_carriage_return(void) {
    reset_fb();
    struct console c;
    console_init(&c, 4, 4, 24, 30, FG, BG);    /* 3 cols x 3 rows */
    console_write(&c, "ABC\rZ");
    check(c.row == 1 && c.col == 1, "carriage return resets column only");
    check(glyph_matches(4, 4, 'A', FG) && glyph_matches(4, 14, 'Z', FG),
          "carriage return overwrites from the start of the current row");
}

static void test_wrapping(void) {
    reset_fb();
    struct console c;
    init_console(&c);
    console_write(&c, "AB");
    check(c.row == 1 && c.col == 0, "horizontal wrap advances to the next row");
    check(glyph_matches(8, 10, 'A', FG) && glyph_matches(16, 10, 'B', FG),
          "wrapped characters occupy consecutive cells on the same row");
}

static void test_scroll_moves_rows(void) {
    reset_fb();
    struct console c;
    init_console(&c);
    console_write(&c, "AB");           /* row 0 full, cursor at row 1 */
    console_putc(&c, 'C');             /* draw on the bottom row */

    uint32_t bottom[24];
    for (uint64_t x = 8; x < 24; x++)
        bottom[x - 8] = read_px(x, 20);

    console_newline(&c);               /* must scroll, not move past the region */

    check(c.row == 1 && c.col == 0,
          "newline on the last row scrolls and keeps cursor in the region");

    int moved = 1;
    for (uint64_t x = 8; x < 24; x++) {
        if (read_px(x, 10) != bottom[x - 8])
            moved = 0;
    }
    check(moved, "scrolling copies the bottom text row to the top text row");
    check(glyph_matches(8, 10, 'C', FG),
          "scrolled glyph remains intact after the row move");
}

static void test_scroll_clears_bottom_line(void) {
    reset_fb();
    struct console c;
    init_console(&c);
    console_write(&c, "AB");
    console_putc(&c, 'C');
    console_newline(&c);

    int cleared = 1;
    for (uint64_t y = 20; y < 30; y++) {
        for (uint64_t x = 8; x < 24; x++) {
            if (read_px(x, y) != BG)
                cleared = 0;
        }
    }
    check(cleared, "scrolling clears the vacated bottom line with the bg colour");
}

static void test_scroll_confined_to_region(void) {
    reset_fb();
    struct console c;
    init_console(&c);
    for (int i = 0; i < 12; i++)
        console_write(&c, "XY\n");

    int outside_ok = 1;
    for (uint64_t y = 0; y < H; y++) {
        for (uint64_t x = 0; x < W; x++) {
            int inside = (x >= 8 && x < 24 && y >= 10 && y < 30);
            if (!inside && read_px(x, y) != SENTINEL)
                outside_ok = 0;
        }
    }
    check(outside_ok,
          "repeated scrolling writes nothing outside the console region");
    check(c.col < c.cols && c.row < c.rows,
          "cursor stays bounded after many scrolling writes");
}

static void test_zero_sized_region(void) {
    reset_fb();
    struct console z;
    console_init(&z, 8, 10, 4, 4, FG, BG);     /* smaller than one cell */
    console_write(&z, "ignored\ntext");
    console_newline(&z);
    console_scroll(&z);
    check(z.cols == 0 && z.rows == 0 && z.col == 0 && z.row == 0,
          "undersized console has no cells and a stable cursor");

    int untouched = 1;
    for (uint64_t y = 0; y < H; y++) {
        for (uint64_t x = 0; x < W; x++) {
            if (read_px(x, y) != SENTINEL)
                untouched = 0;
        }
    }
    check(untouched, "undersized console performs no framebuffer writes");
}

static void test_non_aligned_pitch(void) {
    reset_fb();
    check(fb_width() == W && fb_height() == H,
          "fb_width/fb_height report the mock framebuffer size");

    fb_put_pixel(W - 1, H - 1, 0x0A0B0C0Du);
    const uint8_t *raw = pixels + (H - 1) * PITCH + (W - 1) * 4;
    check(raw[0] == 0x0d && raw[1] == 0x0c &&
          raw[2] == 0x0b && raw[3] == 0x0a,
          "pixel bytes land at y*pitch + x*4 with non-aligned pitch");
    expect_px(W - 1, H - 1, 0x0A0B0C0Du,
              "pixel reads back through the same non-aligned pitch");

    fb_fill_rect(0, 1, 4, 1, 0x11223344u);
    fb_copy_rows(2, 1, 0, 4, 1);
    expect_px(0, 2, 0x11223344u,
              "row copy honours source and destination pitch");
}

static void test_framebuffer_clipping(void) {
    reset_fb();
    fb_put_pixel(W, H, FG);
    check(read_px(W - 1, H - 1) == SENTINEL,
          "out-of-bounds put_pixel is discarded");

    fb_fill_rect(W - 2, H - 2, 4, 4, FG);
    check(read_px(W - 1, H - 1) == FG && read_px(W - 2, H - 2) == FG &&
          read_px(W - 3, H - 1) == SENTINEL && read_px(W - 1, H - 3) == SENTINEL,
          "fill_rect clips to the framebuffer edges");
}

static void test_draw_string_newline(void) {
    reset_fb();
    fb_fill_rect(0, 0, W, H, BG);       /* draw_string does not clear a bg */
    fb_draw_string(4, 4, "AB\nCD", FG);
    check(glyph_matches(4, 4, 'A', FG) && glyph_matches(12, 4, 'B', FG) &&
          glyph_matches(4, 14, 'C', FG) && glyph_matches(12, 14, 'D', FG),
          "draw_string restarts at its own x after a newline");
}

static void test_console_clear_resets_cursor(void) {
    reset_fb();
    struct console c;
    init_console(&c);
    console_write(&c, "AB");
    console_clear(&c);
    check(c.col == 0 && c.row == 0,
          "console_clear resets the cursor");
    check(glyph_matches(8, 10, ' ', BG) && read_px(23, 29) == BG,
          "console_clear repaints the whole region with the bg colour");
}

static void test_control_characters_ignored(void) {
    reset_fb();
    struct console c;
    init_console(&c);
    console_write(&c, "\t\x7f");       /* tab and DEL */
    check(c.col == 0 && c.row == 0,
          "unsupported control characters do not move the cursor");
    check(read_px(8, 10) == BG && read_px(23, 29) == BG,
          "unsupported control characters draw no glyphs");
}

int main(void) {
    test_init_clamps_region();
    test_init_clears_region();
    test_character_rendering();
    test_set_color();
    test_newline();
    test_carriage_return();
    test_wrapping();
    test_scroll_moves_rows();
    test_scroll_clears_bottom_line();
    test_scroll_confined_to_region();
    test_zero_sized_region();
    test_non_aligned_pitch();
    test_framebuffer_clipping();
    test_draw_string_newline();
    test_console_clear_resets_cursor();
    test_control_characters_ignored();

    printf("\n%d checks, %d failures: %s\n",
           checks, failures, failures ? "FAILED" : "ALL TESTS PASSED");
    return failures != 0;
}
