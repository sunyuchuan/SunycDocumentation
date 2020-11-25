#!/bin/bash

cmd="ffmpeg -y -i $1 -vn -sn  "
loudness=" -af loudnorm=I=-24.0:TP=-2.0:LRA=11.0:measured_I=$8:measured_LRA=$9:measured_TP=${10}:measured_thresh=${11}:offset=${12}:linear=true:print_format=summary "
cmd=$cmd$loudness
cmd=$cmd" -ac $4 -ar $5 -ab ${6}k -f mp3 -codec libmp3lame $7"
eval $cmd