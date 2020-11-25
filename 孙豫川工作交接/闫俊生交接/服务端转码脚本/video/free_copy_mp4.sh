#!/bin/bash
ffmpeg -i $1 -vcodec copy -af "volume=${3}dB" -ac 2 -ar 44100 -ab 128k -acodec libfdk_aac -y -movflags +faststart -f mp4 $2