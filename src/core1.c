/**
 * Copyright (c) 2024 Benson ( Olli )
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 *  This is part of the Saba_Module replacement for the Saba Videoplay console
 */

#include <ctype.h>
#include <malloc.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pico/binary_info.h>
#include <pico/multicore.h>
#include <pico/platform.h>
#include <pico/stdlib.h>
#include <pico/util/queue.h>
#include <hardware/gpio.h>

#include "common.h"

#include "bus.h"

uint16_t memory[0x10000];

uint8_t memory_map[64] = {
    1,    /* 0000 */
    1,    /* 0400 */
    1,    /* 0800 */
    1,    /* 0c00 */
    1,    /* 1000 */
    1,    /* 1400 */
    1,    /* 1800 */
    1,    /* 1c00 */
    1,    /* 2000 */
    1,    /* 2400 */
    0x80, /* 2800 */
    0,    /* 2c00 */
    0,    /* 3000 */
    0,    /* 3400 */
    0,    /* 3800 */
    0,    /* 3c00 */
    0,    /* 4000 */
    0,    /* 4400 */
    0,    /* 4800 */
    0,    /* 4c00 */
    0,    /* 5000 */
    0,    /* 5400 */
    0,    /* 5800 */
    0,    /* 5c00 */
    0,    /* 6000 */
    0,    /* 6400 */
    0,    /* 6800 */
    0,    /* 6c00 */
    0,    /* 7000 */
    0,    /* 7400 */
    0,    /* 7800 */
    0,    /* 7c00 */
    0,    /* 8000 */
    0,    /* 8400 */
    0,    /* 8800 */
    0,    /* 8c00 */
    0,    /* 9000 */
    0,    /* 9400 */
    0,    /* 9800 */
    0,    /* 9c00 */
    0,    /* a000 */
    0,    /* a400 */
    0,    /* a800 */
    0,    /* ac00 */
    0,    /* b000 */
    0,    /* b400 */
    0,    /* b800 */
    0,    /* bc00 */
    0,    /* c000 */
    0,    /* c400 */
    0,    /* c800 */
    0,    /* cc00 */
    0,    /* d000 */
    0,    /* d400 */
    0,    /* d800 */
    0,    /* dc00 */
    0,    /* e000 */
    0,    /* e400 */
    0,    /* e800 */
    0,    /* ec00 */
    0,    /* f000 */
    0,    /* f400 */
    0,    /* f800 */
    0     /* fc00 */
};

extern void romc(void);

/******************************************************************************
 * internal functions
 ******************************************************************************/

static inline void bus_init()
{
   gpio_set_dir_in_masked(DATABUS_MASK | ROMC_MASK | WRITE_MASK | PHI_MASK | IRQ_IN_MASK | IRQ_OUT_MASK);

   gpio_put_masked(DB_DIR_MASK, DB_DIR_IN);    // Make sure DB is not output on main-bus
   gpio_put_masked(DB_OE_MASK, DB_OE_ENABLED); // activate Output

   gpio_set_dir_out_masked(DB_OE_MASK | DB_DIR_MASK);
}

void system_init()
{
   memset(memory, 0x00, sizeof(memory));
}

/******************************************************************************
 * public functions
 ******************************************************************************/
void bus_run()
{
   uint16_t trace_counter = 0; // Modulo has fit to size of tracemem !
   bus_init();
   system_init();
   uint16_t clock_in;

#ifdef TEST_LOOP

   do
   {
      clock_in = gpio_get_all() & 0xffff;
   } while (clock_in & (1 << 13) == 0);
   // wait for the first write pulse
   // when not run trace loop forever
   do
   {
      // Fetch everything as fast as possible and put it to the tracemem
      trace_mem[trace_counter++] = (uint16_t)(gpio_get_all() & 0xffff);
   } while (trace_counter);
   while (1)
      ;
#else
   // Jump to ROMC decoder , which never returns
   romc();

#endif
}
