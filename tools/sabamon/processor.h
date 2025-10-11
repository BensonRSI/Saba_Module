#ifndef __processor_h_351445_
#define __processor_h_351445_

#include "c_types.h"

#undef PROZ_8085
#define PROZ_F3850

#define MAX_OF_OPCODE 3

typedef enum
{
	NO_ARG = 0,
	REL_ARG_0, /* add offset before opcode */
	REL_ARG_2, /* add offset after argument */
	REL_ARG_1, /* add offset after opcode */
	BYTE_ARG,
	WORD_ARG,
        BWORD_ARG   /* big endian */
} Argument;

typedef struct
{
	char *mnemonic;
	Argument arg;
	int exitflag; /*for a delimeter after command */
        char *comment;
} Opcode;

extern Opcode *get_opcode_by_index(tByte index);
extern int get_objcode_length(tByte opcode);
extern int get_go_up(tWord adr);


#endif
