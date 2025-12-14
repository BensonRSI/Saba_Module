EXE = test

all:	$(EXE).bin

$(EXE).bin:	*.asm
	dasm $(EXE).asm -f3 -o$@

go:	$(EXE).bin
	mame -d channelf -cartridge $(EXE).bin -w -effect sharp -r 640x480 -ka

