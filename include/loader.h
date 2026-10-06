/* Loading the EL1 image into DRAM. */
#ifndef LOADER_H
#define LOADER_H

#include <stdbool.h>
#include <stddef.h>

/* Size of the embedded EL1 image, in bytes. */
size_t kernel_image_size(void);

/* Copy the embedded image to DRAM_BASE and verify the copy.
 * Returns false and says why if it did not land correctly. */
bool kernel_load(void);

#endif
