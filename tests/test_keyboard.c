/* Host-side tests for scan-code set 1 decoding and Shift/Caps state.
 * These tests exercise pure logic only; they do NOT verify hardware IRQs. */
#include <stdint.h>
#include <stdio.h>

#include "interrupts.h"
#include "keyboard.h"

/* Only keyboard_decode() is exercised on the host.  These stubs satisfy the
 * hardware-initialisation calls that are linked in but never invoked; real
 * IRQ wiring is verified in QEMU, not by this test binary. */
void irq_mask(unsigned irq) { (void)irq; }
void irq_unmask(unsigned irq) { (void)irq; }
void irq_register(unsigned irq, void (*handler)(void)) {
    (void)irq;
    (void)handler;
}

static int checks;
static int failures;

static void check(int condition, const char *label) {
    checks++;
    printf("%s  %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition)
        failures++;
}

static void expect_key(uint8_t code, uint8_t ascii, int pressed,
                       const char *label) {
    struct key_event e;
    int delivered = keyboard_decode(code, &e);

    if (!delivered || e.ascii != ascii || e.pressed != (uint8_t)pressed) {
        printf("       code %02x: delivered=%d ascii=%02x pressed=%d\n",
               code, delivered, e.ascii, e.pressed);
    }
    check(delivered && e.ascii == ascii && e.pressed == (uint8_t)pressed,
          label);
}

static void expect_ignored(uint8_t code, const char *label) {
    struct key_event e;
    check(keyboard_decode(code, &e) == 0, label);
}

static void test_lowercase_and_release(void) {
    keyboard_reset_state();
    expect_key(0x1e, 'a', 1, "lowercase letter press is decoded");
    expect_key(0x9e, 'a', 0, "key release keeps the character but clears pressed");
}

static void test_digits_and_punctuation(void) {
    keyboard_reset_state();
    expect_key(0x02, '1', 1, "digit 1 is decoded");
    expect_key(0x0c, '-', 1, "minus is decoded");
    expect_key(0x33, ',', 1, "comma is decoded");
    expect_key(0x2b, '\\', 1, "backslash is decoded");
    expect_key(0x39, ' ', 1, "space is decoded");
}

static void test_left_shift(void) {
    keyboard_reset_state();
    expect_ignored(0x2a, "left Shift press changes state without an event");
    expect_key(0x1e, 'A', 1, "Shift + letter produces uppercase");
    expect_key(0x02, '!', 1, "Shift affects punctuation too");
    expect_ignored(0xaa, "left Shift release changes state without an event");
    expect_key(0x1e, 'a', 1, "letter is lowercase after Shift release");
}

static void test_right_shift(void) {
    keyboard_reset_state();
    expect_ignored(0x36, "right Shift press is tracked");
    expect_key(0x11, 'W', 1, "right Shift also uppercases letters");
    expect_ignored(0xb6, "right Shift release is tracked");
    expect_key(0x11, 'w', 1, "right Shift release restores lowercase");
}

static void test_both_shifts(void) {
    keyboard_reset_state();
    expect_ignored(0x2a, "left Shift held");
    expect_ignored(0x36, "right Shift held");
    expect_ignored(0xaa, "left Shift released while right remains held");
    expect_key(0x1e, 'A', 1, "Shift stays active while either side is held");
    expect_ignored(0xb6, "last Shift released");
    expect_key(0x1e, 'a', 1, "Shift clears after both sides are released");
}

static void test_caps_lock(void) {
    keyboard_reset_state();
    expect_ignored(0x3a, "Caps Lock press toggles state silently");
    expect_key(0x1e, 'A', 1, "Caps Lock uppercases letters");
    expect_key(0x02, '1', 1, "Caps Lock does not affect digits");
    expect_ignored(0xba, "Caps Lock release does not toggle again");
    expect_ignored(0x3a, "second Caps Lock press toggles off");
    expect_key(0x1e, 'a', 1, "letters are lowercase with Caps Lock off");
}

static void test_caps_with_shift(void) {
    keyboard_reset_state();
    expect_ignored(0x3a, "Caps Lock on");
    expect_ignored(0x2a, "Shift held with Caps Lock on");
    expect_key(0x1e, 'a', 1, "Shift inverts Caps Lock for letters");
    expect_key(0x02, '!', 1, "Shift still selects punctuation with Caps Lock on");
}

static void test_backspace_and_enter(void) {
    keyboard_reset_state();

    struct key_event e;
    check(keyboard_decode(0x0e, &e) && e.backspace && e.pressed && e.ascii == 0,
          "Backspace press produces a backspace event");
    check(keyboard_decode(0x8e, &e) && e.backspace && !e.pressed,
          "Backspace release is reported as released");

    check(keyboard_decode(0x1c, &e) && e.enter && e.pressed && e.ascii == 0,
          "Enter press produces an enter event");
    check(keyboard_decode(0x9c, &e) && e.enter && !e.pressed,
          "Enter release is reported as released");
}

static void test_unsupported_codes(void) {
    keyboard_reset_state();
    expect_ignored(0x37, "keypad multiply is ignored safely");
    expect_ignored(0x3b, "function keys are ignored safely");
    expect_ignored(0x00, "zero scancode is ignored safely");
    expect_ignored(0x7f, "unknown high scancode is ignored safely");
}

static void test_extended_prefixes(void) {
    keyboard_reset_state();
    expect_ignored(0xe0, "E0 prefix is consumed");
    expect_ignored(0x1c, "extended Enter is ignored");
    expect_key(0x1e, 'a', 1, "normal keys still decode after extended prefix");
}

int main(void) {
    test_lowercase_and_release();
    test_digits_and_punctuation();
    test_left_shift();
    test_right_shift();
    test_both_shifts();
    test_caps_lock();
    test_caps_with_shift();
    test_backspace_and_enter();
    test_unsupported_codes();
    test_extended_prefixes();

    printf("\n%d checks, %d failures: %s\n",
           checks, failures, failures ? "FAILED" : "ALL TESTS PASSED");
    return failures != 0;
}
