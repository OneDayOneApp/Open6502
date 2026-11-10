# Open6502 FPGA wrapper

`open6502_fpga_top.sv` wraps the SoC for an FPGA board:

- 21.477272 MHz board/PLL clock
- reset
- 1P/2P controller inputs
- cartridge bus
- VGA hsync/vsync/RGB444
- 1-bit sigma-delta audio output
- video data-enable

Board-specific pin constraints, PLL configuration, cartridge SRAM/flash and
controller level shifting live outside this directory.

Current limitations:

- The VGA timing is the NES 256x240 timing; a scan-doubler/HDMI encoder is
  required for modern displays.
- Audio is a simple first-order sigma-delta DAC.
- The DMC core is not yet mixed into the SoC audio path.
- The CPU/PPU/APU are functional RTL, not cycle-exact hardware models.
