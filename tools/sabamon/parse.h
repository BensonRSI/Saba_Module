#ifndef __parse_h_3145_
#define __parse_h_3145_

#include "c_types.h"

#define ERR_NO_ERR 0
#define ERR_WRONG_CHAR -1
#define ERR_NO_NUMBER -2
#define ERR_NO_ARGS -3
#define ERR_NO_MNEMONIC -4
#define ERR_NO_FILENAME -5
#define ERR_WRONG_ARGS -6
#define ERR_IO -7
#define ERR_DEC_PARSE -8
#define ERR_DEC_CHECKSUM -9


extern int make_command(EditScreen *screen,tByte *line, int size, int column, int *error_pos);
extern void make_upline(EditScreen *screen);
extern void make_downline(EditScreen *screen);
extern int assemble(EditScreen *screen, tByte *line,
                    int *pos, int *error_pos, tByte *mem, int *end, tWord addr);

#endif
