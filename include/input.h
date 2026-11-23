#ifndef MYOS_INPUT_H
#define MYOS_INPUT_H

#include <stdint.h>

#include "console.h"
#include "keyboard.h"

#define INPUT_LINE_MAX 128

struct input_line {
    struct console *console;
    char buffer[INPUT_LINE_MAX + 1];
    unsigned int length;
    int ready;
};

void input_init(struct input_line *line, struct console *console);
void input_handle(struct input_line *line, const struct key_event *event);
int input_ready(const struct input_line *line);
const char *input_text(const struct input_line *line);
unsigned int input_length(const struct input_line *line);

/* Called after the completed line has been displayed by the caller. */
void input_accept(struct input_line *line);

#endif
