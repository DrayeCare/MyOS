/* Host-side tests for the bounded input line and console backspace.
 * Hardware IRQ behavior is verified separately in QEMU, not here. */
#include <stdint.h>
#include <stdio.h>

#include "limine.h"
#include "console.h"
#include "framebuffer.h"
#include "input.h"
#include "keyboard.h"

/* Declared by font8x8_basic.h inside framebuffer.c. */
extern char font8x8_basic[128][8];

#define W 72
#define H 60
#define PITCH 290            /* > W*4 and not a multiple of 4 */
#define SENTINEL 0xDEADBEEFu
#define FG 0x00FF00u
#define BG 0x112233u

static uint8_t pixels[H * PITCH];
static struct limine_framebuffer mock;
static struct console cons;
static struct input_line line;
static int checks;
static int failures;

static void check(int condition, const char *label) {
    checks++;
    printf("%s  %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition)
        failures++;
}

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

static void start(void) {
    reset_fb();
    /* x=4..67, y=4..53 -> 8 columns x 5 rows. */
    console_init(&cons, 4, 4, 64, 50, FG, BG);
    input_init(&line, &cons);
}

static struct key_event char_key(char c) {
    struct key_event e = {0};
    e.ascii = (uint8_t)c;
    e.pressed = 1;
    return e;
}

static struct key_event special_key(int backspace, int enter) {
    struct key_event e = {0};
    e.pressed = 1;
    e.backspace = (uint8_t)backspace;
    e.enter = (uint8_t)enter;
    return e;
}

static void type_string(const char *s) {
    while (*s) {
        struct key_event e = char_key(*s++);
        input_handle(&line, &e);
    }
}

static void test_prompt(void) {
    start();
    check(input_length(&line) == 0 && !input_ready(&line),
          "init starts with an empty, unfinished line");
    check(cons.col == 6 && cons.row == 0,
          "prompt leaves the cursor after 'MyOS> '");
    check(glyph_matches(4, 4, 'M', FG) && glyph_matches(36, 4, '>', FG),
          "prompt is rendered into the console region");
}

static void test_typing_and_wrap(void) {
    start();
    type_string("ab");
    check(input_length(&line) == 2 && input_text(&line)[2] == '\0',
          "typed characters are stored in the line buffer");
    check(input_text(&line)[0] == 'a' && input_text(&line)[1] == 'b',
          "line buffer preserves typed order");
    check(cons.row == 1 && cons.col == 0,
          "input wraps within the console region");
    check(glyph_matches(52, 4, 'a', FG) && glyph_matches(60, 4, 'b', FG),
          "typed characters are echoed as they are typed");
}

static void test_backspace(void) {
    start();
    type_string("ab");
    struct key_event bs = special_key(1, 0);

    input_handle(&line, &bs);
    check(input_length(&line) == 1 && input_text(&line)[0] == 'a',
          "Backspace removes the last buffer character");
    check(glyph_matches(60, 4, ' ', BG),
          "Backspace erases the glyph from the screen");

    input_handle(&line, &bs);
    check(input_length(&line) == 0 && cons.col == 6,
          "Backspace at the start of the input does not move past the prompt");

    input_handle(&line, &bs);
    check(input_length(&line) == 0 && cons.col == 6,
          "extra Backspace on an empty line is harmless");
}

static void test_enter_and_fresh_prompt(void) {
    start();
    type_string("hi");
    struct key_event enter = special_key(0, 1);
    input_handle(&line, &enter);

    check(input_ready(&line) && input_text(&line)[0] == 'h' &&
          input_text(&line)[1] == 'i' && input_text(&line)[2] == '\0',
          "Enter finishes the line and keeps its text available");

    /* The caller displays the completed line, then accepts it. */
    console_write(&cons, input_text(&line));
    input_accept(&line);

    check(!input_ready(&line) && input_length(&line) == 0,
          "accepting a line resets input state");
    check(cons.row == 3 && cons.col == 6,
          "a fresh prompt is printed on a new row");
    check(glyph_matches(4, 34, 'M', FG),
          "fresh prompt is rendered after the submitted line");
}

static void test_releases_ignored(void) {
    start();
    struct key_event e = char_key('a');
    input_handle(&line, &e);
    e.pressed = 0;
    input_handle(&line, &e);

    struct key_event bs = special_key(1, 0);
    bs.pressed = 0;
    input_handle(&line, &bs);

    struct key_event enter = special_key(0, 1);
    enter.pressed = 0;
    input_handle(&line, &enter);

    check(input_length(&line) == 1 && !input_ready(&line),
          "key releases never modify or submit the line");
}

static void test_bounded_buffer(void) {
    start();
    for (int i = 0; i < 200; i++) {
        struct key_event e = char_key('x');
        input_handle(&line, &e);
    }

    check(input_length(&line) == INPUT_LINE_MAX,
          "input buffer stops at its fixed capacity");
    check(input_text(&line)[INPUT_LINE_MAX - 1] == 'x' &&
          input_text(&line)[INPUT_LINE_MAX] == '\0',
          "bounded line remains NUL terminated");
    check(!input_ready(&line), "overflow input does not submit anything");

    int outside_ok = 1;
    for (uint64_t y = 0; y < H; y++) {
        for (uint64_t x = 0; x < W; x++) {
            int inside = (x >= 4 && x < 68 && y >= 4 && y < 54);
            if (!inside && read_px(x, y) != SENTINEL)
                outside_ok = 0;
        }
    }
    check(outside_ok, "long input never writes outside the console region");
}

static void test_control_chars_ignored(void) {
    start();
    struct key_event e = char_key('\0');
    input_handle(&line, &e);
    e.ascii = 127;
    input_handle(&line, &e);
    e.ascii = 31;
    input_handle(&line, &e);

    check(input_length(&line) == 0, "non-printable event payloads are discarded");
    check(glyph_matches(4, 4, 'M', FG),
          "ignored payloads do not disturb the prompt");
}

static void test_backspace_at_origin(void) {
    reset_fb();
    console_init(&cons, 4, 4, 64, 50, FG, BG);
    console_backspace(&cons);

    check(cons.col == 0 && cons.row == 0,
          "Backspace at the console origin is a no-op");
    check(read_px(4, 4) == BG && read_px(67, 53) == BG,
          "Backspace at the origin leaves the region cleared");
}

int main(void) {
    test_prompt();
    test_typing_and_wrap();
    test_backspace();
    test_enter_and_fresh_prompt();
    test_releases_ignored();
    test_bounded_buffer();
    test_control_chars_ignored();
    test_backspace_at_origin();

    printf("\n%d checks, %d failures: %s\n",
           checks, failures, failures ? "FAILED" : "ALL TESTS PASSED");
    return failures != 0;
}
