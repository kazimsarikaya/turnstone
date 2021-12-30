#!/usr/bin/env bash

apt-get update
apt-get install -y gcc-mingw-w64
make clean gendirs qemu
