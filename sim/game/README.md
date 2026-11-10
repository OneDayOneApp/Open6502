# RTL NROM game bring-up

`extract_ines.py` converts a mapper-0 iNES image into Verilog `$readmemh`
images:

```sh
python3 sim/game/extract_ines.py roms/falling.nes /tmp/falling falling
```

`tb/open6502_game_tb.sv` preloads the 8 KiB CHR RAM through the PPU loader port,
feeds PRG from the cartridge bus, and runs the SoC for video frames.

Current status: the Verilator harness runs 56 frames. CPU reaches the main loop
at `PC=0xC0CA`; PPU `ctrl=$90`, `mask=$1E`, but palette RAM is still zero and
the framebuffer is blank. The OAM DMA restart bug was fixed; the next step is
the palette/PPU rendering path.
