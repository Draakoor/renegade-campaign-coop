#!/bin/sh
set -eu
if [ "$#" -gt 0 ]; then
    exec "$@"
fi
cd /home/container
export WINEPREFIX=/home/container/.wine
export DISPLAY=:0
Xvfb "$DISPLAY" -screen 0 1024x768x24 -nolisten tcp >/home/container/xvfb.log 2>&1 &
if [ -n "${STARTUP:-}" ]; then
    exec /bin/bash -lc "$STARTUP"
fi
exec python3 -u /opt/renegade-coop/runtime.py
