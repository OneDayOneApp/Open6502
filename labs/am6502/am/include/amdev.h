/*
 * am6502 device and register ids.
 *
 * Values are nexus-am's magic constants (am/amdev.h) so a `_DEV_*` means the
 * same thing in both projects.  Devices AM has no analogue for use a private
 * 0x0000ac1x range and are marked as extensions.
 */
#ifndef __AMDEV_H__
#define __AMDEV_H__

/* AM devices, unchanged. */
#define _DEV_INPUT   0x0000ac02
#define _DEV_TIMER   0x0000ac03
#define _DEV_SERIAL  0x0000ac05

/* am6502 extensions. */
#define _DEV_PPU     0x0000ac10
#define _DEV_APU     0x0000ac11
#define _DEV_CART    0x0000ac12

/* Register ids, per device. */

/* _DEV_INPUT: buf is one byte; bit order is the NES shift-register order. */
#define _DEVREG_INPUT_PAD1    1
#define _DEVREG_INPUT_PAD2    2
#define _DEVREG_INPUT_A       0x01
#define _DEVREG_INPUT_B       0x02
#define _DEVREG_INPUT_SELECT  0x04
#define _DEVREG_INPUT_START   0x08
#define _DEVREG_INPUT_UP      0x10
#define _DEVREG_INPUT_DOWN    0x20
#define _DEVREG_INPUT_LEFT    0x40
#define _DEVREG_INPUT_RIGHT   0x80

/* _DEV_TIMER: UPTIME is a 32-bit frame counter, low word first. */
#define _DEVREG_TIMER_UPTIME  1

/* _DEV_SERIAL: SEND has the same effect as _putc; RECV returns 0. */
#define _DEVREG_SERIAL_RECV   1
#define _DEVREG_SERIAL_SEND   2

/* _DEV_PPU: reg 1-8 are $2000-$2007; reg 9 triggers OAM DMA. */
#define _DEVREG_PPU_CTRL      1
#define _DEVREG_PPU_MASK      2
#define _DEVREG_PPU_STATUS    3
#define _DEVREG_PPU_OAMADDR   4
#define _DEVREG_PPU_OAMDATA   5
#define _DEVREG_PPU_SCROLL    6
#define _DEVREG_PPU_ADDR      7
#define _DEVREG_PPU_DATA      8
#define _DEVREG_PPU_OAMDMA    9

/* _DEV_APU: reg 1 is the $4000-$4017 register window. */
#define _DEVREG_APU_WINDOW    1

/* _DEV_CART: identity and bank control. */
#define _DEVREG_CART_MAPPER   1
#define _DEVREG_CART_BANK     2
#define _DEVREG_CART_MIRROR   3
#define _DEVREG_CART_IRQACK   4

#endif /* __AMDEV_H__ */
