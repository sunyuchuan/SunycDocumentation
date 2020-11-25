#!/bin/bash

cmd="ffmpeg -y -i $1 -vn -sn  "

if [ $(echo "$2 != 0"|bc) = 1 ]
then
  cmd=$cmd" -af \" volume=${2}dB \""
fi

cmd=$cmd" -ac $4 -ar $5 -ab ${6}k -f mp3 -codec libmp3lame $7"

eval $cmd