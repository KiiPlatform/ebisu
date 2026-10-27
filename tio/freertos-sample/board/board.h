#ifndef BOARD_H
#define BOARD_H

#include <stddef.h>

/* What the sample needs of the board: the MPS2 AN385 as QEMU emulates it. A
   port to another board replaces this directory: the start-up code and the
   linker script, these functions, and FreeRTOS-Plus-TCP's driver for the
   board's network controller. */

/* Before anything else, before the scheduler starts. */
void board_init(void);

/* Writes to the console, a UART. */
void board_write(const char* data, size_t length);

/* Fills buffer from a source of true randomness, for mbedTLS to seed its
   random generator from. 0 on success.

   QEMU's board has none: this reads the host's /dev/urandom through
   semihosting. A device has to use its hardware random number generator, or
   another source of true randomness: every key mbedTLS generates, and so the
   secrecy of every connection, rests on it. */
int board_entropy(unsigned char* buffer, size_t length);

/* Ends the program: QEMU exits, with 0 for status 0 and 1 for any other. */
void board_exit(int status) __attribute__((noreturn));

/* Reports a fatal error on the console, as "FATAL: " and the message, and ends
   the program with status 1. Usable from an interrupt handler. */
void board_fatal(const char* format, ...)
    __attribute__((noreturn, format(printf, 1, 2)));

#endif /* BOARD_H */
