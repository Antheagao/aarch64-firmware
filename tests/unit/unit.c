#include <stdio.h>
#include <string.h>

#include "fake_uart.h"
#include "unit.h"

static unsigned checks;
static unsigned failures;

static void pass(const char *what)
{
    checks++;
    printf("PASS  %s\n", what);
}

static void fail(const char *what)
{
    checks++;
    failures++;
    printf("FAIL  %s\n", what);
}

void unit_expect_text(const char *what, const char *want)
{
    const char *got = fake_uart_text();

    if (strcmp(got, want) == 0) {
        pass(what);
    } else {
        fail(what);
        printf("        got  \"%s\"\n        want \"%s\"\n", got, want);
    }
}

void unit_expect_contains(const char *what, const char *needle)
{
    const char *got = fake_uart_text();

    if (strstr(got, needle) != NULL) {
        pass(what);
    } else {
        fail(what);
        printf("        output does not contain \"%s\"\n", needle);
        printf("        output was:\n%s\n", got);
    }
}

void unit_expect_u32(const char *what, uint32_t got, uint32_t want)
{
    if (got == want) {
        pass(what);
    } else {
        fail(what);
        printf("        got %u, want %u\n", got, want);
    }
}

void unit_expect_true(const char *what, bool got)
{
    if (got)
        pass(what);
    else
        fail(what);
}

unsigned unit_failures(void)
{
    printf("\n%u checks, %u failures\n", checks, failures);
    return failures;
}
