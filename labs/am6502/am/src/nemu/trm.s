; am6502 TRM for `mos6502-nemu`: putc, halt, and the interrupt stubs.
;
; Host I/O is 6502EMU's existing window (NEMU/6502EMU.md):
;   $F001 write  one character to stdout
;   $F002 write  end the simulation with that byte as the exit status
;   $F003 read   one character from stdin

.setcpu "6502"

.export _putc, _halt, am_init
.export _io_read, _io_write, _am_mainargs
.export _nmi, _irq

.segment "CODE"

; Platform init: nothing to do on a flat 64 KiB machine.  Kept as a real
; symbol so the nes platform can put PPU/APU setup here without changing crt0.
.proc am_init
        rts
.endproc

; void _putc(char ch): the byte is already in A.
.proc _putc
        sta $F001
        rts
.endproc

; void _halt(int code): exit status is the low 7 bits of the argument.
.proc _halt
        and #$7F
        sta $F002
@spin:  jmp @spin
.endproc

; No devices are wired up yet (M1): every (dev, reg) access reports "nothing".
; Returning 0 instead of trapping is the contract that lets one app build for
; several platforms (docs/design.md §7).  size_t is 16 bits here, so A/X = 0.
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

; No interrupt sources are enabled in M1; acknowledge and return.
.proc _nmi
        rti
.endproc

.proc _irq
        rti
.endproc

.segment "RODATA"
_am_args_empty:
        .byte $00
