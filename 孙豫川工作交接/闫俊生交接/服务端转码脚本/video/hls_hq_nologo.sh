#!/bin/bash
working_dir=$2
working_dir=${working_dir%/*}
mkdir -p $working_dir/HD

cmd="ffmpeg -i $1 -c:v libx264 -max_muxing_queue_size 8192 "

cmd=$cmd"-s $3x$4 -b:v ${10}k -threads 8 "
cmd=$cmd" -c:a libfdk_aac -ab 128k -ac 2 -ar 44100
      -hls_init_time 10 -hls_time 10 -hls_list_size 0
      -start_number 1 -hls_segment_filename $working_dir'/HD/video_HD-%04d.ts' $working_dir/HD/video_HD.m3u8"
echo $cmd
eval $cmd

echo -e "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1600000\nHD/video_HD.m3u8" >> $2