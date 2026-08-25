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
#include <stdlib.h>

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
#include "flash_store.h"

// #define TEST_PATTERN // Fill memory with test pattern instead of 0x00

extern uint8_t memory[EMULATED_MEMSIZE]; // 64 k for Rom + 64 k banked Rom
// the first 32 bytes of the memory are used as IO-Ports
// The addresses are used for timer and IRQ-control and bank switching
// bit0-3 : IRQ control: 00=disable, 01=enable timer IRQ, others reserved
// bit4: timer start/stop
#define ICR_OFFSET_ADR 0xa
#define IRQ_CTRL_DISABLE 0
#define IRQ_CTRL_MASK 0x03 // for further use ,bit 1+2
#define IRQ_CTRL_ENABLE_TIMER_IRQ 3
#define IRQ_CTRL_ENABLE_EXT_IRQ 2

// this defines a 8-bit timer val, running at 38400 Hz
// with a prescaler od 31 , so 1 timer tick is 26 us *31
// a value of 0xff means timer stopped
// with a maximum Timer Tick of 0xfe, this means a maximum timer value of 0xfe * 806 = 204.724 us = 204 ms

#define IRQ_VECTOR_LOW_ADR 0x8
#define IRQ_VECTOR_HI_ADR 0x9
#define TIMER_VAL_ADR 0xb
#define ROM_BANK_ADR 0xc
#define TIMER_STOP 0xff
#define TIMER_CLOCK_MULTIPLIER 806 // 38400 Hz timer clock * 31  , so 1 timer tick = 806 us

extern volatile uint8_t reset_triggered;
extern volatile uint8_t irq_triggered;

uint8_t current_bank;
uint8_t upload_memory[0x10000 - 0x800 + BANK_SIZE * EXTENSION_BANKS]; /* Maximum size we can use = 64 k +62k*/
uint8_t *shadow_rom = &upload_memory[BANK_START_ADDRESS];             /* Shadow copy of the ROM when bank switching*/
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
#ifndef TEST_PATTERN
   memset(memory, val, EMULATED_MEMSIZE);
#else
   for (uint32_t i = 0; i < EMULATED_MEMSIZE; i += 2)
   {
      memory[i + 1] = i / 2;
      memory[i] = (i >> 8);
   }
   printf("Memory filled with test pattern\n");
#endif
}

// Use alarm 0
#define ALARM_NUM 0
#define ALARM_IRQ TIMER_IRQ_0

static void alarm_irq(void);
int timer_running = 0;
int timer_val = 0;
int timer_val_actual = 0;
int timer_ticks = 0; // holds the value in uS for the next Timer IRQ

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
   if (memory[ICR_OFFSET_ADR] & IRQ_CTRL_ENABLE_TIMER_IRQ)
   {
      // this is done by Core1 , so we can sync to the write-pulse
      // set GPIO pin low to signal IRQ
      // gpio_clr_mask(IRQ_OUT_MASK); // IRQ requested
      irq_triggered = 1;
      // printf("Timer IRQ triggered\n");
   }
   // Retrigger
   // This is not the actual behaviour of a F3853, which will retrigger at maxmium value,
   // but that doesn't make any sense.
   alarm_in_us(timer_ticks);
}

static void alarm_cancel(void)
{
   // Disable the interrupt for our alarm (the timer outputs 4 alarm irqs)
   hw_clear_bits(&timer_hw->inte, 1u << ALARM_NUM);
   // Disable the alarm irq
   irq_set_enabled(ALARM_IRQ, false);
   gpio_set_mask(IRQ_OUT_MASK); // IRQ requested

   timer_running = 0;
}

// Timer conversion table for the 8-bit timer value to the actual counter value in us
// The F3853 implements the timer not as counter, but as shift register . ( What the heck ??? )
// clang-format off
const uint8_t timerval_to_counter[256] = {
    0x18, 0x17, 0x51, 0x56, 0xaf, 0x50, 0xae, 0x15, 0x00, 0x00, 0xab, 0x4f, 0x8f, 0x28, 0x0a, 0x14,  //0x00-0x0F
    0x55, 0x62, 0xad, 0x5e, 0xaa, 0x33, 0x4e, 0xa4, 0x8e, 0x84, 0x27, 0xd2, 0x09, 0xf3, 0x13, 0xe8,  //0x10-0x1F
    0x54, 0x00, 0x61, 0xd5, 0x00, 0xc3, 0x5d, 0x97, 0xa9, 0xc0, 0x32, 0x43, 0x4d, 0x78, 0xa3, 0x03,  //0x20-0x2F
    0x5a, 0x8d, 0x47, 0x83, 0xbb, 0x26, 0xdf, 0xd1, 0x94, 0x08, 0x7f, 0xf2, 0xb5, 0x3a, 0x00, 0xe7,  //0x30-0x3F
    0x53, 0x2b, 0x00, 0x0c, 0x60, 0xa6, 0xd4, 0xea, 0x00, 0xc5, 0xc2, 0x7a, 0x5c, 0xbd, 0x96, 0xb7,  //0x40-0x4F
    0x2d, 0xa8, 0xc7, 0xbf, 0x2f, 0x4a, 0x69, 0x42, 0x67, 0x4c, 0x6e, 0x77, 0x40, 0xa2, 0x22, 0x02,  //0x50-0x5F
    0x1b, 0x59, 0x65, 0x8c, 0xd8, 0x46, 0x00, 0x82, 0xed, 0xba, 0x6c, 0x25, 0xdb, 0xde, 0x75, 0x00,  //0x60-0x6F
    0x93, 0xf7, 0x07, 0x3e, 0x7e, 0x72, 0xf1, 0xa0, 0xb4, 0x9c, 0x11, 0x20, 0x39, 0xcd, 0xe6, 0x00,  //0x70-0x7F
    0x19, 0x52, 0xb0, 0x2a, 0xac, 0x6d, 0x90, 0x00, 0x63, 0x5f, 0x34, 0xa5, 0x8a, 0xd3, 0xf4, 0xe9,  //0x80-0x8F
    0x00, 0xd6, 0xc4, 0x98, 0xc1, 0x44, 0x79, 0x0b, 0x5b, 0x48, 0xbc, 0xe0, 0x95, 0x80, 0xb6, 0x3b,  //0x90-0x9F
    0x2c, 0x0d, 0xa7, 0xeb, 0x00, 0x7b, 0xc6, 0xb8, 0x2e, 0xc8, 0x30, 0x6a, 0x68, 0x6f, 0x41, 0x23,  //0xA0-0xAF
    0x1c, 0x66, 0xd9, 0x4b, 0xee, 0x00, 0xdc, 0x76, 0xf8, 0x3f, 0x73, 0xa1, 0x9d, 0x21, 0xce, 0x01,  //0xB0-0xBF
    0x1a, 0xb1, 0x58, 0x91, 0x64, 0x35, 0x8b, 0xf5, 0xd7, 0x99, 0x45, 0x05, 0x49, 0xe1, 0x81, 0x3c,  //0xC0-0xCF
    0x0e, 0xec, 0x7c, 0xb9, 0xc9, 0x6b, 0x70, 0x00, 0x1d, 0xda, 0xef, 0xdd, 0xf9, 0x74, 0x9e, 0xcf,  //0xD0-0xDF
    0xb2, 0x92, 0x36, 0xf6, 0x9a, 0x06, 0xe2, 0x3d, 0x0f, 0x7d, 0xca, 0x71, 0x1e, 0xf0, 0xfa, 0x9f,  //0xE0-0xEF
    0xb3, 0x37, 0x9b, 0xe3, 0x10, 0xcb, 0x1f, 0xfb, 0x38, 0xe4, 0xcc, 0xfc, 0xe5, 0xfd, 0xfe, 0x00,  //0xF0-0xFF
};
// clang-format on

void timer_control()
{
   timer_val = (memory[TIMER_VAL_ADR]);
   int irq_val = (memory[ICR_OFFSET_ADR] & IRQ_CTRL_MASK);

   if ((timer_val_actual != timer_val) && (irq_val == IRQ_CTRL_ENABLE_TIMER_IRQ))
   {
      timer_val_actual = timer_val;
      if (timer_val == 0xff)
      {
         // stop timer
         // printf("Stopping timer\n");
         alarm_cancel();
      }
      else
      {
         // start timer
         timer_ticks = timerval_to_counter[timer_val] * TIMER_CLOCK_MULTIPLIER; // convert to us

         // printf("Starting timer with val 0x%02x  for %d us\n", timer_val, timer_ticks);
         alarm_in_us(timer_ticks);
      }
   }
}

void simulate_timer(int start)
{
   if (start)
   {
      // set timer value
      memory[TIMER_VAL_ADR] = 0x40;
      memory[ICR_OFFSET_ADR] |= IRQ_CTRL_ENABLE_TIMER_IRQ;
   }
   else
   {
      memory[ICR_OFFSET_ADR] &= ~IRQ_CTRL_ENABLE_TIMER_IRQ;
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
   printf(" p: hexedit for IO-Ports\n");
   printf(" h: hexedit for ROMdump\n");
   printf(" k: hexedit for RAMdump\n");
   printf(" c: print clocksettings\n");
   printf(" u: upload rom-image via xmodem to ram(max 8k)\n");
   printf(" w: write rom-image via xmodem to flash(max 8k)\n");
   printf(" i: inventory of stored ROMs\n");
   printf(" f: fill mem with 0\n");
   printf(" r: run/stop 1-sec timer \n");
   printf(" b: switch bank \n");

#ifdef DEBUG_STATES
   printf(" n: trace current PC0\n");
   printf(" j: trace current PC0, on highbyte change\n");
   printf(" t: trace current PC1\n");
   printf(" d: trace current DC0\n");

   printf(" s: print ROM-States\n");
   printf(" y: clear debug-pins\n\n");
#endif
}

void print_cart_dir()
{

   printf("----------------------------------------------\n");
   printf("SlotNo\t| Size \t\t| Name\n");
   for (int i = 0; i < MAX_CART_NO; i++)
   {
      if (is_cart_data_valid(i))
      {
         printf("    %d\t| ", i);
         dir_entry *cart_dir_entry = get_cart_dir_entry(i);
         printf("%06d\t| %s", cart_dir_entry->size, cart_dir_entry->cart_name);
         printf("\n");
      }
   }
   printf("---------------------------------------------\n\n");
}

int doe_val = 1;
int timer_simulate = 0;
void console_rp2040()
{
   char *in;
   bool leave = false;
   int ret;
   int cart_size;

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
   case 'p':
      hexedit(0x0000);
      clear();
      break;
   case 'h':
      hexedit(0x0800);
      clear();
      break;
   case 'k':
      hexedit(0x2800);
      clear();
      break;
   case 'c':
      debug_clocks();
      break;
   case 'b':
      printf("Enter extension-Bank (00-3f) or (80-b0) for original:  \n");
      fflush(stdout);
      uint8_t actual_bank = strtol(getaline(), NULL, 16);
      memory[ROM_BANK_ADR] = actual_bank;
      if (actual_bank & 0x80)
      {
         printf("Switched to original bank 0x%02x\n", actual_bank & 0x7f);
      }
      else
      {
         printf("Switched to Rom Bank 0x%02x  to extensionbank 0x%02x\n", (actual_bank & 0x30) >> 4, (actual_bank & 0x0f));
      }
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
      ret = xmodemReceive(upload_memory, sizeof(upload_memory));
      printf("Upload complete, %d bytes received\n", ret);
      break;
   case 'w':
      printf("Enter ROM-Slot for writing (00-99): \n");
      fflush(stdout);
      sleep_ms(500); // give some time for the prompt to be sent before waiting for input
      in = getaline();
      int slot = strtol(in, NULL, 10);
      if ((slot < 0) || (slot > 99))
      {
         printf("Invalid slot number\n");
         break;
      }
      printf("Enter the name of the ROM (max 20 chars): \n");
      fflush(stdout);
      sleep_ms(500); // give some time for the prompt to be sent before waiting for input
      in = getaline();
      if (strlen(in) > 20)
      {
         printf("ROM name too long, max 20 chars\n");
         break;
      }
      printf("Write ROM start xmodem transfer now \n");
      cart_size = xmodemReceive(upload_memory, sizeof(upload_memory));
      if (cart_size <= 0)
      {
         printf("Xmodem transfer failed\n");
         break;
      }

      sleep_ms(500); // give some time for the prompt to be sent before waiting for input
      printf("\n\nUpload complete, %d bytes received\n", cart_size);
      ret = write_cart_data(upload_memory, cart_size, slot);
      printf("Data written, crc 0x%04x \n", ret);
      ret = validate_cart_data(slot, in, cart_size, 0, false);
      printf("Data validated, %d result\n", ret);
      printf("ROM-Slot %02d written\n", slot);
      break;
   case 'i':
      print_cart_dir();
      break;
#ifdef DEBUG_STATES
   case 'x':
   {
      uint16_t print_val = debug_val[0] + (debug_val[1] * 0x100);
      printf("PC0: 0x%04x\n", print_val);
   }
   break;
   case 'n':
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
   init_flash_store();
   // readConfiguration();
   debug_clocks();
   getaline_init();
   print_help();
   memset(upload_memory, 0x00, sizeof(upload_memory));
   memcpy(upload_memory, demo_data, sizeof(demo_data));
   for (;;)
   {
      console_rp2040();
      if (reset_triggered)
      {
         alarm_cancel();
         memcpy(&memory[0x0800], upload_memory, EMULATED_MEMSIZE - 0x800); // restore ROM to default
         memset(&memory[0x2800], 0x00, 0x400);                             // clear RAM to 0x00
         memset(&memory[0x0000], 0x00, 0x20);                              // clear IO-bank to 0x00
         timer_val_actual = 0xff;
         reset_triggered = 0;
         printf("System Reset performed\n");
      }
      volatile uint8_t actual_bank = memory[ROM_BANK_ADR];
      if (actual_bank != current_bank)
      {
         current_bank = actual_bank;
         uint8_t rom_bank = (actual_bank & 0x30) >> 4;
         uint8_t original_bank = (actual_bank & 0x80);
         uint8_t extension_bank = actual_bank & 0xf;

         if (original_bank)
         {
            memcpy(&memory[BANK_START_ADDRESS + BANK_SIZE * rom_bank], &shadow_rom[rom_bank * BANK_SIZE], BANK_SIZE); // set bit7 for original bank
         }
         else
         {
            memcpy(&memory[BANK_START_ADDRESS + BANK_SIZE * rom_bank], &upload_memory[EMULATED_MEMSIZE + BANK_SIZE * extension_bank], BANK_SIZE); // set bit7 for original bank
         }
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