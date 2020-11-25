#!/bin/bash

ffmpeg -y -i $1 -map_metadata -1 -c:v libx264 -max_muxing_queue_size 1024 -vf "fps=25" -vf "movie='$5', scale=$6:$7[watermask]; [in][watermask] overlay=$8:$9[out]" -strict -2 -force_key_frames "expr:gte(t,n_forced*5)" -sc_threshold 0 -bf 2 -b_strategy 0 -s $3x$4 -b:v ${10} -c:a libfdk_aac -ab 64k -ac 2 -ar 44100 -movflags faststart $2
