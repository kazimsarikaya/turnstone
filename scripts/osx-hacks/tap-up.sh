#!/usr/bin/env bash
TAPDEV="$1"
BRIDGEDEV="bridge0"
ifconfig $BRIDGEDEV addm $TAPDEV
