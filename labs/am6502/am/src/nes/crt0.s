; am6502 startup for `mos6502-nes` (NROM cartridge).
;
; On the NES the reset vector is part of the cartridge, so this file owns it:
; every image produced by the build can boot on NEMU, GEM6502 and the RTL SoC
; without a bench patching $FFFC.

.setcpu "6502"

.export _start
.import main, _halt, am_init
.import _nmi, _irq

.segment "CODE"

.proc _start
        sei
        cld
        ldx #$FF
        txs                     ; hardware stack $0100-$01FF

        ; Runtime zero page $F0-$FF must read as zero in main() (design §6.2).
        lda #$00
        ldx #$10
@zp:    dex
        sta $F0,x
        bne @zp

        ; Zero the 2 KiB of WRAM above the stack ($0200-$07FF).  PPU VRAM and
        ; OAM are the platform's business, not the app's.
        ldx #$00
@ram:   sta $0200,x
        sta $0300,x
        sta $0400,x
        sta $0500,x
        sta $0600,x
        sta $0700,x
        inx
        bne @ram

        jsr am_init
        jsr main                ; A = exit status
        jsr _halt
.endproc

.segment "VECTORS"
        .word _nmi
        .word _start
        .word _irq
