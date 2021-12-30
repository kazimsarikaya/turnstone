#!/usr/bin/env bash

sudo ifconfig bridge0 create
sudo ifconfig bridge0 192.168.122.1 192.168.122.255
sudo kextload  /Applications/Tunnelblick.app/Contents//Resources/tap-notarized.kext
sudo port load dnsmasq
