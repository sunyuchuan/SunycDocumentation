#!/bin/bash

cmd="ffmpeg -y -i $1 -map_metadata -1 -c:v libx264 -max_muxing_queue_size 8192 "

cmd=$cmd" -s $4x$5 -b:v ${6}k  -threads 4 -c:a libfdk_aac -ab 64k -ac 2 -ar 44100 -movflags faststart $2"

eval $cmd


