#ifndef __windows_0314
#define __windows_0314

typedef struct
{
    int x;
    int y;
    int width;
    int height;
    char **text;
} Win;

Win *WinAlloc(int x, int y, int width, int height);
void WinFree(Win *win);
void WinPaint(Win *win);
void WinPrintCenter(Win *win, int line, char *form, ...);
void WinUnpaint(Win *win, EditScreen *screen);


#endif
