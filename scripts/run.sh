#!/bin/bash
set -e
D="auto"
P="8080"
FPS="60"
BR="8000"
E="auto"

cd /kaggle/working/burstvnc
exec ./build/burst_srv --display $D --port $P --fps $FPS --bitrate $BR --encoder $E "$@"
