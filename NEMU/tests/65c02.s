.setcpu "65C02"
.macro BRFAIL
        beq :+
        jmp fail
:
.endmacro
.segment "CODE"

start:
        lda #$FF
        sta $10
        stz $10
        lda $10
        BRFAIL

        ldx #$5A
        phx
        ldx #0
        plx
        cpx #$5A
        BRFAIL

        ldy #$A5
        phy
        ldy #0
        ply
        cpy #$A5
        BRFAIL

        lda #$0F
        sta $12
        lda #$01
        trb $12
        lda $12
        cmp #$0E
        BRFAIL

        lda #$F0
        tsb $12
        lda $12
        cmp #$FE
        BRFAIL

        lda #0
        sta $13
        smb7 $13
        lda $13
        cmp #$80
        BRFAIL
        rmb7 $13
        lda $13
        BRFAIL

        lda #0
        bit #$00
        BRFAIL
        inc a
        cmp #1
        BRFAIL
        dec a
        BRFAIL

        lda #<$0200
        sta $20
        lda #>$0200
        sta $21
        lda #$42
        sta $0200
        lda ($20)
        cmp #$42
        BRFAIL

        bra bra_ok
        jmp fail
bra_ok:
        lda #0
        sta $30
        bbs0 $30, bbs_hit
        jmp bbs_ok
bbs_hit:
        jmp fail
bbs_ok:
        bbr0 $30, bbr_ok
        jmp fail
bbr_ok:
        lda #<target
        sta $40
        lda #>target
        sta $41
        ldx #0
        jmp ($0040,x)
target:
        lda #0
        sta $F002
fail:
        lda #1
        sta $F002
        jmp fail
