# Building a riscv64 Debian image

This requires `qemu-riscv64-static`. Building on Debian 10 or Ubuntu 19.04 is
recommended; Docker can be used.

## Create the image and partitions

```sh
dd if=/dev/zero of=debian.img bs=1G count=16   # 16 GB image
sudo cfdisk debian.img                          # rootfs + swap partitions
sudo losetup --partscan --show --find debian.img
ls /dev/loop0*                                 # loop0p1, loop0p2
```

## Create filesystems

```sh
sudo mkfs.ext4 /dev/loop0p1
sudo mkswap /dev/loop0p2
```

## Mount the ext4 partition

```sh
sudo mkdir -p /mnt/debian
sudo mount /dev/loop0p1 /mnt/debian
```

## Install Debian

Use `debootstrap` (replace the mirror if needed):

```sh
sudo debootstrap --arch=riscv64 --foreign \
  --include=openssh-server,curl,wget,vim \
  bookworm /mnt/debian http://deb.debian.org/debian
```

Complete the second stage:

```sh
sudo cp /usr/bin/qemu-riscv64-static /mnt/debian/usr/bin/
sudo chroot /mnt/debian /debootstrap/debootstrap --second-stage
```

## Basic configuration

```sh
echo "debian-riscv64" | sudo tee /mnt/debian/etc/hostname
echo "root:root" | sudo chroot /mnt/debian chpasswd
sudo chroot /mnt/debian apt-get update
```

## Unmount

```sh
sudo umount /mnt/debian
sudo losetup -d /dev/loop0
```

The resulting `debian.img` can be booted by NEMU with a suitable RISC-V
configuration. This is legacy NEMU resource documentation; the Open6502
6502/NES build does not use it.
