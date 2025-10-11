#include <stdio.h>
#include "serial.h"
#include "protocol.h"
#include "editscreen.h"
#include "target_memory.h"

int main()
{
//	unsigned char buf[256];
//	int i,j;


#ifndef NO_SERIAL
	open_serial();
#endif
	init_mem();
//	read_mem(0x400, 0x200);
//	read_mem(0x300, 0x400);
//	read_mem(0x0, 0xffff);
//	for(i=0x1000; i < 0x2000; ++i)
//	{
//		set_byte(i,1);
//	}
//	update_mem();
//	exit(0);
	edit_screen();
//	for(j=0; j < 25600; j++)
//	{
//		get_mem(0x4000 + (j & 0xfff) , 4, buf);
//		set_mem(0x2000+j*2, 16, buf);
//		for(i=0; i < 4; ++i)
//		{
//			printf("%02x ",buf[i]);
//		}
//		printf("\n");
//	}
//	set_mem(0x1234, 16,buf);

	close_serial();
	return 0;
}
