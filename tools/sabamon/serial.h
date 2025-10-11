#ifndef serial_h_314
#define serial_h_314

#define BAUDRATE B19200
#define MODEMDEVICE "/dev/ttyUSB1"

extern void open_serial();
extern void close_serial();
extern void write_serial(unsigned char *ptr, int size);
extern void read_serial(unsigned char *ptr, int size);

#endif
