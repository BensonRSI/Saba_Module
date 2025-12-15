	processor f8
	org	$800

        .byte $55,$aa

endless:    
        dci logo
        pi paint_picture
        dci picture
        pi paint_picture
        dci picture2
        pi paint_picture
        jmp endless


paint_picture:
        lr k,pc1
        lm
        lr 4,a
        lm
        lr 5,a
        
        li 4        ;start y coordinate
        lr 3,a
big_loop:
        ;set the palette
        lr 1,a
        li 125
        lr 2,a
        lm
        lr 1,a
        pi plot
        lr a,1
        sl 1
        sl 1
        lr 1,a
        li 126
        lr 2,a
        pi plot
        
        ;send the data
        lr a,4
        lr 6,a
        
        li 5        ;start x coordinate
        lr 2,a
little_loop:
        lm
        lr 1,a

        pi plot

        lr a,2
        ai 1
        lr 2,a
        lr a,6
        ai $ff
        lr 6,a
        bz little_loop_end
        
        lr a,1
        sl 1
        sl 1
        lr 1,a

        pi plot

        lr a,2
        ai 1
        lr 2,a
        lr a,6
        ai $ff
        lr 6,a
        bz little_loop_end
        
        lr a,1
        sl 1
        sl 1
        lr 1,a

        pi plot

        lr a,2
        ai 1
        lr 2,a
        lr a,6
        ai $ff
        lr 6,a
        bz little_loop_end
        
        lr a,1
        sl 1
        sl 1
        lr 1,a

        pi plot

        lr a,2
        ai 1
        lr 2,a
        lr a,6
        ai $ff
        lr 6,a
        bnz little_loop
little_loop_end:
        lr a,3
        ai 1
        lr 3,a
        
        lr a,5
        ai $ff
        lr 5,a
        bnz big_loop
        lr pc1,k
        pop


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

logo:
        .include "logo.asm"

picture:
        .include "pic.asm"

picture2:
        .include "pic2.asm"
