OS DEV
======

This is a custom os project.

The roadmap is at @ref roadmap.

The architecture is at @ref architecture.

For a quick build and gen qemu disk please run in order:

```
make gendirs
make qemu
```

For starting a networkless (may be with default network) qemu with OVF EDK2 uefi bios:

```
scripts/osx-hacks/qemu-efi-hda.sh
```

The qemu terminal will be openned at console and video output will be at tmp/qemu-video.log
