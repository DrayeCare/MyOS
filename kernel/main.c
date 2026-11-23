#include <stdint.h>
#include <limine.h>

#include "console.h"
#include "framebuffer.h"
#include "input.h"
#include "interrupts.h"
#include "keyboard.h"
#include "pmm.h"
#include "shell.h"
#include "version.h"

#define LIMINE_BASE_REVISION(N) \
    { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N) }

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(3);

#define LIMINE_FRAMEBUFFER_REQUEST_ID \
    { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b }

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

#define LIMINE_MEMMAP_REQUEST_ID \
    { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62 }

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

#define LIMINE_HHDM_REQUEST_ID \
    { LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b }

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

#define LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID \
    { LIMINE_COMMON_MAGIC, 0x71ba76863cc55f63, 0xb2644a48c516a487 }

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request executable_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

extern char _kernel_start[];
extern char _kernel_end[];

#define BG       0x101820
#define PANEL    0x1B2838
#define ACCENT   0x00A6A6
#define WHITE    0xE6EDF3
#define WARN     0xFFB86C

static void hcf(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* Small freestanding decimal formatter for boot statistics. */
static const char *boot_decimal(uint64_t value, char *buffer) {
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

/* Static UI: background, central panel, logo mark and accent bar.
 * The console lives inside the panel and is initialised separately. */
static void draw_screen(void) {
    uint64_t w = fb_width();
    uint64_t h = fb_height();

    fb_fill_rect(0, 0, w, h, BG);
    fb_fill_rect(w / 8, h / 5, w * 3 / 4, h * 3 / 5, PANEL);

    uint64_t cx = w / 2;
    uint64_t cy = h / 2 - 30;

    fb_fill_rect(cx - 50, cy - 50, 100, 100, ACCENT);
    fb_fill_rect(cx - 30, cy - 30, 60, 60, PANEL);

    fb_fill_rect(cx - 100, cy + 80, 200, 8, 0x334155);
    fb_fill_rect(cx - 100, cy + 80, 140, 8, ACCENT);

    fb_fill_rect(w / 8, h * 4 / 5, w * 3 / 4, 3, ACCENT);
}

void kmain(void) {
    if (framebuffer_request.response == 0 ||
        framebuffer_request.response->framebuffer_count == 0) {
        hcf();
    }

    struct limine_framebuffer *framebuffer =
        framebuffer_request.response->framebuffers[0];

    if (framebuffer == 0 || framebuffer->address == 0 ||
        framebuffer->bpp != 32) {
        hcf();
    }

    fb_init(framebuffer);
    draw_screen();

    uint64_t left = fb_width() / 8 + 30;
    uint64_t top = fb_height() / 5 + 30;

    fb_draw_string(left, top, "MyOS", ACCENT);
    fb_draw_string(left, top + 25, "Kernel " MYOS_VERSION_STRING, WHITE);

    /* Console region: below the title, above the panel's bottom edge. */
    uint64_t console_top = top + 65;
    uint64_t panel_bottom = fb_height() * 4 / 5;
    uint64_t console_bottom =
        panel_bottom > console_top ? panel_bottom - 16 : console_top;
    uint64_t console_right =
        fb_width() > left + 30 ? fb_width() * 7 / 8 - 30 : left;

    struct console cons;
    console_init(&cons, left, console_top,
                 console_right - left, console_bottom - console_top,
                 WHITE, PANEL);

    console_set_color(&cons, ACCENT);
    console_write(&cons, "[ OK ] Framebuffer initialized\n");
    console_write(&cons, "[ OK ] Limine boot successful\n");
    console_write(&cons, "[ OK ] Kernel running\n");

    /* Initialise the physical allocator from Limine's map before any page
     * can be requested. Metadata is placed through the HHDM offset. */
    int pmm_ok = -1;
    struct pmm_stats stats;

    if (memmap_request.response != 0 &&
        hhdm_request.response != 0 &&
        executable_address_request.response != 0) {

        struct pmm_region regions[PMM_MAX_REGIONS];
        uint64_t count = memmap_request.response->entry_count;
        if (count > PMM_MAX_REGIONS)
            count = PMM_MAX_REGIONS;

        for (uint64_t i = 0; i < count; i++) {
            struct limine_memmap_entry *entry =
                memmap_request.response->entries[i];
            regions[i].base = entry->base;
            regions[i].length = entry->length;
            regions[i].type = entry->type;
        }

        uint64_t kernel_phys =
            executable_address_request.response->physical_base;
        uint64_t kernel_size =
            (uint64_t)_kernel_end - (uint64_t)_kernel_start;

        pmm_ok = pmm_init(hhdm_request.response->offset,
                          kernel_phys, kernel_phys + kernel_size,
                          regions, count);
    }

    if (pmm_ok == 0) {
        pmm_get_stats(&stats);
        char a[21], b[21];

        console_write(&cons, "[ OK ] PMM ready: ");
        console_write(&cons, boot_decimal(stats.total_usable_bytes >> 20, a));
        console_write(&cons, " MiB usable, ");
        console_write(&cons, boot_decimal(stats.free_pages, b));
        console_write(&cons, " free pages\n");
    } else {
        console_set_color(&cons, WARN);
        console_write(&cons, "[ !! ] PMM initialization failed\n");
        console_set_color(&cons, ACCENT);
    }

    interrupts_init();
    console_write(&cons, "[ OK ] IDT and PIC initialized\n");

    /* IRQ1 stays masked during the controller's ACK-polling setup. */
    int keyboard_ok = keyboard_init();
    if (keyboard_ok == 0) {
        console_write(&cons, "[ OK ] PS/2 keyboard ready\n");
    } else {
        console_set_color(&cons, WARN);
        console_write(&cons, "[ !! ] PS/2 keyboard init failed\n");
        console_set_color(&cons, ACCENT);
    }

    console_set_color(&cons, WHITE);
    console_write(&cons, "\nReady.\n");

    struct input_line line;
    input_init(&line, &cons);

    struct shell shell;
    shell_init(&shell, &cons, WHITE);

    /* Interrupts are enabled only after every handler is registered. */
    interrupts_enable();

    /* Deferred input processing: the ISR only queues scancodes. */
    for (;;) {
        struct key_event event;

        while (keyboard_pop(&event)) {
            input_handle(&line, &event);

            if (input_ready(&line)) {
                shell_submit(&shell, &line);
            }
        }

        __asm__ volatile ("hlt");
    }
}
