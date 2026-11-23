#ifndef MYOS_KEYBOARD_H
#define MYOS_KEYBOARD_H

#include <stdint.h>

#define KEYBOARD_QUEUE_SIZE 128

struct key_event {
    uint8_t ascii;      /* printable ASCII, 0 for other keys */
    uint8_t pressed;    /* 1 on make, 0 on break */
    uint8_t backspace;  /* Backspace key (both make and break) */
    uint8_t enter;      /* Enter key (both make and break) */
};

/* Hardware setup. IRQ1 stays masked while ACKs are polled, then is unmasked.
 * Returns 0 on success, -1 if the controller or keyboard did not answer. */
int keyboard_init(void);

/* Decode state and input queue. Reset is also used by host-side tests. */
void keyboard_reset_state(void);
int keyboard_decode(uint8_t scancode, struct key_event *event);
int keyboard_pop(struct key_event *event);

/* IRQ1 handler: read one scancode and queue it. No console work in the ISR. */
void keyboard_irq(void);

#endif
