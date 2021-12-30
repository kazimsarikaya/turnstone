#!/usr/bin/env bash

sudo port unload dnsmasq
sudo ifconfig bridge0 down
sudo ifconfig bridge0 destroy
sudo kextunload  /Applications/Tunnelblick.app/Contents//Resources/tap-notarized.kext
