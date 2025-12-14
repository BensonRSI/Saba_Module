	processor f8
	org	$800

        .byte $55,$aa

        li $00
        lr 7,a
        lr 8,a

big_loop:
        clr
        lr 3,a
palette_loop:
        lr a,7
        lr 1,a
        li 125
        lr 2,a
        pi plot
        
        li 126
        lr 2,a

        lr a,7
        sl 1
        sl 1
        lr 1,a
        pi plot
        
        lr a,3
        ai 1
        lr 3,a
        ci $40
        bnz palette_loop


        li 4
        lr 2,a
        clr
        lr 3,a
        lr a,7
        lr 1,a
loop:
        lr a,1
        ai $05
        lr 1,a
        pi plot
        lr a,2
        ai $01
        ci 106
        bnz l1
        clr
        lr 1,a

        lr a,3
        ai $01
        ci $40
        bnz l2
        jmp end_loop
l2:     lr 3,a

        li 4
l1:     lr 2,a
        jmp loop

end_loop:
        lr  a,7
        ai  $20
        lr  7,a
        ni  $40
        bnz big_loop
        lr  a,7
        ai $40
        lr  7,a        
        jmp big_loop



;---------------;
; Plot Function ;
;---------------;

; plot a single pixel on the screen
; uses three registers as "arguments", load correct data 
; in these registers before making a subroutine call (pi).
; r1 = color
;------------------------
; Valid colors
;------------------------
; green	= $00 (%00000000)
; red	= $40 (%01000000)
; blue	= $80 (%10000000)
; bkg	= $C0 (%11000000)
;------------------------
; r2 = x (0-127)
; r3 = y (0-63)

plot:
	; set the color on port 1 using r1
	lr	A, 1
	outs	1

	; set the column using r2
	lr	A, 2
	com				; x-coordinate needs to be inverted before it's stored
	outs	4			; loaded to port 4

	; set the row using r3
	lr	A, 3
	com				; y-coordinate needs to be inverted before it's stored
;	ni	%00111111		; optional code to mute soundbits
	outs	5			; loaded to port 5

	; transfer data to the screen memory
	lis	6
	sl	4
	outs	0
	sl	1
	outs	0

	; delay until it's fully updated
	lis	6
plot.delay:	
	ai	$ff
	bnz	plot.delay

	pop							; return from the subroutine

