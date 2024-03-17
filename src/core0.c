/**
 * Copyright (c) 2023 Sven Oliver "SvOlli" Moll
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program implements a Native custom platform
 * for the Sorbus Computer
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/util/queue.h>
#include <hardware/clocks.h>

#include "common.h"

#include "bus.h"

extern void system_trap();

bool console_crlf_enabled;

void console_set_crlf(bool enable)
{
   uart_set_translate_crlf(uart0, enable);
   console_crlf_enabled = enable;
}

void console_rp2040()
{
   int in;
   bool leave = false;

   in = toupper(getchar());
   switch (in)
   {
   default:
      break;
   }
}

void console_run()
{
   multicore_lockout_victim_init();

   for (;;)
   {
      console_rp2040();
      tight_loop_contents();
   }
}

void debug_clocks()
{
   uint f_pll_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
   uint f_pll_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY);
   uint f_rosc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
   uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
   uint f_clk_peri = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
   uint f_clk_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB);
   uint f_clk_adc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);
   uint f_clk_rtc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_RTC);

   printf("\n");
   printf("PLL_SYS:             %3d.%03dMHz\n", f_pll_sys / 1000, f_pll_sys % 1000);
   printf("PLL_USB:             %3d.%03dMHz\n", f_pll_usb / 1000, f_pll_usb % 1000);
   printf("ROSC:                %3d.%03dMHz\n", f_rosc / 1000, f_rosc % 1000);
   printf("CLK_SYS:             %3d.%03dMHz\n", f_clk_sys / 1000, f_clk_sys % 1000);
   printf("CLK_PERI:            %3d.%03dMHz\n", f_clk_peri / 1000, f_clk_peri % 1000);
   printf("CLK_USB:             %3d.%03dMHz\n", f_clk_usb / 1000, f_clk_usb % 1000);
   printf("CLK_ADC:             %3d.%03dMHz\n", f_clk_adc / 1000, f_clk_adc % 1000);
   printf("CLK_RTC:             %3d.%03dMHz\n", f_clk_rtc / 1000, f_clk_rtc % 1000);
}