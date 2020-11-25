#!/bin/bash
ffmpeg -y -i $1 -vn -ab 128k -ac 1 -ar 8000 -f wav $2