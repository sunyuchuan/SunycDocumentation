#!/bin/bash

cmd="ffmpeg -y -i $1 -map_metadata -1 -c:v libx264 -max_muxing_queue_size 8192 "

cmd=$cmd" -vf "

delogocmd=" delogo=${11}:${12}:${13}:${14} "

logoprefix=" \"movie='$5', scale=$6:$7[watermask]; [in] "
logosuffix=" [rmlogo];[rmlogo] [watermask] overlay=${8}:${9}, fps=25[out]\" -strict -2 -force_key_frames \"expr:gte(t,n_forced*5)\" -sc_threshold 0 -bf 2 -b_strategy 0 "

if [ $6 -gt 0 ] #调节音量
then
  cmd=$cmd$logoprefix
fi
cmd=$cmd$delogocmd
if [ $6 -gt 0 ] #调节音量
then
  cmd=$cmd$logosuffix
fi

cmd=$cmd" -s $3x$4 -b:v ${10}k  -threads 4 -c:a libfdk_aac -ab 64k -ac 2 -ar 44100 -movflags faststart $2"

echo $cmd
eval $cmd
