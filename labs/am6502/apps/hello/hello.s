; am6502 smoke test: print a line, return 0.
;
; Flat (`mos6502-nemu`):  _putc writes $F001, and that is the whole program.
; Cartridge (`mos6502-nes`): the program also sets up a white screen, so the
; frame itself is evidence the PPU path works on NEMU, GEM6502 and RTL.

.setcpu "6502"

.segment "CODE"
.export main
.import _putc

.proc main
.ifdef AM_PLATFORM_NES
        jsr nes_white_screen
.endif
        ldx #$00
@loop:  lda message,x
        beq @done
        jsr _putc
        inx
        bne @loop
@done:  lda #$00
        rts
.endproc

.ifdef AM_PLATFORM_NES
; A screen that is entirely "palette 0, tile 0": no CHR data needed, and any
; pixel at all means the PPU rendered something.  The app talks to the PPU
; directly because the IOE PPU driver is M3 work.
.proc nes_white_screen
        bit $2002               ; clear a pending vblank flag
@warm1: bit $2002
        bpl @warm1
@warm2: bit $2002
        bpl @warm2

        lda #$00                ; rendering off while VRAM is rewritten
        sta $2000
        sta $2001

        ; Palette: black everywhere, then $3F00 white.  Order matters: $3F10
        ; mirrors $3F00 on real hardware, so filling first and patching last is
        ; the only way the backdrop survives the fill.
        lda #$3F
        sta $2006
        lda #$00
        sta $2006
        ldx #$20
        lda #$0F
@pal:   sta $2007
        dex
        bne @pal

        lda #$3F
        sta $2006
        lda #$00
        sta $2006
        lda #$30
        sta $2007

        lda #$20                ; nametable 0 + attributes: 1 KiB of tile 0
        sta $2006
        lda #$00
        sta $2006
        ldx #$00
        ldy #$04
        lda #$00
@nt:    sta $2007
        inx
        bne @nt
        dey
        bne @nt

        lda #$00                ; scroll 0,0
        sta $2005
        sta $2005

        lda #$08                ; show background
        sta $2001
        lda #$00                ; NMI stays off
        sta $2000

        bit $2002               ; let two frames render
@f1:    bit $2002
        bpl @f1
@f2:    bit $2002
        bpl @f2
        rts
.endproc
.endif

.segment "RODATA"
message:
        .byte "hello from am6502", $0A, $00
