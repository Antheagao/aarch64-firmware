/* Host unit test runner. Each suite reports through tests/unit/unit.c. */
#include "unit.h"

void test_kprintf(void);
void test_cpuid(void);

int main(void)
{
    test_kprintf();
    test_cpuid();
    return unit_failures() ? 1 : 0;
}
