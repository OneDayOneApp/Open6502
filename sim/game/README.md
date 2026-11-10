# RTL NROM game bring-up

`extract_ines.py` converts a mapper-0 iNES image into Verilog `$readmemh`
images:

```sh
python3 sim/game/extract_ines.py roms/falling.nes /tmp/falling falling
```

`tb/open6502_game_tb.sv` preloads the 8 KiB CHR RAM through the PPU loader port,
feeds PRG from the cartridge bus, and runs the SoC for video frames.

Current status: the harness runs and produces five frames, but the framebuffer
is still blank. CPU PC is in the `$2002` vblank-poll routine and PPU
`ctrl/mask` remain zero; the next step is debugging the PPU status read and game
initialization path.
