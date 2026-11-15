; am6502 TRM for `mos6502-nes`: putc, halt, and the interrupt stubs.
;
; Host I/O lives in the Famicom test-register window, which real hardware
; ignores but every host we control can decode (design §6.4):
;   $4018 write  one character to the host console
;   $4019 write  end the simulation with that byte as the exit status

.setcpu "6502"

.export _putc, _halt, am_init
.export _io_read, _io_write, _am_mainargs
.export _nmi, _irq

.segment "CODE"

; Platform init: everything off, so a cartridge cannot inherit the power-on
; state of an emulator that already ran another ROM.
.proc am_init
        lda #$00
        sta $2000               ; NMI off, no rendering
        sta $2001
        sta $4010               ; DMC IRQ off
        sta $4015               ; APU silent
        lda #$40
        sta $4017               ; frame IRQ inhibited
        rts
.endproc

; void _putc(char ch): the byte is already in A.
.proc _putc
        sta $4018
        rts
.endproc

; void _halt(int code): exit status is the low 7 bits of the argument.
.proc _halt
        and #$7F
        sta $4019
@spin:  jmp @spin
.endproc

; No devices are wired up yet: every (dev, reg) access reports "nothing"
; instead of trapping (design §7).  size_t is 16 bits here, so A/X = 0.
.proc _io_read
        lda #$00
        ldx #$00
        rts
.endproc

.proc _io_write
        lda #$00
        ldx #$00
        rts
.endproc

; const char *am_mainargs(void): "" until the build learns to bake a string in.
.proc _am_mainargs
        lda #<_am_args_empty
        ldx #>_am_args_empty
        rts
.endproc

; No interrupt sources are enabled in M2; acknowledge and return.
.proc _nmi
        rti
.endproc

.proc _irq
        rti
.endproc

.segment "RODATA"
_am_args_empty:
        .byte $00
