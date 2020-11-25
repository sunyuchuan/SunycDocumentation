#!/bin/bash

ffprobe $1 -show_entries format:stream -print_format json -v quiet > $2 2>&1