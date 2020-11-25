#!/bin/bash

cmd="ffmpeg -y -i $1 -vn -sn "
loudness=" -af loudnorm=I=-24.0:TP=-2.0:LRA=11.0:measured_I=${10}:measured_LRA=${11}:measured_TP=${12}:measured_thresh=${13}:offset=${14}:linear=true:print_format=summary "
libfdk=" -c:a libfdk_aac -movflags +faststart -signaling explicit_sbr"
map_metadata=" -map_metadata -1 -ab ${4}k -ac $5 -ar $6 "
profile=" -profile:a $7 "
target=" $8 "
cmd=$cmd$loudness

if [ $5 -gt 1 ] #多声道
then
  cmd=$cmd$libfdk
fi

if [ ${9} -ne 0 ] #profile
then
  cmd=$cmd$profile
fi

cmd=$cmd$map_metadata

cmd=$cmd$target
eval $cmd