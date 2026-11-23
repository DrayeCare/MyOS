/* Host-side tests for the command shell. These compile the real shell.c,
 * input.c and console.c against a mock framebuffer; no hardware is used. */
#include <stdint.h>
#include <stdio.h>

#include "limine.h"
#include "console.h"
#include "framebuffer.h"
#include "input.h"
#include "pmm.h"
#include "shell.h"
#include "version.h"

/* Declared by font8x8_basic.h inside framebuffer.c. */
extern char font8x8_basic[128][8];

#define W 256
#define H 400
#define PITCH 1026            /* > W*4 and deliberately not 4-aligned */
#define SENTINEL 0xDEADBEEFu
#define BG 0x112233u
#define FG 0xE6EDF3u
#define GREEN 0x66E08Au

#define COLS 31
#define ROWS 39
#define ORIGIN_X 4
#define ORIGIN_Y 4

static uint8_t pixels[H * PITCH];
static uint8_t pmm_metadata[65536];
static struct limine_framebuffer mock;
static struct console cons;
static struct shell sh;
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

static void start(void) {
    reset_fb();
    console_init(&cons, ORIGIN_X, ORIGIN_Y, 248, 390, FG, BG);
    shell_init(&sh, &cons, FG);
}

static char cell_char(unsigned col, unsigned row) {
    uint8_t bits[8] = {0};

    for (unsigned r = 0; r < 8; r++) {
        for (unsigned c = 0; c < 8; c++) {
            uint32_t p = read_px(ORIGIN_X + col * 8 + c,
                                 ORIGIN_Y + row * 10 + r);
            if ((p & 0x00ffffff) != (BG & 0x00ffffff))
                bits[r] |= (uint8_t)(1u << c);
        }
    }

    int lit = 0;
    for (unsigned r = 0; r < 8; r++)
        lit |= bits[r];
    if (!lit)
        return ' ';

    for (unsigned code = 32; code < 127; code++) {
        int same = 1;
        for (unsigned r = 0; r < 8; r++) {
            if (bits[r] != (uint8_t)font8x8_basic[code][r]) {
                same = 0;
                break;
            }
        }
        if (same)
            return (char)code;
    }
    return '?';
}

/* Reconstruct the visible console text, one '\n' per text row. */
static void capture(char *out, unsigned cap) {
    unsigned n = 0;

    for (unsigned row = 0; row < ROWS && n + 1 < cap; row++) {
        for (unsigned col = 0; col < COLS && n + 1 < cap; col++)
            out[n++] = cell_char(col, row);
        if (n + 1 < cap)
            out[n++] = '\n';
    }
    out[n] = '\0';
}

static int contains(const char *haystack, const char *needle) {
    if (needle[0] == '\0')
        return 1;

    for (const char *p = haystack; *p != '\0'; p++) {
        const char *a = p;
        const char *b = needle;
        while (*a != '\0' && *b != '\0' && *a == *b) {
            a++;
            b++;
        }
        if (*b == '\0')
            return 1;
    }
    return 0;
}

static unsigned count_char(const char *text, char ch) {
    unsigned n = 0;
    while (*text) {
        if (*text == ch)
            n++;
        text++;
    }
    return n;
}

static unsigned lit_pixels(void) {
    unsigned n = 0;
    for (unsigned row = 0; row < ROWS; row++) {
        for (unsigned col = 0; col < COLS; col++) {
            for (unsigned r = 0; r < 8; r++) {
                for (unsigned c = 0; c < 8; c++) {
                    uint32_t p = read_px(ORIGIN_X + col * 8 + c,
                                         ORIGIN_Y + row * 10 + r);
                    if ((p & 0x00ffffff) != (BG & 0x00ffffff))
                        n++;
                }
            }
        }
    }
    return n;
}

static unsigned first_nonblank_row(const char *text, char *out, unsigned cap) {
    const char *row = text;
    unsigned index = 0;

    while (*row) {
        const char *end = row;
        while (*end != '\0' && *end != '\n')
            end++;

        unsigned n = 0;
        for (const char *p = row; p < end && n + 1 < cap; p++) {
            if (*p != ' ')
                break;
            n++;
        }

        if (n < (unsigned)(end - row)) {
            unsigned i = 0;
            for (const char *p = row + n; p < end && i + 1 < cap; p++)
                out[i++] = *p;
            while (i > 0 && out[i - 1] == ' ')
                i--;
            out[i] = '\0';
            return index;
        }

        if (*end == '\0')
            break;
        row = end + 1;
        index++;
    }
    out[0] = '\0';
    return index;
}

static void test_empty_input(void) {
    start();
    shell_execute(&sh, "");
    shell_execute(&sh, "   \t   ");
    check(lit_pixels() == 0, "empty and whitespace-only input produce no output");
}

static void test_leading_trailing_spaces(void) {
    start();
    shell_execute(&sh, "   echo   hi   ");
    char text[2048];
    capture(text, sizeof(text));

    char first[64];
    first_nonblank_row(text, first, sizeof(first));
    check(first[0] == 'h' && first[1] == 'i' && first[2] == '\0',
          "leading and trailing spaces are trimmed before execution");
}

static void test_echo_arguments(void) {
    start();
    shell_execute(&sh, "echo Hello MyOS");
    char text[2048];
    capture(text, sizeof(text));
    check(contains(text, "Hello MyOS"), "echo prints all supplied arguments");
}

static void test_unknown_command(void) {
    start();
    shell_execute(&sh, "invalidcommand");
    char text[2048];
    capture(text, sizeof(text));
    check(contains(text, "Unknown command:") &&
          contains(text, "invalidcommand"),
          "unknown commands produce a clear error");
}

static void test_help(void) {
    start();
    shell_execute(&sh, "help");
    char text[2048];
    capture(text, sizeof(text));

    const char *words[] = {"Available commands:", "help", "clear", "echo",
                           "about", "version", "color", "mem", "pmmtest",
                           "reboot", "shutdown"};
    int all = 1;
    for (unsigned i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        if (!contains(text, words[i]))
            all = 0;
    }
    check(all, "help lists every implemented command");
}

static void test_case_handling(void) {
    start();
    shell_execute(&sh, "HELP");
    char text[2048];
    capture(text, sizeof(text));
    check(contains(text, "Available commands:"),
          "command names are case-insensitive");

    start();
    shell_execute(&sh, "ECHO MiXeD");
    capture(text, sizeof(text));
    check(contains(text, "MiXeD"), "echo preserves the argument's case");
}

static void test_about_and_version(void) {
    start();
    shell_execute(&sh, "about");
    char text[2048];
    capture(text, sizeof(text));
    check(contains(text, "MyOS is a minimal"),
          "about describes MyOS");

    start();
    shell_execute(&sh, "version");
    capture(text, sizeof(text));
    check(contains(text, "MyOS v0.1") && contains(text, "Build:") &&
          contains(text, "Limine"),
          "version reports the actual version and build information");
}

static void test_color(void) {
    start();
    shell_execute(&sh, "color");
    char text[2048];
    capture(text, sizeof(text));
    check(contains(text, "Colors:") && contains(text, "white"),
          "color without arguments lists the palette");

    shell_execute(&sh, "color green");
    capture(text, sizeof(text));
    check(sh.color == GREEN && cons.fg == GREEN &&
          contains(text, "Console color: green"),
          "color sets the console foreground colour");

    shell_execute(&sh, "color bogus");
    capture(text, sizeof(text));
    check(sh.color == GREEN && contains(text, "Unknown color: bogus"),
          "unknown colour names are rejected without changing the colour");
}

static void test_clear(void) {
    start();
    shell_execute(&sh, "echo before clear");
    check(lit_pixels() > 0, "output exists before clear");
    shell_execute(&sh, "clear");
    check(lit_pixels() == 0, "clear removes all console output");
}

static void test_submit_restores_prompt(void) {
    start();

    struct input_line line;
    input_init(&line, &cons);

    const char *command = "echo submitted";
    for (unsigned i = 0; command[i] != '\0'; i++)
        line.buffer[i] = command[i];
    line.length = 0;
    while (command[line.length] != '\0')
        line.length++;
    line.buffer[line.length] = '\0';
    line.ready = 1;

    shell_submit(&sh, &line);

    char text[2048];
    capture(text, sizeof(text));
    check(contains(text, "submitted") && contains(text, "MyOS>"),
          "submitting a line executes it and restores the prompt");
    check(!input_ready(&line) && input_length(&line) == 0,
          "submitting a line resets the input buffer");
}

static void test_bounds(void) {
    start();

    char long_arg[400];
    for (unsigned i = 0; i < sizeof(long_arg) - 1; i++)
        long_arg[i] = 'a';
    long_arg[sizeof(long_arg) - 1] = '\0';

    /* Build the command manually: shell_execute takes a single C string. */
    char command[420];
    unsigned n = 0;
    const char prefix[] = "echo ";
    for (unsigned i = 0; prefix[i] != '\0'; i++)
        command[n++] = prefix[i];
    for (unsigned i = 0; long_arg[i] != '\0'; i++)
        command[n++] = long_arg[i];
    command[n] = '\0';

    shell_execute(&sh, command);
    char text[4096];
    capture(text, sizeof(text));
    check(count_char(text, 'a') == SHELL_ARG_MAX - 1,
          "overlong arguments are bounded to SHELL_ARG_MAX");

    start();
    char long_cmd[240];
    for (unsigned i = 0; i < sizeof(long_cmd) - 1; i++)
        long_cmd[i] = 'z';
    long_cmd[sizeof(long_cmd) - 1] = '\0';
    shell_execute(&sh, long_cmd);
    capture(text, sizeof(text));
    check(contains(text, "Unknown command:"),
          "overlong command names stay bounded and are reported as unknown");
}

static void test_mem_command(void) {
    start();

    shell_execute(&sh, "mem");
    char text[4096];
    capture(text, sizeof(text));
    check(contains(text, "PMM:") && contains(text, "not initialized"),
          "mem reports an uninitialized allocator clearly");

    struct pmm_region regions[] = { { 0x100000, 4 * 1024 * 1024, PMM_USABLE } };
    uint64_t offset = (uint64_t)(uintptr_t)pmm_metadata - 0x100000;
    check(pmm_init(offset, 0, 0, regions, 1) == 0,
          "shell test sets up a synthetic PMM for the mem command");

    shell_execute(&sh, "mem");
    capture(text, sizeof(text));
    check(contains(text, "Usable RAM:") && contains(text, "MiB") &&
          contains(text, "Managed pages:") && contains(text, "Free pages:") &&
          contains(text, "Metadata pages:"),
          "mem displays PMM statistics through the console API");

    shell_execute(&sh, "pmmtest");
    capture(text, sizeof(text));
    check(contains(text, "PMM self-test: OK"),
          "pmmtest runs the runtime allocation diagnostic through the shell");
}

int main(void) {
    test_empty_input();
    test_leading_trailing_spaces();
    test_echo_arguments();
    test_unknown_command();
    test_help();
    test_case_handling();
    test_about_and_version();
    test_color();
    test_clear();
    test_submit_restores_prompt();
    test_bounds();
    test_mem_command();

    printf("\n%d checks, %d failures: %s\n",
           checks, failures, failures ? "FAILED" : "ALL TESTS PASSED");
    return failures != 0;
}
