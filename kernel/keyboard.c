#include <stdint.h>

#include "interrupts.h"
#include "keyboard.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_CMD 0x64

#define STATUS_OUTPUT_FULL 0x01
#define STATUS_INPUT_EMPTY 0x02
#define IO_TIMEOUT 100000

static inline void io_outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %%al, %%dx" :: "a"(value), "d"(port));
}

static inline uint8_t io_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %%dx, %0" : "=a"(value) : "d"(port));
    return value;
}

static inline void io_wait(void) {
    io_outb(0x80, 0);
}

/* Scan code set 1, US layout. Zero means the key is not printable. */
static const uint8_t normal_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0a] = '9', [0x0b] = '0', [0x0c] = '-', [0x0d] = '=',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1a] = '[', [0x1b] = ']',
    [0x1e] = 'a', [0x1f] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2b] = '\\', [0x2c] = 'z', [0x2d] = 'x', [0x2e] = 'c',
    [0x2f] = 'v', [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
    [0x33] = ',', [0x34] = '.', [0x35] = '/', [0x39] = ' '
};

static const uint8_t shifted_map[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0a] = '(', [0x0b] = ')', [0x0c] = '_', [0x0d] = '+',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P', [0x1a] = '{', [0x1b] = '}',
    [0x1e] = 'A', [0x1f] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = ':', [0x28] = '"', [0x29] = '~',
    [0x2b] = '|', [0x2c] = 'Z', [0x2d] = 'X', [0x2e] = 'C',
    [0x2f] = 'V', [0x30] = 'B', [0x31] = 'N', [0x32] = 'M',
    [0x33] = '<', [0x34] = '>', [0x35] = '?', [0x39] = ' '
};

static uint8_t shift_left;
static uint8_t shift_right;
static uint8_t caps_lock;
static uint8_t extended;

static uint8_t queue[KEYBOARD_QUEUE_SIZE];
static unsigned queue_head;
static unsigned queue_tail;
static unsigned queue_count;

void keyboard_reset_state(void) {
    shift_left = 0;
    shift_right = 0;
    caps_lock = 0;
    extended = 0;
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
}

int keyboard_decode(uint8_t scancode, struct key_event *event) {
    event->ascii = 0;
    event->pressed = 1;
    event->backspace = 0;
    event->enter = 0;

    int released = (scancode & 0x80) != 0;
    uint8_t code = (uint8_t)(scancode & 0x7f);

    /* Extended prefixes are raw bytes 0xE0/0xE1; check them BEFORE masking
     * bit 7 (masking 0xE0 would wrongly produce 0x60). */
    if (scancode == 0xe0 || scancode == 0xe1) {
        extended = 1;
        return 0;
    }
    if (extended) {
        extended = 0;
        return 0;
    }

    if (code == 0x2a) {              /* left Shift */
        shift_left = !released;
        return 0;
    }
    if (code == 0x36) {              /* right Shift */
        shift_right = !released;
        return 0;
    }
    if (code == 0x3a) {              /* Caps Lock toggles on make only */
        if (!released)
            caps_lock = !caps_lock;
        return 0;
    }

    if (code == 0x0e || code == 0x1c) {
        event->pressed = !released;
        event->backspace = (code == 0x0e);
        event->enter = (code == 0x1c);
        return 1;
    }

    uint8_t base = normal_map[code];
    if (base == 0)
        return 0;                    /* unsupported key */

    int shifted = shift_left || shift_right;
    uint8_t ascii;

    if (base >= 'a' && base <= 'z') {
        /* Caps Lock affects letters only; Shift inverts its effect. */
        if (shifted != (int)caps_lock)
            ascii = (uint8_t)(base - 'a' + 'A');
        else
            ascii = base;
    } else {
        ascii = shifted ? shifted_map[code] : base;
    }

    event->ascii = ascii;
    event->pressed = !released;
    return 1;
}

void keyboard_irq(void) {
    uint8_t scancode = io_inb(PS2_DATA);

    if (queue_count < KEYBOARD_QUEUE_SIZE) {
        queue[queue_tail] = scancode;
        queue_tail = (queue_tail + 1) % KEYBOARD_QUEUE_SIZE;
        queue_count++;
    }
    /* Overflow drops the newest scancode; no console writes happen here. */
}

int keyboard_pop(struct key_event *event) {
    while (queue_count != 0) {
        uint8_t scancode = queue[queue_head];
        queue_head = (queue_head + 1) % KEYBOARD_QUEUE_SIZE;
        queue_count--;

        if (keyboard_decode(scancode, event))
            return 1;
    }
    return 0;
}

/* Bounded waits: absent hardware degrades to a failed init, never a hang. */
static int wait_input_empty(void) {
    for (unsigned i = 0; i < IO_TIMEOUT; i++) {
        if ((io_inb(PS2_STATUS) & STATUS_INPUT_EMPTY) == 0)
            return 1;
    }
    return 0;
}

static int read_data(void) {
    for (unsigned i = 0; i < IO_TIMEOUT; i++) {
        uint8_t status = io_inb(PS2_STATUS);

        /* QEMU sets status bit 5 on keyboard data even though the mouse is
         * disabled, so do not discard bytes based on that bit here. */
        if ((status & STATUS_OUTPUT_FULL) != 0)
            return io_inb(PS2_DATA);
    }
    return -1;
}

static void flush_output(void) {
    for (unsigned i = 0; i < 256; i++) {
        if ((io_inb(PS2_STATUS) & STATUS_OUTPUT_FULL) == 0)
            return;
        (void)io_inb(PS2_DATA);
    }
}

/* 0xD4 must precede every byte sent to the keyboard, arguments included. */
static int keyboard_command(uint8_t command, uint8_t argument) {
    if (!wait_input_empty())
        return 0;
    io_outb(PS2_CMD, 0xd4);

    if (!wait_input_empty())
        return 0;
    io_outb(PS2_DATA, command);
    int command_acked = (read_data() == 0xfa);
    if (!command_acked)
        return 0;

    /* Some emulated keyboards acknowledge only the command and not the
     * argument byte; the command ACK is sufficient for this driver. */
    io_wait();

    if (!wait_input_empty())
        return 1;
    io_outb(PS2_CMD, 0xd4);

    if (!wait_input_empty())
        return 1;
    io_outb(PS2_DATA, argument);
    (void)read_data();              /* consume a second ACK if present */
    return 1;
}

int keyboard_init(void) {
    keyboard_reset_state();

    /* IRQ1 stays masked while ACKs are polled: otherwise the ISR consumes
     * them before this code can read them. */
    irq_mask(1);

    io_outb(PS2_CMD, 0xad);          /* disable keyboard */
    io_wait();
    io_outb(PS2_CMD, 0xa7);          /* disable auxiliary device */
    io_wait();
    flush_output();

    io_outb(PS2_CMD, 0xaa);          /* controller self-test */
    io_wait();
    int self_test_ok = (read_data() == 0x55);

    /* The device must be enabled before it can answer device commands.
     * IRQ1 is still masked, so ACKs are consumed by this polling code. */
    io_outb(PS2_CMD, 0xae);
    io_wait();

    int set_ok = keyboard_command(0xf0, 0x01);   /* scan code set 1 */

    flush_output();

    irq_register(1, keyboard_irq);
    irq_unmask(1);

    return (self_test_ok && set_ok) ? 0 : -1;
}
