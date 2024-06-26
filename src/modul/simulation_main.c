#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern void romc(void);

uint8_t memory[65536];

int main()
{
    for(int i=0; i < 65536; ++i)
    {
        //memory[i] = random(); 
    }
    romc();
    return 0;
}

uint32_t SIO_BASE[0x180/4];

uint8_t memory_map[64]
    = {
        1,  /* 0000 */
        1,  /* 0400 */
        1,  /* 0800 */
        1,  /* 0c00 */
        1,  /* 1000 */
        1,  /* 1400 */
        1,  /* 1800 */
        1,  /* 1c00 */
        1,  /* 2000 */
        1,  /* 2400 */
        0x80,  /* 2800 */
        0,  /* 2c00 */
        0,  /* 3000 */
        0,  /* 3400 */
        0,  /* 3800 */
        0,  /* 3c00 */
        0,  /* 4000 */
        0,  /* 4400 */
        0,  /* 4800 */
        0,  /* 4c00 */
        0,  /* 5000 */
        0,  /* 5400 */
        0,  /* 5800 */
        0,  /* 5c00 */
        0,  /* 6000 */
        0,  /* 6400 */
        0,  /* 6800 */
        0,  /* 6c00 */
        0,  /* 7000 */
        0,  /* 7400 */
        0,  /* 7800 */
        0,  /* 7c00 */
        0,  /* 8000 */
        0,  /* 8400 */
        0,  /* 8800 */
        0,  /* 8c00 */
        0,  /* 9000 */
        0,  /* 9400 */
        0,  /* 9800 */
        0,  /* 9c00 */
        0,  /* a000 */
        0,  /* a400 */
        0,  /* a800 */
        0,  /* ac00 */
        0,  /* b000 */
        0,  /* b400 */
        0,  /* b800 */
        0,  /* bc00 */
        0,  /* c000 */
        0,  /* c400 */
        0,  /* c800 */
        0,  /* cc00 */
        0,  /* d000 */
        0,  /* d400 */
        0,  /* d800 */
        0,  /* dc00 */
        0,  /* e000 */
        0,  /* e400 */
        0,  /* e800 */
        0,  /* ec00 */
        0,  /* f000 */
        0,  /* f400 */
        0,  /* f800 */
        0   /* fc00 */
    };

void _exit(int)
{
    for(;;);
}

