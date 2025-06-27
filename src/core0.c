/**
 * Copyright (c) 2024 Benson ( Olli )
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 *  This is part of the Saba_Module replacement for the Saba Videoplay console
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/util/queue.h>
#include <hardware/clocks.h>

#include "common.h"
#include "getaline.h"
#include "mcurses.h"
#include "hexedit.h"

extern uint8_t memory[0x10000];
#ifdef DEBUG_STATES
extern volatile uint8_t debug_val[34 * 2];
int trace_dump = 0;
uint16_t print_buffer[2000];
#endif

bool console_crlf_enabled;
void debug_clocks();

// default functions are RAM access functons by pointers
uint8_t peekModuleMemory(uint16_t address)
{
   uint8_t *p;
   return memory[address];
}
void pokeModuleMemory(uint16_t address, uint8_t value)
{
   uint8_t *p;
   memory[address] = value;
}

uint8_t (*FunctionPointer_readModuleMemory)(uint16_t address) = peekModuleMemory;              // set default function
void (*FunctionPointer_writeModuleMemory)(uint16_t address, uint8_t value) = pokeModuleMemory; // set default function

void console_set_crlf(bool enable)
{
   uart_set_translate_crlf(uart0, enable);
   console_crlf_enabled = enable;
}

void fill_ram(uint8_t val)
{

   memset(memory, val, sizeof(memory));
}

void print_help()
{

   printf("\n\n");
   printf("SABA Videoplay Eprom-Emulator\n");
   printf("------------------------------\n\n");
   printf("2025 by Benson and Peiselulli\n");
   printf("          of TRSI\n\n");
   printf(" h: hexedit for memdump\n");
   printf(" c: print clocksettings\n");
   printf(" t: trace current PC\n");
   printf(" f: fill mem with 0\n");
   printf(" d: print ROM-States\n\n");
}

void console_rp2040()
{
   char *in;
   bool leave = false;

   in = getaline();
   switch (in[0])
   {
   case 'a':
      printf("Doppeldoe\n");
      break;
   case 'h':
      hexedit(0);
      clear();
      break;
   case 'c':
      debug_clocks();
      break;
   case 'f':
      printf("Clear Ram\n");
      fill_ram(0);
      break;
#ifdef DEBUG_STATES
   case 't':
      trace_dump++;
      printf("Tracedump State: %d\n", trace_dump);
      {
         uint16_t print_val_old = 0;
         int i;
         for (i = 0; i < 2000;)
         {
            uint16_t print_val = debug_val[0] + (debug_val[1] * 0x100);
            // printf("PC: 0x%04x   0x%04x\n", print_val, print_val_old);
            if (print_val != print_val_old)
            {
               print_buffer[i] = print_val;
               print_val_old = print_val;
               i++;
            }
         }
         for (i = 0; i < 2000; i++)
         {
            printf("PC: 0x%04x\n", print_buffer[i]);
         }
      }
      break;

   case 'd':
      for (int i = 0; i < 32; i++)
      {
         printf("ROMC state %d 0x%02x val 0x%02x\n", i, debug_val[i], debug_val[i + 32]);
      }
      break;
#endif

   default:
      print_help();
      break;
   }
}
void welcome()
{

   printf("\n\nWelcome to the SABA Videoplay Module Emulator\n");
   printf("Made by the mighty wizard of TRSI in 2024\n");
   printf("Coding : PeiselUlli , Benson, SvOlli\n");
   printf("HW-Design: Benson\n\n");
}
void console_run()
{
   multicore_lockout_victim_init();
   // init mcurses
   setFunction_putchar((void (*)(uint8_t))putchar); // putchar_raw
   setFunction_getchar((char (*)(void))getchar);    // putchar_raw
   setFunction_readMemory(FunctionPointer_readModuleMemory);
   setFunction_writeMemory(FunctionPointer_writeModuleMemory);
   initscr();

   welcome();
   debug_clocks();
   getaline_init();
   for (;;)
   {
      console_rp2040();
#ifdef DEBUG_STATES
      if (trace_dump & 0x01)
      {
         printf("PC: 0x%04x\n", debug_val[0] + (debug_val[1] * 0x100));
      }
#endif
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