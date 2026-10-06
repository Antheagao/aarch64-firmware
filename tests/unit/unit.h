/* Minimal assertion helpers shared by the host unit tests. */
#ifndef UNIT_H
#define UNIT_H

#include <stdbool.h>
#include <stdint.h>

/* Compare everything written to the fake UART since the last reset. */
void unit_expect_text(const char *what, const char *want);

/* Assert the fake UART output contains a substring: for multi-line output
 * where pinning every byte would make the test brittle. */
void unit_expect_contains(const char *what, const char *needle);

void unit_expect_u32(const char *what, uint32_t got, uint32_t want);
void unit_expect_true(const char *what, bool got);

unsigned unit_failures(void);

#endif
