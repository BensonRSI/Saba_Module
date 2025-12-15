#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include "c64chars.h"

#define Y_SIZE 2

#define ZOOM

#ifdef ZOOM
    #define ZOOM_SIZE 2
#else
    #define ZOOM_SIZE 1
#endif

#define YCS 1
#define CHARSET_SIZE 0x1000

#define MAX(a,b) (((a) < (b)) ? (b) : (a))

static unsigned int width;
static unsigned int height;
static unsigned int pitch;
static uint32_t *pixels;
static SDL_PixelFormat *format;
static int y_pos_to_print;
static int x_pos_to_print;
static SDL_Surface *mainscreen;


typedef struct _boxes {
	int x;
	int y;
	int x2;
	int y2;
	int clash;
	struct _boxes *next;
} Boxes;

static Boxes *clashes = NULL;

#define KILL { char *a = NULL; *a = 0; }

#define INDEX_BACKGROUND_GREY  3
#define INDEX_BACKGROUND_BLACK  1
#define INDEX_BACKGROUND_GREEN  2
#define INDEX_BACKGROUND_BLUE  3
#define INDEX_FOREGROUND_WHITE  4
#define INDEX_FOREGROUND_RED  5
#define INDEX_FOREGROUND_GREEN  6
#define INDEX_FOREGROUND_BLUE  7

#define INDEX_FOREGROUND_START  4

static unsigned char palette_data[9][3] = {
	{0xe0,  0xe0,  0xe0},   // background = %11 light grey
	{0x10,  0x10,  0x10},   // background = %11 black
	{0x91,  0xff,  0xa6},   // background = %11 light green
	{0xce,  0xd0,  0xff},   // background = %11 light blue

	{0xfc,  0xfc,  0xfc},   // forground = %00, %01 or %10 white
	{0x02,  0xcc,  0x5d},   // forground = %01  green
	{0xff,  0x31,  0x53},   // forground = %00  red
	{0x4b,  0x3f,  0xf3},   // forground = %10  blue
	{255,  0,255}
};

static uint8_t palette_for_picture[4] = {
    0x80,               // lightgrey
    0x90,               // black
    0x00,               // light green
    0x10                // light blue
};

static const char *text_for_palette[4] = {
    "light grey",
    "black",
    "light green",
    "light blue"
};

static double y_u_v[9][3];

static void add_element(Boxes **box_list, int x, int y, int x2, int y2, int clash)
{
	Boxes *new_box;
	Boxes *current, *last;

	new_box = (Boxes *)malloc(sizeof(Boxes));
	new_box->x = x;
	new_box->y = y;
	new_box->x2 = x2;
	new_box->y2 = y2;
	new_box->clash = clash;
	new_box->next = NULL;
	
	current = *box_list;
	if (!current) {
		*box_list = new_box;
	}
	else {
		last = NULL;
		while(current) {
			if ((current->y >= y) && (current->x >= x)) {
				if (!last) {
					*box_list = new_box;
				} else {
					last->next = new_box;
				}
				new_box->next = current;
				return;
			}
			last = current;
			current = current->next;
		}
		last->next = new_box;
	}
}

static void add_clash(int x, int y, int clash) {
	add_element(&clashes, x, y, 0, 0, clash);
}

static void init_y_u_v(void)
{
	int i;
	double y,u,v;
	for(i=0; i < 9; ++i) {
		y = 0.299 * ((double)(palette_data[i][0])) / 255.0 
			+ 0.587 * ((double)(palette_data[i][1])) / 255.0
			+ 0.114 * ((double)(palette_data[i][2])) / 255.0;
		u = ((((double)(palette_data[i][2])) / 255.0) - y) * 0.493;
		v = ((((double)(palette_data[i][0])) / 255.0) - y) * 0.877;
		y_u_v[i][0] = y;
		y_u_v[i][1] = u;
		y_u_v[i][2] = v;
		//printf("%lf %lf %lf\n", y, u, v);
	}
}
static void set_pixel_rgb(SDL_Surface *pic, int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
	uint32_t data;
	uint32_t *pointer;
	if ((x < 0) || x >= pic->w) {
		return;
	}
	if ((y < 0) || y >= pic->h) {
		return;
	}
	data = ((uint32_t)r) << pic->format->Rshift;
	data |= ((uint32_t)g) << pic->format->Gshift;
	data |= ((uint32_t)b) << pic->format->Bshift;
	pointer = (uint32_t *)(((char *)pic->pixels) + y*pic->pitch) + x;
	*pointer = data;
}

static void set_pixel_rgb_2x2(SDL_Surface *pic, int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
	set_pixel_rgb(pic, x*2, y*2, r,g,b);
	set_pixel_rgb(pic, x*2+1, y*2, r,g,b);
	set_pixel_rgb(pic, x*2, y*2+1, r,g,b);
	set_pixel_rgb(pic, x*2+1, y*2+1, r,g,b);
}

static void print_char(SDL_Surface *pic, unsigned char ch)
{
	int i;
	int j;
	unsigned char c;
	unsigned char *ptr = charset_c64 + ((int)ch)*8;
	for(j=0; j < 8; ++j) {
		c = *ptr++;
		for(i=0; i < 8; ++i) {
			if (c & 0x80) {
				set_pixel_rgb(pic, x_pos_to_print+i,
					y_pos_to_print+j*Y_SIZE, 0xff, 0xff, 0xff);
			#if Y_SIZE > 1
				set_pixel_rgb(pic, x_pos_to_print+i,
					y_pos_to_print+j*Y_SIZE+1, 0xff, 0xff, 0xff);
			#endif
			}
			c <<=1;
		}
	}
	x_pos_to_print += 8;
}

static void gprintf(const char *format, ...)
{
	int i;
	char str[256];
	char c;
	va_list ap;
	va_start(ap, format);
	vsnprintf(str, 256, format, ap);
	va_end(ap);
	for(i=0; i < 256; ++i) {
		c = str[i];
		if (!c) {
			break;
		}
		if (c >= 'a' && c <= 'z') {
			c-= 'a'-1;
		} else  if (c == '\n') {
			continue;
		} else if (c == '@') {
			c = 0;
		}
		print_char(mainscreen, c); 
	}
	x_pos_to_print = 8;
	y_pos_to_print += 8*Y_SIZE;
}

static int get_index(unsigned char r, unsigned char g, unsigned char b)
{
	int i;
	double y_d;
	double u_d;
	double v_d;
	double min_difference = 1e+80;
	int min_index = -1;
	double difference;
	double y,u,v;
	double y_t,u_t,v_t;
	
	static int once = 0;
	y = 0.299 * ((double)r)/255.0 + 0.587 * ((double)g)/255.0 + 0.114 * ((double)b)/255.0;
	u = (((double)(b) / 255.0) - y) * 0.493;
	v = (((double)(r) / 255.0) - y) * 0.877;
	for(i=0; i < 9; ++i)
	{
		//if (! once) {
		//	printf("%d %lf %lf %lf <-> %lf %lf %lf\n",
		//		i, y,u,v,y_u_v[i][0], y_u_v[i][1], y_u_v[i][2]);
		//}
		y_d = (y - y_u_v[i][0]); 
		u_d = (u - y_u_v[i][1]); 
		v_d = (v - y_u_v[i][2]);
		difference = y_d *y_d + u_d * u_d + v_d * v_d;
		if (difference < min_difference) {
			min_difference = difference;
			min_index = i;
		}
	}
	once = 1;
	if (min_index < 0) {
		KILL;
	}
	return min_index;
}

static int get_pixel(int x, int y, int *r_ret, int *g_ret, int *b_ret)
{
	uint32_t *addr;
	uint32_t data;
	unsigned char r;
	unsigned char g;
	unsigned char b;
	if (x < 0) {
		KILL;
	}
	if (x > width) {
		KILL;
	}
	if (y < 0) {
		KILL;
	}
	if (y > height) {
		KILL;
	}
	addr = (uint32_t *)((char *)pixels + (y * pitch)) + x;
        if (x >= width)
        {
            return 8;
        }
	data = *addr;
	r = (unsigned char)((data & format->Rmask) >> (format->Rshift));
	g = (unsigned char)((data & format->Gmask) >> (format->Gshift));
	b = (unsigned char)((data & format->Bmask) >> (format->Bshift));
        if (r_ret)
        {
            *r_ret = r;
        }
        if (g_ret)
        {
            *g_ret = g;
        }
        if (b_ret)
        {
            *b_ret = b;
        }
	//printf("%08x %02x %02x %02x ,",data, r,g,b);
	return get_index(r,g,b);
}

static int find_background_color(int y)
{
    for(int i=0; i < width; ++i)
    {
        int index = get_pixel(i, y, NULL, NULL, NULL);
        if (index == INDEX_FOREGROUND_WHITE)
        {
            /* white pixel, only black as background is possible */
            return INDEX_BACKGROUND_BLACK;
        }
        if (index < INDEX_FOREGROUND_START)
        {
            return index;
        }
    }
    return -1;
}

static int color_bits(int background_index, int color_index)
{
    if (background_index != INDEX_BACKGROUND_BLACK)
    {
        switch(color_index)
        {
            case INDEX_FOREGROUND_RED:
                return 0x00;
            case INDEX_FOREGROUND_GREEN:
                return 0x01;
            case INDEX_FOREGROUND_BLUE:
                return 0x02;
            default:
                if (color_index == background_index)
                {
                    return 0x03;
                }
                else
                {
                    return -1;
                }
        }
    }
    else
    {
        switch(color_index)
        {
            case INDEX_FOREGROUND_WHITE:
                return 0x00;
            case INDEX_BACKGROUND_BLACK:
                return 0x03;
            default:
                return -1;
        }
    }
}

static int get_bitmask(int background_index, int x, int y)
{
    int index = get_pixel(x, y, NULL, NULL, NULL);
    return color_bits(background_index, index);
}

int calc_data_size()
{
    int line_bytes = 1 + (width + 3)/4;             /* 2 bits per pixel, 1 byte for palette */
    return 2 /* for x and y */ + line_bytes*height;    
}

static int convert(const char *filename, int asm_mode)
{
    FILE *f = NULL;
    uint8_t *array = NULL, *data = NULL;
    if (asm_mode)
    {
        f = fopen(filename, "w");
        fprintf(f,"        .byte $%02x, $%02x ; x size (%d) and y size (%d)\n", width, height, width, height);
    }
    else
    {
        array = (uint8_t *)malloc(calc_data_size());
        data = array;
        *data++ = width;
        *data++ = height;
    }
    for(int j=0; j < height; ++j)
    {
        int first_byte = 0;
        int error_found = -1;
        int background = find_background_color(j);
        if (asm_mode)
        {
            fprintf(f," .byte $%02x ;  \"%s\" palette for line %d , data following :\n .byte ",
                palette_for_picture[background], text_for_palette[background], j);
        }
        else {
            *data++ = palette_for_picture[background];
        }
        int shift = 6;
        int dat = 0;
        for(int i=0; i < width; ++i)
        {
            int c = get_bitmask(background, i, j);
            if (c < 0)
            {
                add_clash(i, j, 1);
                if (error_found < 0)
                {
                    error_found = i;
                }
                c = 3;  /* take background */
            }
            dat |= c << shift;
            //printf("%c", '0' + c);
            shift -= 2;
            if (shift < 0)
            {
                if (asm_mode)
                {
                    if (!first_byte)
                    {
                        fprintf(f, "$%02x", dat);
                        first_byte = 1;
                    }
                    else
                    {
                        fprintf(f, ",$%02x", dat);                        
                    }
                }
                else
                {
                    *data++ = dat;
                }
                dat = 0;
                shift = 6; 
            }
        }
        /* put rest bits into mask */
        if (shift != 6)
        {
            if (asm_mode)
            {
                if (!first_byte)
                {
                    fprintf(f, "$%02x\n", dat);
                    first_byte = 1;
                }
                else
                {
                    fprintf(f, ",$%02x\n", dat);                        
                }
            }
            else
            {
                *data++ = dat;
            }
        }
        if (error_found >= 0)
        {
            gprintf("colorclash in line %d, starting at xpos %d", j, error_found); 
        }
        //printf("\n");
    }
    //printf("%d %d\n", data - array, calc_data_size());
    if (asm_mode)
    {
        fclose(f);
    }
    else
    {
        if (filename)
        {
            f = fopen(filename, "wb");
            if (f)
            {
                fwrite(array, 1, data - array, f);
                fclose(f);
            }
        }
    }
}


static SDL_Surface *load_image(const char *filename)
{
	SDL_Surface *temp;
	SDL_Surface *return_val;
	temp = IMG_Load(filename);
	if (!temp) {
		return NULL;
	}
	return_val = SDL_DisplayFormatAlpha(temp);
	SDL_FreeSurface(temp);
	return return_val;
}

static Uint32 timer_callback(Uint32 interval, void *param)
{
	SDL_Event event;
	SDL_UserEvent userevent;
	userevent.type = SDL_USEREVENT;
	userevent.code = 0;
	userevent.data1 = NULL;
	userevent.data2 = NULL;
	event.type = SDL_USEREVENT;
	event.user = userevent;
	SDL_PushEvent(&event);
	return (interval);
}

static int clash_print = 1;

static void paint_box(Boxes *b)
{
	int xl,yl,xh,yh;
	int i;
	xl = b->x * ZOOM_SIZE;
	xh = b->x2 * ZOOM_SIZE + (YCS* ZOOM_SIZE)-1;
	yl = b->y * YCS * ZOOM_SIZE;
	yh = b->y2 * YCS * ZOOM_SIZE + (YCS* ZOOM_SIZE)-1;
	for(i=xl; i <= xh; ++i) {
		set_pixel_rgb(mainscreen, i, yl, 0xff,0xff,0xff);		
		set_pixel_rgb(mainscreen, i, yh, 0xff,0xff,0xff);		
	}
	for(i=yl; i <= yh; ++i) {
		set_pixel_rgb(mainscreen, xl, i, 0xff,0xff,0xff);		
		set_pixel_rgb(mainscreen, xh, i, 0xff,0xff,0xff);		
	}
}

static void paint_clashes(void)
{ 
	Boxes *cl;
	int x,y;

	cl = clashes;
	while(cl) {
            set_pixel_rgb(mainscreen, cl->x*ZOOM_SIZE, cl->y*ZOOM_SIZE, 0xff,0x00,0xff);
#if 0
		for(y=0; y < YCS; ++y) {
			for(x=0; x < 8; ++x) {
				switch (cl->clash) {
					case 1:
						set_pixel_rgb(mainscreen,
							(cl->x+x)*ZOOM_SIZE+1, (cl->y+y)*ZOOM_SIZE+1,
						 	0xff,0xff,0x00);
						break;
					case 2:
						set_pixel_rgb(mainscreen,
							(cl->x+x)*ZOOM_SIZE+1, (cl->y+y)*ZOOM_SIZE+1,
						 	0xff,0xff,0xff);
						break;
					case 3:
						set_pixel_rgb(mainscreen,
							(cl->x+x)*ZOOM_SIZE+1, (cl->y+y)*ZOOM_SIZE+1,
						 	0xff,0x00,0xff);
						break;
					case 4:
						set_pixel_rgb(mainscreen,
							(cl->x+x)*ZOOM_SIZE+1, (cl->y+y)*ZOOM_SIZE+1,
						 	0x00,0xff,0x00);
						break;
					default: break;
				}
			}
		}
#endif
		cl=cl->next;
	}
}

static void paintscreen(void)
{
	int index;
	int x,y;
        int r_orig,g_orig,b_orig;
	SDL_LockSurface(mainscreen);
	for(y = 0; y < height; ++y) {
		for(x = 0; x < width; ++x) {
			index = get_pixel(x, y, &r_orig, &g_orig, &b_orig);
			//printf("%01x", index);
#ifdef ZOOM
			set_pixel_rgb_2x2(mainscreen, x, y, 
				palette_data[index][0],
				palette_data[index][1],
				palette_data[index][2]); 
			set_pixel_rgb_2x2(mainscreen, x+width, y, 
				r_orig,
				g_orig,
				b_orig); 
#else
			set_pixel_rgb(mainscreen, x, y, 
				palette_data[index][0],
				palette_data[index][1],
				palette_data[index][2]); 
			set_pixel_rgb(mainscreen, x+width, y, 
				r_orig,
				g_orig,
				b_orig); 
#endif
		}
		//printf("\n");
	}
	clash_print = !clash_print;
	if (clash_print) {
		paint_clashes();
	}
	SDL_UnlockSurface(mainscreen);
	SDL_Flip(mainscreen);
}


int main(int argc, char **argv)
{
        char *level_filename = "level.asm";
	SDL_Surface *pic, *pic2;
	FILE *f;
	
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	init_y_u_v();
	mainscreen = SDL_SetVideoMode(640,480, 32, 
			SDL_DOUBLEBUF | SDL_HWSURFACE);
	if (argc < 2) {
		pic = load_image("Alien_Invasion-title.png");
	} else {
		pic = load_image(argv[1]);
	}
	if (!pic) {
		printf("load failed\n");
		return -1;
	}
	if (pic->format->BitsPerPixel < 15) {
		printf("wrong format\n");
		return -1;
	}

	SDL_FreeSurface(mainscreen);
        width = pic->w;
        height = pic->h;
        y_pos_to_print = height * ZOOM_SIZE;
        x_pos_to_print = 8;
        pixels = (uint32_t*)(pic->pixels);
        format = pic->format;
	pitch = pic->pitch;
	mainscreen = SDL_SetVideoMode(2 * width * ZOOM_SIZE, (height*ZOOM_SIZE)+192, 32, 
			SDL_DOUBLEBUF | SDL_HWSURFACE);

	//remove_markers(pic);
        char *filename = NULL;
        int asm_mode = 0;
        if (argc >= 3)
        {
            filename = argv[2];
            asm_mode = ((strstr(filename, ".asm") != NULL) || (strstr(filename, ".ASM") != NULL));
        }
	convert(filename, asm_mode);
        
	SDL_AddTimer(500, timer_callback, NULL);
	paintscreen();
	if (argc >= 7 ) {
		SDL_Quit();
		return 0;
	}
	for (;;) {
		SDL_Event event;
		if (SDL_WaitEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN) {
				exit(0);
			}
			if (event.type == SDL_USEREVENT) {
				paintscreen();
			}
		}
	}
	SDL_Quit();
	return 0;
}
