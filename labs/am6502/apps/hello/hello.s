; am6502 smoke test: print a line, return 0.
;
; This is the smallest program that proves the whole M1 chain: crt0 sets up the
; machine, the app calls the runtime's _putc, main()'s return value becomes the
; host exit status.

.setcpu "6502"

.segment "RODATA"
message:
        .byte "hello from am6502", $0A, $00

.segment "CODE"
.export main
.import _putc

.proc main
        ldx #$00
@loop:  lda message,x
        beq @done
        jsr _putc
        inx
        bne @loop
@done:  lda #$00
        rts
.endproc
