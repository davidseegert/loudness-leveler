#!/bin/bash
# scripts/png_to_header.sh - Convert PNG icon to a C header file
# This script converts icon.png into a C header file named icon_png.h
# so it can be embedded directly in the executable.

if [ ! -f "icon.png" ]; then
    echo "Error: icon.png not found in the root directory."
    exit 1
fi

echo "Converting icon.png to icon_png.h..."
xxd -i icon.png > icon_png.h

echo "Conversion complete: icon_png.h created."
