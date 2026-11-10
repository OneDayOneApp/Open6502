# 6502EMU NES Demo Game

This is a small NROM game used to exercise the NES frontend:

- PPU background rendering and solid playfield
- sprites, OAM DMA, NMI, palette and controller input
- APU pulse sound on paddle/ball collisions
- iNES mapper 0 (NROM)

It is intentionally small and is not a commercial game.  The source is in
`game.s`; `chrgen.py` generates the 8 KiB CHR ROM.

## Build and run

From the repository root:

```sh
cd NEMU
make -j8
./nesgame/build.sh
./nesgame/run.sh
```

Controls:

- W/A/S/D: player 1 D-pad
- Z/X: player 1 A/B
- C/V: player 1 Select/Start
- Arrow keys: player 2 D-pad
- `,` / `.`: player 2 A/B
- `/` / `'`: player 2 Select/Start
- Escape: quit

This demo game itself is single-player; the player 2 mapping is available to any
NES ROM that reads controller 2.

The demo starts a ball moving between a player-controlled left paddle and a
simple AI right paddle.  Each hit retriggers a short APU pulse.

Headless regression:

```sh
./build/6502EMU --headless --no-audio --frames=180 \
  --screenshot=/tmp/6502emu-nes.ppm --wav=/tmp/6502emu-nes.wav \
  --hash nesgame/game.nes
```

`game.nes` is built from the checked-in source and is included for
convenience.  Only mapper 0 is used by this game; the emulator itself also
implements common mappers 1, 2, 3, 4 and 7.
