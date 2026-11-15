; am6502 startup for `mos6502-nemu`.
;
; Hardware reset state is not a C runtime: SP, decimal mode and the I flag are
; all "whatever the machine did".  This file is the only place allowed to
; assume that, and the only place that touches the reset vector.

.setcpu "6502"

.export _start
.import main, _halt, am_init
.import _nmi, _irq

.segment "CODE"

.proc _start
        sei                     ; no IRQ before the app is ready
        cld                     ; binary arithmetic; decimal is opt-in
        ldx #$FF
        txs                     ; hardware stack $0100-$01FF

        ; Runtime zero page $F0-$FF must read as zero in main() (design §6.2).
        lda #$00
        ldx #$10
@zp:    dex
        sta $F0,x
        bne @zp

        ; Zero the platform RAM area $0200-$03FF.  The image starts zeroed,
        ; but a user-supplied entry point or a warm reset need not.
        ldx #$00
@ram:   sta $0200,x
        sta $0300,x
        inx
        bne @ram

        jsr am_init
        jsr main                ; A = exit status
        jsr _halt
.endproc

; Vectors: NMI, RESET, IRQ, in the 6502's fixed order at $FFFA-$FFFF.
.segment "VECTORS"
        .word _nmi
        .word _start
        .word _irq
