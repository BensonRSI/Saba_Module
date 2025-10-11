#ifndef __output_h__314513
#define __output_h__314513

#include <curses.h>
#include "c_types.h"
#include "editscreen.h"

extern void print_memdump(EditScreen *screen, tWord adr, tByte *mem);
extern void print_disassembly(EditScreen *screen, tWord adr, tByte *mem);
extern void print_hex_value(tByte *ptr, int value, int digits);

#endif
