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
#include <hardware/structs/bus_ctrl.h>

#include "common.h"

#include "bus.h"

// #define TEST_LOOP

#ifdef TEST_LOOP
#define EMULATED_MEMSIZE 0x8000
uint16_t memory[EMULATED_MEMSIZE];
#else
#define EMULATED_MEMSIZE 0x10000
uint8_t memory[EMULATED_MEMSIZE];
#endif
volatile uint8_t reset_triggered = 0;

const uint8_t memory_map[64] = {
    0,    /* 0000 */
    0,    /* 0400 */
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

#ifdef DEBUG_STATES
volatile uint8_t debug_val[18000] = {
    0xff,
    0xff,

};
#endif

/******************************************************************************
 * internal functions
 ******************************************************************************/

static inline void bus_init()
{
   gpio_init_mask(DATABUS_MASK | ROMC_MASK | WRITE_MASK | PHI_MASK | IRQ_IN_MASK | IRQ_OUT_MASK | DB_DIR_MASK | DB_OE_MASK | TEST_PIN0_MASK | TEST_PIN1_MASK | TEST_PIN2_MASK);
   gpio_set_dir_in_masked(DATABUS_MASK | ROMC_MASK | WRITE_MASK | PHI_MASK | IRQ_IN_MASK | IRQ_OUT_MASK);

   gpio_put_masked(DB_DIR_MASK, DB_DIR_IN);    // Make sure DB is not output on main-bus
   gpio_put_masked(DB_OE_MASK, DB_OE_ENABLED); // activate Output

   gpio_set_dir_out_masked(DB_OE_MASK | DB_DIR_MASK | TEST_PIN0_MASK | TEST_PIN1_MASK |  TEST_PIN2_MASK);

   gpio_clr_mask(DB_DIR_MASK); // Make sure DB is not output on main-bus
   gpio_clr_mask(DB_OE_MASK);  // activate Output

   gpio_set_mask(TEST_PIN0_MASK); // activate Output
   gpio_set_mask(TEST_PIN1_MASK); // activate Output
   gpio_set_mask(TEST_PIN2_MASK); // activate Output

   // Set Core 1 priority to high
   bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_PROC1_BITS;
}

void rampattern(void)
{
   for (int i = 0; i < EMULATED_MEMSIZE; i += 2)
   {
      memory[i + 1] = i / 2;
      memory[i] = (i >> 8);
      // memory[i] = i;
      // memory[i + 1] = i + 1;
   }
}

void system_init()
{
   memset(memory, 0x00, sizeof(memory));
   // rampattern();
}

/******************************************************************************
 * public functions
 ******************************************************************************/
void bus_run()
{
   uint16_t trace_counter; // Modulo has fit to size of tracemem !

   bus_init();
   system_init();
   uint16_t clock_in;

#ifdef TEST_LOOP

   do
   {
      do
      {
         clock_in = gpio_get_all() & 0xffff;
      } while ((clock_in & (PHI_MASK)) == 1);
      // wait for phy falling
      /*gpio_set_dir_out_masked(DATABUS_MASK);
      gpio_put_masked(DATABUS_MASK, memory[trace_counter]);
      gpio_set_mask(DB_DIR_MASK);*/
      // Fetch everything as fast as possible and put it to the tracemem
      memory[trace_counter++] = clock_in;
      do
      {
         clock_in = gpio_get_all() & 0xffff;
      } while ((clock_in & (PHI_MASK)) == 0);
      // gpio_clr_mask(DB_DIR_MASK);
      // gpio_set_dir_in_masked(DATABUS_MASK);

   } while (trace_counter & 0x7fff);
   trace_counter = 0;

   while (1)
      ;
#else
   // Jump to ROMC decoder , which never returns
   romc();
   while (1)
      ;

#endif
}
