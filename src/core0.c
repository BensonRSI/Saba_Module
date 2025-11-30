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
#include <stdint.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/util/queue.h>
#include <hardware/clocks.h>

#include "common.h"
#include "getaline.h"
#include "mcurses.h"
#include "hexedit.h"
#include "bus.h"
#include "demo.h" // default demo data
#include "xmodem.h"

extern uint8_t memory[0x10000];
// the first 8 bytes of the memory are used as IO-Ports
// The adresses asre used for timer and IRQ-control
// bit0-3 : IRQ control: 00=disable, 01=enable timer IRQ, others reserved
// bit4: timer start/stop
#define ICR_OFFSET 0
#define IRQ_CTRL_DISABLE 0
#define IRQ_CTRL_MASK 0xfc // for further use ,bit 1
#define IRQ_CTRL_ENABLE_TIMER 1
#define TIMER_STOP_MASK 0xef
#define TIMER_START 0x10

// this defines a 24-bit timer val, running at 1 Mhz
// gives max delay of 16.777216 seconds
// a value of 0 means timer stopped

#define TIMER_VAL_LOW 0x1
#define TIMER_VAL_MID 0x2
#define TIMER_VAL_HI 0x3

extern volatile uint8_t reset_triggered;
uint8_t upload_memory[0x2000]; /* Maximum size we can use = 8k*/
#ifdef DEBUG_STATES
extern volatile uint8_t debug_val[18000];
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

/* For xmodem receive, we need to provide the HW specific functions*/
int _inbyte(int msec)
{

   int c = getchar_timeout_us(msec * 1000);
   if (c == PICO_ERROR_TIMEOUT)
   {
      return -1;
   }
   return c;
}
void _outbyte(unsigned char c)
{

   putchar(c);
}

void console_set_crlf(bool enable)
{
   uart_set_translate_crlf(uart0, enable);
   console_crlf_enabled = enable;
}

void fill_ram(uint8_t val)
{

   memset(memory, val, sizeof(memory));
}

// Use alarm 0
#define ALARM_NUM 0
#define ALARM_IRQ TIMER_IRQ_0

static void alarm_irq(void);
int timer_running = 0;
int timer_val = 0;

static void alarm_in_us(uint32_t delay_us)
{
   // Enable the interrupt for our alarm (the timer outputs 4 alarm irqs)
   hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
   // Set irq handler for alarm irq
   irq_set_exclusive_handler(ALARM_IRQ, alarm_irq);
   // Enable the alarm irq
   irq_set_enabled(ALARM_IRQ, true);
   // Enable interrupt in block and at processor

   // Alarm is only 32 bits so if trying to delay more
   // than that need to be careful and keep track of the upper
   // bits
   uint64_t target = timer_hw->timerawl + delay_us;

   // Write the lower 32 bits of the target time to the alarm which
   // will arm it
   timer_hw->alarm[ALARM_NUM] = (uint32_t)target;
   timer_running = 1;
}

static void alarm_irq(void)
{
   // Clear the alarm irq
   hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);

   // Trigger IRQ , if enabled
   if ((memory[ICR_OFFSET] & IRQ_CTRL_MASK) == IRQ_CTRL_ENABLE_TIMER)
   {
      // set GPIO pin low to signal IRQ
      gpio_put_masked(IRQ_OUT_MASK, IRQ_REQUESTED); // IRQ requested
   }
   // Retrigger
   alarm_in_us(timer_val);
}

static void alarm_cancel(void)
{
   // Disable the interrupt for our alarm (the timer outputs 4 alarm irqs)
   hw_clear_bits(&timer_hw->inte, 1u << ALARM_NUM);
   // Disable the alarm irq
   irq_set_enabled(ALARM_IRQ, false);
   timer_running = 0;
}

void timer_control()
{
   if (((memory[ICR_OFFSET] & TIMER_START)) && (!timer_running))
   {
      // timer not running, so start it
      timer_val = (memory[TIMER_VAL_LOW] + (memory[TIMER_VAL_MID] << 8) + (memory[TIMER_VAL_HI] << 16));
      if (timer_val > 0)
      {
         alarm_in_us(timer_val);
      }
      else
      {
         // zero value means stop timer
         alarm_cancel();
      }
   }
   if ((!(memory[ICR_OFFSET] & TIMER_START)) && (timer_running))
   {
      // stop timer
      alarm_cancel();
   }
}

void simulate_timer(int start)
{
   if (start)
   {
      // set timer value to 1 sec
      memory[TIMER_VAL_LOW] = 0x40;
      memory[TIMER_VAL_MID] = 0x42;
      memory[TIMER_VAL_HI] = 0x0f;
      memory[ICR_OFFSET] |= TIMER_START;
   }
   else
   {
      memory[ICR_OFFSET] &= TIMER_STOP_MASK;
   }
}
#ifdef DEBUG_STATES
void get_tracedump(int offset, int length)
{

   uint16_t print_val_old = 0;
   int i;
   for (i = 0; i < length;)
   {
      uint16_t print_val = debug_val[0 + offset] + (debug_val[1 + offset] * 0x100);
      // printf("PC: 0x%04x   0x%04x\n", print_val, print_val_old);
      if (print_val != print_val_old)
      {
         print_buffer[i] = print_val;
         print_val_old = print_val;
         i++;
      }
   }
}

void get_tracedump_x(int offset, int length)
{

   uint16_t print_val_old = 0;
   int i;
   for (i = 0; i < length;)
   {
      uint16_t print_val = debug_val[0 + offset] + (debug_val[1 + offset] * 0x100);
      // printf("PC: 0x%04x   0x%04x\n", print_val, print_val_old);
      if ((print_val & 0xff00) != (print_val_old & 0xff00))
      {
         print_buffer[i] = print_val;
         print_val_old = print_val;
         i++;
      }
   }
}
#endif
void print_help()
{

   printf("\n\n");
   printf("SABA Videoplay Eprom-Emulator\n");
   printf("------------------------------\n\n");
   printf("2025 by Benson and Peiselulli\n");
   printf("          of TRSI\n\n");
   printf(" h: hexedit for memdump\n");
   printf(" i: hexedit for IOdump\n");
   printf(" c: print clocksettings\n");
   printf(" u: upload rom-image via xmodem (max 8k)\n");
   printf(" f: fill mem with 0\n");
   printf(" r: run/stop 1-sec timer \n");
#ifdef DEBUG_STATES
   printf(" p: trace current PC0\n");
   printf(" j: trace current PC0, on highbyte change\n");
   printf(" t: trace current PC1\n");
   printf(" d: trace current DC0\n");

   printf(" s: print ROM-States\n");
   printf(" y: clear debug-pins\n\n");
#endif
}

int doe_val = 1;
int timer_simulate = 0;
void console_rp2040()
{
   char *in;
   bool leave = false;

   int c = getchar_timeout_us(0);
   if (c == PICO_ERROR_TIMEOUT)
   {
      // no USB/stdin key available — skip blocking read
      return;
   }
   printf("Got key: %d\n", c);
   switch (c)
   {
   case 'a':
      printf("Doppeldoe\n");
      break;
   case 'h':
      hexedit(0x800);
      clear();
      break;
   case 'i':
      hexedit(0x0);
      clear();
      break;
   case 'c':
      debug_clocks();
      break;
   case 'f':
      printf("Clear Ram\n");
      fill_ram(0);
      break;
   case 'r':
      timer_simulate = 1 - timer_simulate;
      printf("Timer %s\n", timer_simulate ? "started" : "stopped");
      simulate_timer(timer_simulate);
      break;
   case 'u':
      printf("Upload ROM start xmodem transfer now \n");
      int ret = xmodemReceive(upload_memory, sizeof(upload_memory));
      printf("Upload complete, %d bytes received\n", ret);
      break;
#ifdef DEBUG_STATES
   case 'x':
   {
      uint16_t print_val = debug_val[0] + (debug_val[1] * 0x100);
      printf("PC0: 0x%04x\n", print_val);
   }
   break;
   case 'p':
      // get_tracedump(0,2000);
      {
         uint32_t *print_p = (uint32_t *)&debug_val[4];
         for (int i = 0; i < 200; i++)
         {
            if ((i % 8) == 0)
            {
               printf("\nPC0: ");
            }
            printf("0x%04x ", *print_p++);
            printf("0x%02x, ", *print_p++);
         }
      }
      break;
   case 'j':
      get_tracedump_x(0, 2);
      for (int i = 0; i < 2; i++)
      {
         printf("PC0: 0x%04x\n", print_buffer[i]);
      }

      break;
   case 't':
      get_tracedump(4, 5);
      for (int i = 0; i < 5; i++)
      {
         printf("PC1: 0x%04x\n", print_buffer[i]);
      }

      break;
   case 'd':
      get_tracedump(8, 50);
      for (int i = 0; i < 50; i++)
      {
         printf("DC0: 0x%04x\n", print_buffer[i]);
      }

      break;

   case 's':
      for (int i = 0; i < 32; i++)
      {
         printf("ROMC state %d 0x%02x val 0x%02x\n", i, debug_val[i], debug_val[i + 32]);
      }
      break;

   case 'y':
      gpio_put(TEST_PIN0_SHIFT, doe_val);
      gpio_put(TEST_PIN1_SHIFT, 0);
      gpio_put(TEST_PIN2_SHIFT, 0);
      doe_val = 1 - doe_val;

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
#ifdef DEBUG_STATES
   memset((void *)debug_val, 0, 18000);
#endif
   welcome();
   debug_clocks();
   getaline_init();
   memset(upload_memory, 0x00, sizeof(upload_memory));
   memcpy(upload_memory, demo_data, sizeof(demo_data));
   for (;;)
   {
      console_rp2040();
      if (reset_triggered)
      {
         memcpy(&memory[0x800], upload_memory, sizeof(upload_memory));
         reset_triggered = 0;
         printf("System Reset performed\n");
      }
#ifdef DEBUG_STATES
      if (trace_dump & 0x01)
      {
         printf("PC: 0x%04x\n", debug_val[0] + (debug_val[1] * 0x100));
      }
#endif
      timer_control();
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