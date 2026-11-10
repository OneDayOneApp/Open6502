.segment "EXTRA"

; 6502EMU console glue.
; $F003: blocking console input, $F001: console output.
MONRDKEY:
        lda     $F003
        rts

MONCOUT:
        sta     $F001
        rts

RAMSTART2:
        .byte   $08,$29,$25,$20,$60,$2A,$E5,$E4
        .byte   $20,$66,$24,$65,$AC,$04,$A4
