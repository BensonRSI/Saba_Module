#ifndef serial_h_314
#define serial_h_314

#define BAUDRATE B115200
#define MODEMDEVICE "/dev/ttyACM0"

extern void open_serial(const char *device_name);
extern void close_serial();
extern void write_serial(unsigned char *ptr, int size);
extern void read_serial(unsigned char *ptr, int size);

#endif
