# NEMU sdhost driver

This driver is trimmed from `linux/drivers/mmc/host/bcm2835.c`. DMA and
interrupts are removed and the driver uses polling, so the processor does not
need DMA or interrupt support.

## Usage

1. Copy `nemu.c` from this directory to `linux/drivers/mmc/host/`.
2. Add `obj-y += nemu.o` to `linux/drivers/mmc/host/Makefile`.
3. In menuconfig, disable `General setup -> Initial RAM filesystem and RAM disk
   (initramfs/initrd) support`.
4. In menuconfig, enable `Device Drivers -> MMC/SD/SDIO card support`.
5. Add the following node to the device tree:

```dts
/ {
  soc {
    sdhci: mmc {
      compatible = "nemu-sdhost";
      reg = <0x0 0xa3000000 0x0 0x1000>;
    };
  };
};
```

This is legacy NEMU resource documentation; the Open6502 6502/NES build does not
use it.
