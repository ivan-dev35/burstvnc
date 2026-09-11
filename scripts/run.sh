#!/bin/bash
set -e
D=":99"
P="8080"
FPS="60"
BR="8000"
E="auto"

if ! ps aux | grep -v grep | grep -q "Xvfb $D"; then
    Xvfb $D -screen 0 1920x1080x24 -ac +extension COMPOSITE +extension DAMAGE +extension RANDR +extension GLX > /dev/null 2>&1 &
    sleep 1
fi

if ! ps aux | grep -v grep | grep -q "xfce4-session"; then
    DISPLAY=$D startxfce4 > /dev/null 2>&1 &
    sleep 1
fi

if ! pulseaudio --check; then
    pulseaudio --start --exit-idle-time=-1 > /dev/null 2>&1 || true
fi

cd /kaggle/working/burstvnc
exec ./build/burst_srv --display $D --port $P --fps $FPS --bitrate $BR --encoder $E "$@"
