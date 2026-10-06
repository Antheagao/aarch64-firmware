/*
 * Host unit tests for kprintf formatting.
 *
 * kprintf is the only way this firmware says anything, and every CHECKS line
 * in tests/run_tests.py is matched against its output, so a formatting bug
 * shows up as a confusing QEMU failure somewhere else. These tests pin the
 * behavior down directly, including the limitations.
 */
#include <stddef.h>
#include <stdint.h>

#include "fake_uart.h"
#include "kprintf.h"
#include "unit.h"

void test_kprintf(void)
{
    fake_uart_reset();
    kprintf("plain text");
    unit_expect_text("literal text passes through", "plain text");

    fake_uart_reset();
    kprintf("a%cc", 'b');
    unit_expect_text("%c prints one character", "abc");

    fake_uart_reset();
    kprintf("[%s]", "str");
    unit_expect_text("%s prints a string", "[str]");

    /* volatile so the compiler cannot see the NULL and fold the call away:
     * the point is what kprintf does with it at run time. */
    const char *volatile null_str = NULL;
    fake_uart_reset();
    kprintf("[%s]", null_str);
    unit_expect_text("%s prints (null) rather than dereferencing NULL", "[(null)]");

    fake_uart_reset();
    kprintf("%d %d", 42, -42);
    unit_expect_text("%d handles both signs", "42 -42");

    fake_uart_reset();
    kprintf("%d", -2147483647 - 1);
    unit_expect_text("%d handles INT_MIN without overflowing the negation", "-2147483648");

    fake_uart_reset();
    kprintf("%u", 4294967295u);
    unit_expect_text("%u prints the full unsigned range", "4294967295");

    fake_uart_reset();
    kprintf("%x", 0xdeadbeefu);
    unit_expect_text("%x prints lowercase hex", "deadbeef");

    fake_uart_reset();
    kprintf("%lx", (unsigned long)0xfedcba9876543210UL);
    unit_expect_text("%lx prints a 64-bit value", "fedcba9876543210");

    fake_uart_reset();
    kprintf("%016lx", (unsigned long)0x1234UL);
    unit_expect_text("%016lx zero-pads to 16 columns", "0000000000001234");

    fake_uart_reset();
    kprintf("[%4u]", 7u);
    unit_expect_text("a width pads with spaces", "[   7]");

    fake_uart_reset();
    kprintf("[%04u]", 7u);
    unit_expect_text("a leading zero in the width pads with zeros", "[0007]");

    fake_uart_reset();
    kprintf("[%2u]", 12345u);
    unit_expect_text("a value wider than the width is not truncated", "[12345]");

    fake_uart_reset();
    kprintf("[%6s]", "ab");
    unit_expect_text("a width right-pads %s, which the feature table relies on", "[    ab]");

    fake_uart_reset();
    kprintf("[%2s]", "abcdef");
    unit_expect_text("%s wider than its width is not truncated", "[abcdef]");

    fake_uart_reset();
    kprintf("%p", (void *)0x1234);
    unit_expect_text("%p prints 0x and 16 hex digits", "0x0000000000001234");

    fake_uart_reset();
    kprintf("100%%");
    unit_expect_text("%% prints one percent sign", "100%");

    /* Known limitations, pinned so a change to them is deliberate rather
     * than a surprise discovered through a failing QEMU check. */
    fake_uart_reset();
    kprintf("%-2u", 7u);
    unit_expect_text("the '-' flag is NOT supported: it prints literally", "%-2u");

    /* kprintf.h carries the printf format attribute, so these malformed
     * formats are deliberately hidden from the compiler's checker behind a
     * variable. Testing what it does with bad input is the whole point. */
    const char *bad_conversion = "%q";
    const char *trailing_percent = "trailing %";

    fake_uart_reset();
    kprintf(bad_conversion, 1);
    unit_expect_text("an unknown conversion prints itself", "%q");

    fake_uart_reset();
    /* The unused argument is only there to satisfy -Wformat-security, which
     * objects to a non-literal format with no arguments at all. */
    kprintf(trailing_percent, 0);
    unit_expect_text("a trailing %% is dropped rather than read past the end", "trailing ");
}
