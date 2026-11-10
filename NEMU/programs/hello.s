; hello.s - print a string through 6502EMU's host UART at $F001,
; then exit with code 0 through $F002.
.setcpu "6502"

.segment "CODE"

start:
    ldx #0
loop:
    lda message,x
    beq done
    sta $F001
    inx
    bne loop
done:
    lda #0
    sta $F002
hang:
    jmp hang

message:
    .byte "Hello, 6502EMU!", $0A, 0
