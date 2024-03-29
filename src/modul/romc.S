    .section .text
    .align 2
        #ifndef SIMULATOR
        .cpu cortex-m0
        #endif
        .thumb
        .global romc
        .global memory
        .global memory_map
        

        #ifndef SIMULATOR
        SIO_BASE = 0xd0000000
        #else
        .global SIO_BASE
        #endif 
        GPIO_IN = 0x004
        GPIO_OUT = 0x010 
        GPIO_SET = 0x014 
        GPIO_CLR = 0x018 
        GPIO_XOR = 0x01c 
        GPIO_OE_OUT = 0x020 
        GPIO_OE_SET = 0x024 
        GPIO_OE_CLR = 0x028 
        GPIO_OE_XOR = 0x02c 
        
        DB_MASK = 0xff
        ROMC_SHIFT = 8
        ROMC_MASK = 0x1f
        WRITE_SHIFT = 13
        PHI_SHIFT = 14
        
        //register used for special purposes
        
        SIO_REG .req r2		// holds base of rapberry pi pico SIO
        JUMP_TABLE .req r12	// holds base of "jumptable"
        MEMORY .req r11		// holds base to "memory"
        MEMORY_MAP .req r10	// holds base to "memory_map"
        WORD_MASK .req r7	// holds 0xffff
        PC0 .req r6		// PC0 reg
        PC1 .req r5		// PC1 reg
        DC0 .req r4		// DC0 reg
        DC1 .req r3		// DC1 reg

	// check if memory given in "check_reg" is in range
	// param : check_reg : register to check
	//         scratch   : register that is poluted by internal use
	// return : EQ -> not ranged, NE -> in range, MI -> writable

        .macro check_map check_reg, scratch
        mov \scratch,\check_reg
        lsr \scratch,#10
        add \scratch,MEMORY_MAP
        ldrb \scratch,[\scratch]
        orr \scratch,\scratch
        .endm

	// wait for falling edge macro

        .macro wait_for_write_falling

        #ifndef SIMULATOR
        
        // real code : wait for falling edge
        
        mov r1,#1
        lsl r1,#WRITE_SHIFT
1:
        ldr r0,[SIO_REG, #GPIO_IN]
        and r0,r1
        beq 1b
        // disable write on dbus
        mov r0,#DB_MASK
        str r0,[SIO_REG, #GPIO_OE_SET]
2:
        ldr r0,[SIO_REG, #GPIO_IN]
        and r0,r1
        bne 2b
        //sleep 50ns ?
        
        #else
        
        //simulator code, read next test pattern

        mov r0,lr
        ldrh r1,[r0]
        mov r0,#2
        add lr,r0
        str r1,[SIO_REG, #GPIO_IN]
        #endif
        .endm

	// write register to data bus
	// param : reg : register value to write, <= 0xff
	//         scratch   : register that is poluted by internal use

        .macro write_data_bus reg, scratch
        mov  \scratch,#DB_MASK
        str  \scratch,[SIO_REG, #GPIO_CLR]
        str  \reg,[SIO_REG, #GPIO_SET]
        str  \scratch,[SIO_REG, #GPIO_OE_CLR]
        .endm

	// write register to data bus
	// param : reg : register value to write, any value, only lowest byte written
	//         scratch   : register that is poluted by internal use

        .macro write_data_bus_masked reg, scratch
        mov  \scratch,#DB_MASK
        str  \scratch,[SIO_REG, #GPIO_CLR]
        and  \scratch,\reg
        str  \scratch,[SIO_REG, #GPIO_SET]
        mov  \scratch,#DB_MASK
        str  \scratch,[SIO_REG, #GPIO_OE_CLR]
        .endm

        #ifdef SIMULATOR
	//wrong, but used because ".cpu cortex-m0" is not handled by emulator
        .macro sxtb reg0,reg1
        mov \reg0,\reg1
        mov \reg1,#0xff
        and \reg0,\reg1        
        .endm
        #endif
       
.global romc
romc:
	// init for interpreting ROMC
.thumb_func 
        ldr SIO_REG,=SIO_BASE
        ldr r0, =jumptable
        mov JUMP_TABLE,r0
        ldr r0, =memory
        mov MEMORY,r0
        ldr r0, =memory_map
        mov MEMORY_MAP,r0
        ldr WORD_MASK, =0xffff
        
        #ifndef SIMULATOR
        mov PC0,#0
        mov PC1,#0
        mov DC0,#0
        mov DC1,#0
        #else
        ldr PC0, =0x0800
        ldr PC1, =0x1800
        ldr DC0, =0x2800
        ldr DC1, =0x1234
        #endif
        b   romcloop

romc1e:
	//11110 	1E 	L 	The device whose address space includes the contents of PC0 must place the low order byte of PC0
	//				onto the data bus. 
        check_map PC0,r1
        beq romcloop
        write_data_bus_masked PC0,r1
        b romcloop

romc1f:
	//11111 	1F 	L 	The device whose address space includes the contents of PC0 must place the high order byte of PC0
	//onto the data bus. 
        check_map PC0,r1
        beq romcloop
        mov r0,PC0
        lsr r0,#8
        write_data_bus r0,r1
        b romcloop
        
	// endless loop for interpreting ROMC
romcloop:
        wait_for_write_falling
        ldr r0,[SIO_REG, #GPIO_IN]
        lsr r0,#ROMC_SHIFT
        mov r1,#ROMC_MASK
        and r0,r1
        lsl r0,#2
        add r0,JUMP_TABLE
        ldr r1,[r0]
        mov pc,r1
romc00:
	//00000 	00 	S,L 	Instruction Fetch. The device whose address space includes the contents of the PC0 register must
	//                              place on the data bus the op code addressed by PC0; then all devices increment the contents of PC0. 
romc03:
	// 00011 	03 	L,S 	Similar to 00, except that it is used for Immediate Operand fetches (using PC0) instead of
	//				instruction fetches. 
        check_map PC0,r1
        beq romc00_cont
        
        mov r1, MEMORY
        ldrb r0, [r1, PC0]
        write_data_bus r0,r1
romc00_cont:
        add PC0,#1
        and PC0,WORD_MASK
        b romcloop
        
romc01:
	//00001 	01 	L 	The device whose address space includes the contents of the PC0 register must place on the data bus
	//				the contents of the memory location addressed by PC0; then all devices add the 8-bit value on the data
	//				bus, as as signed binary number, to PC0. 
        check_map PC0,r1
        beq romc01_cont
        mov r1, MEMORY
        ldsb r0,[r1, PC0]
        write_data_bus_masked r0,r1
romc01_cont:
        add PC0,r0
        and PC0,WORD_MASK
        b romcloop

romc02:
	//00010 	02 	L 	The device whose DC0 addresses a memory word within the address space of that device must
	//				place on the data bus the contents of the memory location addressed by DC0; then all devices
	//				increment DC0. 
        check_map DC0,r1
        beq romc02_cont
        mov r1, MEMORY
        ldrb r0,[r1, DC0]
        write_data_bus r0,r1
romc02_cont:
        add DC0,#1
        and DC0,WORD_MASK
        b romcloop

romc04:
	//00100 	04 	S 	Copy the contents of PC1 into PC0. 
        mov PC0,PC1
        b romcloop
        
romc05:
	//00101 	05 	L 	Store the data bus contents into the memory location pointed to by DC0; increment DC0. 
        check_map DC0,r1
        bpl romcloop
        ldr r1,[SIO_REG, #GPIO_IN]
        mov r0,MEMORY
        strb r1,[r0, DC0]
        b romcloop
       
romc06:
	//00110 	06 	L 	Place the high order byte of DC0 on the data bus. 
        check_map DC0,r1
        beq romcloop
        mov r0,DC0
        lsr r0,#8
        write_data_bus r0,r1
        b romcloop
        
romc07:
	//00111 	07 	L 	Place the high order byte of PC1 on the data bus. 
        check_map PC1,r1
        beq romcloop
        mov r0,PC1
        lsr r0,#8
        write_data_bus r0,r1
        b romcloop

romc08:
	//01000 	08 	L 	All devices copy the contents of PC0 into PC1. The CPU outputs zero on the data bus in this ROMC
	//				state. Load the data bus into both halves of PC0, thus clearing the register. 
        mov pc1,pc0
        mov pc0,#0
        b romcloop
        
romc09:
	//01001 	09 	L 	The device whose address space includes the contents of the DC0 register must place the low order
	//				byte of DC0 onto the data bus. 
        check_map DC0,r1
        beq romcloop
        write_data_bus_masked DC0,r1
        b romcloop
        
romc0a:
	//01010 	0A 	L 	All devices add the 8-bit value on the data bus, treated as a signed binary number, to the data counter. 
        mov r1,#0
        ldr r1,[SIO_REG, #GPIO_IN]
        sxtb r0,r1
        add DC0,r0
        and DC0,WORD_MASK
        b romcloop
       
romc0b:
	//01011 	0B 	L 	The device whose address space includes the value in PC1 must place the low order byte of PC1 on
	//				the data bus. 
        check_map PC1,r1
        beq romc0b_cont
        write_data_bus_masked PC1,r1
romc0b_cont:
        b romcloop

romc0c:
	//01100 	0C 	L 	The device whose address space includes the contents of the PC0 register must place the contents of
	//				the memory word addressed by PC0 onto the data bus; then all devices move the value that has just
	//				been placed on the data bus into the low order byte of PC0. 
        check_map PC0,r1
        beq romc0c_cont
        mov r1,MEMORY
        ldrb r0,[r1, PC0]
        write_data_bus r0,r1
	b romc0c_cont_read
romc0c_cont:
        mov r1,#0xff
        ldr r0,[SIO_REG, #GPIO_IN]
        and r0,r1
romc0c_cont_read:
        lsl r1,#8
        and PC0, r1
        orr PC0, r0
        b romcloop

romc0d:
	//01101 	0D 	S 	All devices store in PC1 the current contents of PC0, incremented by 1; PC0 is unaltered. 
        mov PC1,PC0
        add PC1,#1
        and PC1,WORD_MASK
        b romcloop

romc0e:
	//01110 	0E 	L 	The device whose address space includes the contents of PC0 must place the contents of the word
	//				adressed by PC0 onto the data bus. The value on the data bus is then moved to the low order byte
	//				of DC0 by all devices 
        check_map PC0,r1
        beq romc0e_cont
        mov r1,MEMORY
        ldrb r0,[r1, PC0]
        write_data_bus r0,r1
	b romc0e_cont_read
romc0e_cont:
        mov r1,#0xff
        ldr r0,[SIO_REG, #GPIO_IN]
        and r0,r1
romc0e_cont_read:
        ldr r1,=0xff00
        and DC0, r1
        orr DC0, r0
romc0f: 
	//01111 	0F 	L 	The interrupting device with highest priority must place the low order byte of the interrupt vector on the
	//				data bus. All devices must copy the contents of PC0 into PC1. All devices must move the contents of
	//				the data bus into the low order byte of PC0.
	// *not implemented yet*
romc10: 
	//10000 	10 	L 	Inhibit any modification to the interrupt priority logic. 
        b romcloop

romc11:
	//10001 	11 	L 	The device whose memory space includes the contents of PC0 must place the contents of the
	//				addressed memory word onto the data bus. All devices must then move the contents of the data bus
	//				to the upper byte of DC0. 
        check_map PC0,r1
        beq romc11_cont
        mov r1,MEMORY
        ldrb r0,[r1, PC0]
        write_data_bus r0,r1
	b romc11_cont_read
romc11_cont:
        mov r1,#0xff
        ldr r0,[SIO_REG, #GPIO_IN]
        and r0,r1
romc11_cont_read:
        lsl r0,#8
        and DC0, r1
        orr DC0, r0
        b romcloop

romc12:
	//10010 	12 	L 	All devices copy the contents of PC0 into PC1. All devices then move the contents of the data bus into
	//				the low order byte of PC0. 
        mov PC1,PC0
        and PC0,PC0, r1
        ldr r0,[SIO_REG, #GPIO_IN]
        mov r1,#0xff
        and r0,r1
        lsl r1,#8
        and PC0,PC0, r1
        orr PC0,PC0, r0
romc13:
	//10011 	13 	L 	The interrupting device with highest priority must move the high order half of the interrupt vector onto
	//				data bus. All devices must move the contents of the data bus into the high order byte of PC0. The
	//				interrupting device resets its interupt circuitry (so that it is no longer requesting CPU servicing and can
	//				respond to another interrupt). 
	// *not implemented yet*
        b romcloop
        

romc14:
	//10100 	14 	L 	All devices move the contents of the data bus into the high order byte of PC0. 
        ldr r0,[SIO_REG, #GPIO_IN]
        mov r1,#0xff
        and r0,r1
        and PC0,r1
        lsl r0,#8
        orr PC0,r0
        b romcloop

romc15:
	//10101 	15 	L 	All devices move the contents of the data bus into the high order byte of PC1.
        ldr r0,[SIO_REG, #GPIO_IN]
        mov r1,#0xff
        and r0,r1
        and PC1,r1
        lsl r0,#8
        orr PC1,r0
        b romcloop

romc16:
	//10110 	16 	L 	All devices move the contents of the data bus into the high order byte of DC0. 
        ldr r0,[SIO_REG, #GPIO_IN]
        mov r1,#0xff
        and r0,r1
        and DC0,r1
        lsl r0,#8
        orr DC0,r0
        b romcloop

romc17:
	//10111 	17 	L 	All devices move the contents of the data bus into the low order byte of PC0. 
        ldr r0,[SIO_REG, #GPIO_IN]
        mov r1,#0xff
        and r0,r1
        lsl r1,#8
        and PC0,r1
        orr PC0,r0
        b romcloop

romc18:
	//11000 	18 	L 	All devices move the contents of the data bus into the low order byte of PC1. 
        ldr r0,[SIO_REG, #GPIO_IN]
        mov r1,#0xff
        and r0,r1
        lsl r1,#8
        and PC1,r1
        orr PC1,r0
        b romcloop

romc19:
	//11001 	19 	L 	All devices move the contents of the data bus into the low order byte of DC0. 
        ldr r0,[SIO_REG, #GPIO_IN]
        mov r1,#0xff
        and r0,r1
        lsl r1,#8
        and DC0,r1
        orr DC0,r0
romc1a:
	//11010 	1A 	L 	During the prior cycle, an I/O port timer or interrupt control register was addressed; the device
	//containing the addressed port must move the current contents of the data bus into the addressed port. 
	// *not implemented yet*
romc1b:
	//11011 	1B 	L 	During the prior cycle, the data bus specified the address of an I/O port. The device containing the
	//				addressed I/O port must place the contents of the I/O port on the data bus. (Note that the contents of
	//				timer and interrupt control registers cannot be read back onto the data bus.) 
romc1c:
	//11100 	1C 	L or S 	None. 
        b romcloop

romc1d:
	//11101 	1D 	S 	Devices with DC0 and DC1 registers must switch registers. Devices without a DC1 register perform no
	//			operation. 
        mov r0,DC0
        mov DC0,DC1
        mov DC1,r0
        b romcloop


        .align 4
	// table used for jumps
jumptable:
        .word romc00, romc01, romc02, romc03
        .word romc04, romc05, romc06, romc07
        .word romc08, romc09, romc0a, romc0b
        .word romc0c, romc0d, romc0e, romc0f
        .word romc10, romc11, romc12, romc13
        .word romc14, romc15, romc16, romc17
        .word romc18, romc19, romc1a, romc1b
        .word romc1c, romc1d, romc1e, romc1f
