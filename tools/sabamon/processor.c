#include "processor.h"
#include "target_memory.h"

extern Opcode *get_8085_opcode_by_index(tByte index);
extern Opcode *get_f3850_opcode_by_index(tByte index);

/* function pointer for a specific processor */
#ifdef PROZ_8085
Opcode *(*get_opcode_by_index_func)(tByte index) = get_8085_opcode_by_index;
#endif

#ifdef PROZ_F3850
Opcode *(*get_opcode_by_index_func)(tByte index) = get_f3850_opcode_by_index;
#endif


Opcode *get_opcode_by_index(tByte index)
{
	return get_opcode_by_index_func(index); 
}

int get_objcode_length(tByte opcode)
{
	switch(get_opcode_by_index(opcode)->arg)
	{
		case BYTE_ARG:
		case REL_ARG_0:
		case REL_ARG_1:
		case REL_ARG_2:
			return 2;
		break;
                case BWORD_ARG:
		case WORD_ARG:
			return 3;
		break;
		default:
			return 1;
		break;
	}
}

int get_go_up(tWord adr)
{
	tWord oldtmp;
	unsigned int tmp = adr - 16;
	while(tmp < adr)
	{
		oldtmp = tmp;
		tmp += get_objcode_length(*(read_mem(tmp, MAX_OF_OPCODE)));
	}
	if(tmp == adr)
	{
		return tmp-oldtmp;
	}
	else
	{
		return 1;
	}
}

