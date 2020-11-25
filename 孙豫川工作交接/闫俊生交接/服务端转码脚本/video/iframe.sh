#!/bin/bash

ffmpeg -y -threads 2 -nostats -i $1 \
    -c:v libx264 -max_muxing_queue_size 1024 -vf "fps=25" -vf "movie='$6', scale=$7:$8[watermask]; [in][watermask] overlay=$9:${10}[out]" -strict -2 -force_key_frames "expr:gte(t,n_forced*0.5)" -sc_threshold 0 -bf 2 -b_strategy 0 -threads 8 \
    -c:a libfdk_aac -ab 64k -ac 2 -ar 44100 -s $2x$3 -b:v $4\
	-movflags faststart \
    -f mp4 $5 \