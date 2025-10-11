#ifndef __c_types_h_315564
#define __c_types_h_315564

typedef unsigned short tWord;
typedef unsigned char tByte;

#ifndef TRUE
    #define TRUE (0==0)
#endif
#ifndef FALSE
    #define FALSE (0!=0)
#endif

#ifndef MAX
    #define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif

#ifndef MIN
    #define MIN(a,b) ((a) > (b) ? (b) : (a))
#endif

#ifndef NULL
    #define NULL (void *)0
#endif

#endif
