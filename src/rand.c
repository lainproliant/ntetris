#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include "rand.h"
#include "macros.h"

/* This function does necessitate a syscall, so perhaps we should
only call this to seed a faster PRNG after so many calls */
uint32_t getRand()
{
    uint32_t retVal = 0;
    size_t nRead = fread(&retVal, sizeof(uint32_t), 1, randFile);
    
    if (nRead == 0) {
        if (ferror(randFile)) {
            ERROR("Error reading bytes from random file: %s", strerror(errno));
        }
    }

    return retVal;
}
