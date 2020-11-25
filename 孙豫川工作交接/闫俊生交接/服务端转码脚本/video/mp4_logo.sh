#!/bin/bash

cmd="ffmpeg -y -i $1 -map_metadata -1 -c:v libx264 -max_muxing_queue_size 8192 "

cmd=$cmd" -vf \"movie='$7', scale=$8:$9[watermask]; [in][watermask] overlay=${10}:${11},fps=25[out]\" -strict -2 -force_key_frames \"expr:gte(t,n_forced*5)\" -sc_threshold 0 -bf 2 -b_strategy 0 "

cmd=$cmd" -s $4x$5 -b:v ${6}k  -threads 4 -c:a libfdk_aac -ab 64k -ac 2 -ar 44100 -movflags faststart $2"

echo $cmd
eval $cmd