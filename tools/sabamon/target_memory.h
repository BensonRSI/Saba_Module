#ifndef __target_memory_h_31415_
#define __target_memory_h_31415_

#include "c_types.h"

#define SIZE_OF_MEMDUMP 16

#define SIZE_OF_MEM 0x10000
#define NO_SERIAL

typedef struct 
{
    tByte mem[SIZE_OF_MEM+SIZE_OF_MEMDUMP];
    tByte stat[SIZE_OF_MEM+SIZE_OF_MEMDUMP];
    int dirty;
} TargetMem;

#define MEM_OK 0
#define MEM_NOT_READ 1
#define MEM_NOT_WRITTEN 2

#define MEM_SHOW_SIZE 16

typedef void (*TargetMemCallbackPaint)(void *,char *, tWord adr);
typedef void (*TargetMemCallbackClear)(void *);

extern void init_mem(void);
extern tByte *read_mem(tWord adr, tWord length);
extern void update_mem();
extern void set_byte(tWord adr, tByte value);
extern void invalid_mem(void);
extern void set_callback_mem(TargetMemCallbackPaint callback_paint,
                             TargetMemCallbackClear callback_clear,
                             void *data);

#endif
