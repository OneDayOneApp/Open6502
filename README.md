# Open6502

这个仓库包含基于 NEMU 扩展的 **6502EMU** 模拟器。

- 上游 NEMU 源码：`NEMU/`
- 6502 构建与使用说明：[NEMU/6502EMU.md](NEMU/6502EMU.md)
- 模拟器：`NEMU/build/6502EMU`
- 示例程序：`NEMU/programs/`
- 回归测试：`NEMU/tests/run.sh`
- Microsoft BASIC 1.1：[NEMU/basic/README.md](NEMU/basic/README.md)
- NES 游戏前端与演示：[NEMU/nesgame/README.md](NEMU/nesgame/README.md)
- NES/Famicom Golden Reference：[设计方案](docs/nes-golden-reference/design.md) · [测试方案](docs/nes-golden-reference/test-plan.md) · [CheckList](docs/nes-golden-reference/checklist.md) · [必须功能点](docs/nes-golden-reference/feature-matrix.md)

```sh
cd NEMU
make m6502_defconfig
make -j8
./build/6502EMU -b programs/hello.bin
./basic/run-demo.sh
./nesgame/build.sh && ./nesgame/run.sh
```

- 版本与变更：[CHANGELOG.md](CHANGELOG.md)
