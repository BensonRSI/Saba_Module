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
uint8_t memory[EMULATED_MEMSIZE];
#endif
volatile uint8_t reset_triggered = 0;
volatile uint8_t irq_triggered = 0;
volatile uint8_t actual_bank = 0;

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
    0x80, /* 2c00 */
    1,    /* 3000 */
    1,    /* 3400 */
    1,    /* 3800 */
    1,    /* 3c00 */
    1,    /* 4000 */
    1,    /* 4400 */
    1,    /* 4800 */
    1,    /* 4c00 */
    1,    /* 5000 */
    1,    /* 5400 */
    1,    /* 5800 */
    1,    /* 5c00 */
    1,    /* 6000 */
    1,    /* 6400 */
    1,    /* 6800 */
    1,    /* 6c00 */
    1,    /* 7000 */
    1,    /* 7400 */
    1,    /* 7800 */
    1,    /* 7c00 */
    1,    /* 8000 */
    1,    /* 8400 */
    1,    /* 8800 */
    1,    /* 8c00 */
    1,    /* 9000 */
    1,    /* 9400 */
    1,    /* 9800 */
    1,    /* 9c00 */
    1,    /* a000 */
    1,    /* a400 */
    1,    /* a800 */
    1,    /* ac00 */
    1,    /* b000 */
    1,    /* b400 */
    1,    /* b800 */
    1,    /* bc00 */
    1,    /* c000 */
    1,    /* c400 */
    1,    /* c800 */
    1,    /* cc00 */
    1,    /* d000 */
    1,    /* d400 */
    1,    /* d800 */
    1,    /* dc00 */
    1,    /* e000 */
    1,    /* e400 */
    1,    /* e800 */
    1,    /* ec00 */
    1,    /* f000 */
    1,    /* f400 */
    1,    /* f800 */
    1     /* fc00 */
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
   gpio_set_dir_in_masked(DATABUS_MASK | ROMC_MASK | WRITE_MASK | PHI_MASK | IRQ_IN_MASK);

   gpio_set_mask(IRQ_OUT_MASK); // No IRQ requested
   gpio_clr_mask(DB_DIR_MASK);  // Make sure DB is not output on main-bus
   gpio_clr_mask(DB_OE_MASK);   // activate Output

   gpio_set_dir_out_masked(DB_OE_MASK | DB_DIR_MASK | IRQ_OUT_MASK | TEST_PIN0_MASK | TEST_PIN1_MASK | TEST_PIN2_MASK);
   // gpio_set_dir_out_masked(DB_OE_MASK | DB_DIR_MASK | TEST_PIN0_MASK | TEST_PIN1_MASK | TEST_PIN2_MASK);

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

   multicore_lockout_victim_init();
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
