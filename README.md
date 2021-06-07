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

For starting a networkless (may be with default network) qemu:

```
sudo <qemu-64-binary> \
          -name osdev-hda-boot \
          -M q35 -m 1g -smp cpus=2 \
          -drive index=0,media=disk,format=raw,file=output/qemu-hda \
          -monitor stdio \
          -serial tmp/qemu-video.log
```

The qemu terminal will be openned at console and video output will be at tmp/qemu-video.log
