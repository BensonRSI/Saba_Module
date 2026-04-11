all:	saba_gfx_convert

saba_gfx_convert:	convert.c
	gcc -g `sdl-config --cflags` -o $@ convert.c  `sdl-config --libs` -lSDL -lSDL_image

win:		saba_gfx_convert.exe
saba_gfx_convert.exe:	convert.c
	i586-mingw32msvc-gcc -g `/opt/win32/SDL-1.2.15/bin/sdl-config --cflags` -o $@ convert.c  `/opt/win32/SDL-1.2.15/bin/sdl-config --libs` -lSDL -lSDL_image

clean:
	rm -f saba_gfx_convert *.exe
