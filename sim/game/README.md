# RTL NROM game bring-up

`extract_ines.py` converts a mapper-0 iNES image into Verilog `$readmemh`
images:

```sh
python3 sim/game/extract_ines.py roms/falling.nes /tmp/falling falling
```

Run the Verilator SoC model:

```sh
./sim/game/run.sh roms/falling.nes /tmp/falling-rtl.ppm
```

`tb/open6502_game_tb.sv` preloads the 8 KiB CHR RAM through the PPU loader port,
feeds PRG from the cartridge bus, and runs the SoC for video frames.

Current status: the Verilator harness renders frame 5 of `falling.nes` and the
PPM is byte-identical to the NEMU screenshot:

```text
RTL  SHA-256 41e8ebc137e3b8ede2f0adf915956fb9454346e9454e358c6dad2ee5a58f5a33
NEMU SHA-256 41e8ebc137e3b8ede2f0adf915956fb9454346e9454e358c6dad2ee5a58f5a33
```

The capture samples the raw PPU color index (`dbg_ppu_pixel`) and converts it
through the NES 64-color table; the top-level `vid_rgb` uses an RGB444 palette
LUT.

Key fix: the PPU chip select is qualified with the CPU clock enable
(`cpu_ce`). Before this, one CPU store was committed on all 12 21 MHz clocks,
so `$2006/$2007` address writes were repeated and palette writes landed in CHR.
