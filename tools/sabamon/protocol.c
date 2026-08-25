#include "serial.h"
#include "protocol.h"
#include <string.h>
#include <stdio.h>

void get_mem(tWord adr, tByte length, tByte *ptr)
{
	tByte buffer[256];
	buffer[0] = 'G';
	buffer[1] = (tByte)(adr & 0xff);
	buffer[2] = (tByte)(adr >> 8);
	buffer[3] = length;
	buffer[4] = 'U';
	write_serial(buffer,5);
	read_serial(ptr, (int)length);
}

void set_mem(tWord adr, tByte length, tByte *ptr)
{
	tByte buffer[256+5];
	buffer[0] = 'S';
	buffer[1] = (tByte)(adr & 0xff);
	buffer[2] = (tByte)(adr >> 8);
	buffer[3] = length;
	buffer[4] = 'U';
	memcpy(buffer+5, ptr, length);
	write_serial(buffer,((int)length)+ 5);
}

void jump(tWord adr)
{
	tByte buffer[4];
	buffer[0] = 'J';
	buffer[1] = (tByte)(adr & 0xff);
	buffer[2] = (tByte)(adr >> 8);
	buffer[3] = 'U';
	write_serial(buffer,5);	
}

