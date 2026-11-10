# Open-source NES game ROMs

These ROMs are included for testing 6502EMU.  They are open-source/free
homebrew games, not commercial Nintendo ROMs.  Each license text is in
`LICENSES/`.

| ROM | Source | License | Mapper | 300-frame status |
| --- | --- | --- | --- | --- |
| `falling.nes` | https://github.com/xram64/falling-nes | MIT | 0 (NROM) | boots, title screen |
| `galaxypatrol.nes` | https://github.com/rlannon/NES_GalaxyPatrol | MIT | 0 (NROM) | boots, playable shooter |
| `hanoi.nes` | https://github.com/jdonald/nes-hanoi-tower | MIT | 0 (NROM) | boots, title screen |
| `table_tennis.nes` | https://github.com/mike42/8bit-table-tennis | MIT | 0 (NROM) | boots, title screen |
| `file-fixers.nes` | https://github.com/wendelscardua/file-fixers | MIT | 4 (MMC3) | boots, title screen |
| `stallar.nes` | https://github.com/wendelscardua/stallar | MIT | 4 (MMC3) | boots, title screen |

## Build and run

```sh
cd NEMU
make m6502_defconfig
make -j8

# from the repository root:
./NEMU/build/6502EMU roms/falling.nes
./NEMU/build/6502EMU roms/file-fixers.nes
```

Or use the launcher:

```sh
./roms/play.sh falling
```

## Keyboard

| Key | Controller |
| --- | --- |
| W/A/S/D | 1P D-pad |
| Space/Z | 1P A（跳跃） |
| X | 1P B |
| C/V | 1P Select/Start |
| Arrow keys | 2P D-pad |
| `,` `.` | 2P A/B |
| `/` `'` | 2P Select/Start |
| Esc | quit |

Most of these games use **Start** to begin.

## Current emulator caveats

- PPU is still a functional scanline/dot hybrid, not a complete cycle-exact
  pipeline.
- `galaxypatrol.nes` has a sparse-looking starfield/background; this is a
  known PPU/background-rendering gap, not a ROM problem.
- Commercial ROMs such as Super Mario Bros are not included.
