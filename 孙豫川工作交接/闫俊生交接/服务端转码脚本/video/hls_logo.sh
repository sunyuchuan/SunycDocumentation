#!/bin/bash
working_dir=$2
working_dir=${working_dir%/*}
mkdir -p $working_dir/SD

cmd="ffmpeg -i $1 -c:v libx264 -max_muxing_queue_size 8192 "


cmd=$cmd" -vf \"movie='$5', scale=$6:$7[watermask]; [in][watermask] overlay=$8:$9,fps=25[out]\" -strict -2 -flags -global_header -force_key_frames \"expr:gte(t,n_forced*5)\" -sc_threshold 0 -bf 2 -b_strategy 0 "


cmd=$cmd"-s $3x$4 -b:v ${10}k -threads 8 "
cmd=$cmd" -c:a libfdk_aac -ab 64k -ac 2 -ar 44100
      -hls_init_time 10 -hls_time 10 -hls_list_size 0
      -start_number 1 -hls_segment_filename $working_dir'/SD/video_SD-%04d.ts' $working_dir/SD/video_SD.m3u8"

eval $cmd

echo -e "#EXTM3U\n#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=320000\nSD/video_SD.m3u8" >> $2