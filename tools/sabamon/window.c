#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <alloca.h>


#include "editscreen.h"
#include "window.h"

static char **alloc_window(int x, int y)
{
    int i;
    char **ret = (char **)(malloc(sizeof(char *)*(y+1)));
    ret[0] = (char *)(malloc(x +1));
    memset(ret[0], '#', x);
    ret[0][x] = 0;

    for(i=1; i < (y-1); ++i)
    {
        ret[i] = (char *)(malloc(x +1));
        ret[i][0] = '#';
        memset(ret[i]+1, ' ', x-2);
        ret[i][x-1] = '#';
        ret[i][x] = 0;
    }
    
    ret[y-1] = (char *)(malloc(x +1));
    memset(ret[y-1], '#', x);
    ret[y-1][x] = 0;

    ret[y] = NULL;
    return ret;
}

static void free_window(char **win)
{
    int i=0;
    while(win[i])
    {
        free(win[i]);
        ++i;
    }
    free(win);
}
    
Win *WinAlloc(int x, int y, int width, int height)
{
    Win *ret = (Win *)(malloc(sizeof(Win)));
    ret->x = x;
    ret->y = y;
    ret->width = width;
    ret->height = height;
    ret->text = alloc_window(width,height);
    return ret;
}

void WinFree(Win *win)
{
    free_window(win->text);
    free(win);
}

void WinPaint(Win *win)
{
    int i;
    for(i=0; i < win->height; ++i)
    {
        mvprintw(i+win->y, win->x, win->text[i]);
    }
    refresh();
}

void WinUnpaint(Win *win, EditScreen *screen)
{
    int i;
    for(i=0; i < win->height; ++i)
    {
        update_line(screen,i+win->y);
    }
}

void WinPrintCenter(Win *win, int line, char *form, ...)
{
    va_list ap;
    char *str;
    int size;

    if(line >= win->height)
    {
        return;
    }
    va_start(ap,form);
    str = alloca(win->width+1);
    size = vsnprintf(str, win->width, form, ap);
    memcpy(win->text[line] + ((win->width-size)>>1), str, size);
    va_end(ap);
}


