#!/bin/bash

ffmpeg -ss $3 -y -i $1 -an -vframes 1 -f image2pipe $2