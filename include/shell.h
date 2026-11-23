#ifndef MYOS_SHELL_H
#define MYOS_SHELL_H

#include <stdint.h>

#include "console.h"
#include "input.h"

#define SHELL_COMMAND_MAX 32
#define SHELL_ARG_MAX 128

struct shell {
    struct console *console;
    uint32_t color;         /* current foreground colour */
};

/* Initialise with the console used for all shell output. */
void shell_init(struct shell *sh, struct console *console, uint32_t color);

/* Parse and execute one completed line. All output goes through console.h. */
void shell_execute(struct shell *sh, const char *line);

/* Integration helper: execute the accepted input line, then let the input
 * subsystem print the next prompt. */
void shell_submit(struct shell *sh, struct input_line *line);

#endif
