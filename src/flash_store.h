#ifndef FLASH_STORE_H
#define FLASH_STORE_H

#include <stdint.h>
#include <stdbool.h>

/* For each cart , 1 Mb space ( minus 1 page ) is reserved in Flash
   the last page contains the directory. This is written, when all cart-data has been flashed
   The reserved space has to be a multiple of FLASH_PAGE_SIZE
*/

/* Defines */
#define MAX_CART_NO 100
#define MAX_CART_SIZE 1024 * 8 // 8 k per cart, so we can store 128 carts in 1 MB flash ( minus config and dir entries )

#define FLASH_DATA_OFFSET 0x100000 // offset in flash where cart data starts ( after programm)
#define CFG_CONFIG_SIZE 4096       // one sector

#define CFG_CRC1 30
#define CFG_CRC2 31
#define CFG_SIZE 32

#define CFG_DIR_OFFSET 40

// Dummies
#define SET_CLOCK_125MHZ // set_sys_clock_pll(1500000000, 6, 2)
// for ModPlayer we need a multiple of 44,1kHz *32
#define SET_CLOCK_144MHZ // set_sys_clock_khz( 144000, true );
#define SET_CLOCK_FAST   // set_sys_clock_khz(360000, false)

#define DELAY_Nx3p2_CYCLES(c)             \
    asm volatile("mov  r0, %[_c]\n\t"     \
                 "1: sub  r0, r0, #1\n\t" \
                 "bne   1b" : : [_c] "r"(c) : "r0", "cc", "memory");

/* Flash System structure:
   1. page directory
       40 Bytes per entry
       [0]  Valid Yes = 0x55 /No !=0x55
       [1] Byte unused
       [2..3] Crc / checksum
       [4..7] Size
       [8..11] Offset
       [12..15] unused
       [16..36] cart-name / 0 terminated
   2. page - 3. page Flashdata CART1  ( 8 k )
   4. page - 5. page Flashdata CART2  ( 8 k )
   ....
   up to 128 Carts
*/

/* Typedefs */
typedef struct dir_entry_t
{
    __uint8_t valid;
    __uint8_t unused1;
    __uint16_t crc;
    __uint32_t size;
    __uint32_t unused2;
    __int8_t cart_name[21];
    __uint8_t terminate;

} dir_entry;

/* Function Declarations */

/**
 * Clear all directory entries in configuration
 */
void clearDirEntries(void);

/**
 * Get the next valid directory entry from the given current entry
 * @param currentEntry Pointer to current entry
 * @return Pointer to next valid entry, or NULL if no valid entry found
 */
uint8_t *getNextDirEntry(uint8_t *currentEntry);

/**
 * Erase flash memory at given offset and size
 * @param offset Flash memory offset
 * @param size Size of memory to erase (must be multiple of FLASH_SECTOR_SIZE)
 */
void erase_flash(uint32_t offset, size_t size);

/**
 * Program flash memory at given offset with data
 * @param offset Flash memory offset (must be multiple of FLASH_PAGE_SIZE)
 * @param data Pointer to data to write
 * @param size Size of data to write (must be multiple of FLASH_PAGE_SIZE)
 */
void program_flash(uint32_t offset, const uint8_t *data, size_t size);

/**
 * Read configuration from flash
 */
void readConfiguration(void);

/**
 * Write configuration to flash
 */
void writeConfiguration(void);

/**
 * Get flash offset for given cart number
 * @param cart_no Cart number
 * @return Flash offset for the cart
 */
uint32_t get_cart_flash_offset(int cart_no);

/**
 * Invalidate cart data by erasing the directory entry sector
 * @param cart_no Cart number to invalidate
 */
void invalidate_cart_data(int cart_no);

/**
 * Get directory entry for given cart number
 * @param cart_no Cart number
 * @return Pointer to directory entry
 */
dir_entry *get_cart_dir_entry(int cart_no);

/**
 * Check if cart data is valid
 * @param cart_no Cart number
 * @return true if valid, false otherwise
 */
bool is_cart_data_valid(int cart_no);

/**
 * Get size of cart data
 * @param cart_no Cart number
 * @return Size of cart data, 0 if invalid
 */
uint32_t get_cart_size(int cart_no);

/**
 * Validate and write cart directory entry
 * @param cart_no Cart number
 * @param size Size of cart data
 * @param crc CRC checksum
 * @param use_crc Whether to use CRC validation
 * @return true if successful, false if CRC mismatch
 */
bool validate_cart_data(int cart_no, const char *name, uint32_t size, uint16_t crc, bool use_crc);

/**
 * Write cart data to flash
 * @param cart_data Pointer to cart data
 * @param size Size of cart data
 * @param cart_no Cart number
 * @return CRC checksum of written data
 */
uint16_t write_cart_data(uint8_t *cart_data, uint32_t size, int cart_no);

/**
 * Initialize flash store
 * @return Number of available carts (MAX_CART_NO)
 */
int init_flash_store(void);

#endif // FLASH_STORE_H
