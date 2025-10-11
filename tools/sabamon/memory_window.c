#include <stdlib.h>
#include "memory_window.h"
#include "window.h"
#include "target_memory.h"

static Win *win;

static void repaint_proc(void *data, char *fmt, tWord adr)
{
    WinPrintCenter(win, MEMORY_WINDOW_YSIZE>>1, fmt, adr);
    WinPaint(win);
}

static void clear_proc(void *data)
{
    WinUnpaint(win, (EditScreen *)data);
}


static void delete_memory_window(void)
{
    WinFree(win);
}

void create_memory_window(EditScreen *editscreen)
{
    win = WinAlloc((editscreen->cols - MEMORY_WINDOW_XSIZE)>>1,
                   (editscreen->lines - MEMORY_WINDOW_YSIZE)>>1,
                    MEMORY_WINDOW_XSIZE,
                    MEMORY_WINDOW_YSIZE);
    atexit(delete_memory_window);
    set_callback_mem(repaint_proc, clear_proc, (void*)editscreen);
}

