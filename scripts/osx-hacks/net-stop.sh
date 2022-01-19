#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

sudo port unload dnsmasq
sudo ifconfig bridge0 down
sudo ifconfig bridge0 destroy
sudo kextunload  /Applications/Tunnelblick.app/Contents//Resources/tap-notarized.kext
