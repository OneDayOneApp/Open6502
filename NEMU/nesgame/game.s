; 6502EMU NES demo game: a small two-paddle ball game.
; NROM, 32 KiB PRG, 8 KiB CHR, horizontal mirroring.
.setcpu "6502"

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
OAMADDR   = $2003
OAMDATA   = $2004
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014
JOY1      = $4016
APUSTATUS = $4015
APU0      = $4000
APU1      = $4001
APU2      = $4002
APU3      = $4003

OAM       = $0200
buttons   = $10
player_x  = $11
player_y  = $12
enemy_y   = $13
ball_x    = $14
ball_y    = $15
ball_dx   = $16
ball_dy   = $17
temp      = $18
scroll_x  = $19

.segment "CODE"

reset:
        sei
        cld
        ldx #$FF
        txs
        inx
        stx PPUCTRL
        stx PPUMASK

        ; wait for two vblank periods
wait_vb1:
        bit PPUSTATUS
        bpl wait_vb1
wait_vb2:
        bit PPUSTATUS
        bpl wait_vb2

        ; fill the visible nametable with the solid tile
        lda #$20
        sta PPUADDR
        lda #$00
        sta PPUADDR
        ldx #0
        ldy #4
clear_nt:
        lda #4
        sta PPUDATA
        inx
        bne clear_nt
        dey
        bne clear_nt

        ; attribute table: all quadrants use palette 1
        lda #$23
        sta PPUADDR
        lda #$C0
        sta PPUADDR
        ldx #64
        lda #$55
attr_loop:
        sta PPUDATA
        dex
        bne attr_loop

        ; palette
        lda #$3F
        sta PPUADDR
        lda #$00
        sta PPUADDR
        ldx #0
pal_loop:
        lda palette,x
        sta PPUDATA
        inx
        cpx #32
        bne pal_loop

        ; hide every sprite
        lda #$FF
        ldx #0
oam_clear:
        sta OAM,x
        inx
        bne oam_clear

        lda #16
        sta player_x
        lda #112
        sta player_y
        lda #112
        sta enemy_y
        lda #120
        sta ball_x
        lda #112
        sta ball_y
        lda #1
        sta ball_dx
        sta ball_dy
        lda #0
        sta scroll_x

        lda #$80
        sta PPUCTRL
        lda #$1E
        sta PPUMASK
main:
        jmp main

nmi:
        pha
        txa
        pha
        tya
        pha

        ; read controller 1 (A B Select Start Up Down Left Right)
        lda #1
        sta JOY1
        lda #0
        sta JOY1
        ldx #8
        lda #0
        sta buttons
read_joy:
        lda JOY1
        lsr a
        rol buttons
        dex
        bne read_joy

        ; player movement
        lda buttons
        and #$02
        beq check_right
        lda player_x
        beq check_right
        dec player_x
        dec player_x
check_right:
        lda buttons
        and #$01
        beq check_up
        lda player_x
        cmp #240
        bcs check_up
        inc player_x
        inc player_x
check_up:
        lda buttons
        and #$08
        beq check_down
        lda player_y
        beq check_down
        dec player_y
        dec player_y
check_down:
        lda buttons
        and #$04
        beq move_ball
        lda player_y
        cmp #224
        bcs move_ball
        inc player_y
        inc player_y

move_ball:
        lda ball_x
        clc
        adc ball_dx
        sta ball_x
        cmp #8
        bcc bounce_x
        cmp #248
        bcc move_ball_y
bounce_x:
        lda ball_dx
        eor #$FF
        clc
        adc #1
        sta ball_dx
        lda ball_x
        cmp #128
        bcs set_right
        lda #8
        sta ball_x
        jmp move_ball_y
set_right:
        lda #247
        sta ball_x

move_ball_y:
        lda ball_y
        clc
        adc ball_dy
        sta ball_y
        cmp #8
        bcc bounce_y
        cmp #232
        bcc collide_player
bounce_y:
        lda ball_dy
        eor #$FF
        clc
        adc #1
        sta ball_dy
        lda ball_y
        cmp #120
        bcs set_bottom
        lda #8
        sta ball_y
        jmp collide_player
set_bottom:
        lda #231
        sta ball_y

collide_player:
        lda ball_x
        sec
        sbc player_x
        cmp #16
        bcs collide_enemy
        lda ball_y
        sec
        sbc player_y
        cmp #16
        bcs collide_enemy
        lda ball_dx
        eor #$FF
        clc
        adc #1
        sta ball_dx
        lda player_x
        clc
        adc #16
        sta ball_x
        jsr beep

collide_enemy:
        lda ball_x
        sec
        sbc #232
        cmp #16
        bcs move_enemy
        lda ball_y
        sec
        sbc enemy_y
        cmp #16
        bcs move_enemy
        lda ball_dx
        eor #$FF
        clc
        adc #1
        sta ball_dx
        lda #224
        sta ball_x
        jsr beep

move_enemy:
        lda ball_y
        sec
        sbc enemy_y
        cmp #8
        bcc enemy_up
        lda enemy_y
        cmp #224
        bcs build_oam
        inc enemy_y
        jmp build_oam
enemy_up:
        lda enemy_y
        beq build_oam
        dec enemy_y

build_oam:
        ; player, 16x16
        lda player_y
        sec
        sbc #1
        sta OAM+0
        sta OAM+4
        lda player_y
        clc
        adc #7
        sta OAM+8
        sta OAM+12
        lda #1
        sta OAM+1
        sta OAM+5
        sta OAM+9
        sta OAM+13
        lda #0
        sta OAM+2
        sta OAM+6
        sta OAM+10
        sta OAM+14
        lda player_x
        sta OAM+3
        sta OAM+11
        clc
        adc #8
        sta OAM+7
        sta OAM+15

        ; enemy, 16x16
        lda enemy_y
        sec
        sbc #1
        sta OAM+16
        sta OAM+20
        lda enemy_y
        clc
        adc #7
        sta OAM+24
        sta OAM+28
        lda #1
        sta OAM+17
        sta OAM+21
        sta OAM+25
        sta OAM+29
        lda #2
        sta OAM+18
        sta OAM+22
        sta OAM+26
        sta OAM+30
        lda #232
        sta OAM+19
        sta OAM+27
        lda #240
        sta OAM+23
        sta OAM+31

        ; ball, 8x8
        lda ball_y
        sec
        sbc #1
        sta OAM+32
        lda #2
        sta OAM+33
        lda #1
        sta OAM+34
        lda ball_x
        sta OAM+35

        ; transfer OAM and reset scroll
        lda #0
        sta OAMADDR
        lda #2
        sta OAMDMA
        ; Keep the decorative field static; scrolling this high-contrast
        ; pattern every frame is visually distracting in this tiny demo.
        lda #0
        sta PPUSCROLL
        sta PPUSCROLL

        pla
        tay
        pla
        tax
        pla
        rti

beep:
        lda #1
        sta APUSTATUS
        lda #$9F
        sta APU0
        lda #0
        sta APU1
        lda #$A9
        sta APU2
        lda #0
        sta APU3
        rts

irq:
        rti

palette:
        ; background palettes: low-contrast blue stripes
        .byte $02, $11, $21, $31
        .byte $02, $11, $21, $31
        .byte $02, $11, $21, $31
        .byte $02, $11, $21, $31
        ; sprite palettes (color 0 is transparent; keep the shared
        ; backdrop color blue when the mirrored palette entry is written)
        .byte $02, $16, $27, $38
        .byte $02, $2A, $1A, $2A
        .byte $02, $28, $18, $16
        .byte $02, $30, $10, $00

.segment "VECTORS"
        .word nmi, reset, irq
