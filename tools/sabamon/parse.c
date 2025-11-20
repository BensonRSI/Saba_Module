#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include "editscreen.h"
#include "parse.h"
#include "output.h"
#include "target_memory.h"
#include "processor.h"
#include "window.h"
#include "protocol.h"
#include "memory_window.h"

static int check_esc()
{
    int ret_val;
    nodelay(stdscr, TRUE);
    if (getch() == 27)
    {
        ret_val = TRUE;
    }
    else
    {
        ret_val = FALSE;
    }
    nodelay(stdscr, FALSE);
    return ret_val;
}

static int go_over_whitespace(tByte *line, int pos, int size)
{
    for (; pos < size; ++pos)
    {
        if (!isspace(line[pos]))
        {
            break;
        }
    }
    return pos;
}

static int get_ascii(tByte val)
{
    if (val == '.')
    {
        return ERR_WRONG_CHAR;
    }
    else
    {
        return val;
    }
}

int get_hex(tByte *line, int *pos, int size, int *error_pos, int digits)
{
    int p;
    int result = 0;
    int done = 0;
    int oldpos = *pos;

    p = go_over_whitespace(line, *pos, size);
    if(p==size)
    {
        *pos = oldpos;
        *error_pos = oldpos;
        return ERR_NO_NUMBER;
    }
    for (; p < size; ++p)
    {
        char c = line[p];
        if (digits < 0)
        {
            *error_pos = p;
            return ERR_WRONG_CHAR;
        }
        if (isxdigit(c))
        {
            done = 1;
            c = tolower(c);
            result = result << 4;
            if (c >= '0' && c <= '9')
            {
                result += (int)(c-'0');
            }
            else
            {
                result += (int)(c-'a'+10);
            }
            --digits;
        }
        else
        {
            if (!done)
            {
                *error_pos = p;
                return ERR_WRONG_CHAR;
            }
            else
            {
                break;
            }
        }
    }
    if (done)
    {
        *pos = p;
        return result;
    }
    else
    {
        *error_pos = p;
        return ERR_NO_NUMBER;
    }
}

static int check_hex(tByte *line, int pos, int digits)
{
    int i;
    for(i=0; i < digits; ++i)
    {
        char c = line[pos+i];
        if (!isxdigit(c))
        {
            return 0;
        }
    }
    char c = line[pos+i];
    if (isxdigit(c))
    {
        return 0;
    }
    return 1;
}

static int get_start_end(tByte *line, int *pos, int size, int *error_pos,
                         int *start, int *end)
{
    *start = get_hex(line, pos, size, error_pos, 4);
    if (*start < 0)
    {
        return ERR_NO_ARGS;
    }
    *end = get_hex(line, pos, size, error_pos, 4);
    if (*end == ERR_NO_NUMBER)
    {
        *end = *start + 1;
    }
    if (*end < 0)
    {
        return ERR_NO_ARGS;
    }
    return ERR_NO_ERR;
}

static char filename[1000];  /* must be greater equal editscreens x size */

char* get_filename(tByte *line, int *pos, int size, int *error_pos)
{
    int p;
    int i;
    int oldpos = *pos;

    p = go_over_whitespace(line, *pos, size);
    if(p==size)
    {
        *pos = oldpos;
        *error_pos = oldpos;
        return NULL;
    }
    if (line[p] != '\"')
    {
        *error_pos = p;
        return NULL;
    }
    i=0;
    ++p;
    for (; p < size-2; ++p)
    {
        if (line[p] == '\"')
        {
            filename[i] = 0;
            *pos = p+1;
            return filename;
        }
        filename[i] = line[p];
        ++i;
    }
    *error_pos = p;
    return NULL;
}

static int do_memory_dump(EditScreen *screen,
                          tByte *line, int pos, int size, int *error_pos)
{
    int start,end;
    int ret_val;
    ret_val = get_start_end(line, &pos, size, error_pos, &start, &end);
    if (ret_val >= 0)
    {
        while (start < end)
        {
            go_down(screen);
            print_memdump(screen,start, read_mem(start, SIZE_OF_MEMDUMP));
            start += SIZE_OF_MEMDUMP;
            if (check_esc())
            {
                break;
            }
        }
        ret_val = ERR_NO_ERR;
    }
    return ret_val;
}

static int get_memory_dump(EditScreen *screen,
                           tByte *line, int pos, int size, int *error_pos)
{
    int start, ret_val;
    int i;

    tByte firstbuf[SIZE_OF_MEMDUMP];
    tByte secondbuf[SIZE_OF_MEMDUMP];
    tByte *oldmemory;

    start = get_hex(line, &pos, size, error_pos, 4);
    if (start < ERR_NO_ERR)
    {
        return start;
    }
    oldmemory = read_mem(start, SIZE_OF_MEMDUMP);
    for (i=0; i < SIZE_OF_MEMDUMP; ++i)
    {
        pos = go_over_whitespace(line, pos, size);
        ret_val = get_hex(line, &pos, size, error_pos, 2);
        if (ret_val < ERR_NO_ERR)
        {
            return ret_val;
        }
        firstbuf[i] = (tByte)ret_val;
    }
    pos = go_over_whitespace(line, pos, size);
    if (line[pos] != '\"')
    {
        *error_pos = pos;
        return ERR_WRONG_CHAR;
    }
    for (i=0; i < SIZE_OF_MEMDUMP; ++i)
    {
        ++pos;
        ret_val = get_ascii(line[pos]);
        if (ret_val < 0)
        {
            ret_val = oldmemory[i];
        }
        secondbuf[i] = ret_val;
    }
    if (line[pos+1] != '\"')
    {
        *error_pos = pos;
        return ERR_WRONG_CHAR;
    }
    for (i=0; i < SIZE_OF_MEMDUMP; ++i)
    {
        if (firstbuf[i] != oldmemory[i])
        {
            set_byte(start+i, firstbuf[i]);
        }
        else
        {
            set_byte(start+i, secondbuf[i]);
        }
    }
    update_mem();
    print_memdump(screen, start, read_mem(start, SIZE_OF_MEMDUMP));
    return ERR_NO_ERR;
}

static int do_disassembly(EditScreen *screen,
                          tByte *line, int pos, int size, int *error_pos)
{
    int start,end;
    int ret_val;
    ret_val = get_start_end(line, &pos, size, error_pos, &start, &end);
    if (ret_val >= 0)
    {
        while (start < end)
        {

            go_down(screen);            
            print_disassembly(screen,start, read_mem(start, MAX_OF_OPCODE));
            start += get_objcode_length(*(read_mem(start,1)));
            if (check_esc())
            {
                break;
            }
        }
        ret_val = ERR_NO_ERR;
    }
    return ret_val;
}

static int get_disassembly(EditScreen *screen,
                           tByte *line, int pos, int size, int column, int *error_pos)
{
    int start, ret_val;
    int i;
    int end1, end2;

    tByte firstbuf[SIZE_OF_MEMDUMP];
    tByte secondbuf[SIZE_OF_MEMDUMP];
    tByte *oldmemory;


    start = get_hex(line, &pos, size, error_pos, 4);
    if (start < ERR_NO_ERR)
    {
        return start;
    }
    oldmemory = read_mem(start, MAX_OF_OPCODE);
    for (end1=0; end1 < MAX_OF_OPCODE; ++end1)
    {
        pos = go_over_whitespace(line, pos, size);
        if (pos >= 7+3*MAX_OF_OPCODE)
        {
            break;
        }
        ret_val = get_hex(line, &pos, size, error_pos, 2);
        if (ret_val < ERR_NO_ERR)
        {
            break;;
        }
        firstbuf[end1] = (tByte)ret_val;
    }
    pos = 7+3*MAX_OF_OPCODE;

    ret_val = assemble(screen, line, &pos, error_pos, secondbuf, &end2, start);
    if (ret_val < 0 && end1 == 0)
    {
        if (column == 7+3*MAX_OF_OPCODE)
        {
            /* return press on right position to print new line */
            goto reprint_line;
        }
        return ERR_NO_ERR; /* blank line */
    }
    for (i=0; i < MAX(end1, end2); ++i)
    {
        if (i < end1 && firstbuf[i] != oldmemory[i])
        {
            set_byte(start+i, firstbuf[i]);
        }
        else
        {
            if (i < end2)
            {
                set_byte(start+i, secondbuf[i]);
            }
        }
    }
reprint_line:
    update_mem();
    clear_line(screen, screen->line);
    print_disassembly(screen,start, read_mem(start, MAX_OF_OPCODE));
    go_down(screen);
    clear_line(screen, screen->line);
    start += get_objcode_length(*(read_mem(start, 1)));
    screen->chars[screen->line][0] = ';';
    print_hex_value(screen->chars[screen->line]+1, start, 4);
    screen->chars[screen->line][5] = ' ';
    go_up(screen);
    return ERR_NO_ERR;
}

static int io_error(char *line, int size, int pos, int *error_pos)
{
    char *error_string;
    int error_length;

    error_string = strerror(errno);
    error_length = MIN(strlen(error_string), size-pos+2);
    if (error_length < 1)
    {
        error_length = 0;
    }
    else
    {
        memcpy(line+pos+2, error_string, error_length);
    }
    *error_pos = pos;
    return ERR_IO;
}

static int get_filename_and_args(EditScreen *screen,
                                 tByte *line, int *pos, int size, int *error_pos,
                                 char **filename, int *start, int *end)
{
    int ret;

    *filename = get_filename(line,pos,size,error_pos);

    if (!(*filename))
    {
        return ERR_NO_FILENAME;
    }
    ret = get_start_end(line,pos,size,error_pos, start, end);
    if (ret < ERR_NO_ERR)
    {
        return ret;
    }
    return ERR_NO_ERR;
}


static int save_file(EditScreen *screen,
                     tByte *line, int pos, int size, int *error_pos)
{
    char *filename;
    int start, end;
    int ret;
    tByte *mem;
    FILE *f;

    ret = get_filename_and_args(screen, line, &pos, size, error_pos,
                                &filename, &start, &end);
    if (ret < ERR_NO_ERR)
    {
        return ret;
    }
    if (start >= end)
    {
        *error_pos = pos;
        return ERR_WRONG_ARGS;
    }
    if (end-start == 1)
    {
        *error_pos = pos;
        return ERR_WRONG_ARGS;
    }
    f = fopen(filename,"wb");
    if (!f)
    {
        return io_error(line, size, pos, error_pos);
    }
    mem = read_mem((tWord)start, (tWord)(end-start));
    fwrite(mem, end-start, 1, f);
    fclose(f);
    return ERR_NO_ERR;
}

static int load_file(EditScreen *screen,
                     tByte *line, int pos, int size, int *error_pos)
{
    char *filename;
    int start, end;
    int ret;
    FILE *f;
    int i;
    tByte *tempbuf;
    int old_pos = pos;

    ret = get_filename_and_args(screen, line, &old_pos, size, error_pos,
                                &filename, &start, &end);
    if (ret < ERR_NO_ERR)
    {
        return ret;
    }
    if(end-start == 1)
    {
        end = SIZE_OF_MEM-start;
    }
    if (start+end > SIZE_OF_MEM)
    {
        *error_pos = pos;
        return ERR_WRONG_ARGS;
    }
    f = fopen(filename,"rb");
    if (!f)
    {
        return io_error(line, size, pos, error_pos);
    }
    tempbuf = (tByte *)alloca(end);
    end = fread(tempbuf, 1, end, f);
    for (i=0; i < end; ++i)
    {
        set_byte(start++, tempbuf[i]);
    }
    update_mem();
    fclose(f);
    return ERR_NO_ERR;
}


static int do_jump(EditScreen *screen,
                   tByte *line, int pos, int size, int *error_pos)
{
    int start;
	
    start = get_hex(line, &pos, size, error_pos, 4);
	if(start >= 0)
	{
	    Win * win = WinAlloc((screen->cols - MEMORY_WINDOW_XSIZE)>>1,
    	               (screen->lines - MEMORY_WINDOW_YSIZE)>>1,
        	            MEMORY_WINDOW_XSIZE,
            	        MEMORY_WINDOW_YSIZE);
						
		WinPrintCenter(win, MEMORY_WINDOW_YSIZE/2, "running at %04x", start);
		WinPaint(win);
		jump(start);
		WinUnpaint(win, screen);
		WinFree(win);
		return ERR_NO_ERR;
	}
	return start;
}

static int read_dec_header(FILE *f, int *start, 
					int *end, int *entry,
					int *number_of_entries)
{
	return (fscanf(f, "%d,%d,%d,%d\n", start, end, entry, number_of_entries) == 4);
}

static int read_dec_data(FILE *f, int start, int end,
						 int number_of_entries)
{
	int i;
	unsigned char *data;
	unsigned short checksum, calced_checksum;
	
	data = alloca(number_of_entries);
	
	while(start < end)
	{
		for(i=0; i < number_of_entries; ++i)
		{
			if(fscanf(f,"%hhd,", data+i) != 1)
			{
				return ERR_DEC_PARSE;
			}
		}
		if(fscanf(f,"%hd\n", &checksum) != 1)
		{
			return ERR_DEC_PARSE;
		}
		calced_checksum = 0;
		for(i=0; i < number_of_entries; ++i)
		{
			calced_checksum += (unsigned short)(data[i]);
		}
		if(calced_checksum != checksum)
		{
			return ERR_DEC_CHECKSUM;
		}
		else
        {
            for (i=0; i < number_of_entries; ++i)
            {
                set_byte(start++, data[i]);
            }
        }
	}
    update_mem();
	return ERR_NO_ERR;
}


static int play_game(EditScreen *screen,
                     tByte *line, int pos, int size, int *error_pos)
{
    char *filename;
    int ret;
    int start, end, entry, number_of_entries;
    FILE *f;

    filename = get_filename(line, &pos, size, error_pos);
    if (!filename)
    {
        return ERR_NO_FILENAME;
    }
    f = fopen(filename,"rb");
    if (!f)
    {
        return io_error(line, size, pos, error_pos);
    }
    if (!read_dec_header(f, &start, &end, &entry, &number_of_entries))
    {
        return ERR_DEC_PARSE;
    }
    ret = read_dec_data(f, start, end, number_of_entries);
    if(ret != ERR_NO_ERR)
    {
        return ret;
    }
    Win * win = WinAlloc((screen->cols - MEMORY_WINDOW_XSIZE)>>1,
                   (screen->lines - MEMORY_WINDOW_YSIZE)>>1,
                    MEMORY_WINDOW_XSIZE,
                    MEMORY_WINDOW_YSIZE);

    WinPrintCenter(win, MEMORY_WINDOW_YSIZE/2, "has to run at %04x", start);
    WinPaint(win);
//    jump(entry);
    sleep(2);
    WinUnpaint(win, screen);
    WinFree(win);
    return ERR_NO_ERR;
}

	
int make_command(EditScreen *screen, tByte *line, int size, int column, int *error_pos)
{
    int i;

    i = go_over_whitespace(line, 0, size);
    switch (line[i])
    {
    case '*':
        return ERR_NO_ERR;
        break;
    case 'm':
    case 'M':
        return do_memory_dump(screen, line, i+1, size, error_pos);
        break;
    case ':':
        return get_memory_dump(screen, line, i+1, size, error_pos);
        break;
    case 'd':
    case 'D':
        return do_disassembly(screen, line, i+1, size, error_pos);
        break;
    case ';':
        return get_disassembly(screen, line, i+1, size, column, error_pos);
        break;
    case 'q':
    case 'Q':
    case 'x':
    case 'X':
        screen->quit = TRUE;
        return 0;
        break;
    case 'r':
    case 'R':  /* reread */
        invalid_mem();
        return 0;
        break;
    case 's':
    case 'S':
        return save_file(screen, line, i+1, size, error_pos);
        break;
    case 'l':
    case 'L':
        return load_file(screen, line, i+1, size, error_pos);
        break;
	case 'j':
	case 'J':
	case 'g':
	case 'G':
		init_mem(); /* erase memory */
		return do_jump(screen, line, i+1, size, error_pos);
		break;
    case 'p':
    case 'P':
        return play_game(screen, line, i+1, size, error_pos);
        break;
    default:
        *error_pos = i;
        return -1;
    }
}

void make_upline(EditScreen *screen)
{
    int i;
    int adr,pos;
    int size_of_op;

    for (i=0; i < screen->lines; ++i)
    {
        switch (screen->chars[i][0])
        {
        case ':':
            pos = go_over_whitespace(screen->chars[i], 1, screen->cols);
            adr = get_hex(screen->chars[i], &pos, screen->cols, &pos, 4);
            if (adr >= 0)
            {
                print_memdump(screen, adr-SIZE_OF_MEMDUMP, 
                              read_mem(adr-SIZE_OF_MEMDUMP, SIZE_OF_MEMDUMP));
                update_line(screen, screen->line);
                i = screen->lines; /* end condition */
            }
            break;
        case ';':
            pos = go_over_whitespace(screen->chars[i], 1, screen->cols);
            adr = get_hex(screen->chars[i], &pos, screen->cols, &pos, 4);
            size_of_op = get_go_up(adr);            
            if (adr >= 0)
            {
                print_disassembly(screen, adr-size_of_op, 
                                  read_mem(adr-size_of_op, MAX_OF_OPCODE));
                update_line(screen, screen->line);
                i = screen->lines; /* end condition */
            }
            break;      
        default:
            break;
        }
    }
}

void make_downline(EditScreen *screen){
    int i;
    int adr,pos;
    int size_of_op;

    for (i=screen->lines-1; i >= 0; --i)
    {
        switch (screen->chars[i][0])
        {
        case ':':
            pos = go_over_whitespace(screen->chars[i], 1, screen->cols);
            adr = get_hex(screen->chars[i], &pos, screen->cols, &pos, 4);
            if (adr >= 0)
            {
                print_memdump(screen, adr+SIZE_OF_MEMDUMP, 
                              read_mem(adr+SIZE_OF_MEMDUMP, SIZE_OF_MEMDUMP));
                update_line(screen, screen->line);
                i = -1; /* end condition */
            }
            break;
        case ';':
            pos = go_over_whitespace(screen->chars[i], 1, screen->cols);
            adr = get_hex(screen->chars[i], &pos, screen->cols, &pos, 4);
            if (adr >= 0)
            {
                size_of_op = get_objcode_length(*(read_mem(adr,1)));
                print_disassembly(screen, adr+size_of_op, 
                                  read_mem(adr+size_of_op, MAX_OF_OPCODE));
                update_line(screen, screen->line);
                i = -1; /* end condition */
            }
            break;
        default:
            break;
        }
    }
}

int assemble(EditScreen *screen, tByte *line, int *pos, int *error_pos, tByte *mem, int *end, tWord addr)
{
    Opcode *opcode;
    int i;
    int val;
    int diff_rel_arg;

    *end = 0;

    if (*pos == screen->cols)
    {
        return ERR_NO_ERR;
    }
    opcode = get_opcode_by_index(0);
    for (i=0; i < 256;++i)
    {
        if (strcmp(opcode->mnemonic, "*") == 0)
        {
            ++opcode;
            continue;
        }
        if (strncmp(opcode->mnemonic, line + *pos, strlen(opcode->mnemonic)) == 0)
        {
            int pos_for_check =  *pos + strlen(opcode->mnemonic);
            int size_to_check = 2*(get_objcode_length(i)-1);
            if (!check_hex(line, pos_for_check, size_to_check))
            {
                ++opcode;
                continue;
            }
            pos_for_check += size_to_check;
            const char *postfix = arg_postfix(opcode->arg);
            
            if (strncmp(postfix, line + pos_for_check, strlen(postfix)) == 0)
            {
                *pos += strlen(opcode->mnemonic);
                break;
            }

#if 0            
            if ((opcode->arg != NO_ARG) || (*(line + *pos + strlen(opcode->mnemonic)) == ' '))
            {
                *pos += strlen(opcode->mnemonic);
                break;
            }
#endif
        }
        ++opcode;
    }
    if (i < 256)
    {
        *pos = go_over_whitespace(line,*pos, screen->cols); 
        switch (opcode->arg)
        {
        case WORD_ARG:
        case WORD_ARG_X:
        case WORD_ARG_Y:
        case WORD_ARG_IND:
            val = get_hex(line, pos, screen->cols,error_pos, 4);
            if (val < 0)
            {
                return val;
            }
            mem[*end] = i;
            ++(*end);
            mem[*end] = val & 0xff;
            ++(*end);
            mem[*end] = val >> 8;
            ++(*end);
            break;
        case BWORD_ARG:
            val = get_hex(line, pos, screen->cols,error_pos, 4);
            if (val < 0)
            {
                return val;
            }
            mem[*end] = i;
            ++(*end);
            mem[*end] = val >> 8;
            ++(*end);
            mem[*end] = val & 0xff;
            ++(*end);
            break;
        case BYTE_ARG:
        case BYTE_ARG_X:
        case BYTE_ARG_Y:
        case BYTE_IND_X:
        case BYTE_IND_Y:
            val = get_hex(line, pos, screen->cols,error_pos, 2);
            if (val < 0)
            {
                return val;
            }
            mem[*end] = i;
            ++(*end);
            mem[*end] = val;
            ++(*end);
            break;
        case REL_ARG_0:
            diff_rel_arg = 0;
            goto rel_arg;
        case REL_ARG_1:
            diff_rel_arg = 1;
            goto rel_arg;
        case REL_ARG_2:
            diff_rel_arg = 2;
            goto rel_arg;
rel_arg:
            val = get_hex(line, pos, screen->cols,error_pos, 4);
            if (val < 0)
            {
                return val;
            }
            val -= (addr + diff_rel_arg);
            if ((val < -0x80) || (val > 0x7f))
            {
                return ERR_NO_MNEMONIC;
            } 
            mem[*end] = i;
            ++(*end);
            mem[*end] = val & 0xff;
            ++(*end);
            break;
        default:
            mem[*end] = i;
            ++(*end);
            break;
        }
        return ERR_NO_ERR;
    }
    else
    {
        *error_pos = *pos;
        return ERR_NO_MNEMONIC;
    }
}




