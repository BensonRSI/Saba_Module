/**
 * Copyright (c) 2023 Sven Oliver "SvOlli" Moll
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program implements a Native custom platform
 * for the Sorbus Computer
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

uint16_t trace_mem[0x10000];

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
   memset(trace_mem, 0x00, sizeof(trace_mem));
}

/******************************************************************************
 * public functions
 ******************************************************************************/
void bus_run()
{
   uint16_t trace_counter = 0; // Modulo has fit to size of tracemem !
   bus_init();
   system_init();
   
   // when not run trace loop forever
   for (;;)
   {
      // Fetch everything as fast as possible and put it to the tracemem
      trace_mem[trace_counter++] = (uint16_t)(gpio_get_all() & 0xffff);
   }
} 
