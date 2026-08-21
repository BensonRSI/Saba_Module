
#ifndef _NATIVE_COMMON_H_
#define _NATIVE_COMMON_H_ _NATIVE_COMMON_H_

#define ROM_BANKS 4
#define EXTENSION_BANKS 15
#define BANK_START_ADDRESS 0xc000
#define BANK_SIZE 0x1000
#define EMULATED_MEMSIZE 0x10000

// Core0: handle (user) I/O
void console_run();
void console_set_crlf(bool enable);

// Core1: handle 65c02 bus
void bus_run();
void system_init();
void system_reboot();

// Core1: debug output
void debug_backtrace();
void debug_clocks();
void debug_heap();
void debug_hexdump(uint8_t *memory, uint32_t size, uint16_t address);
void debug_internal_drive();
void debug_queue_event(const char *text);

#endif
