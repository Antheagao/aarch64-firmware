/*
 * Place the EL1 image where EL1 can run it.
 *
 * The firmware executes in place from secure flash, but non-secure EL1
 * cannot fetch from there, so the next stage has to be copied into DRAM
 * first. That is the job TF-A's BL2 does for BL33, and it is why the image
 * is carried inside this binary rather than linked at its final address.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kprintf.h"
#include "loader.h"
#include "platform.h"

/* Defined by src/kernel_image.S around the .incbin. Declared as uint64_t
 * because that is what they are: the assembler aligns both labels to 8, and
 * saying so here removes the cast that would otherwise have to launder the
 * alignment past the compiler. */
extern const uint64_t kernel_image_start[];
extern const uint64_t kernel_image_end[];

size_t kernel_image_size(void)
{
    return (size_t)(kernel_image_end - kernel_image_start) * sizeof(uint64_t);
}

bool kernel_load(void)
{
    size_t size = kernel_image_size();
    /* The MMU is off, so DRAM is Device memory: every access has to be
     * naturally aligned, and the assembler padded the image to suit. */
    const uint64_t *src = kernel_image_start;
    volatile uint64_t *dst = (volatile uint64_t *)DRAM_BASE;
    size_t words = size / sizeof(uint64_t);

    kprintf("kernel: image %u bytes at %p\n", (unsigned)size, (const void *)kernel_image_start);

    /* The image is a whole number of 64-bit words by construction now, so
     * only the empty case is worth guarding. */
    if (size == 0) {
        kprintf("kernel: image is empty: FAIL\n");
        return false;
    }

    for (size_t i = 0; i < words; i++)
        dst[i] = src[i];

    /* Read it back rather than trusting the loop. A copy into the wrong
     * memory is the kind of fault that otherwise shows up much later, as a
     * CPU executing rubbish with no way to tell where it came from. */
    for (size_t i = 0; i < words; i++) {
        if (dst[i] != src[i]) {
            kprintf("kernel: copy differs at word %u: FAIL\n", (unsigned)i);
            return false;
        }
    }

    kprintf("kernel: copied to %p: ok\n", (void *)DRAM_BASE);
    return true;
}
