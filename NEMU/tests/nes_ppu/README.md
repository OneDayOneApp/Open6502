# PPU timing tests

Unit tests for the NES 2C02 scanline/dot state machine:

- reset state
- vblank status and NMI at scanline 241
- `$2002` read clearing
- pre-render frame boundary at scanline 261
- odd-frame skip when rendering is enabled
- no odd-frame skip when rendering is disabled

Run:

```sh
./tests/nes_ppu/run.sh
```

`tests/run.sh` includes this group before the end-to-end NES frontend test.
