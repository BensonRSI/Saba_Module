#ifndef __editscreen_h__3145
#define __editscreen_h__3145

#include <curses.h>

typedef struct
{
    int col;
    int line;
    int cols;
    int lines;
    char ** chars;
    int quit;
} EditScreen;


extern void edit_screen();
extern int go_down(EditScreen *screen);
extern int go_up(EditScreen *screen);
extern void update_line(EditScreen *screen, int line);
extern void clear_line(EditScreen *screen, int line);

#endif
