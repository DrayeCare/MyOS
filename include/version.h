#ifndef MYOS_VERSION_H
#define MYOS_VERSION_H

/* Single source of truth for the version shown in the boot UI and by the
 * `version` shell command. */
#define MYOS_VERSION_MAJOR 0
#define MYOS_VERSION_MINOR 1
#define MYOS_VERSION_STRING "v0.1"

/* Actual compile-time values supplied by the host toolchain. */
#define MYOS_BUILD_COMPILER __VERSION__
#define MYOS_BUILD_DATE __DATE__
#define MYOS_BUILD_TIME __TIME__

#endif
