/*
 * Host unit tests for kprintf formatting.
 *
 * kprintf is the only way this firmware says anything, and every CHECKS line
 * in tests/run_tests.py is matched against its output, so a formatting bug
 * shows up as a confusing QEMU failure somewhere else. These tests pin the
 * behavior down directly, including the limitations.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fake_uart.h"
#include "kprintf.h"

static unsigned checks;
static unsigned failures;

static void expect(const char *what, const char *want)
{
    const char *got = fake_uart_text();

    checks++;
    if (strcmp(got, want) == 0) {
        printf("PASS  %s\n", what);
    } else {
        failures++;
        printf("FAIL  %s\n        got  \"%s\"\n        want \"%s\"\n", what, got, want);
    }
}

int main(void)
{
    fake_uart_reset();
    kprintf("plain text");
    expect("literal text passes through", "plain text");

    fake_uart_reset();
    kprintf("a%cc", 'b');
    expect("%c prints one character", "abc");

    fake_uart_reset();
    kprintf("[%s]", "str");
    expect("%s prints a string", "[str]");

    /* volatile so the compiler cannot see the NULL and fold the call away:
     * the point is what kprintf does with it at run time. */
    const char *volatile null_str = NULL;
    fake_uart_reset();
    kprintf("[%s]", null_str);
    expect("%s prints (null) rather than dereferencing NULL", "[(null)]");

    fake_uart_reset();
    kprintf("%d %d", 42, -42);
    expect("%d handles both signs", "42 -42");

    fake_uart_reset();
    kprintf("%d", -2147483647 - 1);
    expect("%d handles INT_MIN without overflowing the negation", "-2147483648");

    fake_uart_reset();
    kprintf("%u", 4294967295u);
    expect("%u prints the full unsigned range", "4294967295");

    fake_uart_reset();
    kprintf("%x", 0xdeadbeefu);
    expect("%x prints lowercase hex", "deadbeef");

    fake_uart_reset();
    kprintf("%lx", (unsigned long)0xfedcba9876543210UL);
    expect("%lx prints a 64-bit value", "fedcba9876543210");

    fake_uart_reset();
    kprintf("%016lx", (unsigned long)0x1234UL);
    expect("%016lx zero-pads to 16 columns", "0000000000001234");

    fake_uart_reset();
    kprintf("[%4u]", 7u);
    expect("a width pads with spaces", "[   7]");

    fake_uart_reset();
    kprintf("[%04u]", 7u);
    expect("a leading zero in the width pads with zeros", "[0007]");

    fake_uart_reset();
    kprintf("[%2u]", 12345u);
    expect("a value wider than the width is not truncated", "[12345]");

    fake_uart_reset();
    kprintf("%p", (void *)0x1234);
    expect("%p prints 0x and 16 hex digits", "0x0000000000001234");

    fake_uart_reset();
    kprintf("100%%");
    expect("%% prints one percent sign", "100%");

    /* Known limitations, pinned so a change to them is deliberate rather
     * than a surprise discovered through a failing QEMU check. */
    fake_uart_reset();
    kprintf("%-2u", 7u);
    expect("the '-' flag is NOT supported: it prints literally", "%-2u");

    /* kprintf.h carries the printf format attribute, so these malformed
     * formats are deliberately hidden from the compiler's checker behind a
     * variable. Testing what it does with bad input is the whole point. */
    const char *bad_conversion = "%q";
    const char *trailing_percent = "trailing %";

    fake_uart_reset();
    kprintf(bad_conversion, 1);
    expect("an unknown conversion prints itself", "%q");

    fake_uart_reset();
    /* The unused argument is only there to satisfy -Wformat-security, which
     * objects to a non-literal format with no arguments at all. */
    kprintf(trailing_percent, 0);
    expect("a trailing %% is dropped rather than read past the end", "trailing ");

    printf("\n%u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
