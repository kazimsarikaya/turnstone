#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

TAPDEV="$1"
BRIDGEDEV="bridge0"
ifconfig $BRIDGEDEV deletem $TAPDEV
