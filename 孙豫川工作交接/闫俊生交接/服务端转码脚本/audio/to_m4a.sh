#!/bin/bash

cmd="ffmpeg -y -i $1 -vn -sn "
vloumn_adjust=" volume=${2}dB"
libfdk=" -c:a libfdk_aac -movflags +faststart -signaling explicit_sbr"
map_metadata=" -map_metadata -1 -ab ${4}k -ac $5 -ar $6 "
profile=" -profile:a $7 "
target=" $8 "

if [ $(echo "$2 != 0"|bc) = 1 ] #调节音量
then
  cmd=$cmd" -af \""$vloumn_adjust"\""
fi

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