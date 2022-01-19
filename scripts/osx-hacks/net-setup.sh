#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

sudo ifconfig bridge0 create
sudo ifconfig bridge0 192.168.122.1 192.168.122.255
sudo kextload  /Applications/Tunnelblick.app/Contents//Resources/tap-notarized.kext
sudo port load dnsmasq
