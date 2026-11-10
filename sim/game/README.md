# RTL NROM game bring-up

`extract_ines.py` converts a mapper-0 iNES image into Verilog `$readmemh`
images:

```sh
python3 sim/game/extract_ines.py roms/falling.nes /tmp/falling falling
```

`tb/open6502_game_tb.sv` preloads the 8 KiB CHR RAM through the PPU loader port,
feeds PRG from the cartridge bus, and runs the SoC for video frames.

Current status: the Verilator harness runs 56 frames of `falling.nes` and emits
a non-blank 256x240 frame. CPU reaches `PC=0xC0CA`; PPU `ctrl=$90`, `mask=$1E`,
palette `0F 21 31 11`, and the framebuffer has non-black pixels.

Key fix: the PPU chip select is qualified with the CPU clock enable
(`cpu_ce`). Before this, one CPU store was committed on all 12 21 MHz clocks,
so `$2006/$2007` address writes were repeated and palette writes landed in CHR.
