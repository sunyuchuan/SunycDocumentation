#!/bin/bash
ffmpeg -f concat -safe 0  -i $1 -vcodec copy -acodec copy -movflags +faststart $2
