#include <stdint.h>

#include "console.h"
#include "input.h"
#include "keyboard.h"

static void print_prompt(struct input_line *line) {
    console_write(line->console, "MyOS> ");
}

void input_init(struct input_line *line, struct console *console) {
    line->console = console;
    line->length = 0;
    line->ready = 0;
    line->buffer[0] = '\0';
    print_prompt(line);
}

void input_handle(struct input_line *line, const struct key_event *event) {
    struct console *c = line->console;

    /* Key releases never change the line. */
    if (!event->pressed)
        return;

    if (event->enter) {
        line->buffer[line->length] = '\0';
        line->ready = 1;
        console_newline(c);
        return;
    }

    if (event->backspace) {
        if (line->length > 0) {
            line->length--;
            line->buffer[line->length] = '\0';
            console_backspace(c);
        }
        return;
    }

    uint8_t ascii = event->ascii;
    if (ascii < 32 || ascii >= 127)
        return;

    /* Bounded buffer: extra characters are discarded, never appended. */
    if (line->length >= INPUT_LINE_MAX)
        return;

    line->buffer[line->length++] = (char)ascii;
    console_putc(c, (char)ascii);
}

int input_ready(const struct input_line *line) {
    return line->ready;
}

const char *input_text(const struct input_line *line) {
    return line->buffer;
}

unsigned int input_length(const struct input_line *line) {
    return line->length;
}

void input_accept(struct input_line *line) {
    line->length = 0;
    line->ready = 0;
    line->buffer[0] = '\0';

    console_newline(line->console);
    print_prompt(line);
}
