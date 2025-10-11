#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "target_memory.h"

#include "protocol.h"

static TargetMem mem;
static TargetMemCallbackPaint mem_call_back_paint = NULL;
static TargetMemCallbackClear mem_call_back_clear = NULL;
static void *mem_call_back_data = NULL;

void init_mem(void)
{
    memset(mem.mem, 0, SIZE_OF_MEM);
    //for(int i=0; i < SIZE_OF_MEM; ++i)
    //{
    //    mem.mem[i] = random();
    //}
    memset(mem.stat, MEM_NOT_READ, SIZE_OF_MEM);
    mem.dirty = 0;
}

tByte *read_mem(tWord adr, tWord length)
{
    unsigned long current_begin;
    unsigned long current_end;
    unsigned long current_adr = adr;
    unsigned long current_length;
    int is_painted;

    while (length > 0)
    {
        is_painted = FALSE;
        /* search begin of mem that is not read */
        for (current_begin = current_adr;
            current_begin < current_adr+length;
            ++current_begin)
        {
			if(current_begin >= SIZE_OF_MEM)
			{
				break;
			}
            if (mem.stat[current_begin] == MEM_NOT_READ)
            {
                break;
            }
        }

        /* clip away more the 240 bytes */
        current_length = MIN((current_adr+length),SIZE_OF_MEM) - current_begin;
        if (current_length > 240)
        {
            current_length = 240;
        }
        /* find now the end of the block */
        for (current_end = current_begin;
            current_end < current_begin+current_length;
            ++current_end)
        {
			if(current_end >= SIZE_OF_MEM)
			{
				break;
			}
            if (mem.stat[current_end] != MEM_NOT_READ)
            {
                break;
            }
        }
        if (current_end > current_begin)
        {
            /* now we have the first block to get */
            if (current_end - current_begin > MEM_SHOW_SIZE)
            {
                /* show a message */
                if (mem_call_back_paint)
                {
                    mem_call_back_paint(mem_call_back_data,
                                        "read  mem at %04X", current_begin);
                    is_painted = TRUE;
                }
            }
#ifdef NO_SERIAL
            {
//                unsigned int i;
//                for (i=current_begin; i <current_end; ++i)
//                {
//                    mem.mem[i] = (tByte)rand();
//                } 
            }
#else
            get_mem(current_begin,
                    current_end-current_begin,
                    mem.mem+current_begin);
#endif
            memset(mem.stat+current_begin, MEM_OK, current_end-current_begin);
            if (is_painted && mem_call_back_clear)
            {
                mem_call_back_clear(mem_call_back_data);
            }
        }
        /* take away that block */
        length -= (current_end-current_adr);
        current_adr = current_end;
		if (current_adr >= SIZE_OF_MEM)
		{
			length = 0;
		}
    }

    return mem.mem+adr;
}

void update_mem()
{
    unsigned int current_begin;
    unsigned int current_end;
    unsigned int current_adr = 0;
    unsigned int current_length;
    unsigned int length = SIZE_OF_MEM;
    int is_painted;

    while (length > 0)
    {
        is_painted = FALSE;
        /* search begin of mem that is not read */
        for (current_begin = current_adr;
            current_begin < current_adr+length;
            ++current_begin)
        {
			if (current_begin == SIZE_OF_MEM)
			{
				break;
			}
            if (mem.stat[current_begin] == MEM_NOT_WRITTEN)
            {
                break;
            }
        }

        /* clip away more the 240 bytes */
        current_length = MIN((current_adr+length),SIZE_OF_MEM) - current_begin;
        if (current_length > 100)
        {
            current_length = 100;
        }
        /* find now the end of the block */
        for (current_end = current_begin;
            current_end < current_begin+current_length;
            ++current_end)
        {
			if (current_end == SIZE_OF_MEM)
			{
				break;
			}
            if (mem.stat[current_end] != MEM_NOT_WRITTEN)
            {
                break;
            }
        }
        /* now we have the first block to get */
        if (current_end > current_begin)
        {
            if (current_end - current_begin > MEM_SHOW_SIZE)
            {
                /* show a message */
                if (mem_call_back_paint)
                {
                    mem_call_back_paint(mem_call_back_data,
                                        "write mem at %04X", current_begin);
                    is_painted = TRUE;
                }
            }
#ifdef NO_SERIAL
#else
            set_mem(current_begin,
                    current_end-current_begin,
                    mem.mem+current_begin);
#endif
            memset(mem.stat+current_begin, MEM_NOT_READ, current_end-current_begin);
            if (is_painted && mem_call_back_clear)
            {
                mem_call_back_clear(mem_call_back_data);
            }
        }
        /* take away that block */
        length -= (current_end-current_adr);
        current_adr = current_end;
    }
}

void set_byte(tWord adr, tByte value)
{
    mem.mem[adr] = value;
    mem.stat[adr] = MEM_NOT_WRITTEN;
}

void invalid_mem(void)
{
    memset(mem.stat, MEM_NOT_READ, SIZE_OF_MEM);
}

void set_callback_mem(TargetMemCallbackPaint callback_paint,
                      TargetMemCallbackClear callback_clear,
                      void *data)
{
    mem_call_back_paint = callback_paint;
    mem_call_back_clear = callback_clear;
    mem_call_back_data = data;
}





