#!/bin/bash

ffmpeg -i $1 -vn -af loudnorm=I=-24.0:TP=-2.0:LRA=11.0:print_format=json -f null /dev/null > $2 2>&1
