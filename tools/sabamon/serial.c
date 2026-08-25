#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "serial.h"

static struct termios s_oldtio;
static int fd = 0;

void open_serial(const char *device_name)
{
	struct termios newtio;

	fd = open(device_name, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0)
	{
		fprintf(stderr, "error in open modem\n");
		exit(-1);
	}
	if (!isatty(fd))
	{
		fprintf(stderr, "no tty\n");
		exit(-1);
	}
	memset(&s_oldtio, 0, sizeof(s_oldtio));
	if (tcgetattr(fd, &s_oldtio) < 0) /* get old settings */
	{
		fprintf(stderr, "tcgetattr %s %d\n", strerror(errno), errno);
		exit(-1);
	}
	atexit(close_serial);

	memcpy(&newtio, &s_oldtio, sizeof(newtio));
	newtio.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
						INLCR | PARMRK | INPCK | ISTRIP | IXON);
	newtio.c_oflag = 0; /* raw output */
	newtio.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
	newtio.c_cflag &= ~(CSIZE | PARENB);
	newtio.c_cflag |= CS8;

	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 100;

	if (cfsetispeed(&newtio, BAUDRATE) < 0 || cfsetospeed(&newtio, BAUDRATE) < 0)
	{
		fprintf(stderr, "cannot set speed\n");
		exit(-1);
	}
	if (tcsetattr(fd, TCSANOW, &newtio) < 0)
	{
		fprintf(stderr, "tcsetattr %s\n", strerror(errno));
		exit(-1);
	}
}

void close_serial()
{
	if (fd > 0)
	{
		tcflush(fd, TCIFLUSH);
		tcsetattr(fd, TCSANOW, &s_oldtio);
		close(fd);
		fd = 0;
	}
}

#if 0
static void read_nibble(unsigned char *byte)
{
	int readed = 0;
	while (readed <= 0)
	{
		readed = (int)(read(fd, byte, 1));
		if (readed < 0)
		{
			usleep(100);
		}
	}
}
#endif
void write_serial(unsigned char *ptr, int size)
{

	if (fd <= 0)
	{
		fprintf(stderr, "serial not initialized\n");
		exit(-1);
	}
#if 0	
	while (size > 0)
	{
	unsigned char nibble1, nibble2, writebyte;

		writebyte = (*ptr >> 4) + '0';
		if (write(fd, &writebyte, 1) != 1)
		{
			fprintf(stderr,"error on write\n");
			exit(-1);
		}
		writebyte = (*ptr & 0xf) + '0';
		if (write(fd, &writebyte, 1) != 1)
		{
			fprintf(stderr,"error on write\n");
			exit(-1);
		}
		read_nibble(&nibble1);
		read_nibble(&nibble2);
        if((((nibble1-'0') << 4) | (nibble2-'0')) != *ptr)
        {
            exit(-1);
        }
		--size;
		++ptr;
	}

#else
	if (write(fd, ptr, size) != size)
	{
		fprintf(stderr, "error on write\n");
		exit(-1);
	}
#endif

}

void read_serial(unsigned char *ptr, int size)
{
	
	if (fd <= 0)
	{
		fprintf(stderr, "serial not initialized\n");
		exit(-1);
	}
	while (size > 0)
	{
		unsigned long timeout = 0;
#if 0
		unsigned char low, high;
		read_nibble(&high);
		read_nibble(&low);
		*ptr++ = ((high - '0') << 4) + (low - '0');
		--size;
	}
#else
		int n = read(fd, ptr, 1);
		if (n <0)
		{	
			/*
			fprintf(stderr, "error on read %s %d n=%d\n", strerror(errno), errno, n);
			exit(-1);
				// let's ignore it for a while */
		}
		if (n <= 0)
		{
			if (timeout++ > 100)
			{
				fprintf(stderr, "timeout on read at size %d\n", size);
				exit(-1);
			}
			usleep(100);
			continue;
		}
		--size;
		++ptr;
	}
#endif
}
