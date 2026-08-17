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
#include "flash_store.h"

extern uint8_t memory[0x10000];
// the first 8 bytes of the memory are used as IO-Ports
// The adresses asre used for timer and IRQ-control
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
#define TIMER_STOP 0xff
#define TIMER_CLOCK_MULTIPLIER 806 // 38400 Hz timer clock * 31  , so 1 timer tick = 806 us

extern volatile uint8_t reset_triggered;
extern volatile uint8_t irq_triggered;
uint8_t upload_memory[0x10000 - 0x800]; /* Maximum size we can use = 62k*/
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

const uint8_t timerval_to_counter[255] = {
    0x7f, 0xbf, 0x5f, 0x2f, 0x97, 0xcb, 0xe5, 0x72, 0x39, 0x1c, 0x0e, 0x97, 0x43, 0xa1, 0xd0,
    0xe8, 0xf4, 0x7a, 0x3d, 0x1e, 0x0f, 0x07, 0x03, 0x01, 0x00, 0x80, 0xc0, 0x60, 0xb0, 0xd8,
    0xec, 0xf6, 0x7b, 0xbd, 0x5e, 0xaf, 0xa7, 0x6b, 0x35, 0x1a, 0x0d, 0x06, 0x83, 0x41, 0xa0,
    0x50, 0xa8, 0x54, 0xaa, 0x55, 0x2a, 0x15, 0x8a, 0xc5, 0xe2, 0xf1, 0xf8, 0x7c, 0x3d, 0x9f,
    0xcf, 0xe7, 0x73, 0xb9, 0x5c, 0xae, 0x57, 0x2b, 0x95, 0xca, 0x65, 0x32, 0x99, 0xcc, 0x55,
    0xb3, 0x59, 0x2c, 0x16, 0x0b, 0x05, 0x02, 0x81, 0x40, 0x20, 0x10, 0x03, 0x84, 0xc2, 0x61,
    0x30, 0x98, 0x4c, 0x26, 0x13, 0x89, 0x44, 0x22, 0x11, 0x88, 0xc4, 0x62, 0xb1, 0x58, 0xac,
    0x56, 0xab, 0xd5, 0x6a, 0x85, 0x5a, 0xad, 0xd6, 0xeb, 0x75, 0xba, 0xdd, 0x6e, 0xb7, 0x5b,
    0x2d, 0x96, 0x4b, 0xa5, 0xd2, 0xe9, 0x74, 0x3a, 0x9d, 0xce, 0x67, 0x33, 0x19, 0x8c, 0xc6,
    0x63, 0x31, 0x18, 0x8c, 0xc6, 0x63, 0x31, 0x18, 0x0c, 0x86, 0xc3, 0xe1, 0x70, 0x38, 0x9c,
    0x4e, 0x27, 0x93, 0xc9, 0xe4, 0xf2, 0x79, 0xbc, 0xde, 0xef, 0x77, 0xbb, 0x5d, 0x2e, 0x17,
    0x8b, 0x45, 0xa2, 0x51, 0x28, 0x14, 0x0a, 0x84, 0x12, 0x06, 0x04, 0x82, 0xc1, 0xe0, 0xf0,
    0x78, 0x3c, 0x9e, 0x4f, 0xa7, 0xd3, 0x69, 0x34, 0x9a, 0x4d, 0xa6, 0x53, 0x29, 0x94, 0x4a,
    0x25, 0x92, 0x49, 0xa6, 0x52, 0xa9, 0xd4, 0xea, 0xf5, 0xfa, 0x7d, 0xbe, 0xdf, 0x8f, 0x37,
    0x1b, 0x8d, 0x46, 0x23, 0x91, 0xc8, 0x64, 0xb2, 0xd9, 0x6c, 0xb6, 0xdb, 0x6d, 0x36, 0x9b,
    0xcd, 0xe6, 0xf3, 0xf9, 0xfc, 0x7e, 0x3f, 0x1f, 0x8f, 0x47, 0xa3, 0xd1, 0x68, 0xb4, 0xda,
    0xed, 0x76, 0x3b, 0x1d, 0x8e, 0xc7, 0xe3, 0x71, 0xb8, 0xdc, 0xee, 0xf7, 0xfb, 0xfd, 0xfe};

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
      // set timer value to 1 sec
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
         memcpy(&memory[0x0800], upload_memory, sizeof(upload_memory));
         memset(&memory[0x2800], 0x00, 0x400); // clear RAM to 0x00
         memset(&memory[0x0000], 0x00, 0x20);  // clear IO-bank to 0x00
         alarm_cancel();
         timer_val_actual = 0xff;
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