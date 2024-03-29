    .section .text
        .thumb
        .global romc
        .global _start

_start:
        ldr r0,=memory
        ldr r1,=0x10000
        ldr r2,=0x45d3ab57
        mov r3,#0x4
        ldr r5,=0x87954db6
filloop:
        str r2,[r0]
        lsr r2,#1
        bcc no_xor
        eor r2,r5
no_xor:
        add r0,r3
        sub r1,r3
        bne filloop
        
doe:
        b doe
        ldr r0,=romc+1
        mov lr,pc
        bx r0

        .hword 0x00af
        .hword 0x01d3
        .hword 0x02a4
        .hword 0x0356
        .hword 0x0473
        .hword 0x0587
        .hword 0x0634
        .hword 0x0763

        .hword 0x08af
        .hword 0x09d3
        .hword 0x0aa4
        .hword 0x0b56
        .hword 0x0c73
        .hword 0x0d87
        .hword 0x0e34
        .hword 0x0f63

        .hword 0x10af
        .hword 0x11d3
        .hword 0x12a4
        .hword 0x1356
        .hword 0x1473
        .hword 0x1587
        .hword 0x1634
        .hword 0x1763

        .hword 0x18af
        .hword 0x19d3
        .hword 0x1aa4
        .hword 0x1b56
        .hword 0x1c73
        .hword 0x1d87
        .hword 0x1e34
        .hword 0x1f63
