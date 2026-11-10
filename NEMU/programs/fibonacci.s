; fibonacci.s - store the first 16 Fibonacci bytes at $0200..$020F,
; then stop with exit code 0.
.setcpu "6502"

.segment "CODE"

start:
    lda #0
    sta $00          ; previous = 0
    lda #1
    sta $01          ; current  = 1
    ldx #0
loop:
    lda $01
    sta $0200,x
    clc
    adc $00
    sta $02          ; next = previous + current
    lda $01
    sta $00
    lda $02
    sta $01
    inx
    cpx #16
    bne loop

    lda #0
    sta $F002
hang:
    jmp hang
