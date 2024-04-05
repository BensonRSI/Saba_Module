/**
 * Copyright (c) 2024 Benson ( Olli )
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 *  This is part of the Saba_Module replacement for the Saba Videoplay console
 */

#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

// #include "rp2040_purple.h"
#include <pico/stdlib.h>
#include <pico/util/queue.h>
#include <pico/multicore.h>
#include <pico/platform.h>
#include <pico/binary_info.h>

bi_decl(bi_program_name("Sorbus Computer Native Core"))
    bi_decl(bi_program_description("implement an own home computer flavor"))
        bi_decl(bi_program_url("https://xayax.net/sorbus/"))

#include "bus.h"
#include "common.h"

            int main()
{
   // setup UART
   stdio_init_all();
   console_set_crlf(true);

#if 1
   // give some time to connect to console
   sleep_ms(2000);
#endif

   // for toying with overclocking
   //   set_sys_clock_khz( 133000, false );

   // setup the bus and run the bus core
   multicore_launch_core1(bus_run);

   // run interactive console -> should never return
   console_run();

   return 0;
}
