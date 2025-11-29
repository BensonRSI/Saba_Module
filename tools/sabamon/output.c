#include <string.h>
#include "output.h"
#include "parse.h"
#include "processor.h"
#include "target_memory.h"

const char *arg_postfix(Argument arg)
{
    switch(arg)
    {
        case WORD_ARG_IND:
            return ")";
        case BYTE_IND_X:
            return ",x)";
        case BYTE_IND_Y:
            return "),y";
        case WORD_ARG_X:
        case BYTE_ARG_X:
            return ",x";
        case WORD_ARG_Y:
        case BYTE_ARG_Y:
            return ",y";
        default:
            return " ";
    }
}

static void print_nibble(tByte *ptr, int value)
{
	if(value > 9)
	{
		*ptr = value + 'A'-10;
	}
	else
	{
		*ptr = value + '0';
	}
}

void print_hex_value(tByte *ptr, int value, int digits)
{
	ptr+=(digits-1);
	while(digits > 0)
	{
		print_nibble(ptr, value & 0xf);
		value = value >> 4;
		--digits;
		--ptr;
	}
}

void print_ascii(tByte *ptr, int value)
{
	if(value >= 0x20 && value <= 0x7e)
	{
		*ptr = (tByte)value;
	}
	else
	{
		*ptr = '.';
	}
}

void print_memdump(EditScreen *screen, tWord adr, tByte *mem)
{
	int i;
	screen->chars[screen->line][0] = ':';
	print_hex_value(screen->chars[screen->line]+1, adr, 4);
	screen->chars[screen->line][5] = ' ';
	for(i=0; i < SIZE_OF_MEMDUMP; ++i)
	{
		print_hex_value(screen->chars[screen->line]+6 +3*i, (int)mem[i], 2);
		screen->chars[screen->line][8+3*i] = ' ';		
	}
	screen->chars[screen->line][6+3*SIZE_OF_MEMDUMP] ='\"';
	for(i=0; i < SIZE_OF_MEMDUMP; ++i)
	{
		print_ascii(screen->chars[screen->line]+7+3*SIZE_OF_MEMDUMP+i, (int)mem[i]);
	}
	screen->chars[screen->line][7+SIZE_OF_MEMDUMP+3*SIZE_OF_MEMDUMP] ='\"';
}

void print_disassembly(EditScreen *screen, tWord adr, tByte *mem)
{
	int i;
	int obcode_length;
	int pos;
	Opcode *opcode;
	const char *str;
	
	screen->chars[screen->line][0] = ';';
	print_hex_value(screen->chars[screen->line]+1, adr, 4);
	screen->chars[screen->line][5] = ' ';
	
	obcode_length = get_objcode_length(*mem);
	for(i=0; i < obcode_length; ++i)
	{
		print_hex_value(screen->chars[screen->line]+6 +3*i, (int)mem[i], 2);
		screen->chars[screen->line][8+3*i] = ' ';		
	}
	screen->chars[screen->line][6+3*MAX_OF_OPCODE] = ' ';
	pos = 7+3*MAX_OF_OPCODE;
	opcode = get_opcode_by_index(*mem);
	str = opcode->mnemonic;
	if(strcmp(str, "*") == 0)
	{
		str ="???";
	}
	while(*str != 0)
	{
		screen->chars[screen->line][pos++] = *str++;
	}
        tWord hexArg = 0;
        int hexLen = 0;
	switch(opcode->arg)
	{
		case WORD_ARG :
                case WORD_ARG_X:
                case WORD_ARG_Y:
                case WORD_ARG_IND:
                        hexArg = mem[1] + 256 * mem[2];
                        hexLen = 4;
			break;
		case BWORD_ARG :
		        hexArg = mem[2] + 256 * mem[1];
                        hexLen = 4;
			break;
		case BYTE_ARG :	
                case BYTE_ARG_X:
                case BYTE_ARG_Y:
                case BYTE_IND_X:
                case BYTE_IND_Y:
                        hexArg = mem[1];
                        hexLen = 2;
 			break;
                case REL_ARG_0 :
                        hexArg = adr + ((signed char)mem[1]);
                        hexLen = 4;
                        break; 
                case REL_ARG_1 :
                        hexArg = adr + 1 + ((signed char)mem[1]);
                        hexLen = 4;
                        break; 
                case REL_ARG_2 :
                        hexArg = adr + 2 + ((signed char)mem[1]);
                        hexLen = 4;
                        break; 
		default:
			break;
	}
        if (hexLen > 0)
        {
	    print_hex_value(screen->chars[screen->line]+pos, hexArg, hexLen);
            str = arg_postfix(opcode->arg);
            pos += hexLen; 
	    while(*str != 0)
	    {
		screen->chars[screen->line][pos++] = *str++;
	    }
        }
        if (opcode->comment)
        {
            str = opcode->comment;
            pos = 20+3*MAX_OF_OPCODE;
            screen->chars[screen->line][pos++] = ';';
            screen->chars[screen->line][pos++] = ' ';
            while(*str != 0)
	    {
                if (*str == '%')    /* special char for arg in comment */
                {
                    print_hex_value(screen->chars[screen->line]+pos, hexArg, hexLen);
                    pos += hexLen;
                    ++str;
                }
                else if (*str == '\"')  /* special char for highbyte(arg) in comment */
                {
                    print_hex_value(screen->chars[screen->line]+pos, hexArg >> 8, 2);
                    pos += 2;
                    ++str;
                }
                else
                {
		    screen->chars[screen->line][pos++] = *str++;
                }
	    }
        }
        if (opcode->exitflag)
        {
            int pos = COLS-1;
            while(screen->chars[screen->line][pos-1] == ' ')
            {
                screen->chars[screen->line][pos] = '_';
                --pos;
            }
        }
}
