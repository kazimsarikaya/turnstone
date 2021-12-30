# **TURNSTONE**: The fUnny opeRatiNg SysTem Of uNivErse

This is a custom os project.

The roadmap is at @ref roadmap.

The architecture is at @ref architecture.

## Building On OSX

You need packages with macports:
```
x86_64-elf-gcc x86_64-w64-mingw32-gcc
```

```
make clean gendirs qemu
```

## Building On Linux

There is two options install gcc and gcc-mingw-w64 and:

```
make clean gendirs qemu
```

Or build with container. First build the turnstone-builder image:

```
podman build -t localhost/turnstone-builder:latest -f scripts/container-build/Containerfile .
```

Then build turnstone and generate 1GiB raw disk output/qem-hda, which will be used as disk of qemu.

```
podman run --rm -v ./:/osdev:Z localhost/turnstone-builder:latest
```

If you don't use selinux you can omit the Z parameter at volume mount.

## Runnig

On both OSX and Linux you need **qemu**.

On Linux you need **edk2-ovmf** package for efi files for qemu. On OSX macports provides them with **qemu**.

For starting a networkless (may be with default network) qemu with OVMF EDK2 uefi bios:

```
scripts/osx-hacks/qemu-efi-hda.sh
```

The qemu terminal will be openned at console and video output will be at ```tmp/qemu-video.log```.

For discustions please visit [discord channel][discord channel].


[discord channel]: https://discord.gg/q5WxJKz7fd
