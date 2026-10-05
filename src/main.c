#include "semihost.h"
#include "sysreg.h"

void fw_main(void) __attribute__((noreturn));

/* First C code after reset; boot.S calls it on the primary CPU only.
 * Exit status 0 means we really are at EL3. */
void fw_main(void)
{
    semihost_exit(current_el() == 3 ? 0 : 1);
}
