#!/bin/bash -e
# linux-env.sh

# Add /usr/lib/ to LD_LIBRARY_PATH cause Ubuntu is dumb
export LD_LIBRARY_PATH="/usr/lib/:$LD_LIBRARY_PATH"

# Disable Webkit compositing on Wayland cause it breaks stuff
if [[ $(env | grep -i wayland) ]]; then
    export WEBKIT_DISABLE_COMPOSITING_MODE=1
fi
