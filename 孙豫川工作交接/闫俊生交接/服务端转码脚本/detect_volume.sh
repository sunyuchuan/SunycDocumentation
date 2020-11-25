#!/bin/bash

ffmpeg -hide_banner -i $1 -af volumedetect -vn -sn -f null /dev/null > $2 2>&1