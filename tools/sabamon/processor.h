#ifndef __processor_h_351445_
#define __processor_h_351445_

#include "c_types.h"

#undef PROZ_8085
#undef PROZ_6502
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
        BWORD_ARG,  /* big endian */
        WORD_ARG_X,
        WORD_ARG_Y,
        WORD_ARG_IND,
        BYTE_ARG_X,
        BYTE_ARG_Y,
        BYTE_IND_X,
        BYTE_IND_Y
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
extern const char *arg_postfix(Argument);

extern const char *change_processor(const char *proc);

#endif
