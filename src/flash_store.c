/* For all carts , 1 Mb space ( minus 1 page ) is reserved in Flash
   the last page contains the directory. This is written, when all cart-data has been flashed
   The reserved space has to be a multiple of FLASH_PAGE_SIZE
*/

#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// #include "rp2040.h"
#include <pico/stdlib.h>
#include <pico/util/queue.h>
#include <pico/multicore.h>
#include <pico/platform.h>
#include <pico/binary_info.h>

#include <hardware/clocks.h>
#include <hardware/flash.h>

#include "flash_store.h"

uint32_t cart_flash_offset[MAX_CART_NO];
dir_entry local_dir;

unsigned char config[CFG_CONFIG_SIZE];

// TODO : replace with well known table based function
uint16_t crc16(uint8_t *p, uint8_t l)
{
    uint8_t x;
    uint16_t crc = 0xFFFF;

    while (l--)
    {
        x = crc >> 8 ^ *p++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x << 5)) ^ ((uint16_t)x);
    }
    return crc;
}

void clearDirEntries(void)
{

    memset(config, 0, CFG_CONFIG_SIZE - CFG_DIR_OFFSET);
}

uint8_t *getNextDirEntry(uint8_t *currentEntry)
{

    uint8_t *nextEntry = currentEntry + CFG_DIR_OFFSET;

    do
    {
        if (nextEntry[0] == 0x55)
        { // Check if valid
            return nextEntry;
        }

    } while (nextEntry < (uint8_t *)(FLASH_DATA_OFFSET + (MAX_CART_NO * CFG_DIR_OFFSET)));

    return NULL; // no valid entry found
}

void erase_flash(uint32_t offset, size_t size)
{

    assert((offset % FLASH_SECTOR_SIZE) == 0);
    assert((size % FLASH_SECTOR_SIZE) == 0);
    multicore_lockout_start_blocking();
    uint32_t ints = save_and_disable_interrupts();
    SET_CLOCK_125MHZ;
    flash_range_erase(offset, size);
    restore_interrupts(ints);
    multicore_lockout_end_blocking();
    sleep_ms(20);
    SET_CLOCK_FAST;
}
void program_flash(uint32_t offset, const uint8_t *data, size_t size)
{

    assert((offset % FLASH_PAGE_SIZE) == 0);
    assert((size % FLASH_PAGE_SIZE) == 0);
    multicore_lockout_start_blocking();
    uint32_t ints = save_and_disable_interrupts();
    SET_CLOCK_125MHZ;
    flash_range_program(offset, data, size);
    restore_interrupts(ints);
    multicore_lockout_end_blocking();
    sleep_ms(20);
    SET_CLOCK_FAST;
}

uint32_t get_cart_flash_offset(int cart_no)
{

    assert(cart_no <= MAX_CART_NO);
    return cart_flash_offset[cart_no];
}

void invalidate_cart_data(int cart_no)
{

    assert(cart_no <= MAX_CART_NO);
    uint32_t flash_off = get_cart_flash_offset(cart_no);
    erase_flash(flash_off, FLASH_SECTOR_SIZE);
}

dir_entry *get_cart_dir_entry(int cart_no)
{

    assert(cart_no <= MAX_CART_NO);

    uint32_t flash_off = FLASH_DATA_OFFSET + (CFG_DIR_OFFSET * cart_no);
    return (dir_entry *)(flash_off + XIP_BASE);
}

bool is_cart_data_valid(int cart_no)
{

    assert(cart_no <= MAX_CART_NO);

    dir_entry *cart_dir_entry = get_cart_dir_entry(cart_no);

    if (cart_dir_entry->valid == 0x28)
    {
        return true;
    }
    return false;
}

uint32_t get_cart_size(int cart_no)
{

    assert(cart_no <= MAX_CART_NO);

    dir_entry *cart_dir_entry = get_cart_dir_entry(cart_no);

    if (cart_dir_entry->valid != 0x28)
    {
        return 0;
    }
    return cart_dir_entry->size;
}

bool validate_cart_data(int cart_no, const char *name, uint32_t size, uint16_t crc, bool use_crc)
{

    assert(cart_no <= MAX_CART_NO);
    // invalidate_cart_data(cart_no);
    uint8_t local_buffer[FLASH_SECTOR_SIZE];

    local_dir.valid = 0x28;
    local_dir.size = size;
    local_dir.crc = crc16((uint8_t *)(get_cart_flash_offset(cart_no) + XIP_BASE), size);
    printf("Calculated CRC for cart data: 0x%04x\n", local_dir.crc);
    strncpy((char *)local_dir.cart_name, name, 20);
    local_dir.cart_name[20] = 0;
    local_dir.terminate = 0; // make sure it is 0 terminated
    if ((local_dir.crc != crc) && (use_crc))
    {
        // Flashing failed
        return false;
    }
    // Get copy of direntry page, update it and write it back. We cannot write only the entry, because we can only write in page size steps
    memcpy(local_buffer, (uint8_t *)(FLASH_DATA_OFFSET + XIP_BASE), FLASH_SECTOR_SIZE);
    // memset(local_buffer, 0, FLASH_SECTOR_SIZE);
    memcpy(local_buffer + (CFG_DIR_OFFSET * cart_no), &local_dir, sizeof(dir_entry));
    erase_flash(FLASH_DATA_OFFSET, FLASH_SECTOR_SIZE);
    program_flash(FLASH_DATA_OFFSET, local_buffer, FLASH_SECTOR_SIZE);
    return true;
}

uint16_t write_cart_data(uint8_t *cart_data, uint32_t size, int cart_no)
{

    assert(cart_no <= MAX_CART_NO);
    assert(size <= 0x100000);

    uint32_t flash_p = cart_flash_offset[cart_no];
    uint8_t local_page[FLASH_PAGE_SIZE];

    printf("Flashing cart data to flash at offset 0x%08x, size %d bytes\n", flash_p, size);
    fflush(stdout);
    sleep_ms(500); // give some time for the prompt to be sent before waiting for input

    // We can erase the whole area
    erase_flash(flash_p, MAX_CART_SIZE);
    for (uint32_t i = 0; i < size; i += FLASH_PAGE_SIZE)
    {
        uint8_t *cart_src = ((uint8_t *)cart_data) + i;
        memcpy(local_page, cart_src, FLASH_PAGE_SIZE);
        program_flash(flash_p + i, local_page, FLASH_PAGE_SIZE);
    }
    return (crc16((uint8_t *)cart_data, size));
}

int init_flash_store()
{

    for (int i = 0; i < MAX_CART_NO; i++)
    {
        cart_flash_offset[i] = FLASH_DATA_OFFSET + (MAX_CART_SIZE * (i + 1));
    }

    return MAX_CART_NO;
}