#include <stdint.h>

#include "console.h"
#include "input.h"
#include "pmm.h"
#include "shell.h"
#include "version.h"

#define COLOR_WHITE 0xE6EDF3u
#define COLOR_ACCENT 0x00A6A6u
#define COLOR_AMBER 0xFFB86Cu
#define COLOR_GREEN 0x66E08Au
#define COLOR_RED 0xFF6B6Bu

struct parsed_line {
    char command[SHELL_COMMAND_MAX];
    char argument[SHELL_ARG_MAX];
    int has_argument;
};

static inline void io_outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %%al, %%dx" :: "a"(value), "d"(port));
}

static inline uint8_t io_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %%dx, %0" : "=a"(value) : "d"(port));
    return value;
}

static inline void io_outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %%ax, %%dx" :: "a"(value), "d"(port));
}

void shell_init(struct shell *sh, struct console *console, uint32_t color) {
    sh->console = console;
    sh->color = color;
    console_set_color(console, color);
}

/* Bounded tokenizer: only single spaces/tabs separate the command from its
 * argument. The argument keeps its original case and is trimmed at both ends. */
static void parse_line(const char *line, struct parsed_line *out) {
    unsigned i = 0;
    unsigned n = 0;

    out->command[0] = '\0';
    out->argument[0] = '\0';
    out->has_argument = 0;

    while (line[i] == ' ' || line[i] == '\t')
        i++;

    while (line[i] != '\0' && line[i] != ' ' && line[i] != '\t') {
        if (n + 1 < SHELL_COMMAND_MAX)
            out->command[n++] = line[i];
        i++;
    }
    out->command[n] = '\0';

    while (line[i] == ' ' || line[i] == '\t')
        i++;

    unsigned start = i;
    unsigned end = i;
    while (line[end] != '\0')
        end++;
    while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t'))
        end--;

    unsigned length = end - start;
    if (length >= SHELL_ARG_MAX)
        length = SHELL_ARG_MAX - 1;

    for (unsigned j = 0; j < length; j++)
        out->argument[j] = line[start + j];
    out->argument[length] = '\0';
    out->has_argument = length != 0;
}

static int equals_ignore_case(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;

        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb + ('a' - 'A'));
        if (ca != cb)
            return 0;

        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void put(struct shell *sh, const char *text) {
    console_set_color(sh->console, sh->color);
    console_write(sh->console, text);
}

static void put_line(struct shell *sh, const char *text) {
    put(sh, text);
    console_newline(sh->console);
}

static void put_error(struct shell *sh, const char *prefix, const char *value) {
    console_set_color(sh->console, COLOR_AMBER);
    console_write(sh->console, prefix);
    console_write(sh->console, value);
    console_newline(sh->console);
    console_set_color(sh->console, sh->color);
}

static void print_help(struct shell *sh) {
    put_line(sh, "Available commands:");
    put_line(sh, "  help            Show this list");
    put_line(sh, "  clear           Clear the console");
    put_line(sh, "  echo <text>     Print text");
    put_line(sh, "  about           About MyOS");
    put_line(sh, "  version         Version and build info");
    put_line(sh, "  color [name]    white teal amber green red");
    put_line(sh, "  mem             Physical memory statistics");
    put_line(sh, "  pmmtest         Runtime physical page self-test");
    put_line(sh, "  reboot          Reboot via the 8042 controller");
    put_line(sh, "  shutdown        Request shutdown (QEMU ISA port)");
}

static void command_about(struct shell *sh) {
    put_line(sh, "MyOS is a minimal freestanding x86_64 kernel.");
    put_line(sh, "Booted by Limine, tested with QEMU.");
}

static void command_version(struct shell *sh) {
    put_line(sh, "MyOS " MYOS_VERSION_STRING " (x86_64, freestanding)");
    put_line(sh, "Build: " MYOS_BUILD_COMPILER " " MYOS_BUILD_DATE " " MYOS_BUILD_TIME);
    put_line(sh, "Bootloader: Limine (BIOS and UEFI)");
}

static void command_color(struct shell *sh, const struct parsed_line *p) {
    if (!p->has_argument) {
        put_line(sh, "Colors: white teal amber green red");
        return;
    }

    uint32_t color;
    const char *name = p->argument;

    if (equals_ignore_case(name, "white")) {
        color = COLOR_WHITE;
    } else if (equals_ignore_case(name, "teal")) {
        color = COLOR_ACCENT;
    } else if (equals_ignore_case(name, "amber")) {
        color = COLOR_AMBER;
    } else if (equals_ignore_case(name, "green")) {
        color = COLOR_GREEN;
    } else if (equals_ignore_case(name, "red")) {
        color = COLOR_RED;
    } else {
        put_error(sh, "Unknown color: ", name);
        put_line(sh, "Colors: white teal amber green red");
        return;
    }

    sh->color = color;
    console_set_color(sh->console, color);
    put(sh, "Console color: ");
    put_line(sh, name);
}

static const char *format_dec(uint64_t value, char *buffer) {
    char digits[21];
    unsigned n = 0;

    if (value == 0)
        digits[n++] = '0';
    while (value != 0) {
        digits[n++] = (char)('0' + (value % 10));
        value /= 10;
    }

    unsigned i = 0;
    while (n != 0)
        buffer[i++] = digits[--n];
    buffer[i] = '\0';
    return buffer;
}

static void command_mem(struct shell *sh) {
    struct pmm_stats stats;
    pmm_get_stats(&stats);

    if (!pmm_initialized()) {
        put_error(sh, "PMM: ", "not initialized");
        return;
    }

    char a[21], b[21], c[21], d[21], e[21];

    put(sh, "Usable RAM: ");
    put(sh, format_dec(stats.total_usable_bytes >> 20, a));
    put_line(sh, " MiB");
    put(sh, "Managed pages: ");
    put_line(sh, format_dec(stats.managed_pages, b));
    put(sh, "Used pages: ");
    put_line(sh, format_dec(stats.used_pages, c));
    put(sh, "Free pages: ");
    put_line(sh, format_dec(stats.free_pages, d));
    put(sh, "Metadata pages: ");
    put_line(sh, format_dec(stats.metadata_pages, e));
}

static void command_pmmtest(struct shell *sh) {
    int rc = pmm_self_test();

    if (rc == 0) {
        console_set_color(sh->console, COLOR_GREEN);
        put_line(sh, "PMM self-test: OK");
        console_set_color(sh->console, sh->color);
    } else if (rc == -1) {
        put_error(sh, "PMM self-test: ", "allocator not initialized");
    } else {
        put_error(sh, "PMM self-test: ", "allocation or pattern check failed");
    }
}

static void machine_reboot(void) {
    __asm__ volatile ("cli");

    /* Bounded wait for the controller input buffer to drain. */
    for (unsigned i = 0; i < 100000; i++) {
        if ((io_inb(0x64) & 0x02) == 0)
            break;
    }

    /* 0xFE on the 8042 command port pulses the system reset line. */
    io_outb(0x64, 0xfe);

    for (;;)
        __asm__ volatile ("hlt");
}

static void machine_shutdown(void) {
    __asm__ volatile ("cli");

    /* QEMU/Bochs ISA shutdown port. Unclaimed on other hardware, so this is
     * a request, not a guarantee; the message says exactly that. */
    io_outw(0x604, 0x2000);

    for (;;)
        __asm__ volatile ("hlt");
}

void shell_execute(struct shell *sh, const char *line) {
    if (line == 0)
        return;

    struct parsed_line p;
    parse_line(line, &p);

    if (p.command[0] == '\0')
        return;                         /* empty or whitespace-only input */

    if (equals_ignore_case(p.command, "help")) {
        print_help(sh);
    } else if (equals_ignore_case(p.command, "clear")) {
        console_clear(sh->console);
    } else if (equals_ignore_case(p.command, "echo")) {
        if (p.has_argument)
            put_line(sh, p.argument);
        else
            put_error(sh, "Usage: ", "echo <text>");
    } else if (equals_ignore_case(p.command, "about")) {
        command_about(sh);
    } else if (equals_ignore_case(p.command, "version")) {
        command_version(sh);
    } else if (equals_ignore_case(p.command, "color")) {
        command_color(sh, &p);
    } else if (equals_ignore_case(p.command, "mem")) {
        command_mem(sh);
    } else if (equals_ignore_case(p.command, "pmmtest")) {
        command_pmmtest(sh);
    } else if (equals_ignore_case(p.command, "reboot")) {
        put_line(sh, "Rebooting...");
        machine_reboot();
    } else if (equals_ignore_case(p.command, "shutdown")) {
        put_line(sh, "Shutdown requested (QEMU ISA port 0x604).");
        put_line(sh, "If the machine stays on, use the QEMU monitor 'quit'.");
        machine_shutdown();
    } else {
        put_error(sh, "Unknown command: ", p.command);
        put_line(sh, "Type 'help' for available commands.");
    }
}

void shell_submit(struct shell *sh, struct input_line *line) {
    shell_execute(sh, input_text(line));
    input_accept(line);
}
