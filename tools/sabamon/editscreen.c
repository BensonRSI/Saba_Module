#include <stdlib.h>
#include <string.h>
#include "editscreen.h"
#include "parse.h"
#include "processor.h"
#include "memory_window.h"

static void end_win(void)
{
	endwin();
}

static void paint_cursor(EditScreen *screen)
{
#pragma GCC diagnostic ignored "-Wformat-zero-length"
    mvprintw(screen->line, screen->col,"");
#pragma GCC diagnostic warning "-Wformat-zero-length"
    refresh();          /* Print it on to the real screen */
}

static void init_screen(EditScreen *screen)
{
    int i;
    initscr();          /* Start curses mode 		  */
    if (COLS < 80)
    {
        endwin();
        fprintf(stderr, "Screen is too small !!!!\n");
        exit(-1);
    }
    screen->chars = (char **)(malloc(sizeof(char *)*LINES));
    for (i=0; i < LINES; ++i)
    {
        screen->chars[i] = (char *)(malloc(COLS+1));
        memset(screen->chars[i], ' ', COLS);
        /* string end for printing the lines */
        screen->chars[i][COLS] = 0;
    }
#ifdef PROZ_8085
    strcpy(screen->chars[0], "     ****** TRS-80 Serial Monitor (c) Ulrich Schulz *****");
#endif
#ifdef PROZ_F3850
    strcpy(screen->chars[0], "     ****** Failchild 3850 Monitor (c) Ulrich Schulz *****");
#endif
    strcpy(screen->chars[1], "     **** released under the GPL version 2 or higher ****");
    screen->lines = LINES;
    screen->cols = COLS;
//	screen->win = create_newwin(COLS, LINES, 0, 0);

    raw();
    cbreak();
    noecho();
//	nonl();
//	intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    curs_set(2);
    screen->col =0;
    screen->line = 2;
    screen->quit = FALSE;
    create_memory_window(screen);
    atexit(end_win);     /* End curses mode		  */
}

void update_line(EditScreen *screen, int line)
{
    mvprintw(line, 0, "%s", screen->chars[line]);
    paint_cursor(screen);
}

static void update_screen(EditScreen *screen)
{
    int i;
    for (i=0; i < screen->lines; ++i)
    {
        mvprintw(i, 0, "%s", screen->chars[i]);
    }
    paint_cursor(screen);
}

int go_up(EditScreen *screen)
{
    int do_scroll = FALSE;
    if (screen->line > 0)
    {
        --(screen->line);
        paint_cursor(screen);
    }
    else
    {
        int i;
        for (i=screen->lines-2; i >= 0; --i)
        {
            memcpy(screen->chars[i+1], screen->chars[i], screen->cols);
        }
        memset(screen->chars[0], ' ', screen->cols);
        do_scroll = TRUE;
    }
    update_screen(screen);
    return do_scroll;
}

int go_down(EditScreen *screen)
{
    int do_scroll = FALSE;
    if (screen->line < (screen->lines-1))
    {
        ++(screen->line);
        paint_cursor(screen);
    }
    else
    {
        int i;
        for (i=0; i < screen->lines-1; ++i)
        {
            memcpy(screen->chars[i], screen->chars[i+1], screen->cols);
        }
        memset(screen->chars[screen->lines -1], ' ', screen->cols);
        do_scroll = TRUE;
    }
    update_screen(screen);
    return do_scroll;
}

static int go_left(EditScreen *screen)
{
    int do_scroll = FALSE;
    if (screen->col >0)
    {
        --(screen->col);
        paint_cursor(screen);
    }
    else
    {
        screen->col = screen->cols-1;
        do_scroll = go_up(screen);
    }
    return do_scroll;
}

static int go_right(EditScreen *screen)
{
    int do_scroll = FALSE;
    if (screen->col < screen->cols-1)
    {
        ++(screen->col);
        paint_cursor(screen);
    }
    else
    {
        screen->col = 0;
        do_scroll = go_down(screen);
    }
    return do_scroll;
}

static void print_char(EditScreen *screen, int c)
{
    memmove(screen->chars[screen->line] + screen->col+1,
            screen->chars[screen->line] + screen->col,
            screen->cols-(screen->col+1));
    screen->chars[screen->line][screen->col] = (char)c;
    update_line(screen, screen->line);
}

static void do_backspace(EditScreen *screen)
{
    memmove(screen->chars[screen->line] + screen->col-1,
            screen->chars[screen->line] + screen->col,
            screen->cols-(screen->col));
    screen->chars[screen->line][screen->cols-1] = ' ';
    update_line(screen, screen->line);
}

static void do_enter(EditScreen *screen)
{
    int result;
    int error_pos;
    int column;

    column = screen->col;
    screen->col = 0;
    /* here to interpret the line */
    result = make_command(screen,
                          (tByte *)(screen->chars[screen->line]),
                          screen->cols,
                          column,
                          &error_pos);
    if (result < 0)
    {
        if (error_pos < screen->cols)
        {
            ++error_pos;
            screen->chars[screen->line][error_pos] = '?';
            update_line(screen, screen->line);
        }
    }
    go_down(screen);
    if(result >= 0)
    {
        if(screen->chars[screen->line][0] == ';')
        {
            screen->col = 7+3*MAX_OF_OPCODE;
            update_line(screen, screen->line);
        }
    }
}

void clear_line(EditScreen *screen, int line)
{
    memset(screen->chars[line], ' ', screen->cols);
}

void edit_screen()
{
    int c;
    EditScreen screen;

    init_screen(&screen);

    update_screen(&screen);
    update_line(&screen, 0);
    paint_cursor(&screen);
    do
    {
        c = getch();            /* Wait for user input */
        switch (c)
        {
        case KEY_UP : if(go_up(&screen)) make_upline(&screen); break;
        case KEY_DOWN : if(go_down(&screen)) make_downline(&screen); break;
        case KEY_LEFT : if(go_left(&screen)) make_upline(&screen); break;
        case KEY_RIGHT : if(go_right(&screen)) make_downline(&screen); break;
        case KEY_BACKSPACE : do_backspace(&screen); go_left(&screen);break;
        case KEY_DC : do_backspace(&screen); break;
        case KEY_IC : print_char(&screen, ' '); break;
        case KEY_BEG : screen.col = 0; paint_cursor(&screen); break;
        case KEY_END : screen.col = screen.cols-1; 
            paint_cursor(&screen); break;

        case '\n' : do_enter(&screen); break;           
        default :
            if (c >= 0x20 && c <= 0x7e)
            {
                switch (screen.chars[screen.line][0])
                {
                case ':' :
                case ';' :
                    screen.chars[screen.line][screen.col] = (char)c;
                    update_line(&screen, screen.line);
                    break;
                default:
                    print_char(&screen,c);
                    break;
                }
                go_right(&screen);
            }
            break;
        }                           
    }
    while (!(screen.quit));
    exit(0);
}
